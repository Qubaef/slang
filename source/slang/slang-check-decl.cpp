// slang-check-decl.cpp
#include "slang-check-impl.h"

// This file constaints the semantic checking logic and
// related queries for declarations.
//
// Because declarations are the top-level construct
// of the AST (in turn containing all the statements,
// types, and expressions), the declaration-checking
// logic also orchestrates the overall flow and how
// and when things get checked.

#include "slang-lookup.h"

#include "slang-syntax.h"

#include <limits>

namespace Slang
{
        /// Visitor to transition declarations to `DeclCheckState::CheckedModifiers`
    struct SemanticsDeclModifiersVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclModifiersVisitor>
    {
        SemanticsDeclModifiersVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDeclGroup(DeclGroup*) {}
        
        void visitDecl(Decl* decl)
        {
            checkModifiers(decl);
        }
    };

    struct SemanticsDeclHeaderVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclHeaderVisitor>
    {
        SemanticsDeclHeaderVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}
       
        void checkVarDeclCommon(VarDeclBase* varDecl);

        void visitVarDecl(VarDecl* varDecl)
        {
            checkVarDeclCommon(varDecl);
        }

        void visitGlobalGenericValueParamDecl(GlobalGenericValueParamDecl* decl)
        {
            checkVarDeclCommon(decl);
        }

        void visitImportDecl(ImportDecl* decl);

        void visitUsingDecl(UsingDecl* decl);

        void visitGenericTypeParamDecl(GenericTypeParamDecl* decl);

        void visitGenericValueParamDecl(GenericValueParamDecl* decl);

        void visitGenericTypeConstraintDecl(GenericTypeConstraintDecl* decl);

        void visitGenericDecl(GenericDecl* genericDecl);

        void visitTypeDefDecl(TypeDefDecl* decl);

        void visitGlobalGenericParamDecl(GlobalGenericParamDecl* decl);

        void visitAssocTypeDecl(AssocTypeDecl* decl);

        void checkCallableDeclCommon(CallableDecl* decl);

        void visitFuncDecl(FuncDecl* funcDecl);

        void visitParamDecl(ParamDecl* paramDecl);

        void visitConstructorDecl(ConstructorDecl* decl);

        void visitAbstractStorageDeclCommon(ContainerDecl* decl);

        void visitSubscriptDecl(SubscriptDecl* decl);

        void visitPropertyDecl(PropertyDecl* decl);

        void visitStructDecl(StructDecl* decl);

        void visitClassDecl(ClassDecl* decl);

            /// Get the type of the storage accessed by an accessor.
            ///
            /// The type of storage is determined by the parent declaration.
        Type* _getAccessorStorageType(AccessorDecl* decl);

            /// Perform checks common to all types of accessors.
        void _visitAccessorDeclCommon(AccessorDecl* decl);

        void visitAccessorDecl(AccessorDecl* decl);
        void visitSetterDecl(SetterDecl* decl);
    };

    struct SemanticsDeclRedeclarationVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclRedeclarationVisitor>
    {
        SemanticsDeclRedeclarationVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

#define CASE(TYPE) void visit##TYPE(TYPE* decl) { checkForRedeclaration(decl); }

        CASE(FuncDecl)
        CASE(VarDeclBase)
        CASE(SimpleTypeDecl)
        CASE(AggTypeDecl)

#undef CASE
    };

    struct SemanticsDeclBasesVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclBasesVisitor>
    {
        SemanticsDeclBasesVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        void visitInheritanceDecl(InheritanceDecl* inheritanceDecl);

            /// Validate that `decl` isn't illegally inheriting from a type in another module.
            ///
            /// This call checks a single `inheritanceDecl` to make sure that it either
            ///     * names a base type from the same module as `decl`, or
            ///     * names a type that allows cross-module inheritance
        void _validateCrossModuleInheritance(
            AggTypeDeclBase* decl,
            InheritanceDecl* inheritanceDecl);

        void visitInterfaceDecl(InterfaceDecl* decl);

        void visitStructDecl(StructDecl* decl);

        void visitClassDecl(ClassDecl* decl);

        void visitEnumDecl(EnumDecl* decl);

            /// Validate that the target type of an extension `decl` is valid.
        void _validateExtensionDeclTargetType(ExtensionDecl* decl);

        void visitExtensionDecl(ExtensionDecl* decl);
    };

    struct SemanticsDeclBodyVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclBodyVisitor>
    {
        SemanticsDeclBodyVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        void checkVarDeclCommon(VarDeclBase* varDecl);

        void visitVarDecl(VarDecl* varDecl)
        {
            checkVarDeclCommon(varDecl);
        }

        void visitGlobalGenericValueParamDecl(GlobalGenericValueParamDecl* decl)
        {
            checkVarDeclCommon(decl);
        }

        void visitEnumCaseDecl(EnumCaseDecl* decl);

        void visitEnumDecl(EnumDecl* decl);

        void visitFunctionDeclBase(FunctionDeclBase* funcDecl);

        void visitParamDecl(ParamDecl* paramDecl);
    };

        /// Should the given `decl` nested in `parentDecl` be treated as a static rather than instance declaration?
    bool isEffectivelyStatic(
        Decl*           decl,
        ContainerDecl*  parentDecl)
    {
        // Things at the global scope are always "members" of their module.
        //
        if(as<ModuleDecl>(parentDecl))
            return false;

        // Anything explicitly marked `static` and not at module scope
        // counts as a static rather than instance declaration.
        //
        if(decl->hasModifier<HLSLStaticModifier>())
            return true;

        // Next we need to deal with cases where a declaration is
        // effectively `static` even if the language doesn't make
        // the user say so. Most languages make the default assumption
        // that nested types are `static` even if they don't say
        // so (Java is an exception here, perhaps due to some
        // influence from the Scandanavian OOP tradition of Beta/gbeta).
        //
        if(as<AggTypeDecl>(decl))
            return true;
        if(as<SimpleTypeDecl>(decl))
            return true;

        // Initializer/constructor declarations are effectively `static`
        // in Slang. They behave like functions that return an instance
        // of the enclosing type, rather than as functions that are
        // called on a pre-existing value.
        //
        if(as<ConstructorDecl>(decl))
            return true;

        // Things nested inside functions may have dependencies
        // on values from the enclosing scope, but this needs to
        // be dealt with via "capture" so they are also effectively
        // `static`
        //
        if(as<FunctionDeclBase>(parentDecl))
            return true;

        // Type constraint declarations are used in member-reference
        // context as a form of casting operation, so we treat them
        // as if they are instance members. This is a bit of a hack,
        // but it achieves the result we want until we have an
        // explicit representation of up-cast operations in the
        // AST.
        //
        if(as<TypeConstraintDecl>(decl))
            return false;

        return false;
    }

    bool isEffectivelyStatic(
        Decl*           decl)
    {
        // For the purposes of an ordinary declaration, when determining if
        // it is static or per-instance, the "parent" declaration we really
        // care about is the next outer non-generic declaration.
        //
        // TODO: This idiom of getting the "next outer non-generic declaration"
        // comes up just enough that we should probably have a convenience
        // function for it.

        auto parentDecl = decl->parentDecl;
        if(auto genericDecl = as<GenericDecl>(parentDecl))
            parentDecl = genericDecl->parentDecl;

        return isEffectivelyStatic(decl, parentDecl);
    }

        /// Is `decl` a global shader parameter declaration?
    bool isGlobalShaderParameter(VarDeclBase* decl)
    {
        // If it's an *actual* global it is not a global shader parameter
        if (decl->hasModifier<ActualGlobalModifier>()) { return false; }
        
        // A global shader parameter must be declared at global or namespace
        // scope, so that it has a single definition across the module.
        //
        if(!as<NamespaceDeclBase>(decl->parentDecl)) return false;

        // A global variable marked `static` indicates a traditional
        // global variable (albeit one that is implicitly local to
        // the translation unit)
        //
        if(decl->hasModifier<HLSLStaticModifier>()) return false;

        // The `groupshared` modifier indicates that a variable cannot
        // be a shader parameters, but is instead transient storage
        // allocated for the duration of a thread-group's execution.
        //
        if(decl->hasModifier<HLSLGroupSharedModifier>()) return false;

        return true;
    }

    static bool _isLocalVar(VarDeclBase* varDecl)
    {
        auto pp = varDecl->parentDecl;

        if(as<ScopeDecl>(pp))
            return true;

        if(auto genericDecl = as<GenericDecl>(pp))
            pp = genericDecl;

        if(as<FuncDecl>(pp))
            return true;

        return false;
    }

    // Get the type to use when referencing a declaration
    QualType getTypeForDeclRef(
        ASTBuilder*             astBuilder,
        SemanticsVisitor*       sema,
        DiagnosticSink*         sink,
        DeclRef<Decl>           declRef,
        Type**           outTypeResult,
        SourceLoc               loc)
    {
        if( sema )
        {
            // Hack: if we are somehow referencing a local variable declaration
            // before the line of code that defines it, then we need to diagnose
            // an error.
            //
            // TODO: The right answer is that lookup should have been performed in
            // the scope that was in place *before* the variable was declared, but
            // this is a quick fix that at least alerts the user to how we are
            // interpreting their code.
            //
            // We detect the problematic case by looking for an attempt to reference
            // a local variable declaration when it is unchecked, or in the process
            // of being checked (the latter case catches a local variable that refers
            // to itself in its initial-value expression).
            //
            auto checkStateExt = declRef.getDecl()->checkState;
            if( checkStateExt.getState() == DeclCheckState::Unchecked
                || checkStateExt.isBeingChecked() )
            {
                if(auto varDecl = as<VarDecl>(declRef.getDecl()))
                {
                    if(_isLocalVar(varDecl))
                    {
                        sema->getSink()->diagnose(varDecl, Diagnostics::localVariableUsedBeforeDeclared, varDecl);
                        return QualType(astBuilder->getErrorType());
                    }
                }
            }

            // Once we've rules out the case of referencing a local declaration
            // before it has been checked, we will go ahead and ensure that
            // semantic checking has been performed on the chosen declaration,
            // at least up to the point where we can query its type.
            //
            sema->ensureDecl(declRef, DeclCheckState::CanUseTypeOfValueDecl);
        }

        // We need to insert an appropriate type for the expression, based on
        // what we found.
        if (auto varDeclRef = declRef.as<VarDeclBase>())
        {
            QualType qualType;
            qualType.type = getType(astBuilder, varDeclRef);

            bool isLValue = true;
            if(varDeclRef.getDecl()->findModifier<ConstModifier>())
                isLValue = false;

            // Global-scope shader parameters should not be writable,
            // since they are effectively program inputs.
            //
            // TODO: We could eventually treat a mutable global shader
            // parameter as a shorthand for an immutable parameter and
            // a global variable that gets initialized from that parameter,
            // but in order to do so we'd need to support global variables
            // with resource types better in the back-end.
            //
            if(isGlobalShaderParameter(varDeclRef.getDecl()))
                isLValue = false;

            // Variables declared with `let` are always immutable.
            if(varDeclRef.is<LetDecl>())
                isLValue = false;

            // Generic value parameters are always immutable
            if(varDeclRef.is<GenericValueParamDecl>())
                isLValue = false;

            // Function parameters declared in the "modern" style
            // are immutable unless they have an `out` or `inout` modifier.
            if(varDeclRef.is<ModernParamDecl>())
            {
                // Note: the `inout` modifier AST class inherits from
                // the class for the `out` modifier so that we can
                // make simple checks like this.
                //
                if( !varDeclRef.getDecl()->hasModifier<OutModifier>() )
                {
                    isLValue = false;
                }
            }

            qualType.isLeftValue = isLValue;
            return qualType;
        }
        else if( auto propertyDeclRef = declRef.as<PropertyDecl>() )
        {
            // Access to a declared `property` is similar to
            // access to a variable/field, except that it
            // is mediated through accessors (getters, seters, etc.).

            QualType qualType;
            qualType.type = getType(astBuilder, propertyDeclRef);

            bool isLValue = false;

            // If the property has any declared accessors that
            // can be used to set the property, then the resulting
            // expression behaves as an l-value.
            //
            if(propertyDeclRef.getDecl()->getMembersOfType<SetterDecl>().isNonEmpty())
                isLValue = true;
            if(propertyDeclRef.getDecl()->getMembersOfType<RefAccessorDecl>().isNonEmpty())
                isLValue = true;

            qualType.isLeftValue = isLValue;
            return qualType;

        }
        else if( auto enumCaseDeclRef = declRef.as<EnumCaseDecl>() )
        {
            QualType qualType;
            qualType.type = getType(astBuilder, enumCaseDeclRef);
            qualType.isLeftValue = false;
            return qualType;
        }
        else if (auto typeAliasDeclRef = declRef.as<TypeDefDecl>())
        {
            auto type = getNamedType(astBuilder, typeAliasDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto aggTypeDeclRef = declRef.as<AggTypeDecl>())
        {
            auto type = DeclRefType::create(astBuilder, aggTypeDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto simpleTypeDeclRef = declRef.as<SimpleTypeDecl>())
        {
            auto type = DeclRefType::create(astBuilder, simpleTypeDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto genericDeclRef = declRef.as<GenericDecl>())
        {
            auto type = getGenericDeclRefType(astBuilder, genericDeclRef);
            *outTypeResult = type;
            return QualType(astBuilder->getTypeType(type));
        }
        else if (auto funcDeclRef = declRef.as<CallableDecl>())
        {
            auto type = getFuncType(astBuilder, funcDeclRef);
            return QualType(type);
        }
        else if (auto constraintDeclRef = declRef.as<TypeConstraintDecl>())
        {
            // When we access a constraint or an inheritance decl (as a member),
            // we are conceptually performing a "cast" to the given super-type,
            // with the declaration showing that such a cast is legal.
            auto type = getSup(astBuilder, constraintDeclRef);
            return QualType(type);
        }
        else if( auto namespaceDeclRef = declRef.as<NamespaceDeclBase>())
        {
            auto type = getNamespaceType(astBuilder, namespaceDeclRef);
            return QualType(type);
        }
        if( sink )
        {
            // The compiler is trying to form a reference to a declaration
            // that doesn't appear to be usable as an expression or type.
            //
            // In practice, this arises when user code has an undefined-identifier
            // error, but the name that was undefined in context also matches
            // a contextual keyword. Rather than confuse the user with the
            // details of contextual keywords in the compiler, we will diagnose
            // this as an undefined identifier.
            //
            // TODO: This code could break if we ever go down this path with
            // an identifier that doesn't have a name.
            //
            sink->diagnose(loc, Diagnostics::undefinedIdentifier2, declRef.getName());
        }
        return QualType(astBuilder->getErrorType());
    }

    QualType getTypeForDeclRef(
        ASTBuilder*     astBuilder, 
        DeclRef<Decl>   declRef,
        SourceLoc       loc)
    {
        Type* typeResult = nullptr;
        return getTypeForDeclRef(astBuilder, nullptr, nullptr, declRef, &typeResult, loc);
    }

    DeclRef<ExtensionDecl> ApplyExtensionToType(
        SemanticsVisitor*       semantics,
        ExtensionDecl*          extDecl,
        Type*  type)
    {
        if(!semantics)
            return DeclRef<ExtensionDecl>();

        return semantics->ApplyExtensionToType(extDecl, type);
    }

    void ensureDecl(SemanticsVisitor* visitor, Decl* decl, DeclCheckState state)
    {
        visitor->ensureDecl(decl, state);
    }

    GenericSubstitution* createDefaultSubstitutionsForGeneric(
        ASTBuilder*             astBuilder,
        SemanticsVisitor* semantics,
        GenericDecl*            genericDecl,
        Substitutions*   outerSubst)
    {
        GenericSubstitution* cachedResult = nullptr;
        if (astBuilder->m_genericDefaultSubst.TryGetValue(genericDecl, cachedResult))
        {
            if (cachedResult->outer == outerSubst)
                return cachedResult;
        }

        List<Val*> args;

        for( auto mm : genericDecl->members )
        {
            if( auto genericTypeParamDecl = as<GenericTypeParamDecl>(mm) )
            {
                args.add(DeclRefType::create(astBuilder, DeclRef<Decl>(genericTypeParamDecl, outerSubst)));
            }
            else if( auto genericValueParamDecl = as<GenericValueParamDecl>(mm) )
            {
                args.add(astBuilder->getOrCreate<GenericParamIntVal>(
                    genericValueParamDecl->getType(),
                    genericValueParamDecl, outerSubst));
            }
        }

        bool shouldCache = true;

        // create default substitution arguments for constraints
        for (auto mm : genericDecl->members)
        {
            if (auto genericTypeConstraintDecl = as<GenericTypeConstraintDecl>(mm))
            {
                if (semantics)
                {
                    ensureDecl(semantics, genericTypeConstraintDecl, DeclCheckState::ReadyForReference);
                }
                auto constraintDeclRef = DeclRef<GenericTypeConstraintDecl>(genericTypeConstraintDecl, outerSubst);
                DeclaredSubtypeWitness* witness =
                    astBuilder->getOrCreate<DeclaredSubtypeWitness>(
                        getSub(astBuilder, constraintDeclRef),
                        getSup(astBuilder, constraintDeclRef),
                        genericTypeConstraintDecl,
                        outerSubst);
                // TODO: this is an ugly hack to prevent crashing.
                // In early stages of compilation witness->sub and witness->sup may not be checked yet.
                // When semanticVisitor is present we have used that to ensure the type is checked.
                // However due to how the code is written we cannot guarantee semanticVisitor is always available
                // here, and if we can't get the checked sup/sub type this subst is incomplete and should not be
                // cached.
                if (!witness->sub)
                    shouldCache = false;
                args.add(witness);
            }
        }

        GenericSubstitution* genericSubst = astBuilder->getOrCreateGenericSubstitution(genericDecl, args, outerSubst);
        if (shouldCache)
            astBuilder->m_genericDefaultSubst[genericDecl] = genericSubst;
        return genericSubst;
    }

    // Sometimes we need to refer to a declaration the way that it would be specialized
    // inside the context where it is declared (e.g., with generic parameters filled in
    // using their archetypes).
    //
    SubstitutionSet createDefaultSubstitutions(
        ASTBuilder*     astBuilder,
        SemanticsVisitor* semantics,
        Decl*           decl,
        SubstitutionSet outerSubstSet)
    {
        auto dd = decl->parentDecl;
        if( auto genericDecl = as<GenericDecl>(dd) )
        {
            // We don't want to specialize references to anything
            // other than the "inner" declaration itself.
            if(decl != genericDecl->inner)
                return outerSubstSet;

            GenericSubstitution* genericSubst = createDefaultSubstitutionsForGeneric(
                astBuilder,
                semantics,
                genericDecl,
                outerSubstSet.substitutions);

            return SubstitutionSet(genericSubst);
        }

        return outerSubstSet;
    }

    SubstitutionSet createDefaultSubstitutions(
        ASTBuilder* astBuilder,
        SemanticsVisitor* semantics,
        Decl*   decl)
    {
        SubstitutionSet subst;
        if( auto parentDecl = decl->parentDecl )
        {
            subst = createDefaultSubstitutions(astBuilder, semantics, parentDecl);
        }
        subst = createDefaultSubstitutions(astBuilder, semantics, decl, subst);
        return subst;
    }

    bool SemanticsVisitor::isDeclUsableAsStaticMember(
        Decl*   decl)
    {
        if(auto genericDecl = as<GenericDecl>(decl))
            decl = genericDecl->inner;

        if(decl->hasModifier<HLSLStaticModifier>())
            return true;

        if(as<ConstructorDecl>(decl))
            return true;

        if(as<EnumCaseDecl>(decl))
            return true;

        if(as<AggTypeDeclBase>(decl))
            return true;

        if(as<SimpleTypeDecl>(decl))
            return true;

        if(as<TypeConstraintDecl>(decl))
            return true;

        return false;
    }

    bool SemanticsVisitor::isUsableAsStaticMember(
        LookupResultItem const& item)
    {
        // There's a bit of a gotcha here, because a lookup result
        // item might include "breadcrumbs" that indicate more steps
        // along the lookup path. As a result it isn't always
        // valid to just check whether the final decl is usable
        // as a static member, because it might not even be a
        // member of the thing we are trying to work with.
        //

        Decl* decl = item.declRef.getDecl();
        for(auto bb = item.breadcrumbs; bb; bb = bb->next)
        {
            switch(bb->kind)
            {
            // In case lookup went through a `__transparent` member,
            // we are interested in the static-ness of that transparent
            // member, and *not* the static-ness of whatever was inside
            // of it.
            //
            // TODO: This would need some work if we ever had
            // transparent *type* members.
            //
            case LookupResultItem::Breadcrumb::Kind::Member:
                decl = bb->declRef.getDecl();
                break;

            // TODO: Are there any other cases that need special-case
            // handling here?

            default:
                break;
            }
        }

        // Okay, we've found the declaration we should actually
        // be checking, so lets validate that.

        return isDeclUsableAsStaticMember(decl);
    }

        /// Dispatch an appropriate visitor to check `decl` up to state `state`
        ///
        /// The current state of `decl` must be `state-1`.
        /// This call does *not* handle updating the state of `decl`; the
        /// caller takes responsibility for doing so.
        ///
    static void _dispatchDeclCheckingVisitor(Decl* decl, DeclCheckState state, SemanticsContext const& shared);

    // Make sure a declaration has been checked, so we can refer to it.
    // Note that this may lead to us recursively invoking checking,
    // so this may not be the best way to handle things.
    void SemanticsVisitor::ensureDecl(Decl* decl, DeclCheckState state, SemanticsContext* baseContext)
    {
        // If the `decl` has already been checked up to or beyond `state`
        // then there is nothing for us to do.
        //
        if (decl->isChecked(state)) return;

        // Is the declaration already being checked, somewhere up the
        // call stack from us?
        //
        if(decl->checkState.isBeingChecked())
        {
            // We tried to reference the same declaration while checking it!
            //
            // TODO: we should ideally be tracking a "chain" of declarations
            // being checked on the stack, so that we can report the full
            // chain that leads from this declaration back to itself.
            //
            getSink()->diagnose(decl, Diagnostics::cyclicReference, decl);
            return;
        }

        // If we should skip the checking, return now.
        // A common case to skip checking is for the function bodies when we are in
        // the language server. In that case we only care about the function bodies in a
        // specific module and can skip checking the reference modules until they
        // are being opened/edited later.
        if (shouldSkipChecking(decl, state))
        {
            decl->setCheckState(state);
            return;
        }

        // Set the flag that indicates we are checking this declaration,
        // so that the cycle check above will catch us before we go
        // into any infinite loops.
        //
        decl->checkState.setIsBeingChecked(true);

        // Our task is to bring the `decl` up to `state` which may be
        // one or more steps ahead of where it currently is. We can
        // invoke a visitor designed to bring a declaration from state
        // N to state N+1, and in general we might need multiple such
        // passes to get `decl` to where we need it.
        //
        // The coding of this loop is somewhat defensive to deal
        // with special cases that will be described along the way.
        //
        for(;;)
        {
            // The first thing is to check what state the decl is
            // currently in at the start of this loop iteration,
            // and to bail out if it has been checked up to
            // (or beyond) our target state.
            //
            auto currentState = decl->checkState.getState();
            if(currentState >= state)
                break;

            // Because our visitors are only designed to go from state
            // N to N+1 in general, we will aspire to transition to
            // a state that is one greater than `currentState`.
            //
            auto nextState = DeclCheckState(Int(currentState) + 1);

            // We now dispatch an appropriate visitor based on `nextState`.
            //
            // Note that we always dispatch the visitor in a "fresh" semantic-checking
            // context, so that the state at the point where a declaration is *referenced*
            // cannot affect the state in which the declaration is *checked*.
            //
            SemanticsContext subContext = baseContext ? SemanticsContext(*baseContext) : SemanticsContext(getShared());
            _dispatchDeclCheckingVisitor(decl, nextState, subContext);

            // In the common case, the visitor will have done the necessary
            // checking, but will *not* have updated the `checkState` on
            // `decl`. In that case we will do the update here, to save
            // us the complication of having to deal with state update in
            // every single visitor method.
            //
            // However, sometimes a visitor *will* want to manually update
            // the state of a declaration, and it may actually update it
            // *past* the `nextState` we asked for (or even past the
            // eventual target `state`). In those cases we don't want to
            // accidentally set the state of `decl` to something lower
            // than what has actually been checked, so we test for
            // such cases here.
            //
            if(nextState > decl->checkState.getState())
            {
                decl->setCheckState(nextState);
            }
        }

        // Once we are done here, the state of `decl` should have
        // been upgraded to (at least) `state`.
        //
        SLANG_ASSERT(decl->isChecked(state));

        // Now that we are done checking `decl` we need to restore
        // its "is being checked" flag so that we don't generate
        // errors the next time somebody calls `ensureDecl()` on it.
        //
        decl->checkState.setIsBeingChecked(false);
    }

        /// Recursively ensure the tree of declarations under `decl` is in `state`.
        ///
        /// This function does *not* handle declarations nested in function bodies
        /// because those cannot be meaningfully checked outside of the context
        /// of their surrounding statement(s).
        ///
    static void _ensureAllDeclsRec(
        SemanticsDeclVisitorBase*   visitor,
        Decl*                       decl,
        DeclCheckState              state)
    {
        // Ensure `decl` itself first.
        visitor->ensureDecl(decl, state);

        // If `decl` is a container, then we want to ensure its children.
        if(auto containerDecl = as<ContainerDecl>(decl))
        {            
            // NOTE! We purposefully do not iterate with the for(auto childDecl : containerDecl->members) here,
            // because the visitor may add to `members` whilst iteration takes place, invalidating the iterator
            // and likely a crash.
            // 
            // Accessing the members via index side steps the issue.
            const auto& members = containerDecl->members;
            for(Index i = 0; i < members.getCount(); ++i)
            {
                Decl* childDecl = members[i];

                // As an exception, if any of the child is a `ScopeDecl`,
                // then that indicates that it represents a scope for local
                // declarations under a statement (e.g., in a function body),
                // and we don't want to check such local declarations here.
                //

                if(as<ScopeDecl>(childDecl))
                    continue;

                _ensureAllDeclsRec(visitor, childDecl, state);
            }
        }

        // Note: the "inner" declaration of a `GenericDecl` is currently
        // not exposed as one of its children (despite a `GenericDecl`
        // being a `ContainerDecl`), so we need to handle the inner
        // declaration of a generic as another case here.
        //
        if(auto genericDecl = as<GenericDecl>(decl))
        {
            _ensureAllDeclsRec(visitor, genericDecl->inner, state);
        }
    }

    static bool isUnsizedArrayType(Type* type)
    {
        // Not an array?
        auto arrayType = as<ArrayExpressionType>(type);
        if (!arrayType) return false;

        // Explicit element count given?
        auto elementCount = arrayType->arrayLength;
        if (elementCount) return true;

        return true;
    }

    bool SemanticsVisitor::shouldSkipChecking(Decl* decl, DeclCheckState state)
    {
        if (state != DeclCheckState::Checked)
            return false;
        // If we are in language server, we should skip checking all the function bodies
        // except for the module or function that the user cared about.
        // This optimization helps reduce the response time.
        if (!getLinkage()->isInLanguageServer())
        {
            return false;
        }
        if (auto funcDecl = as<FunctionDeclBase>(decl))
        {
            auto& assistInfo = getLinkage()->contentAssistInfo;
            // If this func is not defined in the primary module, skip checking its body.
            auto moduleDecl = getModuleDecl(decl);
            if (moduleDecl && moduleDecl->getName() != assistInfo.primaryModuleName)
                return true;
            if (funcDecl->body)
            {
                auto humaneLoc = getLinkage()->getSourceManager()->getHumaneLoc(
                    decl->loc, SourceLocType::Actual);
                if (humaneLoc.pathInfo.foundPath != assistInfo.primaryModulePath)
                {
                    return true;
                }
                if (assistInfo.checkingMode == ContentAssistCheckingMode::Completion)
                {
                    // For completion requests, we skip all funtion bodies except for the one
                    // that the current cursor is in.
                    auto closingLoc = getLinkage()->getSourceManager()->getHumaneLoc(
                        funcDecl->closingSourceLoc, SourceLocType::Actual);

                    if (assistInfo.cursorLine < humaneLoc.line ||
                        assistInfo.cursorLine > closingLoc.line)
                        return true;
                }
            }
        }
        return false;
    }

    void SemanticsVisitor::_validateCircularVarDefinition(VarDeclBase* varDecl)
    {
        // The easiest way to test if the declaration is circular is to
        // validate it as a constant.
        //
        // TODO: The logic here will only apply for `static const` declarations
        // of integer type, given that our constant folding currently only
        // applies to such types. A more robust fix would involve a truly
        // recursive walk of the AST declarations, and an even *more* robust
        // fix would wait until after IR linking to detect and diagnose circularity
        // in case it crosses module boundaries.
        //
        //
        if(!isScalarIntegerType(varDecl->type))
            return;
        tryConstantFoldDeclRef(DeclRef<VarDeclBase>(varDecl, nullptr), nullptr);
    }

    void SemanticsDeclHeaderVisitor::checkVarDeclCommon(VarDeclBase* varDecl)
    {
        // A variable that didn't have an explicit type written must
        // have its type inferred from the initial-value expression.
        //
        if(!varDecl->type.exp)
        {
            // In this case we need to perform all checking of the
            // variable (including semantic checking of the initial-value
            // expression) during the first phase of checking.

            auto initExpr = varDecl->initExpr;
            if(!initExpr)
            {
                getSink()->diagnose(varDecl, Diagnostics::varWithoutTypeMustHaveInitializer);
                varDecl->type.type = m_astBuilder->getErrorType();
            }
            else
            {
                initExpr = CheckExpr(initExpr);

                // TODO: We might need some additional steps here to ensure
                // that the type of the expression is one we are okay with
                // inferring. E.g., if we ever decide that integer and floating-point
                // literals have a distinct type from the standard int/float types,
                // then we would need to "decay" a literal to an explicit type here.

                varDecl->initExpr = initExpr;
                varDecl->type.type = initExpr->type;

                _validateCircularVarDefinition(varDecl);
            }

            // If we've gone down this path, then the variable
            // declaration is actually pretty far along in checking
            varDecl->setCheckState(DeclCheckState::Checked);
        }
        else
        {
            // A variable with an explicit type is simpler, for the
            // most part.

            TypeExp typeExp = CheckUsableType(varDecl->type);
            varDecl->type = typeExp;
            if (varDecl->type.equals(m_astBuilder->getVoidType()))
            {
                getSink()->diagnose(varDecl, Diagnostics::invalidTypeVoid);
            }

            // If this is an unsized array variable, then we first want to give
            // it a chance to infer an array size from its initializer
            //
            // TODO(tfoley): May need to extend this to handle the
            // multi-dimensional case...
            //
            if(isUnsizedArrayType(varDecl->type))
            {
                if (auto initExpr = varDecl->initExpr)
                {
                    initExpr = CheckTerm(initExpr);
                    initExpr = coerce(varDecl->type.Ptr(), initExpr);
                    varDecl->initExpr = initExpr;

                    maybeInferArraySizeForVariable(varDecl);

                    varDecl->setCheckState(DeclCheckState::Checked);
                }
            }
            //
            // Next we want to make sure that the declared (or inferred)
            // size for the array meets whatever language-specific
            // constraints we want to enforce (e.g., disallow empty
            // arrays in specific cases)
            //
            validateArraySizeForVariable(varDecl);
        }

        // The NVAPI library allows user code to express extended operations
        // (not supported natively by D3D HLSL) by communicating with
        // a specially identified shader parameter called `g_NvidiaExt`.
        //
        // By default, that shader parameter would look like an ordinary
        // global shader parameter to Slang, but we want to be able to
        // associate special behavior with it to make downstream compilation
        // work nicely (especially in the case where certain cross-platform
        // operations in the Slang standard library need to use NVAPI).
        //
        // We will detect a global variable declaration that appears to
        // be declaring `g_NvidiaExt` from NVAPI, and mark it with a special
        // modifier to allow downstream steps to detect it whether or
        // not it has an associated name.
        //
        if( as<ModuleDecl>(varDecl->parentDecl)
            && varDecl->getName()
            && varDecl->getName()->text == "g_NvidiaExt" )
        {
            addModifier(varDecl, m_astBuilder->create<NVAPIMagicModifier>());
        }
        //
        // One thing that the `NVAPIMagicModifier` is going to do is ensure
        // that `g_NvidiaExt` always gets emitted with *exactly* that name,
        // whether or not obfuscation or other steps are enabled.
        //
        // The `g_NvidiaExt` variable is declared as a:
        //
        //      RWStructuredBuffer<NvShaderExtnStruct>
        //
        // and we also want to make sure that the fields of that struct
        // retain their original names in output code. We will detect
        // variable declarations that represent fields of that struct
        // and flag them as "magic" as well.
        //
        // Note: The goal here is to make it so that generated HLSL output
        // can either use these declarations as they have been preocessed
        // by the Slang front-end *or* they can use declarations directly
        // from the NVAPI header during downstream compilation.
        //
        // TODO: It would be nice if we had a way to identify *all* of the
        // declarations that come from the NVAPI header and mark them, so
        // that the Slang front-end doesn't have to take responsibility
        // for generating code from them (and can instead rely on the downstream
        // compiler alone).
        //
        // The NVAPI header doesn't put any kind of macro-defined modifier
        // (defaulting to an empty macro) in front of its declarations,
        // so the most plausible way to add a modifier to all the declarations
        // would be to tag the `nvHLSLExtns.h` header in a list of "magic"
        // headers which should get all their declarations flagged during
        // front-end processing, and then use the same header again during
        // downstream compilation.
        //
        // For now, the current hackery seems a bit less complicated.
        //
        if( auto structDecl = as<StructDecl>(varDecl->parentDecl))
        {
            if( structDecl->getName()
                && structDecl->getName()->text == "NvShaderExtnStruct" )
            {
                addModifier(varDecl, m_astBuilder->create<NVAPIMagicModifier>());
            }
        }

        if (auto interfaceDecl = as<InterfaceDecl>(varDecl->parentDecl))
        {
            if (auto basicType = as<BasicExpressionType>(varDecl->getType()))
            {
                switch (basicType->baseType)
                {
                case BaseType::Bool:
                case BaseType::Int8:
                case BaseType::Int16:
                case BaseType::Int:
                case BaseType::Int64:
                case BaseType::UInt8:
                case BaseType::UInt16:
                case BaseType::UInt:
                case BaseType::UInt64:
                    break;
                default:
                    getSink()->diagnose(varDecl, Diagnostics::staticConstRequirementMustBeIntOrBool);
                    break;
                }
            }
            if (!varDecl->findModifier<HLSLStaticModifier>() || !varDecl->findModifier<ConstModifier>())
            {
                getSink()->diagnose(varDecl, Diagnostics::valueRequirementMustBeCompileTimeConst);
            }
        }
    }

    void SemanticsDeclHeaderVisitor::visitStructDecl(StructDecl* structDecl)
    {
        // As described above in `SemanticsDeclHeaderVisitor::checkVarDeclCommon`,
        // we want to identify and tag the "magic" declarations that make NVAPI
        // work, so that downstream passes can identify them and act accordingly.
        //
        // In this case, we are looking for the `NvShaderExtnStruct` type, which
        // is used by `g_NvidiaExt`.
        //
        if( structDecl->getName()
            && structDecl->getName()->text == "NvShaderExtnStruct" )
        {
            addModifier(structDecl, m_astBuilder->create<NVAPIMagicModifier>());
        }
    }

    void SemanticsDeclHeaderVisitor::visitClassDecl(ClassDecl* classDecl)
    {
        SLANG_UNUSED(classDecl);
    }

    void SemanticsDeclBodyVisitor::checkVarDeclCommon(VarDeclBase* varDecl)
    {
        if (auto initExpr = varDecl->initExpr)
        {
            // If the variable has an explicit initial-value expression,
            // then we simply need to check that expression and coerce
            // it to the type of the variable.
            //
            initExpr = CheckTerm(initExpr);
            initExpr = coerce(varDecl->type.Ptr(), initExpr);
            varDecl->initExpr = initExpr;

            // We need to ensure that any variable doesn't introduce
            // a constant with a circular definition.
            //
            varDecl->setCheckState(DeclCheckState::Checked);
            _validateCircularVarDefinition(varDecl);
        }
        else
        {
            // If a variable doesn't have an explicit initial-value
            // expression, it is still possible that it should
            // be initialized implicitly, because the type of the
            // variable has a default (zero parameter) initializer.
            // That is, for types where it is possible, we will
            // treat a variable declared like this:
            //
            //      MyType myVar;
            //
            // as if it were declared as:
            //
            //      MyType myVar = MyType();
            //
            // Rather than try to code up an ad hoc search for an
            // appropriate initializer here, we will instead fall
            // back on the general-purpose overload-resolution
            // machinery, which can handle looking up initializers
            // and filtering them to ones that are applicable
            // to our "call site" with zero arguments.
            //
            auto type = varDecl->getType();

            OverloadResolveContext overloadContext;
            overloadContext.loc = varDecl->nameAndLoc.loc;
            overloadContext.mode = OverloadResolveContext::Mode::JustTrying;
            AddTypeOverloadCandidates(type, overloadContext);

            if(overloadContext.bestCandidates.getCount() != 0)
            {
                // If there were multiple equally-good candidates to call,
                // then might have an ambiguity.
                //
                // Before issuing any kind of diagnostic we need to check
                // if any of those candidates are actually applicable,
                // because if they aren't then we actually just have
                // an uninitialized varaible.
                //
                if(overloadContext.bestCandidates[0].status != OverloadCandidate::Status::Applicable)
                    return;

                getSink()->diagnose(varDecl, Diagnostics::ambiguousDefaultInitializerForType, type);
            }
            else if(overloadContext.bestCandidate)
            {
                // If we are in the single-candidate case, then we again
                // want to ignore the case where that candidate wasn't
                // actually applicable, because declaring a variable
                // of a type that *doesn't* have a default initializer
                // isn't actually an error.
                //
                if(overloadContext.bestCandidate->status != OverloadCandidate::Status::Applicable)
                    return;

                // If we had a single best candidate *and* it was applicable,
                // then we use it to construct a new initial-value expression
                // for the variable, that will be used for all downstream
                // code generation.
                //
                varDecl->initExpr = CompleteOverloadCandidate(overloadContext, *overloadContext.bestCandidate);
            }
        }
    }

    // Fill in default substitutions for the 'subtype' part of a type constraint decl
    void SemanticsVisitor::CheckConstraintSubType(TypeExp& typeExp)
    {
        if (auto sharedTypeExpr = as<SharedTypeExpr>(typeExp.exp))
        {
            if (auto declRefType = as<DeclRefType>(sharedTypeExpr->base))
            {
                declRefType->declRef.substitutions = createDefaultSubstitutions(m_astBuilder, this, declRefType->declRef.getDecl());

                if (auto typetype = as<TypeType>(typeExp.exp->type))
                    typetype->type = declRefType;
            }
        }
    }

    void SemanticsDeclHeaderVisitor::visitGenericTypeConstraintDecl(GenericTypeConstraintDecl* decl)
    {
        // TODO: are there any other validations we can do at this point?
        //
        // There probably needs to be a kind of "occurs check" to make
        // sure that the constraint actually applies to at least one
        // of the parameters of the generic.
        //
        CheckConstraintSubType(decl->sub);
        decl->sub = TranslateTypeNodeForced(decl->sub);
        decl->sup = TranslateTypeNodeForced(decl->sup);
    }

    void SemanticsDeclHeaderVisitor::visitGenericTypeParamDecl(GenericTypeParamDecl* decl)
    {
        // TODO: could probably push checking the default value
        // for a generic type parameter later.
        //
        decl->initType = CheckProperType(decl->initType);
    }

    void SemanticsDeclHeaderVisitor::visitGenericValueParamDecl(GenericValueParamDecl* decl)
    {
        checkVarDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitGenericDecl(GenericDecl* genericDecl)
    {
        genericDecl->setCheckState(DeclCheckState::ReadyForLookup);

        // NOTE! We purposefully do not iterate with the for(auto m : genericDecl->members) here,
        // because the visitor may add to `members` whilst iteration takes place, invalidating the iterator
        // and likely a crash.
        // 
        // Accessing the members via index side steps the issue.
        const auto& members = genericDecl->members;
        for (Index i = 0; i < members.getCount(); ++i)
        {
            Decl* m = members[i];

            if (auto typeParam = as<GenericTypeParamDecl>(m))
            {
                ensureDecl(typeParam, DeclCheckState::ReadyForReference);
            }
            else if (auto valParam = as<GenericValueParamDecl>(m))
            {
                ensureDecl(valParam, DeclCheckState::ReadyForReference);
            }
            else if (auto constraint = as<GenericTypeConstraintDecl>(m))
            {
                ensureDecl(constraint, DeclCheckState::ReadyForReference);
            }
        }
    }

    void SemanticsDeclBasesVisitor::visitInheritanceDecl(InheritanceDecl* inheritanceDecl)
    {
        // check the type being inherited from
        auto base = inheritanceDecl->base;
        CheckConstraintSubType(base);
        base = TranslateTypeNode(base);
        inheritanceDecl->base = base;

        // Note: we do not check whether the type being inherited from
        // is valid to use for inheritance here, because there could
        // be contextual factors that need to be taken into account
        // based on the declaration that is doing the inheriting.
    }

        // Concretize interface conformances so that we have witnesses as required for lookup.
        // for lookup.
    struct SemanticsDeclConformancesVisitor
        : public SemanticsDeclVisitorBase
        , public DeclVisitor<SemanticsDeclConformancesVisitor>
    {
        SemanticsDeclConformancesVisitor(SemanticsContext const& outer)
            : SemanticsDeclVisitorBase(outer)
        {}

        void visitDecl(Decl*) {}
        void visitDeclGroup(DeclGroup*) {}

        // Any user-defined type may have declared interface conformances,
        // which we should check.
        //
        void visitAggTypeDecl(AggTypeDecl* aggTypeDecl)
        {
            checkAggTypeConformance(aggTypeDecl);
        }

        // Conformances can also come via `extension` declarations, and
        // we should check them against the type(s) being extended.
        //
        void visitExtensionDecl(ExtensionDecl* extensionDecl)
        {
            checkExtensionConformance(extensionDecl);
        }
    };

        /// Recursively register any builtin declarations that need to be attached to the `session`.
        ///
        /// This function should only be needed for declarations in the standard library.
        ///
    static void _registerBuiltinDeclsRec(Session* session, Decl* decl)
    {
        SharedASTBuilder* sharedASTBuilder = session->m_sharedASTBuilder;

        if (auto builtinMod = decl->findModifier<BuiltinTypeModifier>())
        {
            sharedASTBuilder->registerBuiltinDecl(decl, builtinMod);
        }
        if (auto magicMod = decl->findModifier<MagicTypeModifier>())
        {
            sharedASTBuilder->registerMagicDecl(decl, magicMod);
        }

        if(auto containerDecl = as<ContainerDecl>(decl))
        {
            for(auto childDecl : containerDecl->members)
            {
                if(as<ScopeDecl>(childDecl))
                    continue;

                _registerBuiltinDeclsRec(session, childDecl);
            }
        }
        if(auto genericDecl = as<GenericDecl>(decl))
        {
            _registerBuiltinDeclsRec(session, genericDecl->inner);
        }
    }

    void registerBuiltinDecls(Session* session, Decl* decl)
    {
        _registerBuiltinDeclsRec(session, decl);
    }

    void SemanticsDeclVisitorBase::checkModule(ModuleDecl* moduleDecl)
    {
        // When we are dealing with code from the standard library,
        // there is a potential problem where we might need to look
        // up built-in types like `Int` through the session (e.g.,
        // to determine the type for an integer literal), but those
        // types might not have been registered yet. We solve that
        // by doing a pre-process on standard-library code to find
        // and register any built-in declarations.
        //
        // TODO: This could be factored into another visitor pass
        // that fits the more standard checking below, but that would
        // seemingly add overhead to checking things other than
        // the standard library.
        //
        if(isFromStdLib(moduleDecl))
        {
            _registerBuiltinDeclsRec(getSession(), moduleDecl);
        }

        // We need/want to visit any `import` declarations before
        // anything else, to make sure that scoping works.
        //
        // TODO: This could be factored into another visitor pass
        // that fits more with the standard checking below.
        //
        for(auto importDecl : moduleDecl->getMembersOfType<ImportDecl>())
        {
            ensureDecl(importDecl, DeclCheckState::Checked);
        }

        // The entire goal of semantic checking is to get all of the
        // declarations in the module up to `DeclCheckState::Checked`.
        //
        // The main catch is that checking one declaration A up to state M
        // may required that declaration B is checked up to state N.
        // A call to `ensureDecl(B, N)` can guarantee that things are checked
        // when and where we need them, but that runs the risk of creating
        // very deep recursion in the semantic checking.
        //
        // Instead, we would rather do more breadth-first checking,
        // where everything gets checked up to state 1, 2, ...
        // before anything gets too far ahead.
        // We will therefore enumerate the states/phases for checking,
        // and then iteratively try to update all declarations to each
        // state in turn.
        //
        // Note: for a simpler language we could eliminate `ensureDecl`
        // completely and *just* have these phases of checking.
        // Unfortunately, we have some circularity between the phases:
        //
        // * Checking an overloaded call requires knowing the parameter
        //   types of all candidate callees.
        //
        // * Checking the parameter type of a function requires being
        //   able to check type expressions.
        //
        // * A type expression like `vector<T, N>` may have an arbitary
        //   expression for `N`.
        //
        // * An arbitrary expression may include function calls, which
        //   may be to overloaded functions.
        //
        // Languages like C++ solve the apparent problem by making
        // restrictions on order of declaration/definition (and by
        // requiring forward declarations or the `template`/`typename`
        // keywrods in some cases).
        //
        // TODO: We could eventually eliminate the potential recursion
        // in checking by splitting each phase into a "requirements gathering"
        // step and an actual execution step.
        //
        // When checking a declaration D up to state S, the requirements
        // gathering step would produce a list of pairs `(someDecl, someState)`
        // indicating that `someDecl` must be in `someState` before the
        // actual execution of checking for `(D,S)` can proceeed. The checker
        // can then produce an elaborated dependency graph and select nodes
        // for execution in an order that satisfies all the dependencies.
        //
        // Such a more elaborate checking scheme will have to wait for another
        // day, but might be worth it (or even necessary) if/when we want to
        // support incremental compilation.
        //
        DeclCheckState states[] =
        {
            DeclCheckState::ModifiersChecked,
            DeclCheckState::ReadyForReference,
            DeclCheckState::ReadyForLookup,
            DeclCheckState::ReadyForLookup,
            DeclCheckState::Checked
        };
        for(auto s : states)
        {
            // When advancing to state `s` we will recursively
            // advance all declarations rooted in the module
            // up to `s`.
            //
            // TODO: In cases where a large module is split across files,
            // we could potentially parallelize front-end compilation by
            // having multiple instances of the front end where each is
            // only responsible for those declarations in a given file.
            //
            // Under that model, we might only apply later phases of
            // checking (notably the final push to `DeclState::Checked`)
            // to the subset of declarations coming from a given source
            // file.
            //
            _ensureAllDeclsRec(this, moduleDecl, s);
        }

        // Once we have completed the above loop, all declarations not
        // nested in function bodies should be in `DeclState::Checked`.
        // Furthermore, because a fully checked function will have checked
        // its body, this also means that all function bodies and the
        // declarations they contain should be fully checked.
    }

    bool SemanticsVisitor::doesSignatureMatchRequirement(
        DeclRef<CallableDecl>   satisfyingMemberDeclRef,
        DeclRef<CallableDecl>   requiredMemberDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        if(satisfyingMemberDeclRef.getDecl()->hasModifier<MutatingAttribute>()
            && !requiredMemberDeclRef.getDecl()->hasModifier<MutatingAttribute>())
        {
            // A `[mutating]` method can't satisfy a non-`[mutating]` requirement,
            // but vice-versa is okay.
            return false;
        }

        if(satisfyingMemberDeclRef.getDecl()->hasModifier<HLSLStaticModifier>()
            != requiredMemberDeclRef.getDecl()->hasModifier<HLSLStaticModifier>())
        {
            // A `static` method can't satisfy a non-`static` requirement and vice versa.
            return false;
        }

        // A signature matches the required one if it has the right number of parameters,
        // and those parameters have the right types, and also the result/return type
        // is the required one.
        //
        auto requiredParams = getParameters(requiredMemberDeclRef).toArray();
        auto satisfyingParams = getParameters(satisfyingMemberDeclRef).toArray();
        auto paramCount = requiredParams.getCount();
        if(satisfyingParams.getCount() != paramCount)
            return false;

        for(Index paramIndex = 0; paramIndex < paramCount; ++paramIndex)
        {
            auto requiredParam = requiredParams[paramIndex];
            auto satisfyingParam = satisfyingParams[paramIndex];

            auto requiredParamType = getType(m_astBuilder, requiredParam);
            auto satisfyingParamType = getType(m_astBuilder, satisfyingParam);

            if(!requiredParamType->equals(satisfyingParamType))
                return false;
        }

        auto requiredResultType = getResultType(m_astBuilder, requiredMemberDeclRef);
        auto satisfyingResultType = getResultType(m_astBuilder, satisfyingMemberDeclRef);
        if(!requiredResultType->equals(satisfyingResultType))
            return false;

        witnessTable->add(
            requiredMemberDeclRef.getDecl(),
            RequirementWitness(satisfyingMemberDeclRef));
        return true;
    }

    bool SemanticsVisitor::doesAccessorMatchRequirement(
        DeclRef<AccessorDecl>   satisfyingMemberDeclRef,
        DeclRef<AccessorDecl>   requiredMemberDeclRef)
    {
        // We require the AST node class of the satisfying accessor
        // to be a subclass of the one from the required accessor.
        //
        // For our current accessor types, this amounts to requiring
        // an exact match, but using a subtype test means that if
        // we ever add an `ExtraSpecialGetDecl` that is a subclass
        // of `GetDecl`, then one of those would be able to satisfy
        // a `get` requirement.
        //
        auto satisfyingMemberClass = satisfyingMemberDeclRef.getDecl()->getClass();
        auto requiredMemberClass = requiredMemberDeclRef.getDecl()->getClass();
        if(!satisfyingMemberClass.isSubClassOfImpl(requiredMemberClass))
            return false;

        // We do not check the parameters or return types of accessors
        // here, under the assumption that the validity checks for
        // the parent `property` declaration would already make sure
        // they are in order.

        // TODO: There are other checks we need to make here, like not letting
        // an ordinary `set` satisfy a `[nonmutating] set` requirement.

        return true;
    }

    bool SemanticsVisitor::doesPropertyMatchRequirement(
        DeclRef<PropertyDecl>   satisfyingMemberDeclRef,
        DeclRef<PropertyDecl>   requiredMemberDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        // The type of the satisfying member must match the type of the required member.
        //
        // Note: It is possible that a `get`-only property could be satisfied by
        // a declaration that uses a subtype of the requirement, but that would not
        // count as an "exact match" and we would rely on the logic to synthesize
        // a stub implementation in that case.
        //
        auto satisfyingType = getType(getASTBuilder(), satisfyingMemberDeclRef);
        auto requiredType = getType(getASTBuilder(), requiredMemberDeclRef);
        if(!satisfyingType->equals(requiredType))
            return false;

        // Each accessor in the requirement must be accounted for by an accessor
        // in the satisfying member.
        //
        // Note: it is fine for the satisfying member to provide *more* accessors
        // than the original declaration.
        //
        Dictionary<DeclRef<AccessorDecl>, DeclRef<AccessorDecl>> mapRequiredToSatisfyingAccessorDeclRef;
        for( auto requiredAccessorDeclRef : getMembersOfType<AccessorDecl>(requiredMemberDeclRef) )
        {
            // We need to search for an accessor that can satisfy the requirement.
            //
            // For now we will do the simplest (and slowest) thing of a linear search,
            // which is mostly fine because the number of accessors is bounded.
            //
            bool found = false;
            for( auto satisfyingAccessorDeclRef : getMembersOfType<AccessorDecl>(satisfyingMemberDeclRef) )
            {
                if( doesAccessorMatchRequirement(satisfyingAccessorDeclRef, requiredAccessorDeclRef) )
                {
                    // When we find a match on an accessor, we record it so that
                    // we can set up the witness values later, but we do *not*
                    // record it into the actual witness table yet, in case
                    // a later accessor comes along that doesn't find a match.
                    //
                    mapRequiredToSatisfyingAccessorDeclRef.Add(requiredAccessorDeclRef, satisfyingAccessorDeclRef);
                    found = true;
                    break;
                }
            }
            if(!found)
                return false;
        }

        // Once things are done, we will install the satisfying values
        // into the witness table for the requirements.
        //
        for( auto p : mapRequiredToSatisfyingAccessorDeclRef )
        {
            witnessTable->add(
                p.Key,
                RequirementWitness(p.Value));
        }
        //
        // Note: the property declaration itself isn't something that
        // has a useful value/representation in downstream passes, so
        // we are mostly just installing it into the witness table
        // as a way to mark this requirement as being satisfied.
        //
        // TODO: It is possible that having a witness table entry that
        // doesn't actually map to any IR value could create a problem
        // in downstream passes. If such propblems arise, we should
        // probably create a new `RequirementWitness` case that
        // represents a witness value that is only needed by the front-end,
        // and that can be ignored by IR and emit logic.
        //
        witnessTable->add(
            requiredMemberDeclRef.getDecl(),
            RequirementWitness(satisfyingMemberDeclRef));
        return true;
    }

    bool SemanticsVisitor::doesVarMatchRequirement(
        DeclRef<VarDeclBase>   satisfyingMemberDeclRef,
        DeclRef<VarDeclBase>   requiredMemberDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        // The type of the satisfying member must match the type of the required member.
        auto satisfyingType = getType(getASTBuilder(), satisfyingMemberDeclRef);
        auto requiredType = getType(getASTBuilder(), requiredMemberDeclRef);
        if (!satisfyingType->equals(requiredType))
            return false;

        for (auto modifier : requiredMemberDeclRef.getDecl()->modifiers)
        {
            bool found = false;
            for (auto satisfyingModifier : satisfyingMemberDeclRef.getDecl()->modifiers)
            {
                if (satisfyingModifier->astNodeType == modifier->astNodeType)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }

        auto satisfyingVal = tryConstantFoldDeclRef(satisfyingMemberDeclRef, nullptr);
        if (satisfyingVal)
        {
            witnessTable->add(
                requiredMemberDeclRef,
                RequirementWitness(satisfyingVal));
        }
        else
        {
            witnessTable->add(
                requiredMemberDeclRef.getDecl(),
                RequirementWitness(satisfyingMemberDeclRef));
        }
        return true;
    }

    bool SemanticsVisitor::doesGenericSignatureMatchRequirement(
        DeclRef<GenericDecl>        satisfyingGenericDeclRef,
        DeclRef<GenericDecl>        requiredGenericDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        // The signature of a generic is defiend by its members, and we need the
        // satisfying value to have the same number of members for it to be an
        // exact match.
        //
        auto memberCount = requiredGenericDeclRef.getDecl()->members.getCount();
        if(satisfyingGenericDeclRef.getDecl()->members.getCount() != memberCount)
            return false;

        // We then want to check that pairwise members match, in order.
        //
        auto requiredMemberDeclRefs = getMembers(requiredGenericDeclRef);
        auto satisfyingMemberDeclRefs = getMembers(satisfyingGenericDeclRef);
        //
        // We start by performing a superficial "structural" match of the parameters
        // to ensure that the two generics have an equivalent mix of type, value,
        // and constraint parameters in the same order.
        //
        // Note that in this step we do *not* make any checks on the actual types
        // involved in constraints, or on the types of value parameters. The reason
        // for this is that the types on those parameters could be dependent on
        // type parameters in the generic parameter list, and thus there could be
        // a mismatch at this point. For example, if we have:
        //
        //      interface IBase         { void doThing<T, U : IThing<T>>(); }
        //      struct Derived : IBase  { void doThing<X, Y : IThing<X>>(); }
        //
        // We clearly have a signature match here, but the constraint parameters for
        // `U : IThing<T>` and `Y : IThing<X>` have the problem that both the sub-type
        // and super-type they reference are not equivalent without substititions.
        //
        // We will deal with this issue after the structural matching is checked, at
        // which point we can actually verify things like types.
        //
        for (Index i = 0; i < memberCount; i++)
        {
            auto requiredMemberDeclRef = requiredMemberDeclRefs[i];
            auto satisfyingMemberDeclRef = satisfyingMemberDeclRefs[i];

            if (as<GenericTypeParamDecl>(requiredMemberDeclRef))
            {
                if (as<GenericTypeParamDecl>(satisfyingMemberDeclRef))
                {
                }
                else
                    return false;
            }
            else if (auto requiredValueParamDeclRef = requiredMemberDeclRef.as<GenericValueParamDecl>())
            {
                if (auto satisfyingValueParamDeclRef = satisfyingMemberDeclRef.as<GenericValueParamDecl>())
                {
                }
                else
                    return false;
            }
            else if (auto requiredConstraintDeclRef = requiredMemberDeclRef.as<GenericTypeConstraintDecl>())
            {
                if (auto satisfyingConstraintDeclRef = satisfyingMemberDeclRef.as<GenericTypeConstraintDecl>())
                {
                }
                else
                    return false;
            }
        }

        // In order to compare the inner declarations of the two generics, we need to
        // align them so that they are expressed in terms of consistent type parameters.
        //
        // For example, we might have:
        //
        //      interface IBase           { void doThing<T>(T val); }
        //      struct    Derived : IBase { void doThing<U>(U val); }
        //
        // If we directly compare the signatures of the inner `doThing` function declarations,
        // we'd find a mismatch between the `T` and `U` types of the `val` parameter.
        //
        // We can get around this mismatch by constructing a specialized reference and
        // then doing the comparison. For example `IBase::doThing<X>` and `Derived::doThing<X>`
        // should both have the signature `X -> void`.
        //
        // The one big detail that we need to be careful about here is that when we
        // recursively call `doesMemberSatisfyRequirement`, that will eventually store
        // the satisfying `DeclRef` as the value for the given requirement key, and we don't
        // want to store a specialized reference like `Derived::doThing<X>` - we need to
        // somehow store the original declaration.
        //
        // The solution here is to specialize the *required* declaration to the parameters
        // of the satisfying declaration. In the example above that means we are going to
        // compare `Derived::doThing` against `IBase::doThing<U>` where the `U` there is
        // the parameter of `Dervived::doThing`.
        //
        List<Val*> requiredSubstArgs;

        for (Index i = 0; i < memberCount; i++)
        {
            auto requiredMemberDeclRef = requiredMemberDeclRefs[i];
            auto satisfyingMemberDeclRef = satisfyingMemberDeclRefs[i];

            if(auto requiredTypeParamDeclRef = requiredMemberDeclRef.as<GenericTypeParamDecl>())
            {
                auto satisfyingTypeParamDeclRef = satisfyingMemberDeclRef.as<GenericTypeParamDecl>();
                SLANG_ASSERT(satisfyingTypeParamDeclRef);
                auto satisfyingType = DeclRefType::create(m_astBuilder, satisfyingTypeParamDeclRef);

                requiredSubstArgs.add(satisfyingType);
            }
            else if (auto requiredValueParamDeclRef = requiredMemberDeclRef.as<GenericValueParamDecl>())
            {
                auto satisfyingValueParamDeclRef = satisfyingMemberDeclRef.as<GenericValueParamDecl>();
                SLANG_ASSERT(satisfyingValueParamDeclRef);

                auto satisfyingVal = m_astBuilder->getOrCreate<GenericParamIntVal>(
                    requiredValueParamDeclRef.getDecl()->getType(),
                    satisfyingValueParamDeclRef.getDecl(),
                    satisfyingValueParamDeclRef.substitutions.substitutions);
                satisfyingVal->declRef = satisfyingValueParamDeclRef;

                requiredSubstArgs.add(satisfyingVal);
            }
        }
        for (Index i = 0; i < memberCount; i++)
        {
            auto requiredMemberDeclRef = requiredMemberDeclRefs[i];
            auto satisfyingMemberDeclRef = satisfyingMemberDeclRefs[i];

            if(auto requiredConstraintDeclRef = requiredMemberDeclRef.as<GenericTypeConstraintDecl>())
            {
                auto satisfyingConstraintDeclRef = satisfyingMemberDeclRef.as<GenericTypeConstraintDecl>();
                SLANG_ASSERT(satisfyingConstraintDeclRef);

                auto satisfyingWitness = m_astBuilder->getOrCreate<DeclaredSubtypeWitness>();
                satisfyingWitness->sub = getSub(m_astBuilder, satisfyingConstraintDeclRef);
                satisfyingWitness->sup = getSup(m_astBuilder, satisfyingConstraintDeclRef);
                satisfyingWitness->declRef = satisfyingConstraintDeclRef;

                requiredSubstArgs.add(satisfyingWitness);
            }
        }

        GenericSubstitution* requiredSubst = m_astBuilder->getOrCreateGenericSubstitution(
            requiredGenericDeclRef.getDecl(),
            requiredSubstArgs,
            requiredGenericDeclRef.substitutions);

        // Now that we have computed a set of specialization arguments that will
        // specialize the generic requirement at the type parameters of the satisfying
        // generic, we can construct a reference to that declaration and re-run some
        // of the earlier checking logic with more type information usable.
        //
        auto specializedRequiredGenericDeclRef = DeclRef<GenericDecl>(requiredGenericDeclRef.getDecl(), requiredSubst);
        auto specializedRequiredMemberDeclRefs = getMembers(specializedRequiredGenericDeclRef);
        for (Index i = 0; i < memberCount; i++)
        {
            auto requiredMemberDeclRef = specializedRequiredMemberDeclRefs[i];
            auto satisfyingMemberDeclRef = satisfyingMemberDeclRefs[i];

            if(auto requiredTypeParamDeclRef = requiredMemberDeclRef.as<GenericTypeParamDecl>())
            {
                auto satisfyingTypeParamDeclRef = satisfyingMemberDeclRef.as<GenericTypeParamDecl>();
                SLANG_ASSERT(satisfyingTypeParamDeclRef);

                // There are no additional checks we need to make on plain old
                // type parameters at this point.
                //
                // TODO: If we ever support having type parameters of higher kinds,
                // then this is possibly where we'd want to check that the kinds of
                // the two parameters match.
                //
                SLANG_UNUSED(satisfyingGenericDeclRef);
            }
            else if (auto requiredValueParamDeclRef = requiredMemberDeclRef.as<GenericValueParamDecl>())
            {
                auto satisfyingValueParamDeclRef = satisfyingMemberDeclRef.as<GenericValueParamDecl>();
                SLANG_ASSERT(satisfyingValueParamDeclRef);

                // For a generic value parameter, we need to check that the required
                // and satisfying declaration both agree on the type of the parameter.
                //
                auto requiredParamType = getType(m_astBuilder, requiredValueParamDeclRef);
                auto satisfyingParamType = getType(m_astBuilder, satisfyingValueParamDeclRef);
                if (!satisfyingParamType->equals(requiredParamType))
                    return false;
            }
            else if(auto requiredConstraintDeclRef = requiredMemberDeclRef.as<GenericTypeConstraintDecl>())
            {
                auto satisfyingConstraintDeclRef = satisfyingMemberDeclRef.as<GenericTypeConstraintDecl>();
                SLANG_ASSERT(satisfyingConstraintDeclRef);

                // For a generic constraint parameter, we need to check that the sub-type
                // and super-type in the constraint both match.
                //
                // In current code the sub type will always be one of the generic type parameters,
                // and the super-type will always be an interface, but there should be no
                // need to make use of those additional details here.

                auto requiredSubType = getSub(m_astBuilder, requiredConstraintDeclRef);
                auto satisfyingSubType = getSub(m_astBuilder, satisfyingConstraintDeclRef);
                if (!satisfyingSubType->equals(requiredSubType))
                    return false;

                auto requiredSuperType = getSup(m_astBuilder, requiredConstraintDeclRef);
                auto satisfyingSuperType = getSup(m_astBuilder, satisfyingConstraintDeclRef);
                if (!satisfyingSuperType->equals(requiredSuperType))
                    return false;
            }
        }

        // Note: the above logic really only applies to the case of an exact match on signature,
        // even down to the way that constraints were declared. We could potentially be more
        // relaxed by taking advantage of the way that various different generic signatures will
        // actually lower to the same IR generic signature.
        //
        // In theory, all we really care about when it comes to constraints is that the constraints
        // on the required and satisfying declaration are *equivalent*.
        //
        // More generally, a satisfying generic could actually provide *looser* constraints and
        // still work; all that matters is that it can be instantiated at any argument values/types
        // that are valid for the requirement.
        //
        // We leave both of those issues up to the synthesis path: if we do not find a member that
        // provides an exact match, then the compiler should try to synthesize one that is an exact
        // match and makes use of existing declarations that might have require defaulting of arguments
        // or type conversations to fit.

        // Once we've validated that the generic signatures are in an exact match, and devised type
        // arguments for the requirement to make the two align, we can recursively check the inner
        // declaration (whatever it is) for an exact match.
        //
        return doesMemberSatisfyRequirement(
            DeclRef<Decl>(satisfyingGenericDeclRef.getDecl()->inner, satisfyingGenericDeclRef.substitutions),
            DeclRef<Decl>(requiredGenericDeclRef.getDecl()->inner, requiredSubst),
            witnessTable);
    }

    bool SemanticsVisitor::doesTypeSatisfyAssociatedTypeRequirement(
        Type*            satisfyingType,
        DeclRef<AssocTypeDecl>  requiredAssociatedTypeDeclRef,
        RefPtr<WitnessTable>    witnessTable)
    {
        // We need to confirm that the chosen type `satisfyingType`,
        // meets all the constraints placed on the associated type
        // requirement `requiredAssociatedTypeDeclRef`.
        //
        // We will enumerate the type constraints placed on the
        // associated type and see if they can be satisfied.
        //
        bool conformance = true;
        for (auto requiredConstraintDeclRef : getMembersOfType<TypeConstraintDecl>(requiredAssociatedTypeDeclRef))
        {
            // Grab the type we expect to conform to from the constraint.
            auto requiredSuperType = getSup(m_astBuilder, requiredConstraintDeclRef);

            // Perform a search for a witness to the subtype relationship.
            auto witness = tryGetSubtypeWitness(satisfyingType, requiredSuperType);
            if(witness)
            {
                // If a subtype witness was found, then the conformance
                // appears to hold, and we can satisfy that requirement.
                witnessTable->add(requiredConstraintDeclRef, RequirementWitness(witness));
            }
            else
            {
                // If a witness couldn't be found, then the conformance
                // seems like it will fail.
                conformance = false;
            }
        }

        // TODO: if any conformance check failed, we should probably include
        // that in an error message produced about not satisfying the requirement.

        if(conformance)
        {
            // If all the constraints were satisfied, then the chosen
            // type can indeed satisfy the interface requirement.
            witnessTable->add(
                requiredAssociatedTypeDeclRef.getDecl(),
                RequirementWitness(satisfyingType));
        }

        return conformance;
    }

    bool SemanticsVisitor::doesMemberSatisfyRequirement(
        DeclRef<Decl>               memberDeclRef,
        DeclRef<Decl>               requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        // Sanity check: if are checking whether a type `T`
        // implements, say, `IFoo::bar` and lookup of `bar`
        // in type `T` yielded `IFoo::bar`, then that shouldn't
        // be treated as a valid satisfaction of the requirement.
        //
        // TODO: Ideally this check should be comparing the `DeclRef`s
        // and not just the `Decl`s, but we currently don't get exactly
        // the same substitutions when we see the inherited `IFoo::bar`.
        //
        if(memberDeclRef.getDecl() == requiredMemberDeclRef.getDecl())
            return false;

        // At a high level, we want to check that the
        // `memberDecl` and the `requiredMemberDeclRef`
        // have the same AST node class, and then also
        // check that their signatures match.
        //
        // There are a bunch of detailed decisions that
        // have to be made, though, because we might, e.g.,
        // allow a function with more general parameter
        // types to satisfy a requirement with more
        // specific parameter types.
        //
        // If we ever allow for "property" declarations,
        // then we would probably need to allow an
        // ordinary field to satisfy a property requirement.
        //
        // An associated type requirement should be allowed
        // to be satisfied by any type declaration:
        // a typedef, a `struct`, etc.
        //
        if (auto memberFuncDecl = memberDeclRef.as<FuncDecl>())
        {
            if (auto requiredFuncDeclRef = requiredMemberDeclRef.as<FuncDecl>())
            {
                // Check signature match.
                return doesSignatureMatchRequirement(
                    memberFuncDecl,
                    requiredFuncDeclRef,
                    witnessTable);
            }
        }
        else if (auto memberInitDecl = memberDeclRef.as<ConstructorDecl>())
        {
            if (auto requiredInitDecl = requiredMemberDeclRef.as<ConstructorDecl>())
            {
                // Check signature match.
                return doesSignatureMatchRequirement(
                    memberInitDecl,
                    requiredInitDecl,
                    witnessTable);
            }
        }
        else if (auto genDecl = memberDeclRef.as<GenericDecl>())
        {
            // For a generic member, we will check if it can satisfy
            // a generic requirement in the interface.
            //
            // TODO: we could also conceivably check that the generic
            // could be *specialized* to satisfy the requirement,
            // and then install a specialization of the generic into
            // the witness table. Actually doing this would seem
            // to require performing something akin to overload
            // resolution as part of requirement satisfaction.
            //
            if (auto requiredGenDeclRef = requiredMemberDeclRef.as<GenericDecl>())
            {
                return doesGenericSignatureMatchRequirement(genDecl, requiredGenDeclRef, witnessTable);
            }
        }
        else if (auto subAggTypeDeclRef = memberDeclRef.as<AggTypeDecl>())
        {
            if(auto requiredTypeDeclRef = requiredMemberDeclRef.as<AssocTypeDecl>())
            {
                ensureDecl(subAggTypeDeclRef, DeclCheckState::CanUseAsType);

                auto satisfyingType = DeclRefType::create(m_astBuilder, subAggTypeDeclRef);
                return doesTypeSatisfyAssociatedTypeRequirement(satisfyingType, requiredTypeDeclRef, witnessTable);
            }
        }
        else if (auto typedefDeclRef = memberDeclRef.as<TypeDefDecl>())
        {
            // this is a type-def decl in an aggregate type
            // check if the specified type satisfies the constraints defined by the associated type
            if (auto requiredTypeDeclRef = requiredMemberDeclRef.as<AssocTypeDecl>())
            {
                ensureDecl(typedefDeclRef, DeclCheckState::CanUseAsType);

                auto satisfyingType = getNamedType(m_astBuilder, typedefDeclRef);
                return doesTypeSatisfyAssociatedTypeRequirement(satisfyingType, requiredTypeDeclRef, witnessTable);
            }
        }
        else if( auto propertyDeclRef = memberDeclRef.as<PropertyDecl>() )
        {
            if( auto requiredPropertyDeclRef = requiredMemberDeclRef.as<PropertyDecl>() )
            {
                ensureDecl(propertyDeclRef, DeclCheckState::CanUseFuncSignature);
                return doesPropertyMatchRequirement(propertyDeclRef, requiredPropertyDeclRef, witnessTable);
            }
        }
        else if (auto varDeclRef = memberDeclRef.as<VarDeclBase>())
        {
            if (auto requiredVarDeclRef = requiredMemberDeclRef.as<VarDeclBase>())
            {
                ensureDecl(varDeclRef, DeclCheckState::SignatureChecked);
                return doesVarMatchRequirement(varDeclRef, requiredVarDeclRef, witnessTable);
            }
        }
        // Default: just assume that thing aren't being satisfied.
        return false;
    }

    bool SemanticsVisitor::trySynthesizeMethodRequirementWitness(
        ConformanceCheckingContext* context,
        LookupResult const&         lookupResult,
        DeclRef<FuncDecl>           requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        // The situation here is that the context of an inheritance
        // declaration didn't provide an exact match for a required
        // method. E.g.:
        //
        //      interface ICounter { [mutating] int increment(); }
        //      struct MyCounter : ICounter
        //      {
        //          [murtating] int increment(int val = 1) { ... }
        //      }
        //
        // It is clear in this case that the `MyCounter` type *can*
        // satisfy the signature required by `ICounter`, but it has
        // no explicit method declaration that is a perfect match.
        //
        // The approach in this function will be to construct a
        // synthesized method along the lines of:
        //
        //      struct MyCounter ...
        //      {
        //          ...
        //          [murtating] int synthesized()
        //          {
        //              return this.increment();
        //          }
        //      }
        //
        // That is, we construct a method with the exact signature
        // of the requirement (same parameter and result types),
        // and then provide it with a body that simple `return`s
        // the result of applying the desired requirement name
        // (`increment` in this case) to those parameters.
        //
        // If the synthesized method type-checks, then we can say
        // that the type must satisfy the requirement structurally,
        // even if there isn't an exact signature match. More
        // importantly, the method we just synthesized can be
        // used as a witness to the fact that the requirement is
        // satisfied.

        // With the big picture spelled out, we can settle into
        // the work of constructing our synthesized method.
        //
        auto synFuncDecl = m_astBuilder->create<FuncDecl>();

        // For now our synthesized method will use the name and source
        // location of the requirement we are trying to satisfy.
        //
        // TODO: as it stands right now our syntesized method will
        // get a mangled name, which we don't actually want. Leaving
        // out the name here doesn't help matters, because then *all*
        // snthesized methods on a given type would share the same
        // mangled name!
        //
        synFuncDecl->nameAndLoc = requiredMemberDeclRef.getDecl()->nameAndLoc;

        // The result type of our synthesized method will be the expected
        // result type from the interface requirement.
        //
        // TODO: This logic can/will run into problems if the return type
        // is an associated type.
        //
        // The ideal solution is that we should be solving for interface
        // conformance in two phases: a first phase to solve for how
        // associated types are satisfied, and then a second phase to solve
        // for how other requirements are satisfied (where we can substitute
        // in the associated type witnesses for the abstract associated
        // types as part of `requiredMemberDeclRef`).
        //
        // TODO: We should also double-check that this logic will work
        // with a method that returns `This`.
        //
        auto resultType = getResultType(m_astBuilder, requiredMemberDeclRef);
        synFuncDecl->returnType.type = resultType;

        // Our synthesized method will have parameters matching the names
        // and types of those on the requirement, and it will use expressions
        // that reference those parametesr as arguments for the call expresison
        // that makes up the body.
        //
        List<Expr*> synArgs;
        for( auto paramDeclRef : getParameters(requiredMemberDeclRef) )
        {
            auto paramType = getType(m_astBuilder, paramDeclRef);

            // For each parameter of the requirement, we create a matching
            // parameter (same name and type) for the synthesized method.
            //
            auto synParamDecl = m_astBuilder->create<ParamDecl>();
            synParamDecl->nameAndLoc = paramDeclRef.getDecl()->nameAndLoc;
            synParamDecl->type.type = paramType;

            // We need to add the parameter as a child declaration of
            // the method we are building.
            //
            synParamDecl->parentDecl = synFuncDecl;
            synFuncDecl->members.add(synParamDecl);

            // For each paramter, we will create an argument expression
            // for the call in the function body.
            //
            auto synArg = m_astBuilder->create<VarExpr>();
            synArg->declRef = makeDeclRef(synParamDecl);
            synArg->type = paramType;
            synArgs.add(synArg);
        }

        // Required interface methods can be `static` or non-`static`,
        // and non-`static` methods can be `[mutating]` or non-`[mutating]`.
        // All of these details affect how we introduce our `this` parameter,
        // if any.
        //
        ThisExpr* synThis = nullptr;
        if( requiredMemberDeclRef.getDecl()->hasModifier<HLSLStaticModifier>() )
        {
            auto synStaticModifier = m_astBuilder->create<HLSLStaticModifier>();
            synFuncDecl->modifiers.first = synStaticModifier;
        }
        else
        {
            // For a non-`static` requirement, we need a `this` parameter.
            //
            synThis = m_astBuilder->create<ThisExpr>();

            // The type of `this` in our method will be the type for
            // which we are synthesizing a conformance.
            //
            synThis->type.type = context->conformingType;

            if( requiredMemberDeclRef.getDecl()->hasModifier<MutatingAttribute>() )
            {
                // If the interface requirement is `[mutating]` then our
                // synthesized method should be too, and also the `this`
                // parameter should be an l-value.
                //
                synThis->type.isLeftValue = true;

                auto synMutatingAttr = m_astBuilder->create<MutatingAttribute>();
                synFuncDecl->modifiers.first = synMutatingAttr;
            }
        }

        // The body of our synthesized method is going to try to
        // make a call using the name of the method requirement (e.g.,
        // the name `increment` in our example at the top of this function).
        //
        // The caller already passed in a `LookupResult` that represents
        // an attempt to look up the given name in the type of `this`,
        // and we really just need to wrap that result up as an overloaded
        // expression.
        //
        auto synBase = m_astBuilder->create<OverloadedExpr>();
        synBase->name = requiredMemberDeclRef.getDecl()->getName();
        synBase->lookupResult2 = lookupResult;

        // If `synThis` is non-null, then we will use it as the base of
        // the overloaded expression, so that we have an overloaded
        // member reference, and not just an overloaded reference to some
        // static definitions.
        //
        synBase->base = synThis;

        // We now have the reference to the overload group we plan to call,
        // and we already built up the argument list, so we can construct
        // an `InvokeExpr` that represents the call we want to make.
        //
        auto synCall = m_astBuilder->create<InvokeExpr>();
        synCall->functionExpr = synBase;
        synCall->arguments = synArgs;

        // In order to know if our call is well-formed, we need to run
        // the semantic checking logic for overload resolution. If it
        // runs into an error, we don't want that being reported back
        // to the user as some kind of overload-resolution failure.
        //
        // In order to protect the user from whatever errors might
        // occur, we will perform the checking in the context of
        // a temporary diagnostic sink.
        //
        DiagnosticSink tempSink(getSourceManager(), nullptr);
        SemanticsVisitor subVisitor(withSink(&tempSink));

        // With our temporary diagnostic sink soaking up any messages
        // from overload resolution, we can now try to resolve
        // the call to see what happens.
        //
        auto checkedCall = subVisitor.ResolveInvoke(synCall);

        // Of course, it is possible that the call went through fine,
        // but the result isn't of the type we expect/require,
        // so we also need to coerce the result of the call to
        // the expected type.
        //
        auto coercedCall = subVisitor.coerce(resultType, checkedCall);

        // If our overload resolution or type coercion failed,
        // then we have not been able to synthesize a witness
        // for the requirement.
        //
        // TODO: We might want to detect *why* overload resolution
        // or type coercion failed, and report errors accordingly.
        //
        // More detailed diagnostics could help users understand
        // what they did wrong, e.g.:
        //
        // * "We tried to use `foo(int)` but the interface requires `foo(String)`
        //
        // * "You have two methods that can apply as `bar()` and we couldn't tell which one you meant
        //
        // For now we just bail out here and rely on the caller to
        // diagnose a generic "failed to satisfying requirement" error.
        //
        if(tempSink.getErrorCount() != 0)
            return false;

        // If we were able to type-check the call, then we should
        // be able to finish construction of a suitable witness.
        //
        // We've already created the outer declaration (including its
        // parameters), and the inner expression, so the main work
        // that is left is defining the body of the new function,
        // which comprises a single `return` statement.
        //
        auto synReturn = m_astBuilder->create<ReturnStmt>();
        synReturn->expression = coercedCall;

        synFuncDecl->body = synReturn;

        // Once we are sure that we want to use the declaration
        // we've synthesized, aew can go ahead and wire it up
        // to the AST so that subsequent stages can generate
        // IR code from it.
        //
        // Note: we set the parent of the synthesized declaration
        // to the parent of the inheritance declaration being
        // validated (which is either a type declaration or
        // an `extension`), but we do *not* add the syntehsized
        // declaration to the list of child declarations at
        // this point.
        //
        // By leaving the synthesized declaration off of the list
        // of members, we ensure that it doesn't get found
        // by lookup (e.g., in a module that `import`s this type).
        // Unfortunately, we may also break invariants in other parts
        // of the code if they assume that all declarations have
        // to appear in the parent/child hierarchy of the module.
        //
        // TODO: We may need to properly wire the synthesized
        // declaration into the hierarchy, but then attach a modifier
        // to it to indicate that it should be ignored by things like lookup.
        //
        synFuncDecl->parentDecl = context->parentDecl;

        // Once our synthesized declaration is complete, we need
        // to install it as the witness that satifies the given
        // requirement.
        //
        // Subsequent code generation should not be able to tell the
        // difference between our synthetic method and a hand-written
        // one with the same behavior.
        //
        witnessTable->add(requiredMemberDeclRef,
            RequirementWitness(makeDeclRef(synFuncDecl)));
        return true;
    }

    bool SemanticsVisitor::trySynthesizePropertyRequirementWitness(
        ConformanceCheckingContext* context,
        LookupResult const&         lookupResult,
        DeclRef<PropertyDecl>       requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        // The situation here is that the context of an inheritance
        // declaration didn't provide an exact match for a required
        // property. E.g.:
        //
        //      interface ICell { property value : int { get; set; } }
        //      struct MyCell : ICell
        //      {
        //          int value;
        //      }
        //
        // It is clear in this case that the `MyCell` type *can*
        // satisfy the signature required by `ICell`, but it has
        // no explicit `property` declaration, and instead just
        // a field with the right name and type.
        //
        // The approach in this function will be to construct a
        // synthesized `preoperty` along the lines of:
        //
        //      struct MyCounter ...
        //      {
        //          ...
        //          property value_synthesized : int
        //          {
        //              get { return this.value; }
        //              set(newValue) { this.value = newValue; }
        //          }
        //      }
        //
        // That is, we construct a `property` with the correct type
        // and with an accessor for each requirement, where the accesors
        // all try to read or write `this.value`.
        //
        // If those synthesized accessors all type-check, then we can
        // say that the type must satisfy the requirement structurally,
        // even if there isn't an exact signature match. More
        // importantly, the `property` we just synthesized can be
        // used as a witness to the fact that the requirement is
        // satisfied.
        //
        // The big-picture flow of the logic here is similar to
        // `trySynthesizeMethodRequirementWitness()` above, and we
        // will not comment this code as exhaustively, under the
        // assumption that readers of the code don't benefit from
        // having the exact same information stated twice.

        // With the introduction out of the way, let's get started
        // constructing a synthesized `PropertyDecl`.
        //
        auto synPropertyDecl = m_astBuilder->create<PropertyDecl>();

        // For now our synthesized property will use the name and source
        // location of the requirement we are trying to satisfy.
        //
        // TODO: as it stands right now our syntesized property and its
        // accesors will get mangled names, which we don't actually want.
        // Leaving out the name here doesn't help matters, becaues then
        // *all* synthesized members on a given type would share the same
        // mangled name.
        //
        synPropertyDecl->nameAndLoc = requiredMemberDeclRef.getDecl()->nameAndLoc;

        // The type of our synthesized property will be the expected type
        // of the interface requirement.
        //
        // TODO: This logic can/will run into problems if the type is,
        // or uses, an associated type or `This`.
        //
        // Ideally we should be looking up the type using a `DeclRef` that
        // refers to the interface requirement using a `ThisTypeSubstitution`
        // that refers to the satisfying type declaration, and requirement
        // checking for non-associated-type requirements should be done *after*
        // requirement checking for associated-type requirements.
        //
        auto propertyType = getType(m_astBuilder, requiredMemberDeclRef);
        synPropertyDecl->type.type = propertyType;

        // Our synthesized property will have an accessor declaration for
        // each accessor of the requirement.
        //
        // TODO: If we ever start to support synthesis for subscript requirements,
        // then we probably want to factor the accessor-related logic into
        // a subroutine so that it can be shared between properties and subscripts.
        //
        Dictionary<DeclRef<AccessorDecl>, AccessorDecl*> mapRequiredAccessorToSynAccessor;
        for( auto requiredAccessorDeclRef : getMembersOfType<AccessorDecl>(requiredMemberDeclRef) )
        {
            // The synthesized accessor will be an AST node of the same class as
            // the required accessor.
            //
            auto synAccessorDecl = (AccessorDecl*) m_astBuilder->createByNodeType(requiredAccessorDeclRef.getDecl()->astNodeType);

            // Whatever the required accessor returns, that is what our synthesized accessor will return.
            //
            synAccessorDecl->returnType.type = getResultType(m_astBuilder, requiredAccessorDeclRef);

            // Similarly, our synthesized accessor will have parameters matching those of the requirement.
            //
            // Note: in practice we expect that only `set` accessors will have any parameters,
            // and they will only have a single parameter.
            //
            List<Expr*> synArgs;
            for( auto requiredParamDeclRef : getParameters(requiredAccessorDeclRef) )
            {
                auto paramType = getType(m_astBuilder, requiredParamDeclRef);

                // The synthesized parameter will ahve the same name and
                // type as the parameter of the requirement.
                //
                auto synParamDecl = m_astBuilder->create<ParamDecl>();
                synParamDecl->nameAndLoc = requiredParamDeclRef.getDecl()->nameAndLoc;
                synParamDecl->type.type = paramType;

                // We need to add the parameter as a child declaration of
                // the accessor we are building.
                //
                synParamDecl->parentDecl = synAccessorDecl;
                synAccessorDecl->members.add(synParamDecl);

                // For each paramter, we will create an argument expression
                // to represent it in the body of the accessor.
                //
                auto synArg = m_astBuilder->create<VarExpr>();
                synArg->declRef = makeDeclRef(synParamDecl);
                synArg->type = paramType;
                synArgs.add(synArg);
            }

            // We need to create a `this` expression to be used in the body
            // of the synthesized accessor.
            //
            // TODO: if we ever allow `static` properties or subscripts,
            // we will need to handle that case here, by *not* creating
            // a `this` expression.
            //
            ThisExpr* synThis = m_astBuilder->create<ThisExpr>();

            // The type of `this` in our accessor will be the type for
            // which we are synthesizing a conformance.
            //
            synThis->type.type = context->conformingType;

            // A `get` accessor should default to an immutable `this`,
            // while other accessors default to mutable `this`.
            //
            // TODO: If we ever add other kinds of accessors, we will
            // need to check that this assumption stays valid.
            //
            synThis->type.isLeftValue = true;
            if(as<GetterDecl>(requiredAccessorDeclRef))
                synThis->type.isLeftValue = false;

            // If the accessor requirement is `[nonmutating]` then our
            // synthesized accessor should be too, and also the `this`
            // parameter should *not* be an l-value.
            //
            if( requiredAccessorDeclRef.getDecl()->hasModifier<NonmutatingAttribute>() )
            {
                synThis->type.isLeftValue = false;

                auto synAttr = m_astBuilder->create<NonmutatingAttribute>();
                synAccessorDecl->modifiers.first = synAttr;
            }
            //
            // Note: we don't currently support `[mutating] get` accessors,
            // but the desired behavior in that case is clear, so we go
            // ahead and future-proof this code a bit:
            //
            else if( requiredAccessorDeclRef.getDecl()->hasModifier<MutatingAttribute>() )
            {
                synThis->type.isLeftValue = true;

                auto synAttr = m_astBuilder->create<MutatingAttribute>();
                synAccessorDecl->modifiers.first = synAttr;
            }

            // We are going to synthesize an expression and then perform
            // semantic checking on it, but if there are semantic errors
            // we do *not* want to report them to the user as such, and
            // instead want the result to be a failure to synthesize
            // a valid witness.
            //
            // We will buffer up diagnostics into a temporary sink and
            // then throw them away when we are done.
            //
            // TODO: This behavior might be something we want to make
            // into a more fundamental capability of `DiagnosticSink` and/or
            // `SemanticsVisitor` so that code can push/pop the emission
            // of diagnostics more easily.
            //
            DiagnosticSink tempSink(getSourceManager(), nullptr);
            SemanticsVisitor subVisitor(withSink(&tempSink));

            // We start by constructing an expression that represents
            // `this.name` where `name` is the name of the required
            // member. The caller already passed in a `lookupResult`
            // that should indicate all the declarations found by
            // looking up `name`, so we can start with that.
            //
            // TODO: Note that there are many cases for member lookup
            // that are not handled just by using `createLookupResultExpr`
            // because they are currently being special-cased (the most
            // notable cases are swizzles, as well as lookup of static
            // members in types).
            //
            // The main result here is that we will not be able to synthesize
            // a requirement for a built-in scalar/vector/matrix type to
            // a property with a name like `.xy` based on the presence of
            // swizles, even though it seems like such a thing should Just Work.
            //
            // If this is important we could "fix" it by allowing this
            // code to dispatch to the special-case logic used when doing
            // semantic checking for member expressions.
            //
            // Note: an alternative would be to change the stdlib declarations
            // of vectors/matrices so that all the swizzles are defined as
            // `property` declarations. There are some C++ math libraries (like GLM)
            // that implement swizzle syntax by a similar approach of statically
            // enumerating all possible swizzles. The down-side to such an
            // approach is that the combinatorial space of swizzles is quite
            // large (especially for matrices) so that supporting them via
            // general-purpose language features is unlikely to be as efficient
            // as special-case logic.
            //
            auto synMemberRef = subVisitor.createLookupResultExpr(
                requiredMemberDeclRef.getName(),
                lookupResult,
                synThis,
                requiredMemberDeclRef.getLoc(),
                nullptr);
            synMemberRef->loc = requiredMemberDeclRef.getLoc();

            // The body of the accessor will depend on the class of the accessor
            // we are synthesizing (e.g., `get` vs. `set`).
            //
            Stmt* synBodyStmt = nullptr;
            if( as<GetterDecl>(requiredAccessorDeclRef) )
            {
                // A `get` accessor will simply perform:
                //
                //      return this.name;
                //
                // which involves coercing the member access `this.name` to
                // the expected type of the property.
                //
                auto coercedMemberRef = subVisitor.coerce(propertyType, synMemberRef);
                auto synReturn = m_astBuilder->create<ReturnStmt>();
                synReturn->expression = coercedMemberRef;

                synBodyStmt = synReturn;
            }
            else if( as<SetterDecl>(requiredAccessorDeclRef) )
            {
                // We expect all `set` accessors to have a single argument,
                // but we will defensively bail out if that is somehow
                // not the case.
                //
                SLANG_ASSERT(synArgs.getCount() == 1);
                if(synArgs.getCount() != 1)
                    return false;

                // A `set` accessor will simply perform:
                //
                //      this.name = newValue;
                //
                // which involves creating and checking an assignment
                // expression.

                auto synAssign = m_astBuilder->create<AssignExpr>();
                synAssign->left = synMemberRef;
                synAssign->right = synArgs[0];

                auto synCheckedAssign = subVisitor.checkAssignWithCheckedOperands(synAssign);

                auto synExprStmt = m_astBuilder->create<ExpressionStmt>();
                synExprStmt->expression = synCheckedAssign;

                synBodyStmt = synExprStmt;
            }
            else
            {
                // While there are other kinds of accessors than `get` and `set`,
                // those are currently only reserved for stdlib-internal use.
                // We will not bother with synthesis for those cases.
                //
                return false;
            }

            // We bail out if we ran into any errors (meaning that the synthesized
            // accessor is not usable).
            //
            // TODO: If there were *warnings* emitted to the sink, it would probably
            // be good to show those warnings to the user, since they might indicate
            // real issues. E.g., with the current logic a `float` field could
            // satisfying an `int` property requirement, but the user would probably
            // want to be warned when they do such a thing.
            //
            if(tempSink.getErrorCount() != 0)
                return false;

            synAccessorDecl->body = synBodyStmt;

            synAccessorDecl->parentDecl = synPropertyDecl;
            synPropertyDecl->members.add(synAccessorDecl);

            // If synthesis of an accessor worked, then we will record it into
            // a local dictionary. We do *not* install the accessor into the
            // witness table yet, because it is possible that synthesis will
            // succeed for some accessors but not others, and we don't want
            // to leave the witness table in a state where a requirement is
            // "partially satisfied."
            //
            mapRequiredAccessorToSynAccessor.Add(requiredAccessorDeclRef, synAccessorDecl);
        }

        synPropertyDecl->parentDecl = context->parentDecl;

        // Once our synthesized declaration is complete, we need
        // to install it as the witness that satifies the given
        // requirement.
        //
        // Subsequent code generation should not be able to tell the
        // difference between our synthetic property and a hand-written
        // one with the same behavior.
        //
        for(auto p : mapRequiredAccessorToSynAccessor)
        {
            witnessTable->add(p.Key, RequirementWitness(makeDeclRef(p.Value)));
        }
        witnessTable->add(requiredMemberDeclRef,
            RequirementWitness(makeDeclRef(synPropertyDecl)));
        return true;
    }


    bool SemanticsVisitor::trySynthesizeRequirementWitness(
        ConformanceCheckingContext* context,
        LookupResult const&         lookupResult,
        DeclRef<Decl>               requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable)
    {
        SLANG_UNUSED(lookupResult);
        SLANG_UNUSED(requiredMemberDeclRef);
        SLANG_UNUSED(witnessTable);

        if (auto requiredFuncDeclRef = requiredMemberDeclRef.as<FuncDecl>())
        {
            // Check signature match.
            return trySynthesizeMethodRequirementWitness(
                context,
                lookupResult,
                requiredFuncDeclRef,
                witnessTable);
        }

        if( auto requiredPropertyDeclRef = requiredMemberDeclRef.as<PropertyDecl>() )
        {
            return trySynthesizePropertyRequirementWitness(
                context,
                lookupResult,
                requiredPropertyDeclRef,
                witnessTable);
        }

        // TODO: There are other kinds of requirements for which synthesis should
        // be possible:
        //
        // * It should be possible to synthesize required initializers
        //   using an approach similar to what is used for methods.
        //
        // * We should be able to synthesize subscripts with different
        //   signatures (taking into account default parameters).
        //
        // * For specific kinds of generic requirements, we should be able
        //   to wrap the synthesis of the inner declaration in synthesis
        //   of an outer generic with a matching signature.
        //
        // All of these cases can/should use similar logic to
        // `trySynthesizeMethodRequirementWitness` where they construct an AST
        // in the form of what the use site ought to look like, and then
        // apply existing semantic checking logic to generate the code.

        return false;
    }

    bool SemanticsVisitor::findWitnessForInterfaceRequirement(
        ConformanceCheckingContext* context,
        Type*                       subType,
        Type*                       superInterfaceType,
        InheritanceDecl*            inheritanceDecl,
        DeclRef<InterfaceDecl>      superInterfaceDeclRef,
        DeclRef<Decl>               requiredMemberDeclRef,
        RefPtr<WitnessTable>        witnessTable,
        SubtypeWitness*             subTypeConformsToSuperInterfaceWitness)
    {
        SLANG_UNUSED(superInterfaceDeclRef)

        // The goal of this function is to find a suitable
        // value to satisfy the requirement.
        //
        // The 99% case is that the requirement is a named member
        // of the interface, and we need to search for a member
        // with the same name in the type declaration and
        // its (known) extensions.

        // As a first pass, lets check if we already have a
        // witness in the table for the requirement, so
        // that we can bail out early.
        //
        if(witnessTable->requirementDictionary.ContainsKey(requiredMemberDeclRef.getDecl()))
        {
            return true;
        }


        // An important exception to the above is that an
        // inheritance declaration in the interface is not going
        // to be satisfied by an inheritance declaration in the
        // conforming type, but rather by a full "witness table"
        // full of the satisfying values for each requirement
        // in the inherited-from interface.
        //
        if( auto requiredInheritanceDeclRef = requiredMemberDeclRef.as<InheritanceDecl>() )
        {
            // Recursively check that the type conforms
            // to the inherited interface.
            //
            // TODO: we *really* need a linearization step here!!!!

            auto reqType = getBaseType(m_astBuilder, requiredInheritanceDeclRef);

            DeclaredSubtypeWitness* interfaceIsReqWitness =
                m_astBuilder->getOrCreate<DeclaredSubtypeWitness>(
                    superInterfaceType,
                    reqType,
                    requiredInheritanceDeclRef.getDecl(),
                    requiredInheritanceDeclRef.substitutions.substitutions);
            // ...

            TransitiveSubtypeWitness* subIsReqWitness = m_astBuilder->getOrCreateWithDefaultCtor<TransitiveSubtypeWitness>(subType, reqType, interfaceIsReqWitness);
            subIsReqWitness->sub = subType;
            subIsReqWitness->sup = reqType;
            subIsReqWitness->subToMid = subTypeConformsToSuperInterfaceWitness;
            subIsReqWitness->midToSup = interfaceIsReqWitness;
            // ...

            RefPtr<WitnessTable> satisfyingWitnessTable = new WitnessTable();
            satisfyingWitnessTable->witnessedType = subType;
            satisfyingWitnessTable->baseType = reqType;

            witnessTable->add(
                requiredInheritanceDeclRef.getDecl(),
                RequirementWitness(satisfyingWitnessTable));

            if( !checkConformanceToType(
                context,
                subType,
                requiredInheritanceDeclRef.getDecl(),
                reqType,
                subIsReqWitness,
                satisfyingWitnessTable) )
            {
                return false;
            }

            return true;
        }

        // We will look up members with the same name,
        // since only same-name members will be able to
        // satisfy the requirement.
        //
        Name* name = requiredMemberDeclRef.getName();

        // We start by looking up members of the same
        // name, on the type that is claiming to conform.
        //
        // This lookup step could include members that
        // we might not actually want to consider:
        //
        // * Lookup through a type `Foo` where `Foo : IBar`
        //   will be able to find members of `IBar`, which
        //   somewhat obviously shouldn't apply when
        //   determining if `Foo` satisfies the requirements
        //   of `IBar`.
        //
        // * Lookup in the presence of `__transparent` members
        //   may produce references to declarations on a *field*
        //   of the type rather than the type. Conformance through
        //   transparent members could be supported in theory,
        //   but would require synthesizing proxy/forwarding
        //   implementations in the type itself.
        //
        // For the first issue, we will use a flag to influence
        // lookup so that it doesn't include results looked up
        // through interface inheritance clauses (but it *will*
        // look up result through inheritance clauses corresponding
        // to concrete types).
        //
        // The second issue of members that require us to proxy/forward
        // requests will be handled further down. For now we include
        // lookup results that might be usable, but not as-is.
        //
        auto lookupResult = lookUpMember(m_astBuilder, this, name, subType, LookupMask::Default, LookupOptions::IgnoreBaseInterfaces);

        if(!lookupResult.isValid())
        {
            // If we failed to even look up a member with the name of the
            // requirement, then we can be certain that the type doesn't
            // satisfy the requirement.
            //
            // TODO: If we ever allowed certain kinds of requirements to
            // be inferred (e.g., inferring associated types from the
            // signatures of methods, as is done for Swift), we'd
            // need to revisit this step.
            //
            getSink()->diagnose(inheritanceDecl, Diagnostics::typeDoesntImplementInterfaceRequirement, subType, requiredMemberDeclRef);
            getSink()->diagnose(requiredMemberDeclRef, Diagnostics::seeDeclarationOf, requiredMemberDeclRef);
            return false;
        }

        // Iterate over the members and look for one that matches
        // the expected signature for the requirement.
        for (auto member : lookupResult)
        {
            // To a first approximation, any lookup result that required a "breadcrumb"
            // will not be usable to directly satisfy an interface requirement, since
            // each breadcrumb will amount to a manipulation of `this` that is required
            // to make the declaration usable (e.g., casting to a base type).
            //
            if(member.breadcrumbs != nullptr)
                continue;

            if (doesMemberSatisfyRequirement(member.declRef, requiredMemberDeclRef, witnessTable))
                return true;
        }

        // If we reach this point then there were no members suitable
        // for satisfying the interface requirement *diretly*.
        //
        // It is possible that one of the items in `lookupResult` could be
        // used to synthesize an exact-match witness, by generating the
        // code required to handle all the conversions that might be
        // required on `this`.
        //
        if( trySynthesizeRequirementWitness(context, lookupResult, requiredMemberDeclRef, witnessTable) )
        {
            return true;
        }

        // We failed to find a member of the type that can be used
        // to satisfy the requirement (even via synthesis), so we
        // need to report the failure to the user.
        //
        // TODO: Eventually we might want something akin to the current
        // overload resolution logic, where we keep track of a list
        // of "candidates" for satisfaction of the requirement,
        // and if nothing is found we print the candidates that made it
        // furthest in checking.
        //
        getSink()->diagnose(inheritanceDecl, Diagnostics::typeDoesntImplementInterfaceRequirement, subType, requiredMemberDeclRef);
        getSink()->diagnose(requiredMemberDeclRef, Diagnostics::seeDeclarationOf, requiredMemberDeclRef);
        return false;
    }

    RefPtr<WitnessTable> SemanticsVisitor::checkInterfaceConformance(
        ConformanceCheckingContext* context,
        Type*                       subType,
        Type*                       superInterfaceType,
        InheritanceDecl*            inheritanceDecl,
        DeclRef<InterfaceDecl>      superInterfaceDeclRef,
        SubtypeWitness*             subTypeConformsToSuperInterfaceWitnes)
    {
        // Has somebody already checked this conformance,
        // and/or is in the middle of checking it?
        RefPtr<WitnessTable> witnessTable;
        if(context->mapInterfaceToWitnessTable.TryGetValue(superInterfaceDeclRef, witnessTable))
            return witnessTable;

        // We need to check the declaration of the interface
        // before we can check that we conform to it.
        //
        ensureDecl(superInterfaceDeclRef, DeclCheckState::CanReadInterfaceRequirements);

        // We will construct the witness table, and register it
        // *before* we go about checking fine-grained requirements,
        // in order to short-circuit any potential for infinite recursion.

        // Note: we will re-use the witnes table attached to the inheritance decl,
        // if there is one. This catches cases where semantic checking might
        // have synthesized some of the conformance witnesses for us.
        //
        witnessTable = inheritanceDecl->witnessTable;
        if(!witnessTable)
        {
            witnessTable = new WitnessTable();
            witnessTable->baseType = DeclRefType::create(m_astBuilder, superInterfaceDeclRef);
            witnessTable->witnessedType = subType;
        }
        context->mapInterfaceToWitnessTable.Add(superInterfaceDeclRef, witnessTable);

        if(!checkInterfaceConformance(context, subType, superInterfaceType, inheritanceDecl, superInterfaceDeclRef, subTypeConformsToSuperInterfaceWitnes, witnessTable))
            return nullptr;

        return witnessTable;
    }

    static bool isAssociatedTypeDecl(Decl* decl)
    {
        auto d = decl;
        while(auto genericDecl = as<GenericDecl>(d))
            d = genericDecl->inner;
        if(as<AssocTypeDecl>(d))
            return true;
        return false;
    }

    bool SemanticsVisitor::checkInterfaceConformance(
        ConformanceCheckingContext* context,
        Type*                       subType,
        Type*                       superInterfaceType,
        InheritanceDecl*            inheritanceDecl,
        DeclRef<InterfaceDecl>      superInterfaceDeclRef,
        SubtypeWitness*             subTypeConformsToSuperInterfaceWitness,
        WitnessTable*               witnessTable)
    {
        // We need to check the declaration of the interface
        // before we can check that we conform to it.
        //
        ensureDecl(superInterfaceDeclRef, DeclCheckState::CanReadInterfaceRequirements);

        // When comparing things like signatures, we need to do so in the context
        // of a this-type substitution that aligns the signatures in the interface
        // with those in the concrete type. For example, we need to treat any uses
        // of `This` in the interface as equivalent to the concrete type for the
        // purpose of signature matching (and similarly for associated types).
        //
        ThisTypeSubstitution* thisTypeSubst = m_astBuilder->create<ThisTypeSubstitution>();
        thisTypeSubst->interfaceDecl = superInterfaceDeclRef.getDecl();
        thisTypeSubst->witness = subTypeConformsToSuperInterfaceWitness;
        thisTypeSubst->outer = superInterfaceDeclRef.substitutions.substitutions;

        auto specializedSuperInterfaceDeclRef = DeclRef<InterfaceDecl>(superInterfaceDeclRef.getDecl(), thisTypeSubst);

        bool result = true;

        // TODO: If we ever allow for implementation inheritance,
        // then we will need to consider the case where a type
        // declares that it conforms to an interface, but one of
        // its (non-interface) base types already conforms to
        // that interface, so that all of the requirements are
        // already satisfied with inherited implementations...

        // Note: we break this logic into two loops, where we first
        // check conformance for all associated-type requirements
        // and *then* check conformance for all other requirements.
        //
        // Checking associated-type requirements first ensures that
        // we can make use of the identity of the associated types
        // when checking other members.
        //
        // TODO: There could in theory be subtle cases involving
        // circular or recursive dependency chains that make such
        // a simple ordering impractical (e.g., associated type `A`
        // is constrained to `IThing<This>` where `IThing<T>` requires
        // that `T : IOtherThing where T.B == int` for another associated
        // type `B`).
        //
        // The only robust solution long-term is probably to treat this
        // as a type-inference problem by creating type variables to
        // stand in for the associated-type requirements and then to discover
        // constraints and solve for those type variables as part of the
        // conformance-checking process.
        //
        for(auto requiredMemberDeclRef : getMembers(specializedSuperInterfaceDeclRef))
        {
            if(!isAssociatedTypeDecl(requiredMemberDeclRef))
                continue;

            auto requirementSatisfied = findWitnessForInterfaceRequirement(
                context,
                subType,
                superInterfaceType,
                inheritanceDecl,
                specializedSuperInterfaceDeclRef,
                requiredMemberDeclRef,
                witnessTable,
                subTypeConformsToSuperInterfaceWitness);

            result = result && requirementSatisfied;
        }
        for(auto requiredMemberDeclRef : getMembers(specializedSuperInterfaceDeclRef))
        {
            if(isAssociatedTypeDecl(requiredMemberDeclRef))
                continue;

            auto requirementSatisfied = findWitnessForInterfaceRequirement(
                context,
                subType,
                superInterfaceType,
                inheritanceDecl,
                specializedSuperInterfaceDeclRef,
                requiredMemberDeclRef,
                witnessTable,
                subTypeConformsToSuperInterfaceWitness);

            result = result && requirementSatisfied;
        }

        // Extensions that apply to the interface type can create new conformances
        // for the concrete types that inherit from the interface.
        //
        // These new conformances should not be able to introduce new *requirements*
        // for an implementing interface (although they currently can), but we
        // still need to go through this logic to find the appropriate value
        // that will satisfy the requirement in these cases, and also to put
        // the required entry into the witness table for the interface itself.
        //
        // TODO: This logic is a bit slippery, and we need to figure out what
        // it means in the context of separate compilation. If module A defines
        // an interface IA, module B defines a type C that conforms to IA, and then
        // module C defines an extension that makes IA conform to IC, then it is
        // unreasonable to expect the {B:IA} witness table to contain an entry
        // corresponding to {IA:IC}.
        //
        // The simple answer then would be that the {IA:IC} conformance should be
        // fixed, with a single witness table for {IA:IC}, but then what should
        // happen in B explicitly conformed to IC already?
        //
        // For now we will just walk through the extensions that are known at
        // the time we are compiling and handle those, and punt on the larger issue
        // for a bit longer.
        //
        for(auto candidateExt : getCandidateExtensions(specializedSuperInterfaceDeclRef, this))
        {
            // We need to apply the extension to the interface type that our
            // concrete type is inheriting from.
            //
            Type* targetType = DeclRefType::create(m_astBuilder, specializedSuperInterfaceDeclRef);
            auto extDeclRef = ApplyExtensionToType(candidateExt, targetType);
            if(!extDeclRef)
                continue;

            // Only inheritance clauses from the extension matter right now.
            for(auto requiredInheritanceDeclRef : getMembersOfType<InheritanceDecl>(extDeclRef))
            {
                auto requirementSatisfied = findWitnessForInterfaceRequirement(
                    context,
                    subType,
                    superInterfaceType,
                    inheritanceDecl,
                    specializedSuperInterfaceDeclRef,
                    requiredInheritanceDeclRef,
                    witnessTable,
                    subTypeConformsToSuperInterfaceWitness);

                result = result && requirementSatisfied;
            }
        }

        // The conformance was satisfied if all the requirements were satisfied.
        //
        return result;
    }

    bool SemanticsVisitor::checkConformanceToType(
        ConformanceCheckingContext* context,
        Type*                       subType,
        InheritanceDecl*            inheritanceDecl,
        Type*                       superType,
        SubtypeWitness*             subIsSuperWitness,
        WitnessTable*               witnessTable)
    {
        if (auto supereclRefType = as<DeclRefType>(superType))
        {
            auto superTypeDeclRef = supereclRefType->declRef;
            if (auto superInterfaceDeclRef = superTypeDeclRef.as<InterfaceDecl>())
            {
                // The type is stating that it conforms to an interface.
                // We need to check that it provides all of the members
                // required by that interface.
                return checkInterfaceConformance(
                    context,
                    subType,
                    superType,
                    inheritanceDecl,
                    superInterfaceDeclRef,
                    subIsSuperWitness,
                    witnessTable);
            }
            else if( auto superStructDeclRef = superTypeDeclRef.as<StructDecl>() )
            {
                // The type is saying it inherits from a `struct`,
                // which doesn't require any checking at present
                return true;
            }
        }
        if (!as<ErrorType>(superType))
        {
            getSink()->diagnose(
                inheritanceDecl,
                Diagnostics::invalidTypeForInheritance,
                superType);
        }
        return false;
    }

    bool SemanticsVisitor::checkConformance(
        Type*                       subType,
        InheritanceDecl*            inheritanceDecl,
        ContainerDecl*              parentDecl)
    {
        auto superType = inheritanceDecl->base.type;

        if( auto declRefType = as<DeclRefType>(subType) )
        {
            auto declRef = declRefType->declRef;

            if (auto superDeclRefType = as<DeclRefType>(superType))
            {
                auto superTypeDecl = superDeclRefType->declRef.getDecl();
                if (superTypeDecl->findModifier<ComInterfaceAttribute>())
                {
                    // A struct cannot implement a COM Interface.
                    if (auto classDecl = as<ClassDecl>(superTypeDecl))
                    {
                        // OK.
                        SLANG_UNUSED(classDecl);
                    }
                    else if (auto subInterfaceDecl = as<InterfaceDecl>(superTypeDecl))
                    {
                        if (!subInterfaceDecl->findModifier<ComInterfaceAttribute>())
                        {
                            getSink()->diagnose(inheritanceDecl, Diagnostics::interfaceInheritingComMustBeCom);
                        }
                    }
                    else if (auto structDecl = as<StructDecl>(superTypeDecl))
                    {
                        getSink()->diagnose(inheritanceDecl, Diagnostics::structCannotImplementComInterface);
                    }
                }
            }

            // Don't check conformances for abstract types that
            // are being used to express *required* conformances.
            if (auto assocTypeDeclRef = declRef.as<AssocTypeDecl>())
            {
                // An associated type declaration represents a requirement
                // in an outer interface declaration, and its members
                // (type constraints) represent additional requirements.
                return true;
            }
            else if (auto interfaceDeclRef = declRef.as<InterfaceDecl>())
            {
                // HACK: Our semantics as they stand today are that an
                // `extension` of an interface that adds a new inheritance
                // clause acts *as if* that inheritnace clause had been
                // attached to the original `interface` decl: that is,
                // it adds additional requirements.
                //
                // This is *not* a reasonable semantic to keep long-term,
                // but it is required for some of our current example
                // code to work.
                return true;
            }

            
        }

        // Look at the type being inherited from, and validate
        // appropriately.

        DeclaredSubtypeWitness* subIsSuperWitness = m_astBuilder->create<DeclaredSubtypeWitness>();
        subIsSuperWitness->declRef = makeDeclRef(inheritanceDecl);
        subIsSuperWitness->sub = subType;
        subIsSuperWitness->sup = superType;

        ConformanceCheckingContext context;
        context.conformingType = subType;
        context.parentDecl = parentDecl;


        RefPtr<WitnessTable> witnessTable = inheritanceDecl->witnessTable;
        if(!witnessTable)
        {
            witnessTable = new WitnessTable();
            witnessTable->baseType = superType;
            witnessTable->witnessedType = subType;
            inheritanceDecl->witnessTable = witnessTable;
        }

        if( !checkConformanceToType(&context, subType, inheritanceDecl, superType, subIsSuperWitness, witnessTable) )
        {
            return false;
        }

        return true;
    }

    void SemanticsVisitor::checkExtensionConformance(ExtensionDecl* decl)
    {
        auto declRef = createDefaultSubstitutionsIfNeeded(m_astBuilder, this, makeDeclRef(decl)).as<ExtensionDecl>();
        auto targetType = getTargetType(m_astBuilder, declRef);

        for (auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
        {
            checkConformance(targetType, inheritanceDecl, decl);
        }
    }

    void SemanticsVisitor::checkAggTypeConformance(AggTypeDecl* decl)
    {
        // After we've checked members, we need to go through
        // any inheritance clauses on the type itself, and
        // confirm that the type actually provides whatever
        // those clauses require.

        if (auto interfaceDecl = as<InterfaceDecl>(decl))
        {
            // Don't check that an interface conforms to the
            // things it inherits from.
        }
        else if (auto assocTypeDecl = as<AssocTypeDecl>(decl))
        {
            // Don't check that an associated type decl conforms to the
            // things it inherits from.
        }
        else
        {
            // For non-interface types we need to check conformance.
            //

            auto astBuilder = getASTBuilder();

            auto declRef = createDefaultSubstitutionsIfNeeded(astBuilder, this, makeDeclRef(decl)).as<AggTypeDeclBase>();
            auto type = DeclRefType::create(astBuilder, declRef);

            // TODO: Need to figure out what this should do for
            // `abstract` types if we ever add them. Should they
            // be required to implement all interface requirements,
            // just with `abstract` methods that replicate things?
            // (That's what C# does).
            for (auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
            {
                checkConformance(type, inheritanceDecl, decl);
            }
        }
    }

    void SemanticsDeclBasesVisitor::_validateCrossModuleInheritance(
        AggTypeDeclBase* decl,
        InheritanceDecl* inheritanceDecl)
    {
        // Within a single module, users should be allowed to inherit
        // one type from another more or less freely, so long as they
        // don't violate fundamental validity conditions around
        // inheritance.
        //
        // When an inheritance relationship is declared in one module,
        // and the base type is in another module, we may want to
        // enforce more restrictions. As a strong example, we probably
        // don't want people to declare their own subtype of `int`
        // or `Texture2D<float4>`.
        //
        // We start by checking if the type being inherited from is
        // a decl-ref type, since that means it refers to a declaration
        // that can be localized to its original module.
        //
        auto baseType = inheritanceDecl->base.type;
        auto baseDeclRefType = as<DeclRefType>(baseType);
        if( !baseDeclRefType )
        {
            return;
        }
        auto baseDecl = baseDeclRefType->declRef.decl;

        // Using the parent/child hierarchy baked into `Decl`s we
        // can find the modules that contain both the `decl` doing
        // the inheriting, and the `baseDeclRefType` that is being
        // inherited from.
        //
        // If those modules are the same, then we aren't seeing any
        // kind of cross-module inheritance here, and there is nothing
        // that needs enforcing.
        //
        auto moduleWithInheritance = getModule(decl);
        auto moduleWithBaseType = getModule(baseDecl);
        if( moduleWithInheritance == moduleWithBaseType )
        {
            return;
        }

        if( baseDecl->hasModifier<SealedAttribute>() )
        {
            // If the original declaration had the `[sealed]` attribute on it,
            // then it explicitly does *not* allow inheritance from other
            // modules.
            //
            getSink()->diagnose(inheritanceDecl, Diagnostics::cannotInheritFromExplicitlySealedDeclarationInAnotherModule, baseType, moduleWithBaseType->getModuleDecl()->getName());
            return;
        }
        else if( baseDecl->hasModifier<OpenAttribute>() )
        {
            // Conversely, if the original declaration had the `[open]` attribute
            // on it, then it explicit *does* allow inheritance from other
            // modules.
            //
            // In this case we don't need to check anything: the inheritance
            // is allowed.
        }
        else if( as<InterfaceDecl>(baseDecl) )
        {
            // If an interface isn't explicitly marked `[open]` or `[sealed]`,
            // then the default behavior is to treat it as `[open]`, since
            // interfaces are most often used to define protocols that
            // users of a module can opt into.
        }
        else
        {
            // For any non-interface type, if the declaration didn't specify
            // `[open]` or `[sealed]` then we assume `[sealed]` is the default.
            //
            getSink()->diagnose(inheritanceDecl, Diagnostics::cannotInheritFromImplicitlySealedDeclarationInAnotherModule, baseType, moduleWithBaseType->getModuleDecl()->getName());
            return;
        }
    }

    void SemanticsDeclBasesVisitor::visitInterfaceDecl(InterfaceDecl* decl)
    {
        for( auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>() )
        {
            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
            auto baseType = inheritanceDecl->base.type;

            // It is possible that there was an error in checking the base type
            // expression, and in such a case we shouldn't emit a cascading error.
            //
            if( auto baseErrorType = as<ErrorType>(baseType) )
            {
                continue;
            }

            // An `interface` type can only inherit from other `interface` types.
            //
            // TODO: In the long run it might make sense for an interface to support
            // an inheritance clause naming a non-interface type, with the meaning
            // that any type that implements the interface must be a sub-type of the
            // type named in the inheritance clause.
            //
            auto baseDeclRefType = as<DeclRefType>(baseType);
            if( !baseDeclRefType )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfInterfaceMustBeInterface, decl, baseType);
                continue;
            }

            auto baseDeclRef = baseDeclRefType->declRef;
            auto baseInterfaceDeclRef = baseDeclRef.as<InterfaceDecl>();
            if( !baseInterfaceDeclRef )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfInterfaceMustBeInterface, decl, baseType);
                continue;
            }

            // TODO: At this point we have the `baseInterfaceDeclRef`
            // and could use it to perform further validity checks,
            // and/or to build up a more refined representation of
            // the inheritance graph for this type (e.g., a "class
            // precedence list").
            //
            // E.g., we can/should check that we aren't introducing
            // a circular inheritance relationship.

            _validateCrossModuleInheritance(decl, inheritanceDecl);
        }

        if (decl->findModifier<ComInterfaceAttribute>())
        {
            // `associatedtype` declaration is not allowed in a COM interface declaration.
            for (auto associatedType : decl->getMembersOfType<AssocTypeDecl>())
            {
                getSink()->diagnose(
                    associatedType, Diagnostics::associatedTypeNotAllowInComInterface);
            }
        }
    }

    void SemanticsDeclBasesVisitor::visitStructDecl(StructDecl* decl)
    {
        // A `struct` type can only inherit from `struct` or `interface` types.
        //
        // Furthermore, only the first inheritance clause (in source
        // order) is allowed to declare a base `struct` type.
        //
        Index inheritanceClauseCounter = 0;
        for( auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>() )
        {
            Index inheritanceClauseIndex = inheritanceClauseCounter++;

            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
            auto baseType = inheritanceDecl->base.type;

            // It is possible that there was an error in checking the base type
            // expression, and in such a case we shouldn't emit a cascading error.
            //
            if( auto baseErrorType = as<ErrorType>(baseType) )
            {
                continue;
            }

            auto baseDeclRefType = as<DeclRefType>(baseType);
            if( !baseDeclRefType )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfStructMustBeStructOrInterface, decl, baseType);
                continue;
            }

            auto baseDeclRef = baseDeclRefType->declRef;
            if( auto baseInterfaceDeclRef = baseDeclRef.as<InterfaceDecl>() )
            {
            }
            else if( auto baseStructDeclRef = baseDeclRef.as<StructDecl>() )
            {
                // To simplify the task of reading and maintaining code,
                // we require that when a `struct` inherits from another
                // `struct`, the base `struct` is the first item in
                // the list of bases (before any interfaces).
                //
                // This constraint also has the secondary effect of restricting
                // it so that a `struct` cannot multiply inherit from other
                // `struct` types.
                //
                if( inheritanceClauseIndex != 0 )
                {
                    getSink()->diagnose(inheritanceDecl, Diagnostics::baseStructMustBeListedFirst, decl, baseType);
                }
            }
            else
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfStructMustBeStructOrInterface, decl, baseType);
                continue;
            }

            // TODO: At this point we have the `baseDeclRef`
            // and could use it to perform further validity checks,
            // and/or to build up a more refined representation of
            // the inheritance graph for this type (e.g., a "class
            // precedence list").
            //
            // E.g., we can/should check that we aren't introducing
            // a circular inheritance relationship.

            _validateCrossModuleInheritance(decl, inheritanceDecl);
        }
    }

    void SemanticsDeclBasesVisitor::visitClassDecl(ClassDecl* decl)
    {
        // A `class` type can only inherit from `class` or `interface` types.
        //
        // Furthermore, only the first inheritance clause (in source
        // order) is allowed to declare a base `class` type.
        //
        Index inheritanceClauseCounter = 0;
        for (auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
        {
            Index inheritanceClauseIndex = inheritanceClauseCounter++;

            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
            auto baseType = inheritanceDecl->base.type;

            // It is possible that there was an error in checking the base type
            // expression, and in such a case we shouldn't emit a cascading error.
            //
            if (auto baseErrorType = as<ErrorType>(baseType))
            {
                continue;
            }

            auto baseDeclRefType = as<DeclRefType>(baseType);
            if (!baseDeclRefType)
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfClassMustBeClassOrInterface, decl, baseType);
                continue;
            }

            auto baseDeclRef = baseDeclRefType->declRef;
            if (auto baseInterfaceDeclRef = baseDeclRef.as<InterfaceDecl>())
            {
            }
            else if (auto baseStructDeclRef = baseDeclRef.as<ClassDecl>())
            {
                // To simplify the task of reading and maintaining code,
                // we require that when a `class` inherits from another
                // `class`, the base `class` is the first item in
                // the list of bases (before any interfaces).
                //
                // This constraint also has the secondary effect of restricting
                // it so that a `struct` cannot multiply inherit from other
                // `struct` types.
                //
                if (inheritanceClauseIndex != 0)
                {
                    getSink()->diagnose(inheritanceDecl, Diagnostics::baseClassMustBeListedFirst, decl, baseType);
                }
            }
            else
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfClassMustBeClassOrInterface, decl, baseType);
                continue;
            }

            // TODO: At this point we have the `baseDeclRef`
            // and could use it to perform further validity checks,
            // and/or to build up a more refined representation of
            // the inheritance graph for this type (e.g., a "class
            // precedence list").
            //
            // E.g., we can/should check that we aren't introducing
            // a circular inheritance relationship.

            _validateCrossModuleInheritance(decl, inheritanceDecl);
        }
    }

    bool SemanticsVisitor::isIntegerBaseType(BaseType baseType)
    {
        return (BaseTypeInfo::getInfo(baseType).flags & BaseTypeInfo::Flag::Integer) != 0;
    }

    bool SemanticsVisitor::isScalarIntegerType(Type* type)
    {
        auto basicType = as<BasicExpressionType>(type);
        if(!basicType)
            return false;

        return isIntegerBaseType(basicType->baseType) || basicType->baseType == BaseType::Bool;
    }

    bool SemanticsVisitor::isIntValueInRangeOfType(IntegerLiteralValue value, Type* type)
    {
        auto basicType = as<BasicExpressionType>(type);
        if (!basicType)
            return false;

        switch (basicType->baseType)
        {
        case BaseType::UInt8:
            return (value >= 0 && value <= std::numeric_limits<uint8_t>::max()) || (value == -1);
        case BaseType::UInt16:
            return (value >= 0 && value <= std::numeric_limits<uint16_t>::max()) || (value == -1);
        case BaseType::UInt:
            return (value >= 0 && value <= std::numeric_limits<uint32_t>::max()) || (value == -1);
        case BaseType::UInt64:
            return true;
        case BaseType::Int8:
            return value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max();
        case BaseType::Int16:
            return value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max();
        case BaseType::Int:
            return value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max();
        case BaseType::Int64:
            return value >= std::numeric_limits<int64_t>::min() && value <= std::numeric_limits<int64_t>::max();
        default:
            return false;
        }
    }

    void SemanticsVisitor::validateEnumTagType(Type* type, SourceLoc const& loc)
    {
        // Allow the built-in integer types.
        //
        if(isScalarIntegerType(type))
            return;

        // By default, don't allow other types to be used
        // as an `enum` tag type.
        //
        getSink()->diagnose(loc, Diagnostics::invalidEnumTagType, type);
    }

    void SemanticsDeclBasesVisitor::visitEnumDecl(EnumDecl* decl)
    {
        // An `enum` type can inherit from interfaces, and also
        // from a single "tag" type that must:
        //
        // * be a built-in integer type
        // * come first in the list of base types
        //
        Index inheritanceClauseCounter = 0;

        Type* tagType = nullptr;
        InheritanceDecl* tagTypeInheritanceDecl = nullptr;
        for(auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>())
        {
            Index inheritanceClauseIndex = inheritanceClauseCounter++;

            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
            auto baseType = inheritanceDecl->base.type;

            // It is possible that there was an error in checking the base type
            // expression, and in such a case we shouldn't emit a cascading error.
            //
            if( auto baseErrorType = as<ErrorType>(baseType) )
            {
                continue;
            }

            auto baseDeclRefType = as<DeclRefType>(baseType);
            if( !baseDeclRefType )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfEnumMustBeIntegerOrInterface, decl, baseType);
                continue;
            }

            auto baseDeclRef = baseDeclRefType->declRef;
            if( auto baseInterfaceDeclRef = baseDeclRef.as<InterfaceDecl>() )
            {
                _validateCrossModuleInheritance(decl, inheritanceDecl);
            }
            else if( auto baseStructDeclRef = baseDeclRef.as<StructDecl>() )
            {
                // To simplify the task of reading and maintaining code,
                // we require that when an `enum` declares an explicit
                // underlying tag type using an inheritance clause, that
                // type must be the first item in the list of bases.
                //
                // This constraint also has the secondary effect of restricting
                // it so that an `enum` can't possibly have multiple tag
                // types declared.
                //
                if( inheritanceClauseIndex != 0 )
                {
                    getSink()->diagnose(inheritanceDecl, Diagnostics::tagTypeMustBeListedFirst, decl, baseType);
                }
                else
                {
                    tagType = baseType;
                    tagTypeInheritanceDecl = inheritanceDecl;
                }

                // Note: we do *not* apply the code that validates
                // cross-module inheritance to a base that represnts
                // a tag type, because declaring a tag type for an
                // `enum` doesn't actually make it into a subtype
                // of the tag type, and thus doesn't violate the
                // rules when the tag type is `sealed`.
            }
            else
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfEnumMustBeIntegerOrInterface, decl, baseType);
                continue;
            }
        }

        // If a tag type has not been set, then we
        // default it to the built-in `int` type.
        //
        // TODO: In the far-flung future we may want to distinguish
        // `enum` types that have a "raw representation" like this from
        // ones that are purely abstract and don't expose their
        // type of their tag.
        //
        if(!tagType)
        {
            tagType = m_astBuilder->getIntType();
        }
        else
        {
            // TODO: Need to establish that the tag
            // type is suitable. (e.g., if we are going
            // to allow raw values for case tags to be
            // derived automatically, then the tag
            // type needs to be some kind of integer type...)
            //
            // For now we will just be harsh and require it
            // to be one of a few builtin types.
            validateEnumTagType(tagType, tagTypeInheritanceDecl->loc);

            // Note: The `InheritanceDecl` that introduces a tag
            // type isn't actually representing a super-type of
            // the `enum`, and things like name lookup need to
            // know to ignore that "inheritance" relationship.
            //
            // We add a modifier to the `InheritanceDecl` to ensure
            // that it can be detected and ignored by such steps.
            //
            addModifier(tagTypeInheritanceDecl, m_astBuilder->create<IgnoreForLookupModifier>());
        }
        decl->tagType = tagType;


        // An `enum` type should automatically conform to the `__EnumType` interface.
        // The compiler needs to insert this conformance behind the scenes, and this
        // seems like the best place to do it.
        {
            // First, look up the type of the `__EnumType` interface.
            Type* enumTypeType = getASTBuilder()->getEnumTypeType();

            InheritanceDecl* enumConformanceDecl = m_astBuilder->create<InheritanceDecl>();
            enumConformanceDecl->parentDecl = decl;
            enumConformanceDecl->loc = decl->loc;
            enumConformanceDecl->base.type = getASTBuilder()->getEnumTypeType();
            decl->members.add(enumConformanceDecl);

            // The `__EnumType` interface has one required member, the `__Tag` type.
            // We need to satisfy this requirement automatically, rather than require
            // the user to actually declare a member with this name (otherwise we wouldn't
            // let them define a tag value with the name `__Tag`).
            //
            RefPtr<WitnessTable> witnessTable = new WitnessTable();
            witnessTable->baseType = enumConformanceDecl->base.type;
            witnessTable->witnessedType = enumTypeType;
            enumConformanceDecl->witnessTable = witnessTable;

            Name* tagAssociatedTypeName = getSession()->getNameObj("__Tag");
            Decl* tagAssociatedTypeDecl = nullptr;
            if(auto enumTypeTypeDeclRefType = dynamicCast<DeclRefType>(enumTypeType))
            {
                if(auto enumTypeTypeInterfaceDecl = as<InterfaceDecl>(enumTypeTypeDeclRefType->declRef.getDecl()))
                {
                    for(auto memberDecl : enumTypeTypeInterfaceDecl->members)
                    {
                        if(memberDecl->getName() == tagAssociatedTypeName)
                        {
                            tagAssociatedTypeDecl = memberDecl;
                            break;
                        }
                    }
                }
            }
            if(!tagAssociatedTypeDecl)
            {
                SLANG_DIAGNOSE_UNEXPECTED(getSink(), decl, "failed to find built-in declaration '__Tag'");
            }

            // Okay, add the conformance witness for `__Tag` being satisfied by `tagType`
            witnessTable->add(tagAssociatedTypeDecl, RequirementWitness(tagType));

            // TODO: we actually also need to synthesize a witness for the conformance of `tagType`
            // to the `__BuiltinIntegerType` interface, because that is a constraint on the
            // associated type `__Tag`.

            // TODO: eventually we should consider synthesizing other requirements for
            // the min/max tag values, or the total number of tags, so that people don't
            // have to declare these as additional cases.

            enumConformanceDecl->setCheckState(DeclCheckState::Checked);
        }
    }

    void SemanticsDeclBodyVisitor::visitEnumDecl(EnumDecl* decl)
    {
        auto enumType = DeclRefType::create(m_astBuilder, makeDeclRef(decl));

        auto tagType = decl->tagType;

        // Check the enum cases in order.
        for(auto caseDecl : decl->getMembersOfType<EnumCaseDecl>())
        {
            // Each case defines a value of the enum's type.
            //
            // TODO: If we ever support enum cases with payloads,
            // then they would probably have a type that is a
            // `FunctionType` from the payload types to the
            // enum type.
            //
            // TODO(tfoley): the case should grab its type  when
            // doing its own header checking, rather than rely on this...
            caseDecl->type.type = enumType;

            ensureDecl(caseDecl, DeclCheckState::Checked);
        }

        // For any enum case that didn't provide an explicit
        // tag value, derived an appropriate tag value.
        IntegerLiteralValue defaultTag = 0;
        for(auto caseDecl : decl->getMembersOfType<EnumCaseDecl>())
        {
            if(auto explicitTagValExpr = caseDecl->tagExpr)
            {
                // This tag has an initializer, so it should establish
                // the tag value for a successor case that doesn't
                // provide an explicit tag.

                IntVal* explicitTagVal = tryConstantFoldExpr(explicitTagValExpr, nullptr);
                if(explicitTagVal)
                {
                    if(auto constIntVal = as<ConstantIntVal>(explicitTagVal))
                    {
                        defaultTag = constIntVal->value;
                    }
                    else
                    {
                        // TODO: need to handle other possibilities here
                        getSink()->diagnose(explicitTagValExpr, Diagnostics::unexpectedEnumTagExpr);
                    }
                }
                else
                {
                    // If this happens, then the explicit tag value expression
                    // doesn't seem to be a constant after all. In this case
                    // we expect the checking logic to have applied already.
                }
            }
            else
            {
                // This tag has no initializer, so it should use
                // the default tag value we are tracking.
                IntegerLiteralExpr* tagValExpr = m_astBuilder->create<IntegerLiteralExpr>();
                tagValExpr->loc = caseDecl->loc;
                tagValExpr->type = QualType(tagType);
                tagValExpr->value = defaultTag;

                caseDecl->tagExpr = tagValExpr;
            }

            // Default tag for the next case will be one more than
            // for the most recent case.
            //
            // TODO: We might consider adding a `[flags]` attribute
            // that modifies this behavior to be `defaultTagForCase <<= 1`.
            //
            defaultTag++;
        }
    }

    void SemanticsDeclBodyVisitor::visitEnumCaseDecl(EnumCaseDecl* decl)
    {
        // An enum case had better appear inside an enum!
        //
        // TODO: Do we need/want to support generic cases some day?
        auto parentEnumDecl = as<EnumDecl>(decl->parentDecl);
        SLANG_ASSERT(parentEnumDecl);

        // The tag type should have already been set by
        // the surrounding `enum` declaration.
        auto tagType = parentEnumDecl->tagType;
        SLANG_ASSERT(tagType);

        // Need to check the init expression, if present, since
        // that represents the explicit tag for this case.
        if(auto initExpr = decl->tagExpr)
        {
            initExpr = CheckTerm(initExpr);
            initExpr = coerce(tagType, initExpr);

            // We want to enforce that this is an integer constant
            // expression, but we don't actually care to retain
            // the value.
            CheckIntegerConstantExpression(initExpr, IntegerConstantExpressionCoercionType::AnyInteger, nullptr);

            decl->tagExpr = initExpr;
        }
    }

    void SemanticsVisitor::ensureDeclBase(DeclBase* declBase, DeclCheckState state, SemanticsContext* baseContext)
    {
        if(auto decl = as<Decl>(declBase))
        {
            ensureDecl(decl, state, baseContext);
        }
        else if(auto declGroup = as<DeclGroup>(declBase))
        {
            for(auto dd : declGroup->decls)
            {
                ensureDecl(dd, state, baseContext);
            }
        }
        else
        {
            SLANG_UNEXPECTED("unknown case for declaration");
        }
    }

    void SemanticsDeclHeaderVisitor::visitTypeDefDecl(TypeDefDecl* decl)
    {
        decl->type = CheckProperType(decl->type);
    }

    void SemanticsDeclHeaderVisitor::visitGlobalGenericParamDecl(GlobalGenericParamDecl* decl)
    {
        // global generic param only allowed in global scope
        auto program = as<ModuleDecl>(decl->parentDecl);
        if (!program)
            getSink()->diagnose(decl, Slang::Diagnostics::globalGenParamInGlobalScopeOnly);
    }

    void SemanticsDeclHeaderVisitor::visitAssocTypeDecl(AssocTypeDecl* decl)
    {
        // assoctype only allowed in an interface
        auto interfaceDecl = as<InterfaceDecl>(decl->parentDecl);
        if (!interfaceDecl)
            getSink()->diagnose(decl, Slang::Diagnostics::assocTypeInInterfaceOnly);
    }

    void SemanticsDeclBodyVisitor::visitFunctionDeclBase(FunctionDeclBase* decl)
    {
        if (auto body = decl->body)
        {
            checkBodyStmt(body, decl);
        }
    }

    void SemanticsVisitor::getGenericParams(
        GenericDecl*                        decl,
        List<Decl*>&                        outParams,
        List<GenericTypeConstraintDecl*>&   outConstraints)
    {
        for (auto dd : decl->members)
        {
            if (dd == decl->inner)
                continue;

            if (auto typeParamDecl = as<GenericTypeParamDecl>(dd))
                outParams.add(typeParamDecl);
            else if (auto valueParamDecl = as<GenericValueParamDecl>(dd))
                outParams.add(valueParamDecl);
            else if (auto constraintDecl = as<GenericTypeConstraintDecl>(dd))
                outConstraints.add(constraintDecl);
        }
    }

    bool SemanticsVisitor::doGenericSignaturesMatch(
        GenericDecl*                    left,
        GenericDecl*                    right,
        GenericSubstitution**    outSubstRightToLeft)
    {
        // Our first goal here is to determine if `left` and
        // `right` have equivalent lists of explicit
        // generic parameters.
        //
        // Once we have determined that the explicit generic
        // parameters match, we will look at the constraints
        // placed on those parameters to see if they are
        // equivalent.
        //
        // We thus start by extracting the explicit parameters
        // and the constraints from each declaration.
        //
        List<Decl*> leftParams;
        List<GenericTypeConstraintDecl*> leftConstraints;
        getGenericParams(left, leftParams, leftConstraints);

        List<Decl*> rightParams;
        List<GenericTypeConstraintDecl*> rightConstraints;
        getGenericParams(right, rightParams, rightConstraints);

        // For there to be any hope of a match, the two decls
        // need to have the same number of explicit parameters.
        //
        Index paramCount = leftParams.getCount();
        if(paramCount != rightParams.getCount())
            return false;

        // Next we will walk through the parameters and look
        // for a pair-wise match.
        //
        for(Index pp = 0; pp < paramCount; ++pp)
        {
            Decl* leftParam = leftParams[pp];
            Decl* rightParam = rightParams[pp];

            if (auto leftTypeParam = as<GenericTypeParamDecl>(leftParam))
            {
                if (auto rightTypeParam = as<GenericTypeParamDecl>(rightParam))
                {
                    // Right now any two type parameters are a match.
                    // Names are irrelevant to matching, and any constraints
                    // on the type parameters are represented as implicit
                    // extra parameters of the generic.
                    //
                    // TODO: If we ever supported type parameters with
                    // higher kinds we might need to make a check here
                    // that the kind of each parameter matches (which
                    // would in a sense be a kind of recursive check
                    // of the generic signature of the parameter).
                    //
                    continue;
                }
            }
            else if (auto leftValueParam = as<GenericValueParamDecl>(leftParam))
            {
                if (auto rightValueParam = as<GenericValueParamDecl>(rightParam))
                {
                    // In this case we have two generic value parameters,
                    // and they should only be considered to match if
                    // they have the same type.
                    //
                    // Note: We are assuming here that the type of a value
                    // parameter cannot be dependent on any of the type
                    // parameters in the same signature. This is a reasonable
                    // assumption for now, but could get thorny down the road.
                    //
                    if (!leftValueParam->getType()->equals(rightValueParam->getType()))
                    {
                        // If the value parameters have non-matching types,
                        // then the full generic signatures do not match.
                        //
                        return false;
                    }

                    // Generic value parameters with the same type are
                    // always considered to match.
                    //
                    continue;
                }
            }

            // If we get to this point, then we have two parameters that
            // were of different syntatic categories (e.g., one type parameter
            // and one value parameter), so the signatures clearly don't match.
            //
            return false;
        }

        // At this point we know that the explicit generic parameters
        // of `left` and `right` are aligned, but we need to check
        // that the constraints that each declaration places on
        // its parameters match.
        //
        // A first challenge that arises is that `left` and `right`
        // will each express the constraints in terms of their
        // own parameters. For example, consider the following
        // declarations:
        //
        //      void foo1<T : IFoo>(T value);
        //      void foo2<U : IFoo>(U value);
        //
        // It is "obvious" to a human that the signatures here
        // match, but `foo1` has a constraint `T : IFoo` while
        // `foo2` has a constraint `U : IFoo`, and since `T`
        // and `U` are distinct `Decl`s, those constraints
        // are not obviously equivalent.
        //
        // We will work around this first issue by creating
        // a substitution taht lists all the parameters of
        // `left`, which we can use to specialize `right`
        // so that it aligns.
        //
        // In terms of the example above, this is like constructing
        // `foo2<T>` so that its constraint, after specialization,
        // looks like `T : IFoo`.
        //
        auto& substRightToLeft = *outSubstRightToLeft;
        substRightToLeft = createDummySubstitutions(left);
        substRightToLeft->genericDecl = right;

        // We should now be able to enumerate the constraints
        // on `right` in a way that uses the same type parameters
        // as `left`, using `rightDeclRef`.
        //
        // At this point a second problem arises: if/when we support
        // more flexibility in how generic parameter constraints are
        // specified, it will be possible for two declarations to
        // list the "same" constraints in very different ways.
        //
        // For example, if we support a `where` clause for separating
        // the constraints from the parameters, then the following
        // two declarations should have equivalent signatures:
        //
        //      void foo1<T>(T value)
        //          where T : IFoo
        //      { ... }
        //
        //      void foo2<T : IFoo>(T value)
        //      { ... }
        //
        // Similarly, if we allow for general compositions of interfaces
        // to be used as constraints, then there can be more than one
        // way to specify the same constraints:
        //
        //      void foo1<T : IFoo&IBar>(T value);
        //      void foo2<T : IBar&IFoo>(T value);
        //
        // Adding support for equality constraints in `where` clauses
        // also creates opportunities for multiple equivalent expressions:
        //
        //      void foo1<T,U>(...) where T.A == U.A;
        //      void foo2<T,U>(...) where U.A == T.A;
        //
        // A robsut version of the checking logic here should attempt
        // to *canonicalize* all of the constraints. Canonicalization
        // should involve putting constraints into a deterministic
        // order (e.g., for a generic with `<T,U>` all the constraints
        // on `T` should come before those on `U`), rewriting individual
        // constraints into a canonical form (e.g., `T : IFoo & IBar`
        // should turn into two constraints: `T : IFoo` and `T : IBar`),
        // etc.
        //
        // Once the constraints are in a canonical form we should be able
        // to test them for pairwise equivalent. As a safety measure we
        // could also try to test whether one set of constraints implies
        // the other (since implication in both directions should imply
        // equivalence, in which case our canonicalization had better
        // have produced the same result).
        //
        // For now we are taking a simpler short-cut by assuming
        // that constraints are already in a canonical form, which
        // is reasonable for now as the syntax only allows a single
        // constraint per parameter, specified on the parameter itself.
        //
        // Under the assumption of canonical constraints, we can
        // assume that different numbers of constraints must indicate
        // a signature mismatch.
        //
        Index constraintCount = leftConstraints.getCount();
        if(constraintCount != rightConstraints.getCount())
            return false;

        for (Index cc = 0; cc < constraintCount; ++cc)
        {
            // Note that we use a plain `Decl` pointer for the left
            // constraint, but need to use a `DeclRef` for the right
            // constraint so that we can take the substitution
            // arguments into account.
            //
            GenericTypeConstraintDecl* leftConstraint = leftConstraints[cc];
            DeclRef<GenericTypeConstraintDecl> rightConstraint(rightConstraints[cc], substRightToLeft);

            // For now, every constraint has the form `sub : sup`
            // to indicate that `sub` must be a subtype of `sup`.
            //
            // Two such constraints are equivalent if their `sub`
            // and `sup` types are pairwise equivalent.
            //
            auto leftSub = leftConstraint->sub;
            auto rightSub = getSub(m_astBuilder, rightConstraint);
            if(!leftSub->equals(rightSub))
                return false;

            auto leftSup = leftConstraint->sup;
            auto rightSup = getSup(m_astBuilder, rightConstraint);
            if(!leftSup->equals(rightSup))
                return false;
        }

        // If we have checked all of the (canonicalized) constraints
        // and found them to be pairwise equivalent then the two
        // generic signatures seem to match.
        //
        return true;
    }

    bool SemanticsVisitor::doFunctionSignaturesMatch(
        DeclRef<FuncDecl> fst,
        DeclRef<FuncDecl> snd)
    {

        // TODO(tfoley): This copies the parameter array, which is bad for performance.
        auto fstParams = getParameters(fst).toArray();
        auto sndParams = getParameters(snd).toArray();

        // If the functions have different numbers of parameters, then
        // their signatures trivially don't match.
        auto fstParamCount = fstParams.getCount();
        auto sndParamCount = sndParams.getCount();
        if (fstParamCount != sndParamCount)
            return false;

        for (Index ii = 0; ii < fstParamCount; ++ii)
        {
            auto fstParam = fstParams[ii];
            auto sndParam = sndParams[ii];

            // If a given parameter type doesn't match, then signatures don't match
            if (!getType(m_astBuilder, fstParam)->equals(getType(m_astBuilder, sndParam)))
                return false;

            // If one parameter is `out` and the other isn't, then they don't match
            //
            // Note(tfoley): we don't consider `out` and `inout` as distinct here,
            // because there is no way for overload resolution to pick between them.
            if (fstParam.getDecl()->hasModifier<OutModifier>() != sndParam.getDecl()->hasModifier<OutModifier>())
                return false;

            // If one parameter is `ref` and the other isn't, then they don't match.
            //
            if(fstParam.getDecl()->hasModifier<RefModifier>() != sndParam.getDecl()->hasModifier<RefModifier>())
                return false;
        }

        // Note(tfoley): return type doesn't enter into it, because we can't take
        // calling context into account during overload resolution.

        return true;
    }

    GenericSubstitution* SemanticsVisitor::createDummySubstitutions(
        GenericDecl* genericDecl)
    {
        List<Val*> args;
        for (auto dd : genericDecl->members)
        {
            if (dd == genericDecl->inner)
                continue;

            if (auto typeParam = as<GenericTypeParamDecl>(dd))
            {
                auto type = DeclRefType::create(m_astBuilder, makeDeclRef(typeParam));
                args.add(type);
            }
            else if (auto valueParam = as<GenericValueParamDecl>(dd))
            {
                auto val = m_astBuilder->getOrCreate<GenericParamIntVal>(
                    valueParam->getType(),
                    valueParam,
                    nullptr);
                args.add(val);
            }
            // TODO: need to handle constraints here?
        }
        GenericSubstitution* subst = m_astBuilder->getOrCreateGenericSubstitution(genericDecl, args, nullptr);
        return subst;
    }

    typedef Dictionary<Name*, CallableDecl*> TargetDeclDictionary;

    static void _addTargetModifiers(CallableDecl* decl, TargetDeclDictionary& ioDict)
    {
        if (auto specializedModifier = decl->findModifier<SpecializedForTargetModifier>())
        {
            // If it's specialized for target it should have a body...
            if (auto funcDecl = as<FunctionDeclBase>(decl))
            {
                SLANG_ASSERT(funcDecl->body);
            }
            Name* targetName = specializedModifier->targetToken.getName();

            ioDict.AddIfNotExists(targetName, decl);
        }
        else
        {
            for (auto modifier : decl->getModifiersOfType<TargetIntrinsicModifier>())
            {
                Name* targetName = modifier->targetToken.getName();
                ioDict.AddIfNotExists(targetName, decl);
            }

            auto funcDecl = as<FunctionDeclBase>(decl);
            if (funcDecl && funcDecl->body)
            {
                // Should only be one body if it isn't specialized for target.
                // Use nullptr for this scenario
                ioDict.AddIfNotExists(nullptr, decl);
            }
        }  
    }

    Result SemanticsVisitor::checkFuncRedeclaration(
        FuncDecl* newDecl,
        FuncDecl* oldDecl)
    {
        // There are a few different cases that this function needs
        // to check for:
        //
        // * If `newDecl` and `oldDecl` have different signatures such
        //   that they can always be distinguished at call sites, then
        //   they don't conflict and don't count as redeclarations.
        //
        // * If `newDecl` and `oldDecl` have matching signatures, but
        //   differ in return type (or other details that would affect
        //   compatibility), then the declarations conflict and an
        //   error needs to be diagnosed.
        //
        // * If `newDecl` and `oldDecl` have matching/compatible sigantures,
        //   but differ when it comes to target-specific overloading,
        //   then they can co-exist.
        //
        // * If `newDecl` and `oldDecl` have matching/compatible signatures
        //   and are specialized for the same target(s), then only
        //   one can have a body (in which case the other is a forward declaration),
        //   or else we have a redefinition error.

        auto newGenericDecl = as<GenericDecl>(newDecl->parentDecl);
        auto oldGenericDecl = as<GenericDecl>(oldDecl->parentDecl);

        // If one declaration is a prefix/postfix operator, and the
        // other is not a matching operator, then don't consider these
        // to be re-declarations.
        //
        // Note(tfoley): Any attempt to call such an operator using
        // ordinary function-call syntax (if we decided to allow it)
        // would be ambiguous in such a case, of course.
        //
        if (newDecl->hasModifier<PrefixModifier>() != oldDecl->hasModifier<PrefixModifier>())
            return SLANG_OK;
        if (newDecl->hasModifier<PostfixModifier>() != oldDecl->hasModifier<PostfixModifier>())
            return SLANG_OK;

        // If one is generic and the other isn't, then there is no match.
        if ((newGenericDecl != nullptr) != (oldGenericDecl != nullptr))
            return SLANG_OK;

        // We are going to be comparing the signatures of the
        // two functions, but if they are *generic* functions
        // then we will need to compare them with consistent
        // specializations in place.
        //
        // We'll go ahead and create some (unspecialized) declaration
        // references here, just to be prepared.
        //
        DeclRef<FuncDecl> newDeclRef(newDecl, nullptr);
        DeclRef<FuncDecl> oldDeclRef(oldDecl, nullptr);

        // If we are working with generic functions, then we need to
        // consider if their generic signatures match.
        //
        if(newGenericDecl)
        {
            // If one declaration is generic, the other must be.
            // (This condition was already checked above)
            //
            SLANG_ASSERT(oldGenericDecl);

            // As part of checking if the generic signatures match,
            // we will produce a substitution that can be used to
            // reference `oldGenericDecl` with the generic parameters
            // substituted for those of `newDecl`.
            //
            // One way to think about it is that if we have these
            // declarations (ignore the name differences...):
            //
            //     // oldDecl:
            //     void foo1<T>(T x);
            //
            //     // newDecl:
            //     void foo2<U>(U x);
            //
            // Then we will compare the parameter types of `foo2`
            // against the specialization `foo1<U>`.
            //
            GenericSubstitution* subst = nullptr;
            if(!doGenericSignaturesMatch(newGenericDecl, oldGenericDecl, &subst))
                return SLANG_OK;

            oldDeclRef.substitutions.substitutions = subst;
        }

        // If the parameter signatures don't match, then don't worry
        if (!doFunctionSignaturesMatch(newDeclRef, oldDeclRef))
            return SLANG_OK;

        // If we get this far, then we've got two declarations in the same
        // scope, with the same name and signature, so they appear
        // to be redeclarations.
        //
        // We will track that redeclaration occured, so that we can
        // take it into account for overload resolution.
        //
        // A huge complication that we'll need to deal with is that
        // multiple declarations might introduce default values for
        // (different) parameters, and we might need to merge across
        // all of them (which could get complicated if defaults for
        // parameters can reference earlier parameters).

        // If the previous declaration wasn't already recorded
        // as being part of a redeclaration family, then make
        // it the primary declaration of a new family.
        if (!oldDecl->primaryDecl)
        {
            oldDecl->primaryDecl = oldDecl;
        }

        // The new declaration will belong to the family of
        // the previous one, and so it will share the same
        // primary declaration.
        newDecl->primaryDecl = oldDecl->primaryDecl;
        newDecl->nextDecl = nullptr;

        // Next we want to chain the new declaration onto
        // the linked list of redeclarations.
        auto link = &oldDecl->nextDecl;
        while (*link)
            link = &(*link)->nextDecl;
        *link = newDecl;

        // Now that we've added things to a group of redeclarations,
        // we can do some additional validation.

        // First, we will ensure that the return types match
        // between the declarations, so that they are truly
        // interchangeable.
        //
        // Note(tfoley): If we ever decide to add a beefier type
        // system to Slang, we might allow overloads like this,
        // so long as the desired result type can be disambiguated
        // based on context at the call type. In that case we would
        // consider result types earlier, as part of the signature
        // matching step.
        //
        auto resultType = getResultType(m_astBuilder, newDeclRef);
        auto prevResultType = getResultType(m_astBuilder, oldDeclRef);
        if (!resultType->equals(prevResultType))
        {
            // Bad redeclaration
            getSink()->diagnose(newDecl, Diagnostics::functionRedeclarationWithDifferentReturnType, newDecl->getName(), resultType, prevResultType);
            getSink()->diagnose(oldDecl, Diagnostics::seePreviousDeclarationOf, newDecl->getName());

            // Don't bother emitting other errors at this point
            return SLANG_FAIL;
        }

        // TODO: Enforce that the new declaration had better
        // not specify a default value for any parameter that
        // already had a default value in a prior declaration.

        // We are going to want to enforce that we cannot have
        // two declarations of a function both specify bodies.
        // Before we make that check, however, we need to deal
        // with the case where the two function declarations
        // might represent different target-specific versions
        // of a function.
       
        // If both of the declarations have a body, then there
        // is trouble, because we wouldn't know which one to
        // use during code generation.

        // Here to cover the 'bodies'/target_intrinsics, we find all the targets that
        // that are previously defined, and make sure the new definition
        // doesn't try and define what is already defined.
        {
            TargetDeclDictionary currentTargets;
            {
                CallableDecl* curDecl = newDecl->primaryDecl;
                while (curDecl)
                {
                    if (curDecl != newDecl)
                    {
                        _addTargetModifiers(curDecl, currentTargets);
                    }
                    curDecl = curDecl->nextDecl;
                }
            }

            // Add the targets for this new decl
            TargetDeclDictionary newTargets;
            _addTargetModifiers(newDecl, newTargets);

            bool hasConflict = false;
            for (auto& pair : newTargets)
            {
                Name* target = pair.Key;
                auto found = currentTargets.TryGetValue(target);
                if (found)
                {
                    // Redefinition
                    if (!hasConflict)
                    {
                        getSink()->diagnose(newDecl, Diagnostics::functionRedefinition, newDecl->getName());
                        hasConflict = true;
                    }

                    auto prevDecl = *found;
                    getSink()->diagnose(prevDecl, Diagnostics::seePreviousDefinitionOf, prevDecl->getName());
                }
            }

            if (hasConflict)
            {
                return SLANG_FAIL;
            }
        }

        // At this point we've processed the redeclaration and
        // put it into a group, so there is no reason to keep
        // looping and looking at prior declarations.
        //
        // While no diagnostics have been emitted, we return
        // a failure result from the operation to indicate
        // to the caller that they should stop looping over
        // declarations at this point.
        //
        return SLANG_FAIL;
    }

    Result SemanticsVisitor::checkRedeclaration(Decl* newDecl, Decl* oldDecl)
    {
        // If either of the declarations being looked at is generic, then
        // we want to consider the "inner" declaration instead when
        // making decisions about what to allow or not.
        //
        if(auto newGenericDecl = as<GenericDecl>(newDecl))
            newDecl = newGenericDecl->inner;
        if(auto oldGenericDecl = as<GenericDecl>(oldDecl))
            oldDecl = oldGenericDecl->inner;

        // Functions are special in that we can have many declarations
        // with the same name in a given scope, and it is possible
        // for them to co-exist as overloads, or even just be multiple
        // declarations of the same function (thanks to the inherited
        // legacy of C forward declarations).
        //
        // If both declarations are functions, we will check that
        // they are allowed to co-exist using these more nuanced rules.
        //
        if( auto newFuncDecl = as<FuncDecl>(newDecl) )
        {
            if(auto oldFuncDecl = as<FuncDecl>(oldDecl) )
            {
                // Both new and old declarations are functions,
                // so redeclaration may be valid.
                return checkFuncRedeclaration(newFuncDecl, oldFuncDecl);
            }
        }

        // For all other flavors of declaration, we do not
        // allow duplicate declarations with the same name.
        //
        // TODO: We might consider allowing some other cases
        // of overloading that can be safely disambiguated:
        //
        // * A type and a value (function/variable/etc.) of the same name can usually
        //   co-exist because we can distinguish which is needed by context.
        //
        // * Multiple generic types with the same name can co-exist
        //   if their generic parameter lists are sufficient to
        //   tell them apart at a use site.

        // We will diagnose a redeclaration error at the new declaration,
        // and point to the old declaration for context.
        //
        getSink()->diagnose(newDecl, Diagnostics::redeclaration, newDecl->getName());
        getSink()->diagnose(oldDecl, Diagnostics::seePreviousDeclarationOf, oldDecl->getName());
        return SLANG_FAIL;
    }


    void SemanticsVisitor::checkForRedeclaration(Decl* decl)
    {
        // We want to consider a "new" declaration in the context
        // of some parent/container declaration, and compare it
        // to pre-existing "old" declarations of the same name
        // in the same container.
        //
        auto newDecl = decl;
        auto parentDecl = decl->parentDecl;

        // Sanity check: there should always be a parent declaration.
        //
        SLANG_ASSERT(parentDecl);
        if (!parentDecl) return;

        // If the declaration is the "inner" declaration of a generic,
        // then we actually want to look one level up, because the
        // peers/siblings of the declaration will belong to the same
        // parent as the generic, not to the generic.
        //
        if( auto genericParentDecl = as<GenericDecl>(parentDecl) )
        {
            // Note: we need to check here to be sure `newDecl`
            // is the "inner" declaration and not one of the
            // generic parameters, or else we will end up
            // checking them at the wrong scope.
            //
            if( newDecl == genericParentDecl->inner )
            {
                newDecl = parentDecl;
                parentDecl = genericParentDecl->parentDecl;
            }
        }

        // We will now look for other declarations with
        // the same name in the same parent/container.
        //
        buildMemberDictionary(parentDecl);
        for (auto oldDecl = newDecl->nextInContainerWithSameName; oldDecl; oldDecl = oldDecl->nextInContainerWithSameName)
        {
            // For each matching declaration, we will check
            // whether the redeclaration should be allowed,
            // and emit an appropriate diagnostic if not.
            //
            Result checkResult = checkRedeclaration(newDecl, oldDecl);

            // The `checkRedeclaration` function will return a failure
            // status (whether or not it actually emitted a diagnostic)
            // if we should stop checking further redeclarations, because
            // the declaration in question has been dealt with fully.
            //
            if(SLANG_FAILED(checkResult))
                break;
        }
    }


    void SemanticsDeclHeaderVisitor::visitParamDecl(ParamDecl* paramDecl)
    {
        // TODO: This logic should be shared with the other cases of
        // variable declarations. The main reason I am not doing it
        // yet is that we use a `ParamDecl` with a null type as a
        // special case in attribute declarations, and that could
        // trip up the ordinary variable checks.

        auto typeExpr = paramDecl->type;
        if(typeExpr.exp)
        {
            typeExpr = CheckUsableType(typeExpr);
            paramDecl->type = typeExpr;
        }
    }

    void SemanticsDeclBodyVisitor::visitParamDecl(ParamDecl* paramDecl)
    {
        auto typeExpr = paramDecl->type;

        // The "initializer" expression for a parameter represents
        // a default argument value to use if an explicit one is
        // not supplied.
        if(auto initExpr = paramDecl->initExpr)
        {
            // We must check the expression and coerce it to the
            // actual type of the parameter.
            //
            initExpr = CheckTerm(initExpr);
            initExpr = coerce(typeExpr.type, initExpr);
            paramDecl->initExpr = initExpr;

            // TODO: a default argument expression needs to
            // conform to other constraints to be valid.
            // For example, it should not be allowed to refer
            // to other parameters of the same function (or maybe
            // only the parameters to its left...).

            // A default argument value should not be allowed on an
            // `out` or `inout` parameter.
            //
            // TODO: we could relax this by requiring the expression
            // to yield an lvalue, but that seems like a feature
            // with limited practical utility (and an easy source
            // of confusing behavior).
            //
            // Note: the `InOutModifier` class inherits from `OutModifier`,
            // so we only need to check for the base case.
            //
            if(paramDecl->findModifier<OutModifier>())
            {
                getSink()->diagnose(initExpr, Diagnostics::outputParameterCannotHaveDefaultValue);
            }
        }
    }

    void SemanticsDeclHeaderVisitor::checkCallableDeclCommon(CallableDecl* decl)
    {
        for(auto paramDecl : decl->getParameters())
        {
            ensureDecl(paramDecl, DeclCheckState::ReadyForReference);
        }

        auto errorType = decl->errorType;
        if (errorType.exp)
        {
            errorType = CheckProperType(errorType);
        }
        else
        {
            errorType = TypeExp(m_astBuilder->getBottomType());
        }
        decl->errorType = errorType;
    }

    void SemanticsDeclHeaderVisitor::visitFuncDecl(FuncDecl* funcDecl)
    {
        auto resultType = funcDecl->returnType;
        if(resultType.exp)
        {
            resultType = CheckProperType(resultType);
        }
        else
        {
            resultType = TypeExp(m_astBuilder->getVoidType());
        }
        funcDecl->returnType = resultType;

        checkCallableDeclCommon(funcDecl);
    }

    IntegerLiteralValue SemanticsVisitor::GetMinBound(IntVal* val)
    {
        if (auto constantVal = as<ConstantIntVal>(val))
            return constantVal->value;

        // TODO(tfoley): Need to track intervals so that this isn't just a lie...
        return 1;
    }

    void SemanticsVisitor::maybeInferArraySizeForVariable(VarDeclBase* varDecl)
    {
        // Not an array?
        auto arrayType = as<ArrayExpressionType>(varDecl->type);
        if (!arrayType) return;

        // Explicit element count given?
        auto elementCount = arrayType->arrayLength;
        if (elementCount) return;

        // No initializer?
        auto initExpr = varDecl->initExpr;
        if(!initExpr) return;

        // Is the type of the initializer an array type?
        if(auto arrayInitType = as<ArrayExpressionType>(initExpr->type))
        {
            elementCount = arrayInitType->arrayLength;
        }
        else
        {
            // Nothing to do: we couldn't infer a size
            return;
        }

        // Create a new array type based on the size we found,
        // and install it into our type.
        varDecl->type.type = getArrayType(
            m_astBuilder,
            arrayType->baseType,
            elementCount);
    }

    void SemanticsVisitor::validateArraySizeForVariable(VarDeclBase* varDecl)
    {
        auto arrayType = as<ArrayExpressionType>(varDecl->type);
        if (!arrayType) return;

        auto elementCount = arrayType->arrayLength;
        if (!elementCount)
        {
            // Note(tfoley): For now we allow arrays of unspecified size
            // everywhere, because some source languages (e.g., GLSL)
            // allow them in specific cases.
#if 0
            getSink()->diagnose(varDecl, Diagnostics::invalidArraySize);
#endif
            return;
        }

        // TODO(tfoley): How to handle the case where bound isn't known?
        if (GetMinBound(elementCount) <= 0)
        {
            getSink()->diagnose(varDecl, Diagnostics::invalidArraySize);
            return;
        }
    }

    void SemanticsDeclBasesVisitor::_validateExtensionDeclTargetType(ExtensionDecl* decl)
    {
        if (auto targetDeclRefType = as<DeclRefType>(decl->targetType))
        {
            // Attach our extension to that type as a candidate...
            if (auto aggTypeDeclRef = targetDeclRefType->declRef.as<AggTypeDecl>())
            {
                auto aggTypeDecl = aggTypeDeclRef.getDecl();

                getShared()->registerCandidateExtension(aggTypeDecl, decl);

                return;
            }
        }
        if (!as<ErrorType>(decl->targetType.type))
        {
            getSink()->diagnose(decl->targetType.exp, Diagnostics::invalidExtensionOnType, decl->targetType);
        }
    }

    void SemanticsDeclBasesVisitor::visitExtensionDecl(ExtensionDecl* decl)
    {
        // We check the target type expression, and then validate
        // that the type it names is one that it makes sense
        // to extend.
        //
        decl->targetType = CheckProperType(decl->targetType);
        _validateExtensionDeclTargetType(decl);

        for( auto inheritanceDecl : decl->getMembersOfType<InheritanceDecl>() )
        {
            ensureDecl(inheritanceDecl, DeclCheckState::CanUseBaseOfInheritanceDecl);
            auto baseType = inheritanceDecl->base.type;

            // It is possible that there was an error in checking the base type
            // expression, and in such a case we shouldn't emit a cascading error.
            //
            if( auto baseErrorType = as<ErrorType>(baseType) )
            {
                continue;
            }

            // An `extension` can only introduce inheritance from `interface` types.
            //
            // TODO: It might in theory make sense to allow an `extension` to
            // introduce a non-`interface` base if we decide that an `extension`
            // within the same module as the type it extends counts as just
            // a continuation of the type's body (like a `partial class` in C#).
            //
            auto baseDeclRefType = as<DeclRefType>(baseType);
            if( !baseDeclRefType )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfExtensionMustBeInterface, decl, baseType);
                continue;
            }

            auto baseDeclRef = baseDeclRefType->declRef;
            auto baseInterfaceDeclRef = baseDeclRef.as<InterfaceDecl>();
            if( !baseInterfaceDeclRef )
            {
                getSink()->diagnose(inheritanceDecl, Diagnostics::baseOfExtensionMustBeInterface, decl, baseType);
                continue;
            }

            // TODO: At this point we have the `baseInterfaceDeclRef`
            // and could use it to perform further validity checks,
            // and/or to build up a more refined representation of
            // the inheritance graph for this extension (e.g., a "class
            // precedence list").
            //
            // E.g., we can/should check that we aren't introducing
            // an inheritance relationship that already existed
            // on the type as originally declared.

            _validateCrossModuleInheritance(decl, inheritanceDecl);
        }
    }

    Type* SemanticsVisitor::calcThisType(DeclRef<Decl> declRef)
    {
        if( auto interfaceDeclRef = declRef.as<InterfaceDecl>() )
        {
            // In the body of an `interface`, a `This` type
            // refers to the concrete type that will eventually
            // conform to the interface and fill in its
            // requirements.
            //
            ThisType* thisType = m_astBuilder->create<ThisType>();
            thisType->interfaceDeclRef = interfaceDeclRef;
            return thisType;
        }
        else if (auto aggTypeDeclRef = declRef.as<AggTypeDecl>())
        {
            // In the body of an ordinary aggregate type,
            // such as a `struct`, the `This` type just
            // refers to the type itself.
            //
            // TODO: If/when we support `class` types
            // with inheritance, then `This` inside a class
            // would need to refer to the eventual concrete
            // type, much like the `interface` case above.
            //
            return DeclRefType::create(m_astBuilder, aggTypeDeclRef);
        }
        else if (auto extDeclRef = declRef.as<ExtensionDecl>())
        {
            // In the body of an `extension`, the `This`
            // type refers to the type being extended.
            //
            // Note: we currently have this loop back
            // around through `calcThisType` for the
            // type being extended, rather than just
            // using it directly. This makes a difference
            // for polymorphic types like `interface`s,
            // and there are reasonable arguments for
            // the validity of either option.
            //
            // Does `extension IFoo` mean extending
            // exactly the type `IFoo` (an existential,
            // which could at runtime be a value of
            // any type conforming to `IFoo`), or does
            // it implicitly extend every type that
            // conforms to `IFoo`? The difference is
            // significant, and we need to make a choice
            // sooner or later.
            //
            ensureDecl(extDeclRef, DeclCheckState::CanUseExtensionTargetType);
            auto targetType = getTargetType(m_astBuilder, extDeclRef);
            return calcThisType(targetType);
        }
        else
        {
            return nullptr;
        }
    }

    Type* SemanticsVisitor::calcThisType(Type* type)
    {
        if( auto declRefType = as<DeclRefType>(type) )
        {
            return calcThisType(declRefType->declRef);
        }
        else
        {
            return type;
        }
    }

    Type* SemanticsVisitor::findResultTypeForConstructorDecl(ConstructorDecl* decl)
    {
        // We want to look at the parent of the declaration,
        // but if the declaration is generic, the parent will be
        // the `GenericDecl` and we need to skip past that to
        // the grandparent.
        //
        auto parent = decl->parentDecl;
        auto genericParent = as<GenericDecl>(parent);
        if (genericParent)
        {
            parent = genericParent->parentDecl;
        }

        // The result type for a constructor is whatever `This` would
        // refer to in the body of the outer declaration.
        //
        auto thisType = calcThisType(makeDeclRef(parent));
        if( !thisType )
        {
            getSink()->diagnose(decl, Diagnostics::initializerNotInsideType);
            thisType = m_astBuilder->getErrorType();
        }
        return thisType;
    }

    void SemanticsDeclHeaderVisitor::visitConstructorDecl(ConstructorDecl* decl)
    {
        // We need to compute the result tyep for this declaration,
        // since it wasn't filled in for us.
        decl->returnType.type = findResultTypeForConstructorDecl(decl);

        checkCallableDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitAbstractStorageDeclCommon(ContainerDecl* decl)
    {
        // If we have a subscript or property declaration with no accessor declarations,
        // then we should create a single `GetterDecl` to represent
        // the implicit meaning of their declaration, so:
        //
        //      subscript(uint index) -> T;
        //      property x : Y;
        //
        // becomes:
        //
        //      subscript(uint index) -> T { get; }
        //      property x : Y { get; }
        //

        bool anyAccessors = decl->getMembersOfType<AccessorDecl>().isNonEmpty();

        if(!anyAccessors)
        {
            GetterDecl* getterDecl = m_astBuilder->create<GetterDecl>();
            getterDecl->loc = decl->loc;

            getterDecl->parentDecl = decl;
            decl->members.add(getterDecl);
        }
    }

    void SemanticsDeclHeaderVisitor::visitSubscriptDecl(SubscriptDecl* decl)
    {
        decl->returnType = CheckUsableType(decl->returnType);

        visitAbstractStorageDeclCommon(decl);

        checkCallableDeclCommon(decl);
    }

    void SemanticsDeclHeaderVisitor::visitPropertyDecl(PropertyDecl* decl)
    {
        decl->type = CheckUsableType(decl->type);
        visitAbstractStorageDeclCommon(decl);
    }

    Type* SemanticsDeclHeaderVisitor::_getAccessorStorageType(AccessorDecl* decl)
    {
        auto parentDecl = decl->parentDecl;
        if (auto parentSubscript = as<SubscriptDecl>(parentDecl))
        {
            ensureDecl(parentSubscript, DeclCheckState::CanUseTypeOfValueDecl);
            return parentSubscript->returnType;
        }
        else if (auto parentProperty = as<PropertyDecl>(parentDecl))
        {
            ensureDecl(parentProperty, DeclCheckState::CanUseTypeOfValueDecl);
            return parentProperty->type.type;
        }
        else
        {
            return getASTBuilder()->getErrorType();
        }
    }

    void SemanticsDeclHeaderVisitor::_visitAccessorDeclCommon(AccessorDecl* decl)
    {
        // An accessor must appear nested inside a subscript or property declaration.
        //
        auto parentDecl = decl->parentDecl;
        if (as<SubscriptDecl>(parentDecl))
        {}
        else if (as<PropertyDecl>(parentDecl))
        {}
        else
        {
            getSink()->diagnose(decl, Diagnostics::accessorMustBeInsideSubscriptOrProperty);
        }
    }

    void SemanticsDeclHeaderVisitor::visitAccessorDecl(AccessorDecl* decl)
    {
        _visitAccessorDeclCommon(decl);

        // Note: This subroutine is used by both `get`
        // and `ref` accessors, but is bypassed by
        // `set` accessors (which use `visitSetterDecl`
        // intead).

        // Accessors (other than setters) don't support
        // parameters.
        //
        if( decl->getParameters().getCount() != 0 )
        {
            getSink()->diagnose(decl, Diagnostics::nonSetAccessorMustNotHaveParams);
        }

        // By default, the return type of an accessor is treated as
        // the type of the abstract storage location being accessed.
        //
        // A `ref`  accessor currently relies on this logic even though
        // it isn't quite correct, because we don't have support
        // for by-reference return values today. This is a non-issue
        // for now because we don't support user-defined `ref`
        // accessors yet.
        //
        // TODO: Once we can support the by-reference return value
        // correctly *or* we can move to something like a coroutine-based
        // `modify` accessor (a la Swift), we should split out
        // handling of `RefAccessorDecl` and only use this routine
        // for `GetterDecl`s.
        //
        decl->returnType.type = _getAccessorStorageType(decl);
    }

    void SemanticsDeclHeaderVisitor::visitSetterDecl(SetterDecl* decl)
    {
        // Make sure to invoke the common checking logic for all accessors.
        _visitAccessorDeclCommon(decl);

        // A `set` accessor always returns `void`.
        //
        decl->returnType.type = getASTBuilder()->getVoidType();

        // A setter always receives a single value representing
        // the new value to set into the storage.
        //
        // The user may declare that parameter explicitly and
        // thereby control its name, or they can declare no
        // parmaeters and allow the compiler to synthesize one
        // names `newValue`.
        //
        ParamDecl* newValueParam = nullptr;
        auto params = decl->getParameters();
        if( params.getCount() >= 1 )
        {
            // If the user declared an explicit parameter
            // then that is the one that will represent
            // the new value.
            //
            newValueParam = params.getFirst();

            if( params.getCount() > 1 )
            {
                // If the user declared more than one explicit
                // parameter, then that is an error.
                //
                getSink()->diagnose(params[1], Diagnostics::setAccessorMayNotHaveMoreThanOneParam);
            }
        }
        else
        {
            // If the user didn't declare any explicit parameters,
            // then we create an implicit one and add it into
            // the AST.
            //
            newValueParam = m_astBuilder->create<ParamDecl>();
            newValueParam->nameAndLoc.name = getName("newValue");
            newValueParam->nameAndLoc.loc = decl->loc;

            newValueParam->parentDecl = decl;
            decl->members.add(newValueParam);
        }

        // The new-value parameter is expected to have the
        // same type as the abstract storage that the
        // accessor is setting.
        //
        auto newValueType = _getAccessorStorageType(decl);

        // It is allowed and encouraged for the programmer
        // to leave off the type on the new-value parameter,
        // in which case we will set it to the expected
        // type automatically.
        //
        if( !newValueParam->type.exp )
        {
            newValueParam->type.type = newValueType;
        }
        else
        {
            // If the user *did* give the new-value parameter
            // an explicit type, then we need to check it
            // and then enforce that it matches what we expect.
            //
            auto actualType = CheckProperType(newValueParam->type);

            if(as<ErrorType>(actualType))
            {}
            else if(actualType->equals(newValueType))
            {}
            else
            {
                getSink()->diagnose(newValueParam, Diagnostics::setAccessorParamWrongType, newValueParam, actualType, newValueType);
            }
        }
    }

    GenericDecl* SemanticsVisitor::GetOuterGeneric(Decl* decl)
    {
        auto parentDecl = decl->parentDecl;
        if (!parentDecl) return nullptr;
        auto parentGeneric = as<GenericDecl>(parentDecl);
        return parentGeneric;
    }

    DeclRef<ExtensionDecl> SemanticsVisitor::ApplyExtensionToType(
        ExtensionDecl*  extDecl,
        Type*    type)
    {
        DeclRef<ExtensionDecl> extDeclRef = makeDeclRef(extDecl);

        // If the extension is a generic extension, then we
        // need to infer type arguments that will give
        // us a target type that matches `type`.
        //
        if (auto extGenericDecl = GetOuterGeneric(extDecl))
        {
            ConstraintSystem constraints;
            constraints.loc = extDecl->loc;
            constraints.genericDecl = extGenericDecl;

            if (!TryUnifyTypes(constraints, extDecl->targetType.Ptr(), type))
                return DeclRef<ExtensionDecl>();

            auto constraintSubst = TrySolveConstraintSystem(&constraints, DeclRef<Decl>(extGenericDecl, nullptr).as<GenericDecl>());
            if (!constraintSubst)
            {
                return DeclRef<ExtensionDecl>();
            }

            // Construct a reference to the extension with our constraint variables
            // set as they were found by solving the constraint system.
            extDeclRef = DeclRef<Decl>(extDecl, constraintSubst).as<ExtensionDecl>();
        }

        // Now extract the target type from our (possibly specialized) extension decl-ref.
        Type* targetType = getTargetType(m_astBuilder, extDeclRef);

        // As a bit of a kludge here, if the target type of the extension is
        // an interface, and the `type` we are trying to match up has a this-type
        // substitution for that interface, then we want to attach a matching
        // substitution to the extension decl-ref.
        if(auto targetDeclRefType = as<DeclRefType>(targetType))
        {
            if(auto targetInterfaceDeclRef = targetDeclRefType->declRef.as<InterfaceDecl>())
            {
                // Okay, the target type is an interface.
                //
                // Is the type we want to apply to also an interface?
                if(auto appDeclRefType = as<DeclRefType>(type))
                {
                    if(auto appInterfaceDeclRef = appDeclRefType->declRef.as<InterfaceDecl>())
                    {
                        if(appInterfaceDeclRef.getDecl() == targetInterfaceDeclRef.getDecl())
                        {
                            // Looks like we have a match in the types,
                            // now let's see if we have a this-type substitution.
                            if(auto appThisTypeSubst = as<ThisTypeSubstitution>(appInterfaceDeclRef.substitutions.substitutions))
                            {
                                if(appThisTypeSubst->interfaceDecl == appInterfaceDeclRef.getDecl())
                                {
                                    // The type we want to apply to has a this-type substitution,
                                    // and (by construction) the target type currently does not.
                                    //
                                    SLANG_ASSERT(!as<ThisTypeSubstitution>(targetInterfaceDeclRef.substitutions.substitutions));

                                    // We will create a new substitution to apply to the target type.
                                    ThisTypeSubstitution* newTargetSubst = m_astBuilder->create<ThisTypeSubstitution>();
                                    newTargetSubst->interfaceDecl = appThisTypeSubst->interfaceDecl;
                                    newTargetSubst->witness = appThisTypeSubst->witness;
                                    newTargetSubst->outer = targetInterfaceDeclRef.substitutions.substitutions;

                                    targetType = DeclRefType::create(m_astBuilder,
                                        DeclRef<InterfaceDecl>(targetInterfaceDeclRef.getDecl(), newTargetSubst));

                                    // Note: we are constructing a this-type substitution that
                                    // we will apply to the extension declaration as well.
                                    // This is not strictly allowed by our current representation
                                    // choices, but we need it in order to make sure that
                                    // references to the target type of the extension
                                    // declaration have a chance to resolve the way we want them to.

                                    ThisTypeSubstitution* newExtSubst = m_astBuilder->create<ThisTypeSubstitution>();
                                    newExtSubst->interfaceDecl = appThisTypeSubst->interfaceDecl;
                                    newExtSubst->witness = appThisTypeSubst->witness;
                                    newExtSubst->outer = extDeclRef.substitutions.substitutions;

                                    extDeclRef = DeclRef<ExtensionDecl>(
                                        extDeclRef.getDecl(),
                                        newExtSubst);

                                    // TODO: Ideally we should also apply the chosen specialization to
                                    // the decl-ref for the extension, so that subsequent lookup through
                                    // the members of this extension will retain that substitution and
                                    // be able to apply it.
                                    //
                                    // E.g., if an extension method returns a value of an associated
                                    // type, then we'd want that to become specialized to a concrete
                                    // type when using the extension method on a value of concrete type.
                                    //
                                    // The challenge here that makes me reluctant to just staple on
                                    // such a substitution is that it wouldn't follow our implicit
                                    // rules about where `ThisTypeSubstitution`s can appear.
                                }
                            }
                        }
                    }
                }
            }
        }

        // In order for this extension to apply to the given type, we
        // need to have a match on the target types.
        if (!type->equals(targetType))
            return DeclRef<ExtensionDecl>();


        return extDeclRef;
    }

    QualType SemanticsVisitor::GetTypeForDeclRef(DeclRef<Decl> declRef, SourceLoc loc)
    {
        Type* typeResult = nullptr;
        return getTypeForDeclRef(
            m_astBuilder,
            this,
            getSink(),
            declRef,
            &typeResult,
            loc);
    }

    void SemanticsVisitor::importModuleIntoScope(Scope* scope, ModuleDecl* moduleDecl)
    {
        // If we've imported this one already, then
        // skip the step where we modify the current scope.
        auto& importedModulesList = getShared()->importedModulesList;
        auto& importedModulesSet = getShared()->importedModulesSet;
        if (importedModulesSet.Contains(moduleDecl))
        {
            return;
        }
        importedModulesList.add(moduleDecl);
        importedModulesSet.Add(moduleDecl);

        // Create a new sub-scope to wire the module
        // into our lookup chain.
        auto subScope = getASTBuilder()->create<Scope>();
        subScope->containerDecl = moduleDecl;

        subScope->nextSibling = scope->nextSibling;
        scope->nextSibling = subScope;

        // Also import any modules from nested `import` declarations
        // with the `__exported` modifier
        for (auto importDecl : moduleDecl->getMembersOfType<ImportDecl>())
        {
            if (!importDecl->hasModifier<ExportedModifier>())
                continue;

            importModuleIntoScope(scope, importDecl->importedModuleDecl);
        }
    }

    void SemanticsDeclHeaderVisitor::visitImportDecl(ImportDecl* decl)
    {
        // We need to look for a module with the specified name
        // (whether it has already been loaded, or needs to
        // be loaded), and then put its declarations into
        // the current scope.

        auto name = decl->moduleNameAndLoc.name;
        auto scope = decl->scope;

        // Try to load a module matching the name
        auto importedModule = findOrImportModule(
            getLinkage(),
            name,
            decl->moduleNameAndLoc.loc,
            getSink(),
            getShared()->m_environmentModules);

        // If we didn't find a matching module, then bail out
        if (!importedModule)
            return;

        // Record the module that was imported, so that we can use
        // it later during code generation.
        auto importedModuleDecl = importedModule->getModuleDecl();
        decl->importedModuleDecl = importedModuleDecl;

        // Add the declarations from the imported module into the scope
        // that the `import` declaration is set to extend.
        //
        importModuleIntoScope(scope, importedModuleDecl);

        // Record the `import`ed module (and everything it depends on)
        // as a dependency of the module we are compiling.
        if(auto module = getModule(decl))
        {
            module->addModuleDependency(importedModule);
        }
    }

    void SemanticsDeclHeaderVisitor::visitUsingDecl(UsingDecl* decl)
    {
        // First, we need to look up whatever the argument of the `using`
        // declaration names.
        //
        decl->arg = CheckTerm(decl->arg);

        // Next, we want to ensure that whatever is being named by `decl->arg`
        // is a namespace (or a module, since modules are namespace-like).
        //
        // TODO: The logic here assumes that we can't have multiple `NamespaceDecl`s
        // with the same name in scope, but that assumption is only valid in the
        // context of a single module (where we deduplicate `namespace`s during
        // parsing). If a user `import`s multiple modules that all have namespaces
        // of the same name, it would be possible for `decl->arg` to be overloaded.
        // In that case we should really iterate over all the entities that are
        // named and import any that are namespace-like.
        //
        NamespaceDeclBase* namespaceDecl = nullptr;
        if( auto declRefExpr = as<DeclRefExpr>(decl->arg) )
        {
            if( auto namespaceDeclRef = declRefExpr->declRef.as<NamespaceDeclBase>() )
            {
                SLANG_ASSERT(!namespaceDeclRef.substitutions.substitutions);
                namespaceDecl = namespaceDeclRef.getDecl();
            }
        }
        if( !namespaceDecl )
        {
            getSink()->diagnose(decl->arg, Diagnostics::expectedANamespace, decl->arg->type);
            return;
        }

        // Once we have identified the namespace to bring into scope,
        // we need to create a new sibling sub-scope to add to the
        // lookup scope that was in place when the `using` was parsed.
        //
        // Subsequent lookup in that scope will walk through our new
        // sub-scope and see the namespace.
        //
        // TODO: If we update the `containerDecl` in a scope to allow
        // for a more general `DeclRef`, or even a full `DeclRefExpr`,
        // then it would be possible for `using` to apply to more kinds
        // of entities than just namespaces.
        //
        auto scope = decl->scope;
        auto subScope = getASTBuilder()->create<Scope>();
        subScope->containerDecl = namespaceDecl;
        subScope->nextSibling = scope->nextSibling;
        scope->nextSibling = subScope;
    }

        /// Get a reference to the candidate extension list for `typeDecl` in the given dictionary
        ///
        /// Note: this function creates an empty list of candidates for the given type if
        /// a matching entry doesn't exist already.
        ///
    static List<ExtensionDecl*>& _getCandidateExtensionList(
        AggTypeDecl* typeDecl,
        Dictionary<AggTypeDecl*, RefPtr<CandidateExtensionList>>& mapTypeToCandidateExtensions)
    {
        RefPtr<CandidateExtensionList> entry;
        if( !mapTypeToCandidateExtensions.TryGetValue(typeDecl, entry) )
        {
            entry = new CandidateExtensionList();
            mapTypeToCandidateExtensions.Add(typeDecl, entry);
        }
        return entry->candidateExtensions;
    }

    List<ExtensionDecl*> const& SharedSemanticsContext::getCandidateExtensionsForTypeDecl(AggTypeDecl* decl)
    {
        // We are caching the lists of candidate extensions on the shared
        // context, so we will only build the lists if they either have
        // not been built before, or if some code caused the lists to
        // be invalidated.
        //
        // TODO: Similar to the rebuilding of lookup tables in `ContainerDecl`s,
        // we probably want to optimize this logic to gracefully handle new
        // extensions encountered during checking instead of tearing the whole
        // thing down. For now this potentially-quadratic behavior is acceptable
        // because there just aren't that many extension declarations being used.
        //
        if( !m_candidateExtensionListsBuilt )
        {
            m_candidateExtensionListsBuilt = true;

            // We need to make sure that all extensions that were declared
            // as part of our standard-library modules are always visible,
            // even if they are not explicit `import`ed into user code.
            //
            for( auto module : getSession()->stdlibModules )
            {
                _addCandidateExtensionsFromModule(module->getModuleDecl());
            }

            // There are two primary modes in which the `SharedSemanticsContext`
            // gets used.
            //
            // In the first mode, we are checking an entire `ModuelDecl`, and we
            // need to always check things from the "point of view" of that module
            // (so that the extensions that should be visible are based on what
            // that module can access via `import`s).
            //
            // In the second mode, we are checking code related to API interactions
            // by the user (e.g., parsing a type from a string, specializing an
            // entry point to type arguments, etc.). In these cases there is no
            // clear module that should determine the point of view for looking
            // up extensions, and we instead need/want to consider any extensions
            // from all modules loaded into the linkage.
            //
            // We differentiate these cases based on whether a "primary" module
            // was set at the time the `SharedSemanticsContext` was constructed.
            //
            if( m_module )
            {
                // We have a "primary" module that is being checked, and we should
                // look up extensions based on what would be visible to that
                // module.
                //
                // We need to consider the extensions declared in the module itself,
                // along with everything the module imported.
                //
                // Note: there is an implicit assumption here that the `importedModules`
                // member on the `SharedSemanticsContext` is accurate in this case.
                //
                _addCandidateExtensionsFromModule(m_module->getModuleDecl());
                for( auto moduleDecl : this->importedModulesList )
                {
                    _addCandidateExtensionsFromModule(moduleDecl);
                }
            }
            else
            {
                // We are in one of the many ad hoc checking modes where we really
                // want to resolve things based on the totality of what is
                // available/defined within the current linkage.
                //
                for( auto module : m_linkage->loadedModulesList )
                {
                    _addCandidateExtensionsFromModule(module->getModuleDecl());
                }
            }
        }

        // Once we are sure that the dictionary-of-arrays of extensions
        // has been populated, we return to the user the entry they
        // asked for.
        //
        return _getCandidateExtensionList(decl, m_mapTypeDeclToCandidateExtensions);
    }

    void SharedSemanticsContext::registerCandidateExtension(AggTypeDecl* typeDecl, ExtensionDecl* extDecl)
    {
        // The primary cache of extension declarations is on the `ModuleDecl`.
        // We will add the `extDecl` to the cache for the module it belongs to.
        //
        // We can be sure that the resulting cache won't have lifetime issues,
        // because all the extensions it contains are owned by the module itself,
        // and the types used as keys had to be reachable/referenceable from the
        // code inside the module for the given `extDecl` to extend them.
        //
        auto moduleDecl = getModuleDecl(extDecl);
        _getCandidateExtensionList(typeDecl, moduleDecl->mapTypeToCandidateExtensions).add(extDecl);

        // Because we've loaded a new extension, we need to invalidate whatever
        // information the `SharedSemanticsContext` had cached about loaded
        // extensions, and force it to rebuild its cache to include the
        // new extension we just added.
        //
        // TODO: We should probably just go ahead and add `extDecl` directly
        // into the appropriate entry here, and do a similar step on each
        // `import`.
        //
        m_candidateExtensionListsBuilt = false;
        m_mapTypeDeclToCandidateExtensions.Clear();
    }

    void SharedSemanticsContext::_addCandidateExtensionsFromModule(ModuleDecl* moduleDecl)
    {
        for( auto& entry : moduleDecl->mapTypeToCandidateExtensions )
        {
            auto& list = _getCandidateExtensionList(entry.Key, m_mapTypeDeclToCandidateExtensions);
            list.addRange(entry.Value->candidateExtensions);
        }
    }

    List<ExtensionDecl*> const& getCandidateExtensions(
        DeclRef<AggTypeDecl> const& declRef,
        SemanticsVisitor*           semantics)
    {
        auto decl = declRef.getDecl();
        auto shared = semantics->getShared();
        return shared->getCandidateExtensionsForTypeDecl(decl);
    }

    void _foreachDirectOrExtensionMemberOfType(
        SemanticsVisitor*               semantics,
        DeclRef<ContainerDecl> const&   containerDeclRef,
        SyntaxClassBase const&          syntaxClass,
        void                            (*callback)(DeclRefBase, void*),
        void const*                     userData)
    {
        // We are being asked to invoke the given callback on
        // each direct member of `containerDeclRef`, along with
        // any members added via `extension` declarations, that
        // have the correct AST node class (`syntaxClass`).
        //
        // We start with the direct members.
        //
        for( auto memberDeclRef : getMembers(containerDeclRef) )
        {
            if( memberDeclRef.decl->getClass().isSubClassOfImpl(syntaxClass) )
            {
                callback(memberDeclRef, (void*)userData);
            }
        }

        // Next, in the case wher ethe type can be subject to extensions,
        // we loop over the applicable extensions and their member.s
        //
        if(auto aggTypeDeclRef = containerDeclRef.as<AggTypeDecl>())
        {
            auto aggType = DeclRefType::create(semantics->getASTBuilder(), aggTypeDeclRef);
            for(auto extDecl : getCandidateExtensions(aggTypeDeclRef, semantics))
            {
                // Note that `extDecl` may have been declared for a type
                // base on the declaration that `aggTypeDeclRef` refers
                // to, but that does not guarantee that it applies to
                // the type itself. E.g., we might have an extension of
                // `vector<float, N>` for any `N`, but the current type is
                // `vector<int, 2>` so that the extension doesn't match.
                //
                // In order to make sure that we don't enumerate members
                // that don't make sense in context, we must apply
                // the extension to the type and see if we succeed in
                // making a match.
                //
                auto extDeclRef = ApplyExtensionToType(semantics, extDecl, aggType);
                if(!extDeclRef)
                    continue;

                for( auto memberDeclRef : getMembers(extDeclRef) )
                {
                    if( memberDeclRef.decl->getClass().isSubClassOfImpl(syntaxClass) )
                    {
                        callback(memberDeclRef, (void*)userData);
                    }
                }
            }
        }
    }


    static void _dispatchDeclCheckingVisitor(Decl* decl, DeclCheckState state, SemanticsContext const& shared)
    {
        switch(state)
        {
        case DeclCheckState::ModifiersChecked:
            SemanticsDeclModifiersVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::SignatureChecked:
            SemanticsDeclHeaderVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForReference:
            SemanticsDeclRedeclarationVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForLookup:
            SemanticsDeclBasesVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::ReadyForConformances:
            SemanticsDeclConformancesVisitor(shared).dispatch(decl);
            break;

        case DeclCheckState::Checked:
            SemanticsDeclBodyVisitor(shared).dispatch(decl);
            break;
        }
    }

    static void _getCanonicalConstraintTypes(List<Type*>& outTypeList, Type* type)
    {
        if (auto andType = as<AndType>(type))
        {
            _getCanonicalConstraintTypes(outTypeList, andType->left);
            _getCanonicalConstraintTypes(outTypeList, andType->right);
        }
        else
        {
            outTypeList.add(type);
        }
    }
    OrderedDictionary<GenericTypeParamDecl*, List<Type*>> getCanonicalGenericConstraints(
        DeclRef<ContainerDecl> genericDecl)
    {
        OrderedDictionary<GenericTypeParamDecl*, List<Type*>> genericConstraints;
        for (auto mm : getMembersOfType<GenericTypeParamDecl>(genericDecl))
        {
            genericConstraints[mm.getDecl()] = List<Type*>();
        }
        for (auto genericTypeConstraintDecl : getMembersOfType<GenericTypeConstraintDecl>(genericDecl))
        {
            assert(
                genericTypeConstraintDecl.getDecl()->sub.type->astNodeType ==
                ASTNodeType::DeclRefType);
            auto typeParamDecl = as<DeclRefType>(genericTypeConstraintDecl.getDecl()->sub.type)->declRef.getDecl();
            List<Type*>* constraintTypes = genericConstraints.TryGetValue(typeParamDecl);
            assert(constraintTypes);
            constraintTypes->add(genericTypeConstraintDecl.getDecl()->getSup().type);
        }

        OrderedDictionary<GenericTypeParamDecl*, List<Type*>> result;
        for (auto& constraints : genericConstraints)
        {
            List<Type*> typeList;
            for (auto type : constraints.Value)
            {
                _getCanonicalConstraintTypes(typeList, type);
            }
            // TODO: we also need to sort the types within the list for each generic type param.
            result[constraints.Key] = typeList;
        }
        return result;
    }

}
