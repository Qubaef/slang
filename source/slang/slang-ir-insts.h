// slang-ir-insts.h
#ifndef SLANG_IR_INSTS_H_INCLUDED
#define SLANG_IR_INSTS_H_INCLUDED

// This file extends the core definitions in `ir.h`
// with a wider variety of concrete instructions,
// and a "builder" abstraction.
//
// TODO: the builder probably needs its own file.

#include "slang-capability.h"
#include "slang-compiler.h"
#include "slang-ir.h"
#include "slang-syntax.h"
#include "slang-type-layout.h"

namespace Slang {

class Decl;

struct IRCapabilitySet : IRInst
{
    IR_LEAF_ISA(CapabilitySet);

    CapabilitySet getCaps();
};

struct IRDecoration : IRInst
{
    IR_PARENT_ISA(Decoration)

    IRDecoration* getNextDecoration()
    {
        return as<IRDecoration>(getNextInst());
    }
};

// Associates an IR-level decoration with a source declaration
// in the high-level AST, that can be used to extract
// additional information that informs code emission.
struct IRHighLevelDeclDecoration : IRDecoration
{
    enum { kOp = kIROp_HighLevelDeclDecoration };
    IR_LEAF_ISA(HighLevelDeclDecoration)

    IRPtrLit* getDeclOperand() { return cast<IRPtrLit>(getOperand(0)); }
    Decl* getDecl() { return (Decl*) getDeclOperand()->getValue(); }
};

enum IRLoopControl
{
    kIRLoopControl_Unroll,
    kIRLoopControl_Loop,
};

struct IRLoopControlDecoration : IRDecoration
{
    enum { kOp = kIROp_LoopControlDecoration };
    IR_LEAF_ISA(LoopControlDecoration)

    IRConstant* getModeOperand() { return cast<IRConstant>(getOperand(0)); }

    IRLoopControl getMode()
    {
        return IRLoopControl(getModeOperand()->value.intVal);
    }
};


struct IRTargetSpecificDecoration : IRDecoration
{
    IR_PARENT_ISA(TargetSpecificDecoration)

    IRCapabilitySet* getTargetCapsOperand() { return cast<IRCapabilitySet>(getOperand(0)); }

    CapabilitySet getTargetCaps() { return getTargetCapsOperand()->getCaps(); }
};

struct IRTargetDecoration : IRTargetSpecificDecoration
{
    enum { kOp = kIROp_TargetDecoration };
    IR_LEAF_ISA(TargetDecoration)
};

struct IRTargetIntrinsicDecoration : IRTargetSpecificDecoration
{
    enum { kOp = kIROp_TargetIntrinsicDecoration };
    IR_LEAF_ISA(TargetIntrinsicDecoration)

    IRStringLit* getDefinitionOperand() { return cast<IRStringLit>(getOperand(1)); }

    UnownedStringSlice getDefinition()
    {
        return getDefinitionOperand()->getStringSlice();
    }
};

struct IRGLSLOuterArrayDecoration : IRDecoration
{
    enum { kOp = kIROp_GLSLOuterArrayDecoration };
    IR_LEAF_ISA(GLSLOuterArrayDecoration)

    IRStringLit* getOuterArraynameOperand() { return cast<IRStringLit>(getOperand(0)); }

    UnownedStringSlice getOuterArrayName()
    {
        return getOuterArraynameOperand()->getStringSlice();
    }
};

enum class IRInterpolationMode
{
    Linear,
    NoPerspective,
    NoInterpolation,

    Centroid,
    Sample,

    PerVertex,
};

struct IRInterpolationModeDecoration : IRDecoration
{
    enum { kOp = kIROp_InterpolationModeDecoration };
    IR_LEAF_ISA(InterpolationModeDecoration)

    IRConstant* getModeOperand() { return cast<IRConstant>(getOperand(0)); }

    IRInterpolationMode getMode()
    {
        return IRInterpolationMode(getModeOperand()->value.intVal);
    }
};

/// A decoration that provides a desired name to be used
/// in conjunction with the given instruction. Back-end
/// code generation may use this to help derive symbol
/// names, emit debug information, etc.
struct IRNameHintDecoration : IRDecoration
{
    enum { kOp = kIROp_NameHintDecoration };
    IR_LEAF_ISA(NameHintDecoration)

    IRStringLit* getNameOperand() { return cast<IRStringLit>(getOperand(0)); }

    UnownedStringSlice getName()
    {
        return getNameOperand()->getStringSlice();
    }
};

/// A decoration on a RTTIObject providing type size information.
struct IRRTTITypeSizeDecoration : IRDecoration
{
    enum { kOp = kIROp_RTTITypeSizeDecoration };
    IR_LEAF_ISA(RTTITypeSizeDecoration)

    IRIntLit* getTypeSizeOperand() { return cast<IRIntLit>(getOperand(0)); }
    IRIntegerValue getTypeSize()
    {
        return getTypeSizeOperand()->getValue();
    }
};

/// A decoration on `IRInterfaceType` that marks the size of `AnyValue` that should
/// be used to represent a polymorphic value of the interface.
struct IRAnyValueSizeDecoration : IRDecoration
{
    enum { kOp = kIROp_AnyValueSizeDecoration };
    IR_LEAF_ISA(AnyValueSizeDecoration)

    IRIntLit* getSizeOperand() { return cast<IRIntLit>(getOperand(0)); }
    IRIntegerValue getSize()
    {
        return getSizeOperand()->getValue();
    }
};

struct IRSpecializeDecoration : IRDecoration
{
    enum { kOp = kIROp_SpecializeDecoration };
    IR_LEAF_ISA(SpecializeDecoration)
};

struct IRComInterfaceDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_ComInterfaceDecoration
    };
    IR_LEAF_ISA(ComInterfaceDecoration)
};

struct IRCOMWitnessDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_COMWitnessDecoration
    };
    IR_LEAF_ISA(COMWitnessDecoration)

    IRInst* getWitnessTable() { return getOperand(0); }
};

/// A decoration on `IRParam`s that represent generic parameters,
/// marking the interface type that the generic parameter conforms to.
/// A generic parameter can have more than one `IRTypeConstraintDecoration`s
struct IRTypeConstraintDecoration : IRDecoration
{
    enum { kOp = kIROp_TypeConstraintDecoration };
    IR_LEAF_ISA(TypeConstraintDecoration)

    IRInst* getConstraintType() { return getOperand(0); }
};

#define IR_SIMPLE_DECORATION(NAME)      \
    struct IR##NAME : IRDecoration      \
    {                                   \
        enum { kOp = kIROp_##NAME };    \
    IR_LEAF_ISA(NAME)                   \
    };                                  \
    /**/

bool isSimpleDecoration(IROp op);

/// A decoration that indicates that a variable represents
/// a vulkan ray payload, and should have a location assigned
/// to it.
IR_SIMPLE_DECORATION(VulkanRayPayloadDecoration)

/// A decoration that indicates that a variable represents
/// a vulkan callable shader payload, and should have a location assigned
/// to it.
IR_SIMPLE_DECORATION(VulkanCallablePayloadDecoration)

/// A decoration that indicates that a variable represents
/// vulkan hit attributes, and should have a location assigned
/// to it.
IR_SIMPLE_DECORATION(VulkanHitAttributesDecoration)

struct IRRequireGLSLVersionDecoration : IRDecoration
{
    enum { kOp = kIROp_RequireGLSLVersionDecoration };
    IR_LEAF_ISA(RequireGLSLVersionDecoration)

    IRConstant* getLanguageVersionOperand() { return cast<IRConstant>(getOperand(0)); }

    Int getLanguageVersion()
    {
        return Int(getLanguageVersionOperand()->value.intVal);
    }
};

struct IRRequireSPIRVVersionDecoration : IRDecoration
{
    enum { kOp = kIROp_RequireSPIRVVersionDecoration };
    IR_LEAF_ISA(RequireGLSLVersionDecoration)

    IRConstant* getSPIRVVersionOperand() { return cast<IRConstant>(getOperand(0)); }
    IntegerLiteralValue getSPIRVVersion()
    {
        return getSPIRVVersionOperand()->value.intVal;
    }
};

struct IRRequireCUDASMVersionDecoration : IRDecoration
{
    enum { kOp = kIROp_RequireCUDASMVersionDecoration };
    IR_LEAF_ISA(RequireCUDASMVersionDecoration)

    IRConstant* getCUDASMVersionOperand() { return cast<IRConstant>(getOperand(0)); }
    IntegerLiteralValue getCUDASMVersion()
    {
        return getCUDASMVersionOperand()->value.intVal;
    }
};

struct IRRequireGLSLExtensionDecoration : IRDecoration
{
    enum { kOp = kIROp_RequireGLSLExtensionDecoration };
    IR_LEAF_ISA(RequireGLSLExtensionDecoration)

    IRStringLit* getExtensionNameOperand() { return cast<IRStringLit>(getOperand(0)); }

    UnownedStringSlice getExtensionName()
    {
        return getExtensionNameOperand()->getStringSlice();
    }
};

IR_SIMPLE_DECORATION(ReadNoneDecoration)
IR_SIMPLE_DECORATION(EarlyDepthStencilDecoration)
IR_SIMPLE_DECORATION(GloballyCoherentDecoration)
IR_SIMPLE_DECORATION(PreciseDecoration)
IR_SIMPLE_DECORATION(PublicDecoration)
IR_SIMPLE_DECORATION(HLSLExportDecoration)
IR_SIMPLE_DECORATION(KeepAliveDecoration)
IR_SIMPLE_DECORATION(RequiresNVAPIDecoration)
IR_SIMPLE_DECORATION(NoInlineDecoration)

struct IRNVAPIMagicDecoration : IRDecoration
{
    enum { kOp = kIROp_NVAPIMagicDecoration };
    IR_LEAF_ISA(NVAPIMagicDecoration)

    IRStringLit* getNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getName() { return getNameOperand()->getStringSlice(); }
};

struct IRNVAPISlotDecoration : IRDecoration
{
    enum { kOp = kIROp_NVAPISlotDecoration };
    IR_LEAF_ISA(NVAPISlotDecoration)

    IRStringLit* getRegisterNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getRegisterName() { return getRegisterNameOperand()->getStringSlice(); }

    IRStringLit* getSpaceNameOperand() { return cast<IRStringLit>(getOperand(1)); }
    UnownedStringSlice getSpaceName() { return getSpaceNameOperand()->getStringSlice(); }
};

struct IROutputControlPointsDecoration : IRDecoration
{
    enum { kOp = kIROp_OutputControlPointsDecoration };
    IR_LEAF_ISA(OutputControlPointsDecoration)

    IRIntLit* getControlPointCount() { return cast<IRIntLit>(getOperand(0)); }
};

struct IROutputTopologyDecoration : IRDecoration
{
    enum { kOp = kIROp_OutputTopologyDecoration };
    IR_LEAF_ISA(OutputTopologyDecoration)

    IRStringLit* getTopology() { return cast<IRStringLit>(getOperand(0)); }
};

struct IRPartitioningDecoration : IRDecoration
{
    enum { kOp = kIROp_PartitioningDecoration };
    IR_LEAF_ISA(PartitioningDecoration)

    IRStringLit* getPartitioning() { return cast<IRStringLit>(getOperand(0)); }
};

struct IRDomainDecoration : IRDecoration
{
    enum { kOp = kIROp_DomainDecoration };
    IR_LEAF_ISA(DomainDecoration)

    IRStringLit* getDomain() { return cast<IRStringLit>(getOperand(0)); }
};

struct IRMaxVertexCountDecoration : IRDecoration
{
    enum { kOp = kIROp_MaxVertexCountDecoration };
    IR_LEAF_ISA(MaxVertexCountDecoration)

    IRIntLit* getCount() { return cast<IRIntLit>(getOperand(0)); }
};

struct IRInstanceDecoration : IRDecoration
{
    enum { kOp = kIROp_InstanceDecoration };
    IR_LEAF_ISA(InstanceDecoration)

    IRIntLit* getCount() { return cast<IRIntLit>(getOperand(0)); }
};

struct IRNumThreadsDecoration : IRDecoration
{
    enum { kOp = kIROp_NumThreadsDecoration };
    IR_LEAF_ISA(NumThreadsDecoration)

    IRIntLit* getX() { return cast<IRIntLit>(getOperand(0)); }
    IRIntLit* getY() { return cast<IRIntLit>(getOperand(1)); }
    IRIntLit* getZ() { return cast<IRIntLit>(getOperand(2)); }

    IRIntLit* getExtentAlongAxis(int axis) { return cast<IRIntLit>(getOperand(axis)); }
};

struct IREntryPointDecoration : IRDecoration
{
    enum { kOp = kIROp_EntryPointDecoration };
    IR_LEAF_ISA(EntryPointDecoration)

    IRIntLit* getProfileInst() { return cast<IRIntLit>(getOperand(0)); }
    Profile getProfile() { return Profile(Profile::RawVal(getIntVal(getProfileInst()))); }

    IRStringLit* getName()  { return cast<IRStringLit>(getOperand(1)); }
    IRStringLit* getModuleName() { return cast<IRStringLit>(getOperand(2)); }
};

struct IRGeometryInputPrimitiveTypeDecoration: IRDecoration
{
    IR_PARENT_ISA(GeometryInputPrimitiveTypeDecoration)
};

IR_SIMPLE_DECORATION(PointInputPrimitiveTypeDecoration)
IR_SIMPLE_DECORATION(LineInputPrimitiveTypeDecoration)
IR_SIMPLE_DECORATION(TriangleInputPrimitiveTypeDecoration)
IR_SIMPLE_DECORATION(LineAdjInputPrimitiveTypeDecoration)
IR_SIMPLE_DECORATION(TriangleAdjInputPrimitiveTypeDecoration)

    /// This is a bit of a hack. The problem is that when GLSL legalization takes place
    /// the parameters from the entry point are globalized *and* potentially split
    /// So even if we did copy a suitable decoration onto the globalized parameters,
    /// it would potentially output multiple times without extra logic.
    /// Using this decoration we can copy the StreamOut type to the entry point, and then
    /// emit as part of entry point attribute emitting.  
struct IRStreamOutputTypeDecoration : IRDecoration
{
    enum { kOp = kIROp_StreamOutputTypeDecoration };
    IR_LEAF_ISA(StreamOutputTypeDecoration)

    IRHLSLStreamOutputType* getStreamType() { return cast<IRHLSLStreamOutputType>(getOperand(0)); }
};

    /// A decoration that marks a value as having linkage. 
    /// A value with linkage is either exported from its module,
    /// or will have a definition imported from another module.
    /// In either case, it requires a mangled name to use when
    /// matching imports and exports.
struct IRLinkageDecoration : IRDecoration
{
    IR_PARENT_ISA(LinkageDecoration)

    IRStringLit* getMangledNameOperand() { return cast<IRStringLit>(getOperand(0)); }

    UnownedStringSlice getMangledName()
    {
        return getMangledNameOperand()->getStringSlice();
    }
};

struct IRImportDecoration : IRLinkageDecoration
{
    enum { kOp = kIROp_ImportDecoration };
    IR_LEAF_ISA(ImportDecoration)
};

struct IRExportDecoration : IRLinkageDecoration
{
    enum { kOp = kIROp_ExportDecoration };
    IR_LEAF_ISA(ExportDecoration)
};

struct IRExternCppDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_ExternCppDecoration
    };
    IR_LEAF_ISA(ExternCppDecoration)

    IRStringLit* getNameOperand() { return cast<IRStringLit>(getOperand(0)); }

    UnownedStringSlice getName() { return getNameOperand()->getStringSlice(); }
};

struct IRDllImportDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_DllImportDecoration
    };
    IR_LEAF_ISA(DllImportDecoration)

    IRStringLit* getLibraryNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getLibraryName() { return getLibraryNameOperand()->getStringSlice(); }

    IRStringLit* getFunctionNameOperand() { return cast<IRStringLit>(getOperand(1)); }
    UnownedStringSlice getFunctionName() { return getFunctionNameOperand()->getStringSlice(); }
};

struct IRDllExportDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_DllExportDecoration
    };
    IR_LEAF_ISA(DllExportDecoration)

    IRStringLit* getFunctionNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getFunctionName() { return getFunctionNameOperand()->getStringSlice(); }
};

struct IRFormatDecoration : IRDecoration
{
    enum { kOp = kIROp_FormatDecoration };
    IR_LEAF_ISA(FormatDecoration)

    IRConstant* getFormatOperand() { return cast<IRConstant>(getOperand(0)); }

    ImageFormat getFormat()
    {
        return ImageFormat(getFormatOperand()->value.intVal);
    }
};

IR_SIMPLE_DECORATION(UnsafeForceInlineEarlyDecoration)

struct IRNaturalSizeAndAlignmentDecoration : IRDecoration
{
    enum { kOp = kIROp_NaturalSizeAndAlignmentDecoration };
    IR_LEAF_ISA(NaturalSizeAndAlignmentDecoration)

    IRIntLit* getSizeOperand() { return cast<IRIntLit>(getOperand(0)); }
    IRIntLit* getAlignmentOperand() { return cast<IRIntLit>(getOperand(1)); }

    IRIntegerValue getSize() { return getSizeOperand()->getValue(); }
    IRIntegerValue getAlignment() { return getAlignmentOperand()->getValue(); }
};

struct IRNaturalOffsetDecoration : IRDecoration
{
    enum { kOp = kIROp_NaturalOffsetDecoration };
    IR_LEAF_ISA(NaturalOffsetDecoration)

    IRIntLit* getOffsetOperand() { return cast<IRIntLit>(getOperand(0)); }

    IRIntegerValue getOffset() { return getOffsetOperand()->getValue(); }
};

struct IRBuiltinDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_BuiltinDecoration
    };
    IR_LEAF_ISA(BuiltinDecoration)
};

struct IRSequentialIDDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_SequentialIDDecoration
    };
    IR_LEAF_ISA(SequentialIDDecoration)

    IRIntLit* getSequentialIDOperand() { return cast<IRIntLit>(getOperand(0)); }
    IRIntegerValue getSequentialID() { return getSequentialIDOperand()->getValue(); }
};

struct IRJVPDerivativeReferenceDecoration : IRDecoration
{
    enum
    {
        kOp = kIROp_JVPDerivativeReferenceDecoration
    };
    IR_LEAF_ISA(JVPDerivativeReferenceDecoration)

    IRFunc* getJVPFunc() { return as<IRFunc>(getOperand(0)); }
};


// An instruction that replaces the function symbol
// with it's derivative function.
struct IRJVPDifferentiate : IRInst
{
    enum
    {
        kOp = kIROp_JVPDifferentiate
    };
    // The base function for the call.
    IRUse base;
    IRInst* getBaseFn() { return getOperand(0); }

    IR_LEAF_ISA(JVPDifferentiate)
};

// An instruction that specializes another IR value
// (representing a generic) to a particular set of generic arguments 
// (instructions representing types, witness tables, etc.)
struct IRSpecialize : IRInst
{
    // The "base" for the call is the generic to be specialized
    IRUse base;
    IRInst* getBase() { return getOperand(0); }

    // after the generic value come the arguments
    UInt getArgCount() { return getOperandCount() - 1; }
    IRInst* getArg(UInt index) { return getOperand(index + 1); }

    IR_LEAF_ISA(Specialize)
};

// An instruction that looks up the implementation
// of an interface operation identified by `requirementDeclRef`
// in the witness table `witnessTable` which should
// hold the conformance information for a specific type.
struct IRLookupWitnessMethod : IRInst
{
    IRUse witnessTable;
    IRUse requirementKey;

    IRInst* getWitnessTable() { return witnessTable.get(); }
    IRInst* getRequirementKey() { return requirementKey.get(); }

    IR_LEAF_ISA(lookup_interface_method)
};

// Returns the sequential ID of an RTTI object.
struct IRGetSequentialID : IRInst
{
    IR_LEAF_ISA(GetSequentialID)

    IRInst* getRTTIOperand() { return getOperand(0); }
};

struct IRLookupWitnessTable : IRInst
{
    IRUse sourceType;
    IRUse interfaceType;
};

/// Allocates space from local stack.
///
struct IRAlloca : IRInst
{
    IR_LEAF_ISA(Alloca)

    IRInst* getAllocSize() { return getOperand(0); }
};

/// Packs a value into an `AnyValue`.
/// Return type is `IRAnyValueType`.
struct IRPackAnyValue : IRInst
{
    IR_LEAF_ISA(PackAnyValue)
    IRInst* getValue() { return getOperand(0); }
};

/// Unpacks a `AnyValue` value into a concrete type.
/// Operand must have `IRAnyValueType`.
struct IRUnpackAnyValue : IRInst
{
    IR_LEAF_ISA(UnpackAnyValue)
    IRInst* getValue() { return getOperand(0); }
};

// Layout decorations

    /// A decoration that marks a field key as having been associated
    /// with a particular simple semantic (e.g., `COLOR` or `SV_Position`,
    /// but not a `register` semantic).
    ///
    /// This is currently needed so that we can round-trip HLSL `struct`
    /// types that get used for varying input/output. This is an unfortunate
    /// case where some amount of "layout" information can't just come
    /// in via the `TypeLayout` part of things.
    ///
struct IRSemanticDecoration : public IRDecoration
{
    IR_LEAF_ISA(SemanticDecoration)

    IRStringLit* getSemanticNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getSemanticName() { return getSemanticNameOperand()->getStringSlice(); }

    IRIntLit* getSemanticIndexOperand() { return cast<IRIntLit>(getOperand(1)); }
    int getSemanticIndex() { return int(getIntVal(getSemanticIndexOperand())); }
};

struct IRStageAccessDecoration : public IRDecoration
{
    IR_PARENT_ISA(StageAccessDecoration)

    Int getStageCount() { return (Int) getOperandCount(); }
    IRStringLit* getStageOperand(Int index) { return cast<IRStringLit>(getOperand(index)); }
    UnownedStringSlice getStageName(Int index) { return getStageOperand(index)->getStringSlice(); }
};

struct IRStageReadAccessDecoration : public IRStageAccessDecoration
{
    IR_LEAF_ISA(StageReadAccessDecoration)
};

struct IRStageWriteAccessDecoration : public IRStageAccessDecoration
{
    IR_LEAF_ISA(StageWriteAccessDecoration)
};

struct IRPayloadDecoration : public IRDecoration
{
    IR_LEAF_ISA(PayloadDecoration)
};

    /// An attribute that can be attached to another instruction as an operand.
    ///
    /// Attributes serve a similar role to decorations, in that both are ways
    /// to attach additional information to an instruction, where the operand
    /// of the attribute/decoration identifies the purpose of the additional
    /// information.
    ///
    /// The key difference between decorations and attributes is that decorations
    /// are stored as children of an instruction (in terms of the ownership
    /// hierarchy), while attributes are referenced as operands.
    ///
    /// The key benefit of having attributes be operands is that they must
    /// be present at the time an instruction is created, which means that
    /// they can affect the conceptual value/identity of an instruction
    /// in cases where we deduplicate/hash instructions by value.
    ///
struct IRAttr : public IRInst
{
    IR_PARENT_ISA(Attr);
};

    /// An attribute that specifies layout information for a single resource kind.
struct IRLayoutResourceInfoAttr : public IRAttr
{
    IR_PARENT_ISA(LayoutResourceInfoAttr);

    IRIntLit* getResourceKindInst() { return cast<IRIntLit>(getOperand(0)); }
    LayoutResourceKind getResourceKind() { return LayoutResourceKind(getIntVal(getResourceKindInst())); }
};

    /// An attribute that specifies offset information for a single resource kind.
    ///
    /// This operation can appear as `varOffset(kind, offset)` or
    /// `varOffset(kind, offset, space)`. The latter form is only
    /// used when `space` is non-zero.
    ///
struct IRVarOffsetAttr : public IRLayoutResourceInfoAttr
{
    IR_LEAF_ISA(VarOffsetAttr);

    IRIntLit* getOffsetInst() { return cast<IRIntLit>(getOperand(1)); }
    UInt getOffset() { return UInt(getIntVal(getOffsetInst())); }

    IRIntLit* getSpaceInst()
    {
        if(getOperandCount() > 2)
            return cast<IRIntLit>(getOperand(2));
        return nullptr;
    }

    UInt getSpace()
    {
        if(auto spaceInst = getSpaceInst())
            return UInt(getIntVal(spaceInst));
        return 0;
    }
};

    /// An attribute that specifies the error type a function is throwing
struct IRFuncThrowTypeAttr : IRAttr
{
    IR_LEAF_ISA(FuncThrowTypeAttr)

    IRType* getErrorType() { return (IRType*)getOperand(0); }
};

    /// An attribute that specifies size information for a single resource kind.
struct IRTypeSizeAttr : public IRLayoutResourceInfoAttr
{
    IR_LEAF_ISA(TypeSizeAttr);

    IRIntLit* getSizeInst() { return cast<IRIntLit>(getOperand(1)); }
    LayoutSize getSize() { return LayoutSize::fromRaw(LayoutSize::RawValue(getIntVal(getSizeInst()))); }
    size_t getFiniteSize() { return getSize().getFiniteValue(); }
};

// Layout

    /// Base type for instructions that represent layout information.
    ///
    /// Layout instructions are effectively just meta-data constants.
    ///
struct IRLayout : IRInst
{
    IR_PARENT_ISA(Layout)
};

struct IRVarLayout;

    /// An attribute to specify that a layout has another layout attached for "pending" data.
    ///
    /// "Pending" data refers to the parts of a type or variable that
    /// couldn't be laid out until the concrete types for existential
    /// type slots were filled in. The layout of pending data may not
    /// be contiguous with the layout of the original type/variable.
    ///
struct IRPendingLayoutAttr : IRAttr
{
    IR_LEAF_ISA(PendingLayoutAttr);

    IRLayout* getLayout() { return cast<IRLayout>(getOperand(0)); }
};

    /// Layout information for a type.
    ///
    /// The most important thing this instruction provides is the
    /// resource usage (aka "size") of the type for each of the
    /// resource kinds it consumes.
    ///
    /// Subtypes of `IRTypeLayout` will include additional type-specific
    /// operands or attributes. For example, a type layout for a
    /// `struct` type will include offset information for its fields.
    ///
struct IRTypeLayout : IRLayout
{
    IR_PARENT_ISA(TypeLayout);

        /// Find the attribute that stores offset information for `kind`.
        ///
        /// Returns null if no attribute is found, indicating that this
        /// type does not consume any resources of `kind`.
        ///
    IRTypeSizeAttr* findSizeAttr(LayoutResourceKind kind);

        /// Get all the attributes representing size information.
    IROperandList<IRTypeSizeAttr> getSizeAttrs();

        /// Unwrap any layers of array-ness and return the outer-most non-array type.
    IRTypeLayout* unwrapArray();

        /// Get the layout for pending data, if present.
    IRTypeLayout* getPendingDataTypeLayout();

        /// A builder for constructing `IRTypeLayout`s
    struct Builder
    {
            /// Begin building.
            ///
            /// The `irBuilder` will be used to construct the
            /// type layout and any additional instructions required.
            ///
        Builder(IRBuilder* irBuilder);

            /// Add `size` units of resource `kind` to the resource usage of this type.
        void addResourceUsage(
            LayoutResourceKind  kind,
            LayoutSize          size);

            /// Add the resource usage specified by `sizeAttr`.
        void addResourceUsage(IRTypeSizeAttr* sizeAttr);

            /// Add all resource usage from `typeLayout`.
        void addResourceUsageFrom(IRTypeLayout* typeLayout);

            /// Set the (optional) layout for pending data.
        void setPendingTypeLayout(
            IRTypeLayout* typeLayout)
        {
            m_pendingTypeLayout = typeLayout;
        }

            /// Build a type layout according to the information specified so far.
        IRTypeLayout* build();

    protected:
        // The following services are provided so that
        // subtypes of `IRTypeLayout` can provide their
        // own `Builder` subtypes that construct appropriate
        // layouts.

            /// Override to customize the opcode of the generated layout.
        virtual IROp getOp() { return kIROp_TypeLayoutBase; }

            /// Override to add additional operands to the generated layout.
        virtual void addOperandsImpl(List<IRInst*>&) {}

            /// Override to add additional attributes to the generated layout.
        virtual void addAttrsImpl(List<IRInst*>&) {}

            /// Use to access the underlying IR builder.
        IRBuilder* getIRBuilder() { return m_irBuilder; };

    private:
        void addOperands(List<IRInst*>&);
        void addAttrs(List<IRInst*>& ioOperands);

        IRBuilder* m_irBuilder = nullptr;
        IRTypeLayout* m_pendingTypeLayout = nullptr;

        struct ResInfo
        {
            LayoutResourceKind  kind = LayoutResourceKind::None;
            LayoutSize          size = 0;
        };
        ResInfo m_resInfos[SLANG_PARAMETER_CATEGORY_COUNT];
    };
};

    /// Type layout for parameter groups (constant buffers and parameter blocks)
struct IRParameterGroupTypeLayout : IRTypeLayout
{
private:
    typedef IRTypeLayout Super;

public:
    IR_LEAF_ISA(ParameterGroupTypeLayout)

    IRVarLayout* getContainerVarLayout()
    {
        return cast<IRVarLayout>(getOperand(0));
    }

    IRVarLayout* getElementVarLayout()
    {
        return cast<IRVarLayout>(getOperand(1));
    }

    // TODO: There shouldn't be a need for the IR to store an "offset" element type layout,
    // but there are just enough places that currently use that information so that removing
    // it would require some careful refactoring.
    //
    IRTypeLayout* getOffsetElementTypeLayout()
    {
        return cast<IRTypeLayout>(getOperand(2));
    }

        /// Specialized builder for parameter group type layouts.
    struct Builder : Super::Builder
    {
    public:
        Builder(IRBuilder* irBuilder)
            : Super::Builder(irBuilder)
        {}

        void setContainerVarLayout(IRVarLayout* varLayout)
        {
            m_containerVarLayout = varLayout;
        }

        void setElementVarLayout(IRVarLayout* varLayout)
        {
            m_elementVarLayout = varLayout;
        }

        void setOffsetElementTypeLayout(IRTypeLayout* typeLayout)
        {
            m_offsetElementTypeLayout = typeLayout;
        }

        IRParameterGroupTypeLayout* build();

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_ParameterGroupTypeLayout; }
        void addOperandsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        IRVarLayout* m_containerVarLayout;
        IRVarLayout* m_elementVarLayout;
        IRTypeLayout* m_offsetElementTypeLayout;
    };
};

    /// Specialized layout information for array types
struct IRArrayTypeLayout : IRTypeLayout
{
    typedef IRTypeLayout Super;

    IR_LEAF_ISA(ArrayTypeLayout)

    IRTypeLayout* getElementTypeLayout()
    {
        return cast<IRTypeLayout>(getOperand(0));
    }

    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder, IRTypeLayout* elementTypeLayout)
            : Super::Builder(irBuilder)
            , m_elementTypeLayout(elementTypeLayout)
        {}

        IRArrayTypeLayout* build()
        {
            return cast<IRArrayTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_ArrayTypeLayout; }
        void addOperandsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        IRTypeLayout* m_elementTypeLayout;
    };
};

    /// Specialized layout information for stream-output types
struct IRStreamOutputTypeLayout : IRTypeLayout
{
    typedef IRTypeLayout Super;

    IR_LEAF_ISA(StreamOutputTypeLayout)

    IRTypeLayout* getElementTypeLayout()
    {
        return cast<IRTypeLayout>(getOperand(0));
    }

    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder, IRTypeLayout* elementTypeLayout)
            : Super::Builder(irBuilder)
            , m_elementTypeLayout(elementTypeLayout)
        {}

        IRArrayTypeLayout* build()
        {
            return cast<IRArrayTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_StreamOutputTypeLayout; }
        void addOperandsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        IRTypeLayout* m_elementTypeLayout;
    };
};

    /// Specialized layout information for matrix types
struct IRMatrixTypeLayout : IRTypeLayout
{
    typedef IRTypeLayout Super;

    IR_LEAF_ISA(MatrixTypeLayout)

    MatrixLayoutMode getMode()
    {
        return MatrixLayoutMode(getIntVal(cast<IRIntLit>(getOperand(0))));
    }

    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder, MatrixLayoutMode mode);

        IRMatrixTypeLayout* build()
        {
            return cast<IRMatrixTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_MatrixTypeLayout; }
        void addOperandsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        IRInst* m_modeInst = nullptr;
    };
};

    /// Attribute that specifies the layout for one field of a structure type.
struct IRStructFieldLayoutAttr : IRAttr
{
    IR_LEAF_ISA(StructFieldLayoutAttr)

    IRInst* getFieldKey()
    {
        return getOperand(0);
    }

    IRVarLayout* getLayout()
    {
        return cast<IRVarLayout>(getOperand(1));
    }
};

    /// Specialized layout information for structure types.
struct IRStructTypeLayout : IRTypeLayout
{
    IR_LEAF_ISA(StructTypeLayout)

    typedef IRTypeLayout Super;

        /// Get all of the attributes that represent field layouts.
    IROperandList<IRStructFieldLayoutAttr> getFieldLayoutAttrs()
    {
        return findAttrs<IRStructFieldLayoutAttr>();
    }

        /// Get the number of fields for which layout information is stored.
    UInt getFieldCount()
    {
        return getFieldLayoutAttrs().getCount();
    }

        /// Get the layout information for a field by `index`
    IRVarLayout* getFieldLayout(UInt index)
    {
        return getFieldLayoutAttrs()[index]->getLayout();
    }

        /// Specialized builder for structure type layouts.
    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder)
            : Super::Builder(irBuilder)
        {}

        void addField(IRInst* key, IRVarLayout* layout)
        {
            FieldInfo info;
            info.key = key;
            info.layout = layout;
            m_fields.add(info);
        }

        IRStructTypeLayout* build()
        {
            return cast<IRStructTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_StructTypeLayout; }
        void addAttrsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        struct FieldInfo
        {
            IRInst* key;
            IRVarLayout* layout;
        };

        List<FieldInfo> m_fields;
    };
};

    /// Attribute that represents the layout for one case of a union type
struct IRCaseTypeLayoutAttr : IRAttr
{
    IR_LEAF_ISA(CaseTypeLayoutAttr);

    IRTypeLayout* getTypeLayout()
    {
        return cast<IRTypeLayout>(getOperand(0));
    }
};

    /// Specialized layout information for tagged union types
struct IRTaggedUnionTypeLayout : IRTypeLayout
{
    typedef IRTypeLayout Super;

    IR_LEAF_ISA(TaggedUnionTypeLayout)

        /// Get the (byte) offset of the tagged union's tag (aka "discriminator") field
    LayoutSize getTagOffset()
    {
        return LayoutSize::fromRaw(LayoutSize::RawValue(getIntVal(cast<IRIntLit>(getOperand(0)))));
    }

        /// Get all the attributes representing layouts for the difference cases
    IROperandList<IRCaseTypeLayoutAttr> getCaseTypeLayoutAttrs()
    {
        return findAttrs<IRCaseTypeLayoutAttr>();
    }

        /// Get the number of cases for which layout information is stored
    UInt getCaseCount()
    {
        return getCaseTypeLayoutAttrs().getCount();
    }

        /// Get the layout information for the case at the given `index`
    IRTypeLayout* getCaseTypeLayout(UInt index)
    {
        return getCaseTypeLayoutAttrs()[index]->getTypeLayout();
    }

        /// Specialized builder for tagged union type layouts
    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder, LayoutSize tagOffset);

        void addCaseTypeLayout(IRTypeLayout* typeLayout);

        IRTaggedUnionTypeLayout* build()
        {
            return cast<IRTaggedUnionTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_TaggedUnionTypeLayout; }
        void addOperandsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;
        void addAttrsImpl(List<IRInst*>& ioOperands) SLANG_OVERRIDE;

        IRInst* m_tagOffset = nullptr;
        List<IRAttr*> m_caseTypeLayoutAttrs;
    };
};

    /// Type layout for an existential/interface type.
struct IRExistentialTypeLayout : IRTypeLayout
{
    typedef IRTypeLayout Super;

    IR_LEAF_ISA(ExistentialTypeLayout)

    struct Builder : Super::Builder
    {
        Builder(IRBuilder* irBuilder)
            : Super::Builder(irBuilder)
        {}

        IRExistentialTypeLayout* build()
        {
            return cast<IRExistentialTypeLayout>(Super::Builder::build());
        }

    protected:
        IROp getOp() SLANG_OVERRIDE { return kIROp_ExistentialTypeLayout; }
    };
};


    /// Layout information for an entry point
struct IREntryPointLayout : IRLayout
{
    IR_LEAF_ISA(EntryPointLayout)

        /// Get the layout information for the entry point parameters.
        ///
        /// The parameters layout will either be a structure type layout
        /// with one field per parameter, or a parameter group type
        /// layout wrapping such a structure, if the entry point parameters
        /// needed to be allocated into a constant buffer.
        ///
    IRVarLayout* getParamsLayout()
    {
        return cast<IRVarLayout>(getOperand(0));
    }

        /// Get the layout information for the entry point result.
        ///
        /// This represents the return value of the entry point.
        /// Note that it does *not* represent all of the entry
        /// point outputs, because the parameter list may also
        /// contain `out` or `inout` parameters.
        ///
    IRVarLayout* getResultLayout()
    {
        return cast<IRVarLayout>(getOperand(1));
    }
};

    /// Given an entry-point layout, extract the layout for the parameters struct.
IRStructTypeLayout* getScopeStructLayout(IREntryPointLayout* scopeLayout);

    /// Attribute that associates a variable layout with a known stage.
struct IRStageAttr : IRAttr
{
    IR_LEAF_ISA(StageAttr);

    IRIntLit* getStageOperand() { return cast<IRIntLit>(getOperand(0)); }
    Stage getStage() { return Stage(getIntVal(getStageOperand())); }
};

    /// Base type for attributes that associate a variable layout with a semantic name and index.
struct IRSemanticAttr : IRAttr
{
    IR_PARENT_ISA(SemanticAttr);

    IRStringLit* getNameOperand() { return cast<IRStringLit>(getOperand(0)); }
    UnownedStringSlice getName() { return getNameOperand()->getStringSlice(); }

    IRIntLit* getIndexOperand() { return cast<IRIntLit>(getOperand(1)); }
    UInt getIndex() { return UInt(getIntVal(getIndexOperand())); }
};

    /// Attribute that associates a variable with a system-value semantic name and index
struct IRSystemValueSemanticAttr : IRSemanticAttr
{
    IR_LEAF_ISA(SystemValueSemanticAttr);
};

    /// Attribute that associates a variable with a user-defined semantic name and index
struct IRUserSemanticAttr : IRSemanticAttr
{
    IR_LEAF_ISA(UserSemanticAttr);
};

    /// Layout infromation for a single parameter/field
struct IRVarLayout : IRLayout
{
    IR_LEAF_ISA(VarLayout)

        /// Get the type layout information for this variable
    IRTypeLayout* getTypeLayout() { return cast<IRTypeLayout>(getOperand(0)); }

        /// Get all the attributes representing resource-kind-specific offsets
    IROperandList<IRVarOffsetAttr> getOffsetAttrs();

        /// Find the offset information (if present) for the given resource `kind`
    IRVarOffsetAttr* findOffsetAttr(LayoutResourceKind kind);

        /// Does this variable use any resources of the given `kind`?
    bool usesResourceKind(LayoutResourceKind kind);

        /// Get the fixed/known stage that this variable is associated with.
        ///
        /// This will be a specific stage for entry-point parameters, but
        /// will be `Stage::Unknown` for any parameter that is not bound
        /// solely to one entry point.
        ///
    Stage getStage();

        /// Find the system-value semantic attribute for this variable, if any.
    IRSystemValueSemanticAttr* findSystemValueSemanticAttr();

        /// Get the (optional) layout for any "pending" data assocaited with this variable.
    IRVarLayout* getPendingVarLayout();

        /// Builder for construction `IRVarLayout`s in a stateful fashion
    struct Builder
    {
            /// Begin building a variable layout with the given `typeLayout`
            ///
            /// The result layout and any instructions needed along the way
            /// will be allocated with `irBuilder`.
            ///
        Builder(
            IRBuilder*      irBuilder,
            IRTypeLayout*   typeLayout);

            /// Represents resource-kind-specific offset information
        struct ResInfo
        {
            LayoutResourceKind  kind = LayoutResourceKind::None;
            UInt                offset = 0;
            UInt                space = 0;
        };

            /// Has any resource usage/offset been registered for the given resource `kind`?
        bool usesResourceKind(LayoutResourceKind kind);

            /// Either fetch or add a `ResInfo` record for `kind` and return it
        ResInfo* findOrAddResourceInfo(LayoutResourceKind kind);

            /// Set the (optional) variable layout for pending data.
        void setPendingVarLayout(IRVarLayout* varLayout)
        {
            m_pendingVarLayout = varLayout;
        }

            /// Set the (optional) system-valeu semantic for this variable.
        void setSystemValueSemantic(String const& name, UInt index);

            /// Set the (optional) user-defined semantic for this variable.
        void setUserSemantic(String const& name, UInt index);

            /// Set the (optional) known stage for this variable.
        void setStage(Stage stage);

            /// Clone all of the layout information from the `other` layout, except for offsets.
            ///
            /// This is convenience when one wants to build a variable layout "like that other one, but..."
        void cloneEverythingButOffsetsFrom(
            IRVarLayout* other);

            /// Build a variable layout using the current state that has been set.
        IRVarLayout* build();

    private:
        IRBuilder* m_irBuilder;
        IRBuilder* getIRBuilder() { return m_irBuilder; };

        IRTypeLayout* m_typeLayout = nullptr;
        IRVarLayout* m_pendingVarLayout = nullptr;

        IRSystemValueSemanticAttr* m_systemValueSemantic = nullptr;
        IRUserSemanticAttr* m_userSemantic = nullptr;
        IRStageAttr* m_stageAttr = nullptr;

        ResInfo m_resInfos[SLANG_PARAMETER_CATEGORY_COUNT];
    };
};

bool isVaryingResourceKind(LayoutResourceKind kind);
bool isVaryingParameter(IRTypeLayout* typeLayout);
bool isVaryingParameter(IRVarLayout* varLayout);

    /// Associate layout information with an instruction.
    ///
    /// This decoration is used in three main ways:
    ///
    /// * To attach an `IRVarLayout` to an `IRGlobalParam` or entry-point `IRParam` representing a shader parameter
    /// * To attach an `IREntryPointLayout` to an `IRFunc` representing an entry point
    /// * To attach an `IRTaggedUnionTypeLayout` to an `IRTaggedUnionType`
    ///
struct IRLayoutDecoration : IRDecoration
{
    enum { kOp = kIROp_LayoutDecoration };
    IR_LEAF_ISA(LayoutDecoration)

        /// Get the layout that is being attached to the parent instruction
    IRLayout* getLayout() { return cast<IRLayout>(getOperand(0)); }
};

//

struct IRCall : IRInst
{
    IR_LEAF_ISA(Call)

    IRInst* getCallee() { return getOperand(0); }

    UInt getArgCount() { return getOperandCount() - 1; }
    IRUse* getArgs() { return getOperands() + 1; }
    IRInst* getArg(UInt index) { return getOperand(index + 1); }
};

struct IRLoad : IRInst
{
    IRUse ptr;
    IR_LEAF_ISA(Load)

    IRInst* getPtr() { return ptr.get(); }
};

struct IRStore : IRInst
{
    IRUse ptr;
    IRUse val;
    IR_LEAF_ISA(Store)

    IRInst* getPtr() { return ptr.get(); }
    IRInst* getVal() { return val.get(); }
};

struct IRFieldExtract : IRInst
{
    IRUse   base;
    IRUse   field;

    IRInst* getBase() { return base.get(); }
    IRInst* getField() { return field.get(); }
    IR_LEAF_ISA(FieldExtract)

};

struct IRFieldAddress : IRInst
{
    IRUse   base;
    IRUse   field;

    IRInst* getBase() { return base.get(); }
    IRInst* getField() { return field.get(); }
    IR_LEAF_ISA(FieldAddress)

};

struct IRGetElement : IRInst
{
    IR_LEAF_ISA(getElement);
    IRInst* getBase() { return getOperand(0); }
    IRInst* getIndex() { return getOperand(1); }
};

struct IRGetElementPtr : IRInst
{
    IR_LEAF_ISA(getElementPtr);
    IRInst* getBase() { return getOperand(0); }
    IRInst* getIndex() { return getOperand(1); }
};

struct IRGetNativePtr : IRInst
{
    IR_LEAF_ISA(GetNativePtr);
    IRInst* getElementType() { return getOperand(0); }
};

struct IRGetManagedPtrWriteRef : IRInst
{
    IR_LEAF_ISA(GetManagedPtrWriteRef);
    IRInst* getPtrToManagedPtr() { return getOperand(0); }
};

struct IRGetAddress : IRInst
{
    IR_LEAF_ISA(getAddr);
};

struct IRImageSubscript : IRInst
{
    IR_LEAF_ISA(ImageSubscript);
    IRInst* getImage() { return getOperand(0); }
    IRInst* getCoord() { return getOperand(1); }
};

struct IRImageLoad : IRInst
{
    IR_LEAF_ISA(ImageLoad);
    IRInst* getImage() { return getOperand(0); }
    IRInst* getCoord() { return getOperand(1); }
};

struct IRImageStore : IRInst
{
    IR_LEAF_ISA(ImageStore);
    IRInst* getImage() { return getOperand(0); }
    IRInst* getCoord() { return getOperand(1); }
    IRInst* getValue() { return getOperand(2); }
};
// Terminators

struct IRReturn : IRTerminatorInst
{
    IR_LEAF_ISA(Return);

    IRInst* getVal() { return getOperand(0); }
};

struct IRDiscard : IRTerminatorInst
{};

// Signals that this point in the code should be unreachable.
// We can/should emit a dataflow error if we can ever determine
// that a block ending in one of these can actually be
// executed.
struct IRUnreachable : IRTerminatorInst
{
    IR_PARENT_ISA(Unreachable);
};

struct IRMissingReturn : IRUnreachable
{
    IR_LEAF_ISA(MissingReturn);
};

struct IRBlock;

struct IRUnconditionalBranch : IRTerminatorInst
{
    IRUse block;

    IRBlock* getTargetBlock() { return (IRBlock*)block.get(); }

    UInt getArgCount();
    IRUse* getArgs();
    IRInst* getArg(UInt index);

    IR_PARENT_ISA(UnconditionalBranch);
};

// Special cases of unconditional branch, to handle
// structured control flow:
struct IRBreak : IRUnconditionalBranch {};
struct IRContinue : IRUnconditionalBranch {};

// The start of a loop is a special control-flow
// instruction, that records relevant information
// about the loop structure:
struct IRLoop : IRUnconditionalBranch
{
    // The next block after the loop, which
    // is where we expect control flow to
    // re-converge, and also where a
    // `break` will target.
    IRUse breakBlock;

    // The block where control flow will go
    // on a `continue`.
    IRUse continueBlock;

    IRBlock* getBreakBlock() { return (IRBlock*)breakBlock.get(); }
    IRBlock* getContinueBlock() { return (IRBlock*)continueBlock.get(); }
};

struct IRConditionalBranch : IRTerminatorInst
{
    IR_PARENT_ISA(ConditionalBranch)

    IRUse condition;
    IRUse trueBlock;
    IRUse falseBlock;

    IRInst* getCondition() { return condition.get(); }
    IRBlock* getTrueBlock() { return (IRBlock*)trueBlock.get(); }
    IRBlock* getFalseBlock() { return (IRBlock*)falseBlock.get(); }
};

// A conditional branch that represent the test inside a loop
struct IRLoopTest : IRConditionalBranch
{
};

// A conditional branch that represents a one-sided `if`:
//
//     if( <condition> ) { <trueBlock> }
//     <falseBlock>
struct IRIf : IRConditionalBranch
{
    IRBlock* getAfterBlock() { return getFalseBlock(); }
};

// A conditional branch that represents a two-sided `if`:
//
//     if( <condition> ) { <trueBlock> }
//     else              { <falseBlock> }
//     <afterBlock>
//
struct IRIfElse : IRConditionalBranch
{
    IRUse afterBlock;

    IRBlock* getAfterBlock() { return (IRBlock*)afterBlock.get(); }
};

// A multi-way branch that represents a source-level `switch`
struct IRSwitch : IRTerminatorInst
{
    IR_LEAF_ISA(Switch);

    IRUse condition;
    IRUse breakLabel;
    IRUse defaultLabel;

    IRInst* getCondition() { return condition.get(); }
    IRBlock* getBreakLabel() { return (IRBlock*) breakLabel.get(); }
    IRBlock* getDefaultLabel() { return (IRBlock*) defaultLabel.get(); }

    // remaining args are: caseVal, caseLabel, ...

    UInt getCaseCount() { return (getOperandCount() - 3) / 2; }
    IRInst* getCaseValue(UInt index) { return            getOperand(3 + index*2 + 0); }
    IRBlock* getCaseLabel(UInt index) { return (IRBlock*) getOperand(3 + index*2 + 1); }
};

struct IRThrow : IRTerminatorInst
{
    IR_LEAF_ISA(Throw);

    IRInst* getValue() { return getOperand(0); }
};

struct IRTryCall : IRTerminatorInst
{
    IR_LEAF_ISA(TryCall);

    IRBlock* getSuccessBlock() { return cast<IRBlock>(getOperand(0)); }
    IRBlock* getFailureBlock() { return cast<IRBlock>(getOperand(1)); }
    IRInst* getCallee() { return getOperand(2); }
    UInt getArgCount() { return getOperandCount() - 3; }
    IRUse* getArgs() { return getOperands() + 3; }
    IRInst* getArg(UInt index) { return getOperand(index + 3); }
};

struct IRSwizzle : IRInst
{
    IRUse base;

    IRInst* getBase() { return base.get(); }
    UInt getElementCount()
    {
        return getOperandCount() - 1;
    }
    IRInst* getElementIndex(UInt index)
    {
        return getOperand(index + 1);
    }
};

struct IRSwizzleSet : IRInst
{
    IRUse base;
    IRUse source;

    IRInst* getBase() { return base.get(); }
    IRInst* getSource() { return source.get(); }
    UInt getElementCount()
    {
        return getOperandCount() - 2;
    }
    IRInst* getElementIndex(UInt index)
    {
        return getOperand(index + 2);
    }
};

struct IRSwizzledStore : IRInst
{
    IRInst* getDest() { return getOperand(0); }
    IRInst* getSource() { return getOperand(1); }
    UInt getElementCount()
    {
        return getOperandCount() - 2;
    }
    IRInst* getElementIndex(UInt index)
    {
        return getOperand(index + 2);
    }

    IR_LEAF_ISA(SwizzledStore)
};


struct IRPatchConstantFuncDecoration : IRDecoration
{
    enum { kOp = kIROp_PatchConstantFuncDecoration };
    IR_LEAF_ISA(PatchConstantFuncDecoration)

    IRInst* getFunc() { return getOperand(0); }
}; 

// An IR `var` instruction conceptually represents
// a stack allocation of some memory.
struct IRVar : IRInst
{
    IRPtrType* getDataType()
    {
        return cast<IRPtrType>(IRInst::getDataType());
    }

    static bool isaImpl(IROp op) { return op == kIROp_Var; }
};

/// @brief A global variable.
///
/// Represents a global variable in the IR.
/// If the variable has an initializer, then
/// it is represented by the code in the basic
/// blocks nested inside this value.
struct IRGlobalVar : IRGlobalValueWithCode
{
    IR_LEAF_ISA(GlobalVar)

    IRPtrType* getDataType()
    {
        return cast<IRPtrType>(IRInst::getDataType());
    }
};

/// @brief A global shader parameter.
///
/// Represents a uniform (as opposed to varying) shader parameter
/// passed at the global scope (entry-point `uniform` parameters
/// are encoded as ordinary function parameters.
///
/// Note that an `IRGlobalParam` directly represents the value of
/// the parameter, unlike an `IRGlobalVar`, which represents the
/// *address* of the value. As a result, global parameters are
/// immutable, and subject to various SSA simplifications that
/// do not work for global variables.
///
struct IRGlobalParam : IRInst
{
    IR_LEAF_ISA(GlobalParam)
};

/// @brief A global constnat.
///
/// Represents a global constant that may have a name and linkage.
/// If it has an operand, then this operand is the value of
/// the constants. If there is no operand, then the instruction
/// represents an "extern" constant that will be defined in another
/// module, and which is thus expected to have linkage.
///
struct IRGlobalConstant : IRInst
{
    IR_LEAF_ISA(GlobalConstant);

    /// Get the value of this global constant, or null if the value is not known.
    IRInst* getValue()
    {
        return getOperandCount() != 0 ? getOperand(0) : nullptr;
    }

};

// An entry in a witness table (see below)
struct IRWitnessTableEntry : IRInst
{
    // The AST-level requirement
    IRUse requirementKey;

    // The IR-level value that satisfies the requirement
    IRUse satisfyingVal;

    IRInst* getRequirementKey() { return getOperand(0); }
    IRInst* getSatisfyingVal()  { return getOperand(1); }

    IR_LEAF_ISA(WitnessTableEntry)
};

// A witness table is a global value that stores
// information about how a type conforms to some
// interface. It basically takes the form of a
// map from the required members of the interface
// to the IR values that satisfy those requirements.
struct IRWitnessTable : IRInst
{
    IRInstList<IRWitnessTableEntry> getEntries()
    {
        return IRInstList<IRWitnessTableEntry>(getChildren());
    }

    IRInst* getConformanceType()
    {
        return cast<IRWitnessTableType>(getDataType())->getConformanceType();
    }

    IRType* getConcreteType()
    {
        return (IRType*) getOperand(0);
    }

    IR_LEAF_ISA(WitnessTable)
};

/// Represents an RTTI object.
/// An IRRTTIObject has 1 operand, specifying the type
/// this RTTI object provides info for.
/// All type info are encapsualted as `IRRTTI*Decoration`s attached
/// to the object.
struct IRRTTIObject : IRInst
{
    IR_LEAF_ISA(RTTIObject)
};

// An instruction that yields an undefined value.
//
// Note that we make this an instruction rather than a value,
// so that we will be able to identify a variable that is
// used when undefined.
struct IRUndefined : IRInst
{
};

// A global-scope generic parameter (a type parameter, a
// constraint parameter, etc.)
struct IRGlobalGenericParam : IRInst
{
    IR_LEAF_ISA(GlobalGenericParam)
};

// An instruction that binds a global generic parameter
// to a particular value.
struct IRBindGlobalGenericParam : IRInst
{
    IRGlobalGenericParam* getParam() { return cast<IRGlobalGenericParam>(getOperand(0)); }
    IRInst* getVal() { return getOperand(1); }

    IR_LEAF_ISA(BindGlobalGenericParam)
};

// An Instruction that creates a tuple value.
struct IRMakeTuple : IRInst
{
    IR_LEAF_ISA(MakeTuple)
};

struct IRGetTupleElement : IRInst
{
    IR_LEAF_ISA(GetTupleElement)
    IRInst* getTuple() { return getOperand(0); }
    IRInst* getElementIndex() { return getOperand(1); }
};

// An Instruction that creates a differential pair value from a
// primal and differential.
struct IRMakeDifferentialPair : IRInst
{
    IR_LEAF_ISA(MakeDifferentialPair)
    IRInst* getPrimalValue() { return getOperand(0); }
    IRInst* getDifferentialValue() { return getOperand(1); }
};

struct IRDifferentialPairGetDifferential : IRInst
{
    IR_LEAF_ISA(DifferentialPairGetDifferential)
    IRInst* getBase() { return getOperand(0); }
};

struct IRDifferentialPairGetPrimal : IRInst
{
    IR_LEAF_ISA(DifferentialPairGetPrimal)
    IRInst* getBase() { return getOperand(0); }
};

// Constructs an `Result<T,E>` value from an error code.
struct IRMakeResultError : IRInst
{
    IR_LEAF_ISA(MakeResultError)

    IRInst* getErrorValue() { return getOperand(0); }
};

// Constructs an `Result<T,E>` value from an valid value.
struct IRMakeResultValue : IRInst
{
    IR_LEAF_ISA(MakeResultValue)

    IRInst* getValue() { return getOperand(0); }
};

// Determines if a `Result` value represents an error.
struct IRIsResultError : IRInst
{
    IR_LEAF_ISA(IsResultError)

    IRInst* getResultOperand() { return getOperand(0); }
};

// Extract the value from a `Result`.
struct IRGetResultValue : IRInst
{
    IR_LEAF_ISA(GetResultValue)

    IRInst* getResultOperand() { return getOperand(0); }
};

// Extract the error code from a `Result`.
struct IRGetResultError : IRInst
{
    IR_LEAF_ISA(GetResultError)

    IRInst* getResultOperand() { return getOperand(0); }
};

struct IROptionalHasValue : IRInst
{
    IR_LEAF_ISA(OptionalHasValue)

    IRInst* getOptionalOperand() { return getOperand(0); }
};

struct IRGetOptionalValue : IRInst
{
    IR_LEAF_ISA(GetOptionalValue)

    IRInst* getOptionalOperand() { return getOperand(0); }
};

struct IRMakeOptionalValue : IRInst
{
    IR_LEAF_ISA(MakeOptionalValue)

    IRInst* getValue() { return getOperand(0); }
};

struct IRMakeOptionalNone : IRInst
{
    IR_LEAF_ISA(MakeOptionalNone)
    IRInst* getDefaultValue() { return getOperand(0); }
};

    /// An instruction that packs a concrete value into an existential-type "box"
struct IRMakeExistential : IRInst
{
    IRInst* getWrappedValue() { return getOperand(0); }
    IRInst* getWitnessTable() { return getOperand(1); }

    IR_LEAF_ISA(MakeExistential)
};

struct IRMakeExistentialWithRTTI : IRInst
{
    IRInst* getWrappedValue() { return getOperand(0); }
    IRInst* getWitnessTable() { return getOperand(1); }
    IRInst* getRTTI() { return getOperand(2); }


    IR_LEAF_ISA(MakeExistentialWithRTTI)
};

struct IRCreateExistentialObject : IRInst
{
    IRInst* getTypeID() { return getOperand(0); }
    IRInst* getValue() { return getOperand(1); }

    IR_LEAF_ISA(CreateExistentialObject)
};

    /// Generalizes `IRMakeExistential` by allowing a type with existential sub-fields to be boxed
struct IRWrapExistential : IRInst
{
    IRInst* getWrappedValue() { return getOperand(0); }

    UInt getSlotOperandCount() { return getOperandCount() - 1; }
    IRInst* getSlotOperand(UInt index) { return getOperand(index + 1); }
    IRUse* getSlotOperands() { return getOperands() + 1; }

    IR_LEAF_ISA(WrapExistential)
};

struct IRGetValueFromBoundInterface : IRInst
{
    IR_LEAF_ISA(GetValueFromBoundInterface);
};

struct IRExtractExistentialValue : IRInst
{
    IR_LEAF_ISA(ExtractExistentialValue);
};

struct IRExtractExistentialType : IRInst
{
    IR_LEAF_ISA(ExtractExistentialType);
};

struct IRExtractExistentialWitnessTable : IRInst
{
    IR_LEAF_ISA(ExtractExistentialWitnessTable);
};

/* Base class for instructions that track liveness */
struct IRLiveRangeMarker : IRInst
{
    IR_PARENT_ISA(LiveRangeMarker)

    // TODO(JS): It might be useful to track how many bytes are live in the item referenced. 
    // It's not entirely clear how that will work across different targets, or even what such a 
    // size means on some targets.
    // 
    // Here we assume the size is the size of the type being referenced (whatever that means on a target)
    //
    // Potentially we could have a count, for defining (say) a range of an array. It's not clear this is 
    // needed, so we just have the item referenced.

        /// The referenced item whose liveness starts after this instruction
    IRInst* getReferenced() { return getOperand(0); }
};

/// Identifies then the item references starts being live.
struct IRLiveRangeStart : IRLiveRangeMarker
{
    IR_LEAF_ISA(LiveRangeStart);        
};

struct IRIsType : IRInst
{
    IR_LEAF_ISA(IsType);

    IRInst* getValue() { return getOperand(0); }
    IRInst* getValueWitness() { return getOperand(1); }

    IRInst* getTypeOperand() { return getOperand(2); }
    IRInst* getTargetWitness() { return getOperand(3); }
};

/// Demarks where the referenced item is no longer live, optimimally (although not
/// necessarily) at the previous instruction. 
/// 
/// There *can* be acceses to the referenced item after the end, if those accesses
/// can never be seen. For example if there is a store, without any subsequent loads, 
/// the store will never be seen (by a load) and so can be ignored.
///
/// In general there can be one or more 'ends' for every start.
struct IRLiveRangeEnd : IRLiveRangeMarker
{
    IR_LEAF_ISA(LiveRangeEnd);
};

// Description of an instruction to be used for global value numbering
struct IRInstKey
{
    IRInst* inst;

    HashCode getHashCode();
};

bool operator==(IRInstKey const& left, IRInstKey const& right);

struct IRConstantKey
{
    IRConstant* inst;

    bool operator==(const IRConstantKey& rhs) const { return inst->equal(rhs.inst); }
    HashCode getHashCode() const { return inst->getHashCode(); }
};

struct SharedIRBuilder
{
public:
    SharedIRBuilder()
    {}

    explicit SharedIRBuilder(IRModule* module)
    {
        init(module);
    }

    void init(IRModule* module)
    {
        m_module = module;
        m_session = module->getSession();

        m_globalValueNumberingMap.Clear();
        m_constantMap.Clear();
    }

    IRModule* getModule()
    {
        return m_module;
    }

    Session* getSession()
    {
        return m_session;
    }

    void insertBlockAlongEdge(IREdge const& edge);

    // Rebuilds `globalValueNumberingMap`. This is necessary if any existing
    // keys are modified (thus its hash code is changed).
    void deduplicateAndRebuildGlobalNumberingMap();

    // Replaces all uses of oldInst with newInst, and ensures the global numbering map is valid after the replacement.
    void replaceGlobalInst(IRInst* oldInst, IRInst* newInst);

    typedef Dictionary<IRInstKey, IRInst*> GlobalValueNumberingMap;
    typedef Dictionary<IRConstantKey, IRConstant*> ConstantMap;

    GlobalValueNumberingMap& getGlobalValueNumberingMap() { return m_globalValueNumberingMap; }
    ConstantMap& getConstantMap() { return m_constantMap; }

    bool isGloballyNumberedInst(IRInst* inst);

private:
    // The module that will own all of the IR
    IRModule* m_module;

    // The parent compilation session
    Session* m_session;

    GlobalValueNumberingMap m_globalValueNumberingMap;
    ConstantMap m_constantMap;
};

struct IRBuilderSourceLocRAII;

struct IRBuilder
{
private:
        /// Shared state for all IR builders working on the same module
    SharedIRBuilder*    m_sharedBuilder = nullptr;

        /// Default location for inserting new instructions as they are emitted
    IRInsertLoc m_insertLoc;

        /// Information that controls how source locations are associatd with instructions that get emitted
    IRBuilderSourceLocRAII* m_sourceLocInfo = nullptr;

public:
    IRBuilder()
    {}

    explicit IRBuilder(SharedIRBuilder* sharedBuilder)
        : m_sharedBuilder(sharedBuilder)
    {}

    explicit IRBuilder(SharedIRBuilder& sharedBuilder)
        : m_sharedBuilder(&sharedBuilder)
    {}

    void init(SharedIRBuilder* sharedBuilder)
    {
        *this = IRBuilder(sharedBuilder);
    }

    void init(SharedIRBuilder& sharedBuilder)
    {
        *this = IRBuilder(sharedBuilder);
    }

    SharedIRBuilder* getSharedBuilder() const
    {
        return m_sharedBuilder;
    }

    Session* getSession() const
    {
        return m_sharedBuilder->getSession();
    }

    IRModule* getModule() const
    {
        return m_sharedBuilder->getModule();
    }

    IRInsertLoc const& getInsertLoc() const { return m_insertLoc; }

    void setInsertLoc(IRInsertLoc const& loc) { m_insertLoc = loc; }

    // Get the current basic block we are inserting into (if any)
    IRBlock*                getBlock() { return m_insertLoc.getBlock(); }

    // Get the current function (or other value with code)
    // that we are inserting into (if any).
    IRGlobalValueWithCode*  getFunc() { return m_insertLoc.getFunc(); }

    void setInsertInto(IRInst* insertInto) { setInsertLoc(IRInsertLoc::atEnd(insertInto)); }
    void setInsertBefore(IRInst* insertBefore) { setInsertLoc(IRInsertLoc::before(insertBefore)); }

    void setInsertInto(IRModule* module) { setInsertInto(module->getModuleInst()); }

    IRBuilderSourceLocRAII* getSourceLocInfo() const { return m_sourceLocInfo; }
    void setSourceLocInfo(IRBuilderSourceLocRAII* sourceLocInfo) { m_sourceLocInfo = sourceLocInfo; }

    //
    // Low-level interface for instruction creation/insertion.
    //

        /// Either find or create an `IRConstant` that matches the value of `keyInst`.
        ///
        /// This operation will re-use an existing constant with the same type and
        /// value if one can be found (currently identified through the `SharedIRBuilder`).
        /// Otherwise it will create a new `IRConstant` with the given value and register it.
        ///
    IRConstant* _findOrEmitConstant(
        IRConstant&     keyInst);

        /// Create a new instruction with the given `type` and `op`, with an allocated
        /// size of at least `minSizeInBytes`, and with its operand list initialized
        /// from the provided lists of "fixed" and "variable" operands.
        ///
        /// The `fixedArgs` array must contain `fixedArgCount` operands, and will be
        /// the initial operands in the operand list of the instruction.
        ///
        /// After the fixed arguments, the instruction may have zero or more additional
        /// lists of "variable" operands, which are all concatenated. The total number
        /// of such additional lists is given by `varArgsListCount`. The number of
        /// operands in list `i` is given by `listArgCounts[i]`, and the arguments in
        /// list `i` are pointed to by `listArgs[i]`.
        ///
        /// The allocation for the instruction created will be at least `minSizeInBytes`,
        /// but may be larger if the total number of operands provided implies a larger
        /// size.
        ///
        /// Note: This is an extremely low-level operation and clients of an `IRBuilder`
        /// should not be using it when other options are available.
        ///
    IRInst* _createInst(
        size_t                  minSizeInBytes,
        IRType*                 type,
        IROp                    op,
        Int                     fixedArgCount,
        IRInst* const*          fixedArgs,
        Int                     varArgListCount,
        Int const*              listArgCounts,
        IRInst* const* const*   listArgs);



        /// Create a new instruction with the given `type` and `op`, with an allocated
        /// size of at least `minSizeInBytes`, and with zero operands.
        ///
    IRInst* _createInst(
        size_t          minSizeInBytes,
        IRType*         type,
        IROp            op)
    {
        return _createInst(minSizeInBytes, type, op, 0, nullptr, 0, nullptr, nullptr);
    }

        /// Attempt to attach a useful source location to `inst`.
        ///
        /// This operation looks at the source location information that has been
        /// attached to the builder. If it finds a valid source location, it will
        /// attach that location to `inst`.
        ///
    void _maybeSetSourceLoc(
        IRInst*     inst);


    //

    void addInst(IRInst* inst);

    IRInst* getBoolValue(bool value);
    IRInst* getIntValue(IRType* type, IRIntegerValue value);
    IRInst* getFloatValue(IRType* type, IRFloatingPointValue value);
    IRStringLit* getStringValue(const UnownedStringSlice& slice);
    IRPtrLit* getPtrValue(void* value);
    IRVoidLit* getVoidValue();
    IRInst* getCapabilityValue(CapabilitySet const& caps);

    IRBasicType* getBasicType(BaseType baseType);
    IRBasicType* getVoidType();
    IRBasicType* getBoolType();
    IRBasicType* getIntType();
    IRBasicType* getUIntType();
    IRBasicType* getUInt64Type();
    IRBasicType* getCharType();
    IRStringType* getStringType();
    IRNativeStringType* getNativeStringType();
    IRNativePtrType* getNativePtrType(IRType* valueType);

    IRType* getCapabilitySetType();

    IRAssociatedType* getAssociatedType(ArrayView<IRInterfaceType*> constraintTypes);
    IRThisType* getThisType(IRInterfaceType* interfaceType);
    IRRawPointerType* getRawPointerType();
    IRRTTIPointerType* getRTTIPointerType(IRInst* rttiPtr);
    IRRTTIType* getRTTIType();
    IRRTTIHandleType* getRTTIHandleType();
    IRAnyValueType* getAnyValueType(IRIntegerValue size);
    IRAnyValueType* getAnyValueType(IRInst* size);
    IRDynamicType* getDynamicType();

    IRTupleType* getTupleType(UInt count, IRType* const* types);
    IRTupleType* getTupleType(List<IRType*> const& types)
    {
        return getTupleType(types.getCount(), types.getBuffer());
    }

    IRTupleType* getTupleType(IRType* type0, IRType* type1);
    IRTupleType* getTupleType(IRType* type0, IRType* type1, IRType* type2);
    IRTupleType* getTupleType(IRType* type0, IRType* type1, IRType* type2, IRType* type3);

    IRResultType* getResultType(IRType* valueType, IRType* errorType);
    IROptionalType* getOptionalType(IRType* valueType);

    IRBasicBlockType*   getBasicBlockType();
    IRWitnessTableType* getWitnessTableType(IRType* baseType);
    IRWitnessTableIDType* getWitnessTableIDType(IRType* baseType);
    IRType* getTypeType() { return getType(IROp::kIROp_TypeType); }
    IRType* getKeyType() { return nullptr; }

    IRTypeKind*     getTypeKind();
    IRGenericKind*  getGenericKind();

    IRPtrType*  getPtrType(IRType* valueType);
    IROutType*  getOutType(IRType* valueType);
    IRInOutType*  getInOutType(IRType* valueType);
    IRRefType*  getRefType(IRType* valueType);
    IRPtrTypeBase*  getPtrType(IROp op, IRType* valueType);
    IRPtrType* getPtrType(IROp op, IRType* valueType, IRIntegerValue addressSpace);

    IRComPtrType* getComPtrType(IRType* valueType);

        /// Get a 'SPIRV literal' 
    IRSPIRVLiteralType* getSPIRVLiteralType(IRType* type);

    IRArrayTypeBase* getArrayTypeBase(
        IROp    op,
        IRType* elementType,
        IRInst* elementCount);

    IRArrayType* getArrayType(
        IRType* elementType,
        IRInst* elementCount);

    IRUnsizedArrayType* getUnsizedArrayType(
        IRType* elementType);

    IRVectorType* getVectorType(
        IRType* elementType,
        IRInst* elementCount);

    IRMatrixType* getMatrixType(
        IRType* elementType,
        IRInst* rowCount,
        IRInst* columnCount);

    IRDifferentialPairType* getDifferentialPairType(
        IRType* valueType,
        IRWitnessTable* witnessTable);

    IRFuncType* getFuncType(
        UInt            paramCount,
        IRType* const*  paramTypes,
        IRType*         resultType);

    IRFuncType* getFuncType(
        UInt paramCount, IRType* const* paramTypes, IRType* resultType, IRAttr* attribute);

    IRFuncType* getFuncType(
        List<IRType*> const&    paramTypes,
        IRType*                 resultType)
    {
        return getFuncType(paramTypes.getCount(), paramTypes.getBuffer(), resultType);
    }

    IRConstantBufferType* getConstantBufferType(
        IRType* elementType);

    IRConstExprRate* getConstExprRate();
    IRGroupSharedRate* getGroupSharedRate();
    IRActualGlobalRate* getActualGlobalRate();

    IRRateQualifiedType* getRateQualifiedType(
        IRRate* rate,
        IRType* dataType);

    IRType* getTaggedUnionType(
        UInt            caseCount,
        IRType* const*  caseTypes);

    IRType* getTaggedUnionType(
        List<IRType*> const& caseTypes)
    {
        return getTaggedUnionType(caseTypes.getCount(), caseTypes.getBuffer());
    }

    IRType* getBindExistentialsType(
        IRInst*         baseType,
        UInt            slotArgCount,
        IRInst* const*  slotArgs);

    IRType* getBindExistentialsType(
        IRInst*         baseType,
        UInt            slotArgCount,
        IRUse const*    slotArgs);

    IRType* getBoundInterfaceType(
        IRType* interfaceType,
        IRType* concreteType,
        IRInst* witnessTable);

    IRType* getPseudoPtrType(
        IRType* concreteType);

    IRType* getConjunctionType(
        UInt            typeCount,
        IRType* const*  types);

    IRType* getConjunctionType(
        IRType* type0,
        IRType* type1)
    {
        IRType* types[] = { type0, type1 };
        return getConjunctionType(2, types);
    }

    IRType* getAttributedType(
        IRType*         baseType,
        UInt            attributeCount,
        IRAttr* const*  attributes);

    IRType* getAttributedType(
        IRType*         baseType,
        List<IRAttr*>   attributes)
    {
        return getAttributedType(baseType, attributes.getCount(), attributes.getBuffer());
    }

        /// Emit an LiveRangeStart instruction indicating the referenced item is live following this instruction
    IRLiveRangeStart* emitLiveRangeStart(IRInst* referenced);

        /// Emit a LiveRangeEnd instruction indicating the referenced item is no longer live when this instruction is reached.
    IRLiveRangeEnd* emitLiveRangeEnd(IRInst* referenced);

    // Set the data type of an instruction, while preserving
    // its rate, if any.
    void setDataType(IRInst* inst, IRType* dataType);

        /// Extract the value wrapped inside an existential box.
    IRInst* emitGetValueFromBoundInterface(IRType* type, IRInst* boundInterfaceValue);

        /// Given an existential value, extract the underlying "real" value
    IRInst* emitExtractExistentialValue(
        IRType* type,
        IRInst* existentialValue);

        /// Given an existential value, extract the underlying "real" type
    IRType* emitExtractExistentialType(
        IRInst* existentialValue);

        /// Given an existential value, extract the witness table showing how the value conforms to the existential type.
    IRInst* emitExtractExistentialWitnessTable(
        IRInst* existentialValue);

    IRInst* emitJVPDifferentiateInst(IRType* type, IRInst* baseFn);

    IRInst* emitMakeDifferentialPair(IRType* type, IRInst* primal, IRInst* differential);

    IRInst* emitSpecializeInst(
        IRType*         type,
        IRInst*         genericVal,
        UInt            argCount,
        IRInst* const*  args);

    IRInst* emitLookupInterfaceMethodInst(
        IRType* type,
        IRInst* witnessTableVal,
        IRInst* interfaceMethodVal);

    IRInst* emitGetSequentialIDInst(IRInst* rttiObj);

    IRInst* emitAlloca(IRInst* type, IRInst* rttiObjPtr);

    IRInst* emitPackAnyValue(IRType* type, IRInst* value);

    IRInst* emitUnpackAnyValue(IRType* type, IRInst* value);

    IRInst* emitCallInst(
        IRType*         type,
        IRInst*         func,
        UInt            argCount,
        IRInst* const*  args);

    IRInst* emitCallInst(
        IRType*                 type,
        IRInst*                 func,
        List<IRInst*> const&    args)
    {
        return emitCallInst(type, func, args.getCount(), args.getBuffer());
    }

    IRInst* emitTryCallInst(
        IRType* type,
        IRBlock* successBlock,
        IRBlock* failureBlock,
        IRInst* func,
        UInt argCount,
        IRInst* const* args);

    IRInst* createIntrinsicInst(
        IRType*         type,
        IROp            op,
        UInt            argCount,
        IRInst* const*  args);

    IRInst* emitIntrinsicInst(
        IRType*         type,
        IROp            op,
        UInt            argCount,
        IRInst* const*  args);

    IRInst* emitConstructorInst(
        IRType*         type,
        UInt            argCount,
        IRInst* const* args);

    IRInst* emitMakeUInt64(IRInst* low, IRInst* high);

    // Creates an RTTI object. Result is of `IRRTTIType`.
    IRInst* emitMakeRTTIObject(IRInst* typeInst);

    IRInst* emitMakeTuple(IRType* type, UInt count, IRInst* const* args);
    IRInst* emitMakeTuple(UInt count, IRInst* const* args);

    IRInst* emitMakeTuple(IRType* type, List<IRInst*> const& args)
    {
        return emitMakeTuple(type, args.getCount(), args.getBuffer());
    }

    IRInst* emitMakeTuple(List<IRInst*> const& args)
    {
        return emitMakeTuple(args.getCount(), args.getBuffer());
    }

    IRInst* emitMakeTuple(IRInst* arg0, IRInst* arg1)
    {
        IRInst* args[] = { arg0, arg1 };
        return emitMakeTuple(SLANG_COUNT_OF(args), args);
    }

    IRInst* emitMakeString(IRInst* nativeStr);

    IRInst* emitGetNativeString(IRInst* str);

    IRInst* emitGetTupleElement(IRType* type, IRInst* tuple, UInt element);

    IRInst* emitMakeResultError(IRType* resultType, IRInst* errorVal);
    IRInst* emitMakeResultValue(IRType* resultType, IRInst* val);
    IRInst* emitIsResultError(IRInst* result);
    IRInst* emitGetResultError(IRInst* result);
    IRInst* emitGetResultValue(IRInst* result);
    IRInst* emitOptionalHasValue(IRInst* optValue);
    IRInst* emitGetOptionalValue(IRInst* optValue);
    IRInst* emitMakeOptionalValue(IRInst* optType, IRInst* value);
    IRInst* emitMakeOptionalNone(IRInst* optType, IRInst* defaultValue);
    IRInst* emitMakeVector(
        IRType*         type,
        UInt            argCount,
        IRInst* const* args);

    IRInst* emitMakeVector(
        IRType*                 type,
        List<IRInst*> const&    args)
    {
        return emitMakeVector(type, args.getCount(), args.getBuffer());
    }

    IRInst* emitMakeMatrix(
        IRType*         type,
        UInt            argCount,
        IRInst* const* args);

    IRInst* emitMakeArray(
        IRType*         type,
        UInt            argCount,
        IRInst* const* args);

    IRInst* emitMakeStruct(
        IRType*         type,
        UInt            argCount,
        IRInst* const* args);

    IRInst* emitMakeStruct(
        IRType*                 type,
        List<IRInst*> const&    args)
    {
        return emitMakeStruct(type, args.getCount(), args.getBuffer());
    }

    IRInst* emitMakeExistential(
        IRType* type,
        IRInst* value,
        IRInst* witnessTable);

    IRInst* emitMakeExistentialWithRTTI(
        IRType* type,
        IRInst* value,
        IRInst* witnessTable,
        IRInst* rtti);

    IRInst* emitWrapExistential(
        IRType*         type,
        IRInst*         value,
        UInt            slotArgCount,
        IRInst* const*  slotArgs);

    IRInst* emitWrapExistential(
        IRType*         type,
        IRInst*         value,
        UInt            slotArgCount,
        IRUse const*    slotArgs)
    {
        List<IRInst*> slotArgVals;
        for(UInt ii = 0; ii < slotArgCount; ++ii)
            slotArgVals.add(slotArgs[ii].get());

        return emitWrapExistential(type, value, slotArgCount, slotArgVals.getBuffer());
    }

    IRInst* emitManagedPtrAttach(IRInst* managedPtrVar, IRInst* value);

    IRInst* emitManagedPtrDetach(IRType* type, IRInst* managedPtrVal);

    IRInst* emitGetNativePtr(IRInst* value);

    IRInst* emitGetManagedPtrWriteRef(IRInst* ptrToManagedPtr);

    IRInst* emitGpuForeach(List<IRInst*> args);

    IRUndefined* emitUndefined(IRType* type);

    IRInst* emitReinterpret(IRInst* type, IRInst* value);

    IRInst* findOrAddInst(
         IRType*                 type,
         IROp                    op,
         UInt                    operandListCount,
         UInt const*             listOperandCounts,
         IRInst* const* const*   listOperands);

    IRInst* findOrEmitHoistableInst(
        IRType*                 type,
        IROp                    op,
        UInt                    operandListCount,
        UInt const*             listOperandCounts,
        IRInst* const* const*   listOperands);
    IRInst* findOrEmitHoistableInst(
        IRType*         type,
        IROp            op,
        UInt            operandCount,
        IRInst* const*  operands);
    IRInst* findOrEmitHoistableInst(
        IRType*         type,
        IROp            op,
        IRInst*         operand,
        UInt            operandCount,
        IRInst* const*  operands);

    IRFunc* createFunc();
    IRGlobalVar* createGlobalVar(
        IRType* valueType);
    IRGlobalParam* createGlobalParam(
        IRType* valueType);
    
    /// Creates an IRWitnessTable value.
    /// @param baseType: The comformant-to type of this witness.
    /// @param subType: The type that is doing the conforming.
    IRWitnessTable* createWitnessTable(IRType* baseType, IRType* subType);
    IRWitnessTableEntry* createWitnessTableEntry(
        IRWitnessTable* witnessTable,
        IRInst*        requirementKey,
        IRInst*        satisfyingVal);

    IRInterfaceRequirementEntry* createInterfaceRequirementEntry(
        IRInst* requirementKey,
        IRInst* requirementVal);

    // Create an initially empty `struct` type.
    IRStructType*   createStructType();

    // Create an initially empty `class` type.
    IRClassType* createClassType();

    // Create an empty `interface` type.
    IRInterfaceType* createInterfaceType(UInt operandCount, IRInst* const* operands);

    // Create a global "key" to use for indexing into a `struct` type.
    IRStructKey*    createStructKey();

    // Create a field nested in a struct type, declaring that
    // the specified field key maps to a field with the specified type.
    IRStructField*  createStructField(
        IRType*         aggType,
        IRStructKey*    fieldKey,
        IRType*         fieldType);

    IRGeneric* createGeneric();
    IRGeneric* emitGeneric();

    // Low-level operation for creating a type.
    IRType* getType(
        IROp            op,
        UInt            operandCount,
        IRInst* const*  operands);
    IRType* getType(
        IROp            op);
    IRType* getType(
        IROp            op,
        IRInst*         operand0);

        /// Create an empty basic block.
        ///
        /// The created block will not be inserted into the current
        /// function; call `insertBlock()` to attach the block
        /// at an appropriate point.
        ///
    IRBlock* createBlock();

        /// Insert a block into the current function.
        ///
        /// This attaches the given `block` to the current function,
        /// and makes it the current block for
        /// new instructions that get emitted.
        ///
    void insertBlock(IRBlock* block);

        /// Emit a new block into the current function.
        ///
        /// This function is equivalent to using `createBlock()`
        /// and then `insertBlock()`.
        ///
    IRBlock* emitBlock();

    

    IRParam* createParam(
        IRType* type);
    IRParam* emitParam(
        IRType* type);
    IRParam* emitParamAtHead(
        IRType* type);

    IRInst* emitAllocObj(IRType* type);

    IRVar* emitVar(
        IRType* type);

    IRInst* emitLoad(
        IRType* type,
        IRInst* ptr);

    IRInst* emitLoad(
        IRInst*    ptr);

    IRInst* emitStore(
        IRInst*    dstPtr,
        IRInst*    srcVal);

    IRInst* emitImageLoad(
        IRType* type,
        IRInst* image,
        IRInst* coord);

    IRInst* emitImageStore(
        IRType* type,
        IRInst* image,
        IRInst* coord,
        IRInst* value);

    IRInst* emitIsType(IRInst* value, IRInst* witness, IRInst* typeOperand, IRInst* targetWitness);

    IRInst* emitFieldExtract(
        IRType*         type,
        IRInst*        base,
        IRInst*        field);

    IRInst* emitFieldAddress(
        IRType*         type,
        IRInst*        basePtr,
        IRInst*        field);

    IRInst* emitElementExtract(
        IRType*     type,
        IRInst*    base,
        IRInst*    index);

    IRInst* emitElementAddress(
        IRType*     type,
        IRInst*    basePtr,
        IRInst*    index);

    IRInst* emitGetAddress(
        IRType* type,
        IRInst* value);

    IRInst* emitSwizzle(
        IRType*         type,
        IRInst*        base,
        UInt            elementCount,
        IRInst* const* elementIndices);

    IRInst* emitSwizzle(
        IRType*         type,
        IRInst*        base,
        UInt            elementCount,
        UInt const*     elementIndices);

    IRInst* emitSwizzleSet(
        IRType*         type,
        IRInst*        base,
        IRInst*        source,
        UInt            elementCount,
        IRInst* const* elementIndices);

    IRInst* emitSwizzleSet(
        IRType*         type,
        IRInst*        base,
        IRInst*        source,
        UInt            elementCount,
        UInt const*     elementIndices);

    IRInst* emitSwizzledStore(
        IRInst*         dest,
        IRInst*         source,
        UInt            elementCount,
        IRInst* const*  elementIndices);

    IRInst* emitSwizzledStore(
        IRInst*         dest,
        IRInst*         source,
        UInt            elementCount,
        UInt const*     elementIndices);



    IRInst* emitReturn(
        IRInst*    val);

    IRInst* emitReturn();

    IRInst* emitThrow(IRInst* val);

    IRInst* emitDiscard();

    IRInst* emitUnreachable();
    IRInst* emitMissingReturn();

    IRInst* emitBranch(
        IRBlock*    block);

   IRInst* emitBranch(IRBlock* block, Int argCount, IRInst*const* args);

    IRInst* emitBreak(
        IRBlock*    target);

    IRInst* emitContinue(
        IRBlock*    target);

    IRInst* emitLoop(
        IRBlock*    target,
        IRBlock*    breakBlock,
        IRBlock*    continueBlock);

    IRInst* emitBranch(
        IRInst*    val,
        IRBlock*    trueBlock,
        IRBlock*    falseBlock);

    IRInst* emitIf(
        IRInst*    val,
        IRBlock*    trueBlock,
        IRBlock*    afterBlock);

    IRInst* emitIfElse(
        IRInst*    val,
        IRBlock*    trueBlock,
        IRBlock*    falseBlock,
        IRBlock*    afterBlock);

    // Create basic blocks and insert an `IfElse` inst at current position that jumps into the blocks.
    // The current insert position is changed to inside `outTrueBlock` after the call.
    IRInst* emitIfElseWithBlocks(
        IRInst* val, IRBlock*& outTrueBlock, IRBlock*& outFalseBlock, IRBlock*& outAfterBlock);

    // Create basic blocks and insert an `If` inst at current position that jumps into the blocks.
    // The current insert position is changed to inside `outTrueBlock` after the call.
    IRInst* emitIfWithBlocks(
        IRInst* val, IRBlock*& outTrueBlock, IRBlock*& outAfterBlock);

    IRInst* emitLoopTest(
        IRInst*    val,
        IRBlock*    bodyBlock,
        IRBlock*    breakBlock);

    IRInst* emitSwitch(
        IRInst*        val,
        IRBlock*        breakLabel,
        IRBlock*        defaultLabel,
        UInt            caseArgCount,
        IRInst* const* caseArgs);

    IRGlobalGenericParam* emitGlobalGenericParam(
        IRType* type);

    IRGlobalGenericParam* emitGlobalGenericTypeParam()
    {
        return emitGlobalGenericParam(getTypeKind());
    }

    IRGlobalGenericParam* emitGlobalGenericWitnessTableParam(IRType* comformanceType)
    {
        return emitGlobalGenericParam(getWitnessTableType(comformanceType));
    }

    IRBindGlobalGenericParam* emitBindGlobalGenericParam(
        IRInst* param,
        IRInst* val);

    IRDecoration* addBindExistentialSlotsDecoration(
        IRInst*         value,
        UInt            argCount,
        IRInst* const*  args);

    IRInst* emitExtractTaggedUnionTag(
        IRInst* val);

    IRInst* emitExtractTaggedUnionPayload(
        IRType* type,
        IRInst* val,
        IRInst* tag);

    IRInst* emitBitCast(
        IRType* type,
        IRInst* val);

    IRInst* emitCastPtrToBool(IRInst* val);

    IRGlobalConstant* emitGlobalConstant(
        IRType* type);

    IRGlobalConstant* emitGlobalConstant(
        IRType* type,
        IRInst* val);

    IRInst* emitWaveMaskBallot(IRType* type, IRInst* mask, IRInst* condition);
    IRInst* emitWaveMaskMatch(IRType* type, IRInst* mask, IRInst* value);

    IRInst* emitBitAnd(IRType* type, IRInst* left, IRInst* right);
    IRInst* emitBitOr(IRType* type, IRInst* left, IRInst* right);
    IRInst* emitBitNot(IRType* type, IRInst* value);

    IRInst* emitAdd(IRType* type, IRInst* left, IRInst* right);
    IRInst* emitSub(IRType* type, IRInst* left, IRInst* right);
    IRInst* emitMul(IRType* type, IRInst* left, IRInst* right);
    IRInst* emitDiv(IRType* type, IRInst* numerator, IRInst* denominator);
    IRInst* emitEql(IRInst* left, IRInst* right);
    IRInst* emitNeq(IRInst* left, IRInst* right);
    IRInst* emitLess(IRInst* left, IRInst* right);

    IRInst* emitShr(IRType* type, IRInst* op0, IRInst* op1);
    IRInst* emitShl(IRType* type, IRInst* op0, IRInst* op1);

    //
    // Decorations
    //

    IRDecoration* addDecoration(IRInst* value, IROp op, IRInst* const* operands, Int operandCount);

    IRDecoration* addDecoration(IRInst* value, IROp op)
    {
        return addDecoration(value, op, (IRInst* const*) nullptr, 0);
    }

    IRDecoration* addDecoration(IRInst* value, IROp op, IRInst* operand)
    {
        return addDecoration(value, op, &operand, 1);
    }

    IRDecoration* addDecoration(IRInst* value, IROp op, IRInst* operand0, IRInst* operand1)
    {
        IRInst* operands[] = { operand0, operand1 };
        return addDecoration(value, op, operands, SLANG_COUNT_OF(operands));
    }

    template<typename T>
    void addSimpleDecoration(IRInst* value)
    {
        addDecoration(value, IROp(T::kOp), (IRInst* const*) nullptr, 0);
    }

    void addHighLevelDeclDecoration(IRInst* value, Decl* decl);

//    void addLayoutDecoration(IRInst* value, Layout* layout);
    void addLayoutDecoration(IRInst* value, IRLayout* layout);

//    IRLayout* getLayout(Layout* astLayout);

    IRTypeSizeAttr* getTypeSizeAttr(
        LayoutResourceKind kind,
        LayoutSize size);
    IRVarOffsetAttr* getVarOffsetAttr(
        LayoutResourceKind  kind,
        UInt                offset,
        UInt                space = 0);
    IRPendingLayoutAttr* getPendingLayoutAttr(
        IRLayout* pendingLayout);
    IRStructFieldLayoutAttr* getFieldLayoutAttr(
        IRInst*         key,
        IRVarLayout*    layout);
    IRCaseTypeLayoutAttr* getCaseTypeLayoutAttr(
        IRTypeLayout*   layout);

    IRSemanticAttr* getSemanticAttr(
        IROp            op,
        String const&   name,
        UInt            index);
    IRSystemValueSemanticAttr* getSystemValueSemanticAttr(
        String const&   name,
        UInt            index)
    {
        return cast<IRSystemValueSemanticAttr>(getSemanticAttr(
            kIROp_SystemValueSemanticAttr,
            name,
            index));
    }
    IRUserSemanticAttr* getUserSemanticAttr(
        String const&   name,
        UInt            index)
    {
        return cast<IRUserSemanticAttr>(getSemanticAttr(
            kIROp_UserSemanticAttr,
            name,
            index));
    }

    IRStageAttr* getStageAttr(Stage stage);

    IRAttr* getAttr(IROp op, UInt operandCount, IRInst* const* operands);

    IRAttr* getAttr(IROp op, List<IRInst*> const& operands)
    {
        return getAttr(op, operands.getCount(), operands.getBuffer());
    }

    IRAttr* getAttr(IROp op)
    {
        return getAttr(op, 0, nullptr);
    }

    IRTypeLayout* getTypeLayout(IROp op, List<IRInst*> const& operands);
    IRVarLayout* getVarLayout(List<IRInst*> const& operands);
    IREntryPointLayout* getEntryPointLayout(
        IRVarLayout* paramsLayout,
        IRVarLayout* resultLayout);


    void addNameHintDecoration(IRInst* value, IRStringLit* name)
    {
        addDecoration(value, kIROp_NameHintDecoration, name);
    }

    void addNameHintDecoration(IRInst* value, UnownedStringSlice const& text)
    {
        addNameHintDecoration(value, getStringValue(text));
    }

    void addGLSLOuterArrayDecoration(IRInst* value, UnownedStringSlice const& text)
    {
        addDecoration(value, kIROp_GLSLOuterArrayDecoration, getStringValue(text));
    }

    void addInterpolationModeDecoration(IRInst* value, IRInterpolationMode mode)
    {
        addDecoration(value, kIROp_InterpolationModeDecoration, getIntValue(getIntType(), IRIntegerValue(mode)));
    }

    void addLoopControlDecoration(IRInst* value, IRLoopControl mode)
    {
        addDecoration(value, kIROp_LoopControlDecoration, getIntValue(getIntType(), IRIntegerValue(mode)));
    }

    void addSemanticDecoration(IRInst* value, UnownedStringSlice const& text, int index = 0)
    {
        addDecoration(value, kIROp_SemanticDecoration, getStringValue(text), getIntValue(getIntType(), index));
    }

    void addTargetIntrinsicDecoration(IRInst* value, IRInst* caps, UnownedStringSlice const& definition)
    {
        addDecoration(value, kIROp_TargetIntrinsicDecoration, caps, getStringValue(definition));
    }

    void addTargetIntrinsicDecoration(IRInst* value, CapabilitySet const& caps, UnownedStringSlice const& definition)
    {
        addTargetIntrinsicDecoration(value, getCapabilityValue(caps), definition);
    }

    void addTargetDecoration(IRInst* value, IRInst* caps)
    {
        addDecoration(value, kIROp_TargetDecoration, caps);
    }

    void addTargetDecoration(IRInst* value, CapabilitySet const& caps)
    {
        addTargetDecoration(value, getCapabilityValue(caps));
    }

    void addRequireGLSLExtensionDecoration(IRInst* value, UnownedStringSlice const& extensionName)
    {
        addDecoration(value, kIROp_RequireGLSLExtensionDecoration, getStringValue(extensionName));
    }

    void addRequireGLSLVersionDecoration(IRInst* value, Int version)
    {
        addDecoration(value, kIROp_RequireGLSLVersionDecoration, getIntValue(getIntType(), IRIntegerValue(version)));
    }

    void addRequireSPIRVVersionDecoration(IRInst* value, const SemanticVersion& version)
    {
        SemanticVersion::IntegerType intValue = version.toInteger();
        addDecoration(value, kIROp_RequireSPIRVVersionDecoration, getIntValue(getBasicType(BaseType::UInt64), intValue));
    }

    void addRequireCUDASMVersionDecoration(IRInst* value, const SemanticVersion& version)
    {
        SemanticVersion::IntegerType intValue = version.toInteger();
        addDecoration(value, kIROp_RequireCUDASMVersionDecoration, getIntValue(getBasicType(BaseType::UInt64), intValue));
    }

    void addPatchConstantFuncDecoration(IRInst* value, IRInst* patchConstantFunc)
    {
        addDecoration(value, kIROp_PatchConstantFuncDecoration, patchConstantFunc);
    }

    void addImportDecoration(IRInst* value, UnownedStringSlice const& mangledName)
    {
        addDecoration(value, kIROp_ImportDecoration, getStringValue(mangledName));
    }

    void addExportDecoration(IRInst* value, UnownedStringSlice const& mangledName)
    {
        addDecoration(value, kIROp_ExportDecoration, getStringValue(mangledName));
    }

    void addExternCppDecoration(IRInst* value, UnownedStringSlice const& mangledName)
    {
        addDecoration(value, kIROp_ExternCppDecoration, getStringValue(mangledName));
    }

    void addJVPDerivativeMarkerDecoration(IRInst* value)
    {
        addDecoration(value, kIROp_JVPDerivativeMarkerDecoration);
    }

    void addJVPDerivativeReferenceDecoration(IRInst* value, IRInst* jvpFn)
    {
        addDecoration(value, kIROp_JVPDerivativeReferenceDecoration, jvpFn);
    }

    void addCOMWitnessDecoration(IRInst* value, IRInst* witnessTable)
    {
        addDecoration(value, kIROp_COMWitnessDecoration, &witnessTable, 1);
    }

    void addDllImportDecoration(IRInst* value, UnownedStringSlice const& libraryName, UnownedStringSlice const& functionName)
    {
        addDecoration(value, kIROp_DllImportDecoration, getStringValue(libraryName), getStringValue(functionName));
    }

    void addDllExportDecoration(IRInst* value, UnownedStringSlice const& functionName)
    {
        addDecoration(value, kIROp_DllExportDecoration, getStringValue(functionName));
    }

    void addEntryPointDecoration(IRInst* value, Profile profile, UnownedStringSlice const& name, UnownedStringSlice const& moduleName)
    {
        IRInst* operands[] = { getIntValue(getIntType(), profile.raw), getStringValue(name), getStringValue(moduleName) };
        addDecoration(value, kIROp_EntryPointDecoration, operands, SLANG_COUNT_OF(operands));
    }

    void addKeepAliveDecoration(IRInst* value)
    {
        addDecoration(value, kIROp_KeepAliveDecoration);
    }

    void addPublicDecoration(IRInst* value)
    {
        addDecoration(value, kIROp_PublicDecoration);   
    }
    void addHLSLExportDecoration(IRInst* value)
    {
        addDecoration(value, kIROp_HLSLExportDecoration);
    }
    void addNVAPIMagicDecoration(IRInst* value, UnownedStringSlice const& name)
    {
        addDecoration(value, kIROp_NVAPIMagicDecoration, getStringValue(name));
    }

    void addNVAPISlotDecoration(IRInst* value, UnownedStringSlice const& registerName, UnownedStringSlice const& spaceName)
    {
        addDecoration(value, kIROp_NVAPISlotDecoration, getStringValue(registerName), getStringValue(spaceName));
    }

        /// Add a decoration that indicates that the given `inst` depends on the given `dependency`.
        ///
        /// This decoration can be used to ensure that a value that an instruction
        /// implicitly depends on cannot be eliminated so long as the instruction
        /// itself is kept alive.
        ///
    void addDependsOnDecoration(IRInst* inst, IRInst* dependency)
    {
        addDecoration(inst, kIROp_DependsOnDecoration, dependency);
    }

    void addFormatDecoration(IRInst* inst, ImageFormat format)
    {
        addFormatDecoration(inst, getIntValue(getIntType(), IRIntegerValue(format)));
    }

    void addFormatDecoration(IRInst* inst, IRInst* format)
    {
        addDecoration(inst, kIROp_FormatDecoration, format);
    }

    void addRTTITypeSizeDecoration(IRInst* inst, IRIntegerValue value)
    {
        addDecoration(inst, kIROp_RTTITypeSizeDecoration, getIntValue(getIntType(), value));
    }

    void addAnyValueSizeDecoration(IRInst* inst, IRIntegerValue value)
    {
        addDecoration(inst, kIROp_AnyValueSizeDecoration, getIntValue(getIntType(), value));
    }

    void addSpecializeDecoration(IRInst* inst)
    {
        addDecoration(inst, kIROp_SpecializeDecoration);
    }

    void addComInterfaceDecoration(IRInst* inst, UnownedStringSlice guid)
    {
        addDecoration(inst, kIROp_ComInterfaceDecoration, getStringValue(guid));
    }

    void addTypeConstraintDecoration(IRInst* inst, IRInst* constraintType)
    {
        addDecoration(inst, kIROp_TypeConstraintDecoration, constraintType);
    }

    void addBuiltinDecoration(IRInst* inst)
    {
        addDecoration(inst, kIROp_BuiltinDecoration);
    }

    void addSequentialIDDecoration(IRInst* inst, IRIntegerValue id)
    {
        addDecoration(inst, kIROp_SequentialIDDecoration, getIntValue(getUIntType(), id));
    }

    void addVulkanRayPayloadDecoration(IRInst* inst, int location)
    {
        addDecoration(inst, kIROp_VulkanRayPayloadDecoration, getIntValue(getIntType(), location));
    }

    void addVulkanCallablePayloadDecoration(IRInst* inst, int location)
    {
        addDecoration(inst, kIROp_VulkanCallablePayloadDecoration, getIntValue(getIntType(), location));
    }
};

void addHoistableInst(
    IRBuilder*  builder,
    IRInst*     inst);

// Helper to establish the source location that will be used
// by an IRBuilder.
struct IRBuilderSourceLocRAII
{
    IRBuilder*  builder;
    SourceLoc   sourceLoc;
    IRBuilderSourceLocRAII* next;

    IRBuilderSourceLocRAII(
        IRBuilder*  builder,
        SourceLoc   sourceLoc)
        : builder(builder)
        , sourceLoc(sourceLoc)
        , next(nullptr)
    {
        next = builder->getSourceLocInfo();
        builder->setSourceLocInfo(this);
    }

    ~IRBuilderSourceLocRAII()
    {
        SLANG_ASSERT(builder->getSourceLocInfo() == this);
        builder->setSourceLocInfo(next);
    }
};

//

void markConstExpr(
    IRBuilder*  builder,
    IRInst*     irValue);

//

IRTargetIntrinsicDecoration* findAnyTargetIntrinsicDecoration(
        IRInst*                 val);

IRTargetSpecificDecoration* findBestTargetDecoration(
        IRInst*                 val,
        CapabilitySet const&    targetCaps);

IRTargetSpecificDecoration* findBestTargetDecoration(
        IRInst*         val,
        CapabilityAtom  targetCapabilityAtom);

inline IRTargetIntrinsicDecoration* findBestTargetIntrinsicDecoration(
    IRInst* inInst,
    CapabilitySet const& targetCaps)
{
    return as<IRTargetIntrinsicDecoration>(findBestTargetDecoration(inInst, targetCaps));
}


}

#endif
