//------------------------------------------------------------------------------
//! \file       ARAIPCProxyPlugIn.cpp
//!             implementation of host-side ARA IPC proxy plug-in
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2022, Celemony Software GmbH, All Rights Reserved.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------

#include "ARAIPCProxyPlugIn.h"
#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

namespace ARA {
namespace ProxyPlugIn {

/*******************************************************************************/
// configuration switches for debug output
// each can be defined as a nonzero integer to enable the associated logging

// log each entry from the host into the document controller (except for notifyModelUpdates (), which is called too often)
#ifndef ARA_ENABLE_HOST_ENTRY_LOG
    #define ARA_ENABLE_HOST_ENTRY_LOG 0
#endif

// log the creation and destruction of plug-in objects
#ifndef ARA_ENABLE_OBJECT_LIFETIME_LOG
    #define ARA_ENABLE_OBJECT_LIFETIME_LOG 0
#endif

// conditional logging helper functions based on the above switches
#if ARA_ENABLE_HOST_ENTRY_LOG
    #define ARA_LOG_HOST_ENTRY(object) ARA_LOG ("Host calls into %s (%p)", __FUNCTION__, object);
#else
    #define ARA_LOG_HOST_ENTRY(object) ((void) 0)
#endif

#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    #define ARA_LOG_MODELOBJECT_LIFETIME(message, object)  ARA_LOG ("Plug success: document controller %p %s %p", object->getDocumentController (), message, object)
#else
    #define ARA_LOG_MODELOBJECT_LIFETIME(message, object) ((void) 0)
#endif

/*******************************************************************************/

struct AudioSource : public InstanceValidator<AudioSource>
{
    AudioSource (ARAAudioSourceHostRef hostRef, ARAAudioSourceRef remoteRef, ARAChannelCount channelCount
#if ARA_VALIDATE_API_CALLS
                 , ARASampleCount sampleCount, ARASampleRate sampleRate
#endif
                 )
    : _hostRef { hostRef }, _remoteRef { remoteRef }, _channelCount { channelCount }
#if ARA_VALIDATE_API_CALLS
    , _sampleCount { sampleCount }, _sampleRate { sampleRate }
#endif
    {}

    ARAAudioSourceHostRef _hostRef;
    ARAAudioSourceRef _remoteRef;
    ARAChannelCount _channelCount;
#if ARA_VALIDATE_API_CALLS
    ARASampleCount _sampleCount;
    ARASampleRate _sampleRate;
#endif
};
ARA_MAP_REF (AudioSource, ARAAudioSourceRef)
ARA_MAP_HOST_REF (AudioSource, ARAAudioSourceHostRef)

struct ContentReader : public InstanceValidator<ContentReader>
{
    ContentReader (ARAContentReaderRef remoteRef, ARAContentType type)
    : _remoteRef { remoteRef }, _decoder { type }
    {}

    ARAContentReaderRef _remoteRef;
    ARAIPCContentEventDecoder _decoder;
};
ARA_MAP_REF (ContentReader, ARAContentReaderRef)

struct HostContentReader
{
    ARAContentReaderHostRef hostRef;
    ARAContentType contentType;
};
ARA_MAP_HOST_REF (HostContentReader, ARAContentReaderHostRef)

struct HostAudioReader
{
    AudioSource* audioSource;
    ARAAudioReaderHostRef hostRef;
    ARABool use64BitSamples;
};
ARA_MAP_HOST_REF (HostAudioReader, ARAAudioReaderHostRef)

/*******************************************************************************/

#if ARA_VALIDATE_API_CALLS
template<typename SubclassT>
std::set<const InstanceValidator<SubclassT>*> InstanceValidator<SubclassT>::_instances;

template<typename SubclassT>
inline bool isValidInstance (const SubclassT* instance)
{
    return InstanceValidator<SubclassT>::isValid (instance);
}
#endif

/*******************************************************************************/

DocumentController::DocumentController (IPCPort& port, const ARAFactory* factory, const ARADocumentControllerHostInstance* instance, const ARADocumentProperties* properties) noexcept
: ARAIPCMessageSender { port },
  _factory { factory },
  _hostAudioAccessController { instance },
  _hostArchivingController { instance },
  _hostContentAccessController { instance },
  _hostModelUpdateController { instance },
  _hostPlaybackController { instance },
  _instance { this }
{
    ARAAudioAccessControllerHostRef audioAccessControllerHostRef { toHostRef (this) };
    ARAArchivingControllerHostRef archivingControllerHostRef { toHostRef (this) };
    ARAContentAccessControllerHostRef contentAccessControllerHostRef { toHostRef (this) };
    ARAModelUpdateControllerHostRef modelUpdateControllerHostRef { toHostRef (this) };
    ARAPlaybackControllerHostRef playbackControllerHostRef { toHostRef (this) };
    remoteCallWithReply (_remoteRef, kCreateDocumentControllerMethodID,
                          audioAccessControllerHostRef, archivingControllerHostRef,
                          (_hostContentAccessController.isProvided ()) ? kARATrue : kARAFalse, contentAccessControllerHostRef,
                          (_hostModelUpdateController.isProvided ()) ? kARATrue : kARAFalse, modelUpdateControllerHostRef,
                          (_hostPlaybackController.isProvided ()) ? kARATrue : kARAFalse, playbackControllerHostRef,
                          properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create document controller", _remoteRef);
}

void DocumentController::destroyDocumentController () noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy document controller", _remoteRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyDocumentController), _remoteRef);

    _hasBeenDestroyed = true;

    destroyIfUnreferenced ();
}

void DocumentController::destroyIfUnreferenced () noexcept
{
    // still in use by host?
    if (!_hasBeenDestroyed)
        return;

    // still referenced from plug-in instances?
    if (!_plugInExtensions.empty ())
        return;

    delete this;
}

/*******************************************************************************/

const ARAFactory* DocumentController::getFactory () const noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    return _factory;
}

/*******************************************************************************/

void DocumentController::beginEditing () noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, beginEditing), _remoteRef);
}

void DocumentController::endEditing () noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, endEditing), _remoteRef);
}

void DocumentController::notifyModelUpdates () noexcept
{
#if ARA_ENABLE_HOST_ENTRY_LOG
    static int logCount { 0 };
    constexpr int maxLogCount { 3 };
    if ((++logCount) <= maxLogCount)
    {
        ARA_LOG_HOST_ENTRY (this);
        if (logCount >= maxLogCount)
            ARA_LOG ("notifyModelUpdates () called %i times, will now suppress logging future calls to it", maxLogCount);
    }
#endif
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    if (!_hostModelUpdateController.isProvided ())
        return;

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, notifyModelUpdates), _remoteRef);
}

bool DocumentController::restoreObjectsFromArchive (ARAArchiveReaderHostRef archiveReaderHostRef, const ARARestoreObjectsFilter* filter) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARABool success;
    remoteCallWithReply (success, PLUGIN_METHOD_ID (ARADocumentControllerInterface, restoreObjectsFromArchive), _remoteRef, archiveReaderHostRef, filter);
    return (success != kARAFalse);
}

bool DocumentController::storeObjectsToArchive (ARAArchiveWriterHostRef archiveWriterHostRef, const ARAStoreObjectsFilter* filter) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAStoreObjectsFilter tempFilter;
    std::vector<ARAAudioSourceRef> remoteAudioSourceRefs;
    if ((filter != nullptr) && (filter->audioSourceRefsCount > 0))
    {
        remoteAudioSourceRefs.reserve (filter->audioSourceRefsCount);
        for (auto i { 0U }; i < filter->audioSourceRefsCount; ++i)
            remoteAudioSourceRefs.emplace_back (fromRef (filter->audioSourceRefs[i])->_remoteRef);

        tempFilter = *filter;
        tempFilter.audioSourceRefs = remoteAudioSourceRefs.data ();
        filter = &tempFilter;
    }

    ARABool success;
    remoteCallWithReply (success, PLUGIN_METHOD_ID (ARADocumentControllerInterface, storeObjectsToArchive), _remoteRef, archiveWriterHostRef, filter);
    return (success!= kARAFalse);
}

bool DocumentController::storeAudioSourceToAudioFileChunk (ARAArchiveWriterHostRef archiveWriterHostRef, ARAAudioSourceRef audioSourceRef, ARAPersistentID* documentArchiveID, bool* openAutomatically) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));
    ARA_VALIDATE_API_ARGUMENT (documentArchiveID, documentArchiveID != nullptr);
    ARA_VALIDATE_API_ARGUMENT (openAutomatically, openAutomatically != nullptr);

    // keep local copy of message before decoding it so that all pointer data remains valid until properly copied
    IPCMessage replyMsg;
    remoteCallWithReply (replyMsg, PLUGIN_METHOD_ID (ARADocumentControllerInterface, storeAudioSourceToAudioFileChunk),
                        _remoteRef, archiveWriterHostRef, audioSource->_remoteRef);
    ARAIPCStoreAudioSourceToAudioFileChunkReply reply;
    decodeReply (reply, replyMsg);

    // find ID string in factory because our return value is a temporary copy
    if (0 == std::strcmp (reply.documentArchiveID, _factory->documentArchiveID))
    {
        *documentArchiveID = _factory->documentArchiveID;
    }
    else
    {
        *documentArchiveID = nullptr;
        for (auto i { 0U }; i < _factory->compatibleDocumentArchiveIDsCount; ++i)
        {
            if (0 == std::strcmp (reply.documentArchiveID, _factory->compatibleDocumentArchiveIDs[i]))
            {
                *documentArchiveID = _factory->compatibleDocumentArchiveIDs[i];
                break;
            }
        }
        ARA_INTERNAL_ASSERT(*documentArchiveID != nullptr);
    }

    *openAutomatically = (reply.openAutomatically != kARAFalse);
    return (reply.result != kARAFalse);
}

void DocumentController::updateDocumentProperties (PropertiesPtr<ARADocumentProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARADocumentProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateDocumentProperties), _remoteRef, *properties);
}

/*******************************************************************************/

ARAMusicalContextRef DocumentController::createMusicalContext (ARAMusicalContextHostRef hostRef, PropertiesPtr<ARAMusicalContextProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAMusicalContextProperties);

    ARAMusicalContextRef musicalContextRef;
    remoteCallWithReply (musicalContextRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createMusicalContext), _remoteRef, hostRef, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create musical context", musicalContextRef);
    return musicalContextRef;
}

void DocumentController::updateMusicalContextProperties (ARAMusicalContextRef musicalContextRef, PropertiesPtr<ARAMusicalContextProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (musicalContextRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAMusicalContextProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateMusicalContextProperties), _remoteRef, musicalContextRef, *properties);
}

void DocumentController::updateMusicalContextContent (ARAMusicalContextRef musicalContextRef, const ARAContentTimeRange* range, ContentUpdateScopes flags) noexcept
{
    ARA_LOG_HOST_ENTRY (musicalContextRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateMusicalContextContent), _remoteRef, musicalContextRef, range, flags);
}

void DocumentController::destroyMusicalContext (ARAMusicalContextRef musicalContextRef) noexcept
{
    ARA_LOG_HOST_ENTRY (musicalContextRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy musical context", musicalContextRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyMusicalContext), _remoteRef, musicalContextRef);
}

/*******************************************************************************/

ARARegionSequenceRef DocumentController::createRegionSequence (ARARegionSequenceHostRef hostRef, PropertiesPtr<ARARegionSequenceProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARARegionSequenceProperties);

    ARARegionSequenceRef regionSequenceRef;
    remoteCallWithReply (regionSequenceRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createRegionSequence), _remoteRef, hostRef, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create region sequence", regionSequenceRef);
    return regionSequenceRef;
}

void DocumentController::updateRegionSequenceProperties (ARARegionSequenceRef regionSequenceRef, PropertiesPtr<ARARegionSequenceProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (regionSequenceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARARegionSequenceProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateRegionSequenceProperties), _remoteRef, regionSequenceRef, *properties);
}

void DocumentController::destroyRegionSequence (ARARegionSequenceRef regionSequenceRef) noexcept
{
    ARA_LOG_HOST_ENTRY (regionSequenceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy region sequence", regionSequenceRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyRegionSequence), _remoteRef, regionSequenceRef);
}

/*******************************************************************************/

ARAAudioSourceRef DocumentController::createAudioSource (ARAAudioSourceHostRef hostRef, PropertiesPtr<ARAAudioSourceProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAAudioSourceProperties);

    auto audioSource { new AudioSource { hostRef, nullptr, properties->channelCount
#if ARA_VALIDATE_API_CALLS
                                                , properties->sampleCount, properties->sampleRate
#endif
                                        } };

    remoteCallWithReply (audioSource->_remoteRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSource),
                        _remoteRef, ARAAudioSourceHostRef { toHostRef (audioSource) }, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create audio source", audioSourceRef);
    return toRef (audioSource);
}

void DocumentController::updateAudioSourceProperties (ARAAudioSourceRef audioSourceRef, PropertiesPtr<ARAAudioSourceProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAAudioSourceProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioSourceProperties), _remoteRef, audioSource->_remoteRef, *properties);
}

void DocumentController::updateAudioSourceContent (ARAAudioSourceRef audioSourceRef, const ARAContentTimeRange* range, ContentUpdateScopes flags) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioSourceContent), _remoteRef, audioSource->_remoteRef, range, flags);
}

void DocumentController::enableAudioSourceSamplesAccess (ARAAudioSourceRef audioSourceRef, bool enable) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, enableAudioSourceSamplesAccess), _remoteRef, audioSource->_remoteRef, (enable) ? kARATrue : kARAFalse);
}

void DocumentController::deactivateAudioSourceForUndoHistory (ARAAudioSourceRef audioSourceRef, bool deactivate) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, deactivateAudioSourceForUndoHistory), _remoteRef, audioSource->_remoteRef, (deactivate) ? kARATrue : kARAFalse);
}

void DocumentController::destroyAudioSource (ARAAudioSourceRef audioSourceRef) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy audio source", audioSource->_remoteRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioSource), _remoteRef, audioSource->_remoteRef);
    delete audioSource;
}

/*******************************************************************************/

ARAAudioModificationRef DocumentController::createAudioModification (ARAAudioSourceRef audioSourceRef, ARAAudioModificationHostRef hostRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAAudioModificationProperties);

    ARAAudioModificationRef audioModificationRef;
    remoteCallWithReply (audioModificationRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioModification),
                        _remoteRef, audioSource->_remoteRef, hostRef, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create audio modification", audioModificationRef);
    return audioModificationRef;
}

ARAAudioModificationRef DocumentController::cloneAudioModification (ARAAudioModificationRef srcAudioModificationRef, ARAAudioModificationHostRef hostRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (srcAudioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAAudioModificationProperties);

    ARAAudioModificationRef clonedAudioModificationRef;
    remoteCallWithReply (clonedAudioModificationRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, cloneAudioModification),
                        _remoteRef, srcAudioModificationRef, hostRef, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create cloned audio modification", clonedAudioModificationRef);
    return clonedAudioModificationRef;
}

void DocumentController::updateAudioModificationProperties (ARAAudioModificationRef audioModificationRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAAudioModificationProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioModificationProperties), _remoteRef, audioModificationRef, *properties);
}

bool DocumentController::isAudioModificationPreservingAudioSourceSignal (ARAAudioModificationRef audioModificationRef) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioModificationPreservingAudioSourceSignal), _remoteRef, audioModificationRef);
    return (result != kARAFalse);
}

void DocumentController::deactivateAudioModificationForUndoHistory (ARAAudioModificationRef audioModificationRef, bool deactivate) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, deactivateAudioModificationForUndoHistory), _remoteRef, audioModificationRef, (deactivate) ? kARATrue : kARAFalse);
}

void DocumentController::destroyAudioModification (ARAAudioModificationRef audioModificationRef) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy audio modification", audioModification);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioModification), _remoteRef, audioModificationRef);
}

/*******************************************************************************/

ARAPlaybackRegionRef DocumentController::createPlaybackRegion (ARAAudioModificationRef audioModificationRef, ARAPlaybackRegionHostRef hostRef, PropertiesPtr<ARAPlaybackRegionProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAPlaybackRegionProperties);

    ARAPlaybackRegionRef playbackRegionRef;
    remoteCallWithReply (playbackRegionRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createPlaybackRegion),
                        _remoteRef, audioModificationRef, hostRef, *properties);

    ARA_LOG_MODELOBJECT_LIFETIME ("did create playback region", playbackRegionRef);
    return playbackRegionRef;
}

void DocumentController::updatePlaybackRegionProperties (ARAPlaybackRegionRef playbackRegionRef, PropertiesPtr<ARAPlaybackRegionProperties> properties) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (properties, ARAPlaybackRegionProperties);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, updatePlaybackRegionProperties), _remoteRef, playbackRegionRef, *properties);
}

void DocumentController::getPlaybackRegionHeadAndTailTime (ARAPlaybackRegionRef playbackRegionRef, ARATimeDuration* headTime, ARATimeDuration* tailTime) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_ARGUMENT (headTime, headTime != nullptr);
    ARA_VALIDATE_API_ARGUMENT (tailTime, tailTime != nullptr);

    ARAIPCGetPlaybackRegionHeadAndTailTimeReply reply;
    remoteCallWithReply (reply, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getPlaybackRegionHeadAndTailTime),
                        _remoteRef, playbackRegionRef, (headTime != nullptr) ? kARATrue : kARAFalse, (tailTime != nullptr) ? kARATrue : kARAFalse);
    if (headTime != nullptr)
        *headTime = reply.headTime;
    if (tailTime != nullptr)
        *tailTime = reply.tailTime;
}

void DocumentController::destroyPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy playback region", playbackRegionRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyPlaybackRegion), _remoteRef, playbackRegionRef);
}

/*******************************************************************************/

bool DocumentController::isAudioSourceContentAvailable (ARAAudioSourceRef audioSourceRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAvailable), _remoteRef, audioSource->_remoteRef, type);
    return (result != kARAFalse);
}

ARAContentGrade DocumentController::getAudioSourceContentGrade (ARAAudioSourceRef audioSourceRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARAContentGrade grade;
    remoteCallWithReply (grade, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getAudioSourceContentGrade), _remoteRef, audioSource->_remoteRef, type);
    return grade;
}

ARAContentReaderRef DocumentController::createAudioSourceContentReader (ARAAudioSourceRef audioSourceRef, ARAContentType type, const ARAContentTimeRange* range) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARAContentReaderRef contentReaderRef;
    remoteCallWithReply (contentReaderRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSourceContentReader),
                        _remoteRef, audioSource->_remoteRef, type, range);

    auto contentReader { new ContentReader { contentReaderRef, type } };
#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    ARA_LOG ("Plug success: did create content reader %p for audio source %p", contentReaderRef, audioSourceRef);
#endif
    return toRef (contentReader);
}

/*******************************************************************************/

bool DocumentController::isAudioModificationContentAvailable (ARAAudioModificationRef audioModificationRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioModificationContentAvailable), _remoteRef, audioModificationRef, type);
    return (result != kARAFalse);
}

ARAContentGrade DocumentController::getAudioModificationContentGrade (ARAAudioModificationRef audioModificationRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAContentGrade grade;
    remoteCallWithReply (grade, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getAudioModificationContentGrade), _remoteRef, audioModificationRef, type);
    return grade;
}

ARAContentReaderRef DocumentController::createAudioModificationContentReader (ARAAudioModificationRef audioModificationRef, ARAContentType type, const ARAContentTimeRange* range) noexcept
{
    ARA_LOG_HOST_ENTRY (audioModificationRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAContentReaderRef contentReaderRef;
    remoteCallWithReply (contentReaderRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioModificationContentReader),
                        _remoteRef, audioModificationRef, type, range);

    auto contentReader { new ContentReader { contentReaderRef, type } };
#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    ARA_LOG ("Plug success: did create content reader %p for audio modification %p", contentReaderRef, audioModificationRef);
#endif
    return toRef (contentReader);
}

/*******************************************************************************/

bool DocumentController::isPlaybackRegionContentAvailable (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isPlaybackRegionContentAvailable), _remoteRef, playbackRegionRef, type);
    return (result != kARAFalse);
}

ARAContentGrade DocumentController::getPlaybackRegionContentGrade (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAContentGrade grade;
    remoteCallWithReply (grade, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getPlaybackRegionContentGrade), _remoteRef, playbackRegionRef, type);
    return grade;
}

ARAContentReaderRef DocumentController::createPlaybackRegionContentReader (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type, const ARAContentTimeRange* range) noexcept
{
    ARA_LOG_HOST_ENTRY (playbackRegionRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAContentReaderRef contentReaderRef;
    remoteCallWithReply (contentReaderRef, PLUGIN_METHOD_ID (ARADocumentControllerInterface, createPlaybackRegionContentReader),
                        _remoteRef, playbackRegionRef, type, range);

    auto contentReader { new ContentReader { contentReaderRef, type } };
#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    ARA_LOG ("Plug success: did create content reader %p for playback region %p", contentReaderRef, playbackRegionRef);
#endif
    return toRef (contentReader);
}

/*******************************************************************************/

ARAInt32 DocumentController::getContentReaderEventCount (ARAContentReaderRef contentReaderRef) noexcept
{
    ARA_LOG_HOST_ENTRY (contentReaderRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto contentReader { fromRef (contentReaderRef) };
    ARA_VALIDATE_API_ARGUMENT (contentReader, isValidInstance (contentReader));

    ARAInt32 count;
    remoteCallWithReply (count, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderEventCount), _remoteRef, contentReader->_remoteRef);
    return count;
}

const void* DocumentController::getContentReaderDataForEvent (ARAContentReaderRef contentReaderRef, ARAInt32 eventIndex) noexcept
{
    ARA_LOG_HOST_ENTRY (contentReaderRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto contentReader { fromRef (contentReaderRef) };
    ARA_VALIDATE_API_ARGUMENT (contentReader, isValidInstance (contentReader));

    IPCMessage reply;
    remoteCallWithReply (reply, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderDataForEvent),
                        _remoteRef, contentReader->_remoteRef, eventIndex);
    return contentReader->_decoder.decode (reply);
}

void DocumentController::destroyContentReader (ARAContentReaderRef contentReaderRef) noexcept
{
    ARA_LOG_HOST_ENTRY (contentReaderRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto contentReader { fromRef (contentReaderRef) };
    ARA_VALIDATE_API_ARGUMENT (contentReader, isValidInstance (contentReader));

    ARA_LOG_MODELOBJECT_LIFETIME ("will destroy content reader", contentReader->remoteRef);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyContentReader), _remoteRef, contentReader->_remoteRef);

    delete contentReader;
}

/*******************************************************************************/

bool DocumentController::isAudioSourceContentAnalysisIncomplete (ARAAudioSourceRef audioSourceRef, ARAContentType type) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAnalysisIncomplete),
                        _remoteRef, audioSource->_remoteRef, type);
    return (result != kARAFalse);
}

void DocumentController::requestAudioSourceContentAnalysis (ARAAudioSourceRef audioSourceRef, ARASize contentTypesCount, const ARAContentType contentTypes[]) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    std::vector<ARAContentType> types;
    types.assign (contentTypes, contentTypes + contentTypesCount);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestAudioSourceContentAnalysis), _remoteRef, audioSource->_remoteRef, types);
}

ARAInt32 DocumentController::getProcessingAlgorithmsCount () noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    ARAInt32 count;
    remoteCallWithReply (count, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmsCount), _remoteRef);
    return count;
}

const ARAProcessingAlgorithmProperties* DocumentController::getProcessingAlgorithmProperties (ARAInt32 algorithmIndex) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    // keep local copy of message before decoding it so that all pointer data remains valid until properly copied
    IPCMessage replyMsg;
    remoteCallWithReply (replyMsg, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmProperties), _remoteRef, algorithmIndex);
    ARAProcessingAlgorithmProperties reply;
    decodeReply (reply, replyMsg);
    _processingAlgorithmStrings.persistentID = reply.persistentID;
    _processingAlgorithmStrings.name = reply.name;
    _processingAlgorithmData = reply;
    _processingAlgorithmData.persistentID = _processingAlgorithmStrings.persistentID.c_str ();
    _processingAlgorithmData.name = _processingAlgorithmStrings.name.c_str ();
    return &_processingAlgorithmData;
}

ARAInt32 DocumentController::getProcessingAlgorithmForAudioSource (ARAAudioSourceRef audioSourceRef) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    ARAInt32 result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmForAudioSource), _remoteRef, audioSource->_remoteRef);
    return result;
}

void DocumentController::requestProcessingAlgorithmForAudioSource (ARAAudioSourceRef audioSourceRef, ARAInt32 algorithmIndex) noexcept
{
    ARA_LOG_HOST_ENTRY (audioSourceRef);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    const auto audioSource { fromRef (audioSourceRef) };
    ARA_VALIDATE_API_ARGUMENT (audioSource, isValidInstance (audioSource));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestProcessingAlgorithmForAudioSource), _remoteRef, audioSource->_remoteRef, algorithmIndex);
}

/*******************************************************************************/

bool DocumentController::isLicensedForCapabilities (bool runModalActivationDialogIfNeeded, ARASize contentTypesCount, const ARAContentType contentTypes[], ARAPlaybackTransformationFlags transformationFlags) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    std::vector<ARAContentType> types;
    types.assign (contentTypes, contentTypes + contentTypesCount);
    ARABool result;
    remoteCallWithReply (result, PLUGIN_METHOD_ID (ARADocumentControllerInterface, isLicensedForCapabilities),
                        _remoteRef, (runModalActivationDialogIfNeeded) ? kARATrue : kARAFalse, types, transformationFlags);
    return (result != kARAFalse);
}

/*******************************************************************************/

PlaybackRenderer::PlaybackRenderer (IPCPort& port, ARAPlaybackRendererRef remoteRef) noexcept
: ARAIPCMessageSender { port },
  _remoteRef { remoteRef }
{}

void PlaybackRenderer::addPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAPlaybackRendererInterface, addPlaybackRegion), _remoteRef, playbackRegionRef);
}

void PlaybackRenderer::removePlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAPlaybackRendererInterface, removePlaybackRegion), _remoteRef, playbackRegionRef);
}

/*******************************************************************************/

EditorRenderer::EditorRenderer (IPCPort& port, ARAEditorRendererRef remoteRef) noexcept
: ARAIPCMessageSender { port },
  _remoteRef { remoteRef }
{}

void EditorRenderer::addPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorRendererInterface, addPlaybackRegion), _remoteRef, playbackRegionRef);
}

void EditorRenderer::removePlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorRendererInterface, removePlaybackRegion), _remoteRef, playbackRegionRef);
}

void EditorRenderer::addRegionSequence (ARARegionSequenceRef regionSequenceRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorRendererInterface, addRegionSequence), _remoteRef, regionSequenceRef);
}

void EditorRenderer::removeRegionSequence (ARARegionSequenceRef regionSequenceRef) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorRendererInterface, removeRegionSequence), _remoteRef, regionSequenceRef);
}

/*******************************************************************************/

EditorView::EditorView (IPCPort& port, ARAEditorViewRef remoteRef) noexcept
: ARAIPCMessageSender { port },
  _remoteRef { remoteRef }
{}

void EditorView::notifySelection (SizedStructPtr<ARAViewSelection> selection) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));
    ARA_VALIDATE_API_STRUCT_PTR (selection, ARAViewSelection);

    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorViewInterface, notifySelection), _remoteRef, *selection);
}

void EditorView::notifyHideRegionSequences (ARASize regionSequenceRefsCount, const ARARegionSequenceRef regionSequenceRefs[]) noexcept
{
    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (this, isValidInstance (this));

    std::vector<ARARegionSequenceRef> sequences;
    sequences.assign (regionSequenceRefs, regionSequenceRefs + regionSequenceRefsCount);
    remoteCallWithoutReply (PLUGIN_METHOD_ID (ARAEditorViewInterface, notifyHideRegionSequences), _remoteRef, sequences);
}

/*******************************************************************************/

PlugInExtension::PlugInExtension (IPCPort& port, ARADocumentControllerRef documentControllerRef,
                                  ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles,
                                  size_t remotePlugInExtensionRef) noexcept
: _documentController { PlugIn::fromRef<DocumentController> (documentControllerRef) },
  _instance { (((knownRoles & kARAPlaybackRendererRole) == 0) || ((assignedRoles & kARAPlaybackRendererRole) != 0)) ?
                    new PlaybackRenderer (port, reinterpret_cast<ARAPlaybackRendererRef> (remotePlugInExtensionRef)) : nullptr,
              (((knownRoles & kARAEditorRendererRole) == 0) || ((assignedRoles & kARAEditorRendererRole) != 0)) ?
                    new EditorRenderer (port, reinterpret_cast<ARAEditorRendererRef> (remotePlugInExtensionRef)) : nullptr,
              (((knownRoles & kARAEditorViewRole) == 0) || ((assignedRoles & kARAEditorViewRole) != 0)) ?
                    new EditorView (port, reinterpret_cast<ARAEditorViewRef> (remotePlugInExtensionRef)) : nullptr }
{
    _instance.plugInExtensionRef = reinterpret_cast<ARAPlugInExtensionRef> (remotePlugInExtensionRef);

    ARA_LOG_HOST_ENTRY (this);
    ARA_VALIDATE_API_ARGUMENT (documentControllerRef, isValidInstance (_documentController));

    _documentController->addPlugInExtension (this);

#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    ARA_LOG ("Plug success: did create plug-in extension %p (playbackRenderer %p, editorRenderer %p, editorView %p)", this, getPlaybackRenderer (), getEditorRenderer (), getEditorView ());
#endif
}

PlugInExtension::~PlugInExtension () noexcept
{
    ARA_LOG_HOST_ENTRY (this);
#if ARA_ENABLE_OBJECT_LIFETIME_LOG
    ARA_LOG ("Plug success: will destroy plug-in extension %p (playbackRenderer %p, editorRenderer %p, editorView %p)", this, getPlaybackRenderer (), getEditorRenderer (), getEditorView ());
#endif

    _documentController->removePlugInExtension (this);

    delete getEditorView ();
    delete getEditorRenderer ();
    delete getPlaybackRenderer ();
}

/*******************************************************************************/

Factory::Factory (IPCPort& hostCommandsPort)
: _hostCommandsPort { hostCommandsPort }
{
    // keep local copy of message before decoding it so that all pointer data remains valid until properly copied
    IPCMessage reply;
    ARA::ARAIPCMessageSender (_hostCommandsPort).remoteCallWithReply (reply, kGetFactoryMethodID);
    decodeReply (_factory, reply);

    ARA_VALIDATE_API_ARGUMENT(&_factory, _factory.highestSupportedApiGeneration >= kARAAPIGeneration_2_0_Final);

    _factoryStrings.factoryID = _factory.factoryID;
    _factory.factoryID = _factoryStrings.factoryID.c_str ();

    _factoryStrings.plugInName = _factory.plugInName;
    _factory.plugInName = _factoryStrings.plugInName.c_str ();
    _factoryStrings.manufacturerName = _factory.manufacturerName;
    _factory.manufacturerName = _factoryStrings.manufacturerName.c_str ();
    _factoryStrings.informationURL = _factory.informationURL;
    _factory.informationURL = _factoryStrings.informationURL.c_str ();
    _factoryStrings.version = _factory.version;
    _factory.version = _factoryStrings.version.c_str ();

    _factoryStrings.documentArchiveID = _factory.documentArchiveID;
    _factory.documentArchiveID = _factoryStrings.documentArchiveID.c_str ();

    _factoryCompatibleIDStrings.reserve (_factory.compatibleDocumentArchiveIDsCount);
    _factoryCompatibleIDs.reserve (_factory.compatibleDocumentArchiveIDsCount);
    for (auto i { 0U }; i < _factory.compatibleDocumentArchiveIDsCount; ++i)
    {
        _factoryCompatibleIDStrings.emplace_back (_factory.compatibleDocumentArchiveIDs[i]);
        _factoryCompatibleIDs.emplace_back (_factoryCompatibleIDStrings[i].c_str ());
    }
    _factory.compatibleDocumentArchiveIDs = _factoryCompatibleIDs.data ();

    _factoryAnalyzableTypes.reserve (_factory.analyzeableContentTypesCount);
    for (auto i { 0U }; i < _factory.analyzeableContentTypesCount; ++i)
        _factoryAnalyzableTypes.emplace_back (_factory.analyzeableContentTypes[i]);
    _factory.analyzeableContentTypes = _factoryAnalyzableTypes.data ();
}

const ARADocumentControllerInstance* Factory::createDocumentControllerWithDocument (const ARADocumentControllerHostInstance* hostInstance, const ARADocumentProperties* properties)
{
    auto result { new DocumentController { _hostCommandsPort, &_factory, hostInstance, properties } };
    return result->getInstance ();
}

std::unique_ptr<PlugInExtension> Factory::createPlugInExtension (size_t remoteExtensionRef, IPCPort& port, ARADocumentControllerRef documentControllerRef,
                                                                 ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles)
{
    return std::make_unique<PlugInExtension> (port, documentControllerRef, knownRoles, assignedRoles, remoteExtensionRef);
}

template<typename FloatT>
IPCMessage _readAudioSamples (DocumentController* documentController, HostAudioReader* reader,
                                 ARASamplePosition samplePosition, ARASampleCount samplesPerChannel)
{
    std::vector<ARAByte> bufferData;
    bufferData.resize (sizeof (FloatT) * static_cast<size_t> (reader->audioSource->_channelCount * samplesPerChannel));
    void* sampleBuffers[32];
    ARA_INTERNAL_ASSERT(reader->audioSource->_channelCount < 32);
    for (auto i { 0 }; i < reader->audioSource->_channelCount; ++i)
        sampleBuffers[i] = bufferData.data () + sizeof (FloatT) * static_cast<size_t> (i * samplesPerChannel);

    if (documentController->getHostAudioAccessController ()->readAudioSamples (reader->hostRef, samplePosition, samplesPerChannel, sampleBuffers))
        return encodeReply (bufferData);
    else
        return {};
}

IPCMessage Factory::plugInCallbacksDispatcher (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("_plugInCallbackDispatcher received message %s", decodeHostMethodID (messageID));

    // ARAAudioAccessControllerInterface
    if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, createAudioReaderForSource))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARABool use64BitSamples;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, use64BitSamples);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        auto reader { new HostAudioReader { audioSource, nullptr, use64BitSamples } };
        reader->hostRef = documentController->getHostAudioAccessController ()->createAudioReaderForSource (audioSource->_hostRef, (use64BitSamples) ? kARATrue : kARAFalse);
        ARAAudioReaderHostRef audioReaderHostRef { toHostRef (reader) };
        return encodeReply (audioReaderHostRef);
    }
    else if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, readAudioSamples))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioReaderHostRef audioReaderHostRef;
        ARASamplePosition samplePosition;
        ARASampleCount samplesPerChannel;
        decodeArguments (message, controllerHostRef, audioReaderHostRef, samplePosition, samplesPerChannel);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        auto reader { fromHostRef (audioReaderHostRef) };
        if (reader->use64BitSamples)
            return _readAudioSamples<double> (documentController, reader, samplePosition, samplesPerChannel);
        else
            return _readAudioSamples<float> (documentController, reader, samplePosition, samplesPerChannel);
    }
    else if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, destroyAudioReader))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioReaderHostRef audioReaderHostRef;
        decodeArguments (message, controllerHostRef, audioReaderHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto reader { fromHostRef (audioReaderHostRef) };

        documentController->getHostAudioAccessController ()->destroyAudioReader (reader->hostRef);
        delete reader;
    }

    // ARAArchivingControllerInterface
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, getArchiveSize))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        ARAArchiveReaderHostRef archiveReaderHostRef;
        decodeArguments (message, controllerHostRef, archiveReaderHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        return encodeReply (documentController->getHostArchivingController ()->getArchiveSize (archiveReaderHostRef));
    }
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, readBytesFromArchive))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        ARAArchiveReaderHostRef archiveReaderHostRef;
        ARASize position;
        ARASize length;
        decodeArguments (message, controllerHostRef, archiveReaderHostRef, position, length);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        std::vector<ARAByte> bytes;
        bytes.resize (length);
        if (!documentController->getHostArchivingController ()->readBytesFromArchive (archiveReaderHostRef, position, length, bytes.data ()))
            bytes.clear ();
        return encodeReply (bytes);
    }
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, writeBytesToArchive))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        ARAArchiveWriterHostRef archiveWriterHostRef;
        ARASize position;
        std::vector<ARAByte> bytes;
        decodeArguments (message, controllerHostRef, archiveWriterHostRef, position, bytes);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        return encodeReply (documentController->getHostArchivingController ()->writeBytesToArchive (archiveWriterHostRef, position, bytes.size (), bytes.data ()));
    }
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, notifyDocumentArchivingProgress))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        float value;
        decodeArguments (message, controllerHostRef, value);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostArchivingController ()->notifyDocumentArchivingProgress (value);
    }
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, notifyDocumentUnarchivingProgress))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        float value;
        decodeArguments (message, controllerHostRef, value);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostArchivingController ()->notifyDocumentUnarchivingProgress (value);
    }
    else if (messageID == HOST_METHOD_ID (ARAArchivingControllerInterface, getDocumentArchiveID))
    {
        ARAArchivingControllerHostRef controllerHostRef;
        ARAArchiveReaderHostRef archiveReaderHostRef;
        decodeArguments (message, controllerHostRef, archiveReaderHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        return encodeReply (documentController->getHostArchivingController ()->getDocumentArchiveID (archiveReaderHostRef));
    }

    // ARAContentAccessControllerInterface
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, isMusicalContextContentAvailable))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAMusicalContextHostRef musicalContextHostRef;
        ARAContentType contentType;
        decodeArguments (message, controllerHostRef, musicalContextHostRef, contentType);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        return encodeReply ((documentController->getHostContentAccessController ()->isMusicalContextContentAvailable (musicalContextHostRef, contentType)) ? kARATrue : kARAFalse);
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, getMusicalContextContentGrade))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAMusicalContextHostRef musicalContextHostRef;
        ARAContentType contentType;
        decodeArguments (message, controllerHostRef, musicalContextHostRef, contentType);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        return encodeReply (documentController->getHostContentAccessController ()->getMusicalContextContentGrade (musicalContextHostRef, contentType));
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, createMusicalContextContentReader))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAMusicalContextHostRef musicalContextHostRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> range;
        decodeArguments (message, controllerHostRef, musicalContextHostRef, contentType, range);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        auto hostContentReader { new HostContentReader };
        hostContentReader->hostRef = documentController->getHostContentAccessController ()->createMusicalContextContentReader (musicalContextHostRef, contentType, (range.second) ? &range.first : nullptr);
        hostContentReader->contentType = contentType;

        return encodeReply (ARAContentReaderHostRef { toHostRef (hostContentReader) });
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, isAudioSourceContentAvailable))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARAContentType contentType;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, contentType);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        return encodeReply ((documentController->getHostContentAccessController ()->isAudioSourceContentAvailable (audioSource->_hostRef, contentType)) ? kARATrue : kARAFalse);
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, getAudioSourceContentGrade))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARAContentType contentType;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, contentType);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        return encodeReply (documentController->getHostContentAccessController ()->getAudioSourceContentGrade (audioSource->_hostRef, contentType));
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, createAudioSourceContentReader))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> range;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, contentType, range);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        auto hostContentReader { new HostContentReader };
        hostContentReader->hostRef = documentController->getHostContentAccessController ()->createAudioSourceContentReader (audioSource->_hostRef, contentType, (range.second) ? &range.first : nullptr);
        hostContentReader->contentType = contentType;
        return encodeReply (ARAContentReaderHostRef { toHostRef (hostContentReader) });
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, getContentReaderEventCount))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAContentReaderHostRef contentReaderHostRef;
        decodeArguments (message, controllerHostRef, contentReaderHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto hostContentReader { fromHostRef (contentReaderHostRef) };

        return encodeReply (documentController->getHostContentAccessController ()->getContentReaderEventCount (hostContentReader->hostRef));
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, getContentReaderDataForEvent))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAContentReaderHostRef contentReaderHostRef;
        ARAInt32 eventIndex;
        decodeArguments (message, controllerHostRef, contentReaderHostRef, eventIndex);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto hostContentReader { fromHostRef (contentReaderHostRef) };

        const void* eventData { documentController->getHostContentAccessController ()->getContentReaderDataForEvent (hostContentReader->hostRef, eventIndex) };
        return encodeContentEvent (hostContentReader->contentType, eventData);
    }
    else if (messageID == HOST_METHOD_ID (ARAContentAccessControllerInterface, destroyContentReader))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAContentReaderHostRef contentReaderHostRef;
        decodeArguments (message, controllerHostRef, contentReaderHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto hostContentReader { fromHostRef (contentReaderHostRef) };

        documentController->getHostContentAccessController ()->destroyContentReader (hostContentReader->hostRef);
        delete hostContentReader;
    }

    // ARAModelUpdateControllerInterface
    else if (messageID == HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioSourceAnalysisProgress))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARAAnalysisProgressState state;
        float value;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, state, value);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        documentController->getHostModelUpdateController ()->notifyAudioSourceAnalysisProgress (audioSource->_hostRef, state, value);
    }
    else if (messageID == HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioSourceContentChanged))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        OptionalArgument<ARAContentTimeRange*> range;
        ARAContentUpdateFlags scopeFlags;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, range, scopeFlags);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));
        auto audioSource { fromHostRef (audioSourceHostRef) };
        ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, isValidInstance (audioSource));

        documentController->getHostModelUpdateController ()->notifyAudioSourceContentChanged (audioSource->_hostRef, (range.second) ? &range.first : nullptr, scopeFlags);
    }
    else if (messageID == HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioModificationContentChanged))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAAudioModificationHostRef audioModificationHostRef;
        OptionalArgument<ARAContentTimeRange*> range;
        ARAContentUpdateFlags scopeFlags;
        decodeArguments (message, controllerHostRef, audioModificationHostRef, range, scopeFlags);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostModelUpdateController ()->notifyAudioModificationContentChanged (audioModificationHostRef, (range.second) ? &range.first : nullptr, scopeFlags);
    }
    else if (messageID == HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyPlaybackRegionContentChanged))
    {
        ARAModelUpdateControllerHostRef controllerHostRef;
        ARAPlaybackRegionHostRef playbackRegionHostRef;
        OptionalArgument<ARAContentTimeRange*> range;
        ARAContentUpdateFlags scopeFlags;
        decodeArguments (message, controllerHostRef, playbackRegionHostRef, range, scopeFlags);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostModelUpdateController ()->notifyPlaybackRegionContentChanged (playbackRegionHostRef, (range.second) ? &range.first : nullptr, scopeFlags);
    }

    // ARAPlaybackControllerInterface
    else if (messageID == HOST_METHOD_ID (ARAPlaybackControllerInterface, requestStartPlayback))
    {
        ARAPlaybackControllerHostRef controllerHostRef;
        decodeArguments (message, controllerHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostPlaybackController ()->requestStartPlayback ();
    }
    else if (messageID == HOST_METHOD_ID (ARAPlaybackControllerInterface, requestStopPlayback))
    {
        ARAPlaybackControllerHostRef controllerHostRef;
        decodeArguments (message, controllerHostRef);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostPlaybackController ()->requestStopPlayback ();
    }
    else if (messageID == HOST_METHOD_ID (ARAPlaybackControllerInterface, requestSetPlaybackPosition))
    {
        ARAPlaybackControllerHostRef controllerHostRef;
        ARATimePosition timePosition;
        decodeArguments (message, controllerHostRef, timePosition);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostPlaybackController ()->requestSetPlaybackPosition (timePosition);
    }
    else if (messageID == HOST_METHOD_ID (ARAPlaybackControllerInterface, requestSetCycleRange))
    {
        ARAPlaybackControllerHostRef controllerHostRef;
        ARATimePosition startTime;
        ARATimeDuration duration;
        decodeArguments (message, controllerHostRef, startTime, duration);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostPlaybackController ()->requestSetCycleRange (startTime, duration);
    }
    else if (messageID == HOST_METHOD_ID (ARAPlaybackControllerInterface, requestEnableCycle))
    {
        ARAPlaybackControllerHostRef controllerHostRef;
        ARABool enable;
        decodeArguments (message, controllerHostRef, enable);

        auto documentController { fromHostRef (controllerHostRef) };
        ARA_VALIDATE_API_ARGUMENT (controllerHostRef, isValidInstance (documentController));

        documentController->getHostPlaybackController ()->requestEnableCycle (enable != kARAFalse);
    }
    else
    {
        ARA_INTERNAL_ASSERT (false && "unhandled message ID");
    }
    return {};
}

}   // namespace ProxyPlugIn
}   // namespace ARA
