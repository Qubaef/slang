// slang-artifact-handler-impl.cpp
#include "slang-artifact-handler-impl.h"

#include "slang-artifact-impl.h"
#include "slang-artifact-representation-impl.h"

#include "slang-artifact-desc-util.h"

#include "slang-artifact-helper.h"
#include "slang-artifact-util.h"

#include "../core/slang-castable-util.h"

#include "slang-slice-allocator.h"

#include "../core/slang-file-system.h"
#include "../core/slang-io.h"
#include "../core/slang-shared-library.h"

namespace Slang {

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! DefaultArtifactHandler !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

/* static */DefaultArtifactHandler DefaultArtifactHandler::g_singleton;

SlangResult DefaultArtifactHandler::queryInterface(SlangUUID const& uuid, void** outObject)
{
	if (auto ptr = getInterface(uuid))
	{
		addRef();
		*outObject = static_cast<IArtifactHandler*>(this);
		return SLANG_OK;
	}
	return SLANG_E_NO_INTERFACE;
}

void* DefaultArtifactHandler::castAs(const Guid& guid)
{
	if (auto ptr = getInterface(guid))
	{
		return ptr;
	}
	return getObject(guid);
}

void* DefaultArtifactHandler::getInterface(const Guid& uuid)
{
	if (uuid == ISlangUnknown::getTypeGuid() ||
		uuid == ICastable::getTypeGuid() ||
		uuid == IArtifactHandler::getTypeGuid())
	{
		return static_cast<IArtifactHandler*>(this);
	}

	return nullptr;
}

void* DefaultArtifactHandler::getObject(const Guid& uuid)
{
	SLANG_UNUSED(uuid);
	return nullptr;
}

SlangResult DefaultArtifactHandler::_addRepresentation(IArtifact* artifact, ArtifactKeep keep, ISlangUnknown* rep, ICastable** outCastable)
{
	SLANG_ASSERT(rep);

	// See if it implements ICastable
	{
		ComPtr<ICastable> castable;
		if (SLANG_SUCCEEDED(rep->queryInterface(ICastable::getTypeGuid(), (void**)castable.writeRef())) && castable)
		{
			return _addRepresentation(artifact, keep, castable, outCastable);
		}
	}

	// We have to wrap 
	ComPtr<IUnknownCastableAdapter> adapter(new UnknownCastableAdapter(rep));
	return _addRepresentation(artifact, keep, adapter, outCastable);
}

SlangResult DefaultArtifactHandler::_addRepresentation(IArtifact* artifact, ArtifactKeep keep, ICastable* castable, ICastable** outCastable)
{
	SLANG_ASSERT(castable);

	if (canKeep(keep))
	{
		artifact->addRepresentation(castable);
	}

	castable->addRef();
	*outCastable = castable;
	return SLANG_OK;
}

SlangResult DefaultArtifactHandler::expandChildren(IArtifactContainer* container)
{
	SlangResult res = container->getExpandChildrenResult();
	if (res != SLANG_E_UNINITIALIZED)
	{
		// It's already expanded
		return res;
	}

	// For the generic container type, we just expand as empty
	const auto desc = container->getDesc();
	if (desc.kind == ArtifactKind::Container)
	{
		container->setChildren(nullptr, 0);
		return SLANG_OK;
	}
	// TODO(JS):
	// Proper implementation should (for example) be able to expand a Zip file etc.
	return SLANG_E_NOT_IMPLEMENTED;
}

SlangResult DefaultArtifactHandler::getOrCreateRepresentation(IArtifact* artifact, const Guid& guid, ArtifactKeep keep, ICastable** outCastable)
{
	const auto reps = artifact->getRepresentations();
	
	// See if we already have a rep of this type
	for (ICastable* rep : reps)
	{
		if (rep->castAs(guid))
		{
			rep->addRef();
			*outCastable = rep;
			return SLANG_OK;
		}
	}
	
	// We can ask each representation if they can do the conversion to the type, if they can we just use that
	for (ICastable* castable : reps)
	{
		if (auto rep = as<IArtifactRepresentation>(castable))
		{
			ComPtr<ICastable> created;
			if (SLANG_SUCCEEDED(rep->createRepresentation(guid, created.writeRef())))
			{
				SLANG_ASSERT(created);
				// Add the rep
				return _addRepresentation(artifact, keep, created, outCastable);
			}
		}
	}

	// Special case shared library
	if (guid == ISlangSharedLibrary::getTypeGuid())
	{
		ComPtr<ISlangSharedLibrary> sharedLib;
		SLANG_RETURN_ON_FAIL(_loadSharedLibrary(artifact, sharedLib.writeRef()));
		return _addRepresentation(artifact, keep, sharedLib, outCastable);
	}
	else if (guid == IOSFileArtifactRepresentation::getTypeGuid())
	{
		ComPtr<IOSFileArtifactRepresentation> fileRep;
		SLANG_RETURN_ON_FAIL(_createOSFile(artifact, getIntermediateKeep(keep), fileRep.writeRef()));
		return _addRepresentation(artifact, keep, fileRep, outCastable);
	}

	return SLANG_E_NOT_AVAILABLE;
}

SlangResult DefaultArtifactHandler::_createOSFile(IArtifact* artifact, ArtifactKeep keep, IOSFileArtifactRepresentation** outFileRep)
{
	// Look if we have an IExtFile representation, as we might already have a OS file associated with that
	if (auto extRep = findRepresentation<IExtFileArtifactRepresentation>(artifact))
	{
		auto system = extRep->getFileSystem();

		String path;
		switch (system->getOSPathKind())
		{
			case OSPathKind::Direct:
			{
				path = UnownedStringSlice(extRep->getPath());
				break;
			}
			case OSPathKind::Canonical:
			{
				ComPtr<ISlangBlob> canonicalPathBlob;
				if (SLANG_SUCCEEDED(system->getCanonicalPath(extRep->getPath(), canonicalPathBlob.writeRef())))
				{
					path = StringUtil::getString(canonicalPathBlob);
				}
				break;
			}
			default: break;
		}

		if (path.getLength())
		{
			auto fileRep = OSFileArtifactRepresentation::create(IOSFileArtifactRepresentation::Kind::Reference, path.getUnownedSlice() , nullptr);
			// As a sanity check make sure it exists!
			if (fileRep->exists())
			{
				*outFileRep = fileRep.detach();
				return SLANG_OK;
			}
		}
	}

	// Okay looks like we will need to generate a temporary file
	auto helper = DefaultArtifactHelper::getSingleton();

	// If we are going to access as a file we need to be able to write it, and to do that we need a blob
	ComPtr<ISlangBlob> blob;
	SLANG_RETURN_ON_FAIL(artifact->loadBlob(getIntermediateKeep(keep), blob.writeRef()));

	// Find some name associated 
	auto name = ArtifactUtil::findName(artifact);
	if (name.getLength() == 0)
	{
		name = toSlice("unknown");
	}

	// Okay we need to store as a temporary. Get a lock file.
	ComPtr<IOSFileArtifactRepresentation> lockFile;
	SLANG_RETURN_ON_FAIL(helper->createLockFile(asCharSlice(name), lockFile.writeRef()));

	// Now we need the appropriate name for this item
	ComPtr<ISlangBlob> pathBlob;
	SLANG_RETURN_ON_FAIL(helper->calcArtifactPath(artifact, lockFile->getPath(), pathBlob.writeRef()));

	const auto path = StringUtil::getSlice(pathBlob);

	// Write the contents
	SLANG_RETURN_ON_FAIL(File::writeAllBytes(path, blob->getBufferPointer(), blob->getBufferSize()));

	ComPtr<IOSFileArtifactRepresentation> fileRep;

	// TODO(JS): This path comparison is perhaps not perfect, in that it assumes the path is not changed
	// in any way. For example an impl of calcArtifactPath that changed slashes or used a canonical path
	// might mean the lock file and the rep have the same path. 
	// As it stands calcArtifactPath impl doesn't do that, but that is perhaps somewhatfragile

	// If the paths are identical, we can just use the lock file for the rep
	if (UnownedStringSlice(lockFile->getPath()) == path)
	{
		fileRep.swap(lockFile);
	}
	else
	{
		// Create a new rep that references the lock file
		fileRep = new OSFileArtifactRepresentation(IOSFileArtifactRepresentation::Kind::Owned, path, lockFile);
	}

	// Return the file
	*outFileRep = fileRep.detach();
	return SLANG_OK;
}

SlangResult DefaultArtifactHandler::_loadSharedLibrary(IArtifact* artifact, ISlangSharedLibrary** outSharedLibrary)
{
	// If it is 'shared library' for a CPU like thing, we can try and load it
	const auto desc = artifact->getDesc();
	if ((isDerivedFrom(desc.kind, ArtifactKind::HostCallable) ||
		isDerivedFrom(desc.kind, ArtifactKind::SharedLibrary)) &&
		isDerivedFrom(desc.payload, ArtifactPayload::CPULike))
	{
		// Get as a file represenation on the OS file system
		ComPtr<IOSFileArtifactRepresentation> fileRep;

		// We want to keep the file representation, otherwise every request, could produce a new file
		// and that seems like a bad idea.
		SLANG_RETURN_ON_FAIL(artifact->requireFile(ArtifactKeep::Yes, fileRep.writeRef()));

		// Try loading the shared library
		SharedLibrary::Handle handle;
		if (SLANG_FAILED(SharedLibrary::loadWithPlatformPath(fileRep->getPath(), handle)))
		{
			return SLANG_FAIL;
		}

		// Output
		*outSharedLibrary = ScopeSharedLibrary::create(handle, fileRep).detach();
		return SLANG_OK;
	}

	return SLANG_FAIL;
}

} // namespace Slang
