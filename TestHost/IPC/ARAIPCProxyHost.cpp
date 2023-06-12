//------------------------------------------------------------------------------
//! \file       ARAIPCProxyHost.cpp
//!             implementation of host-side ARA IPC proxy host
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

#include "ARAIPCProxyHost.h"

namespace ARA {
namespace ProxyHost {

/*******************************************************************************/

struct RemoteAudioSource
{
    ARAAudioSourceHostRef mainHostRef;
    ARAAudioSourceRef plugInRef;
    ARAChannelCount channelCount;
};
ARA_MAP_REF (RemoteAudioSource, ARAAudioSourceRef)
ARA_MAP_HOST_REF (RemoteAudioSource, ARAAudioSourceHostRef)

struct RemoteAudioReader
{
    RemoteAudioSource* audioSource;
    ARAAudioReaderHostRef mainHostRef;
    ARABool use64BitSamples;
};
ARA_MAP_HOST_REF (RemoteAudioReader, ARAAudioReaderHostRef)

struct RemoteContentReader
{
    ARAContentReaderRef plugInRef;
    ARAContentType contentType;
};
ARA_MAP_REF (RemoteContentReader, ARAContentReaderRef)

struct RemoteHostContentReader
{
    ARAContentReaderHostRef remoteHostRef;
    ARAIPCContentEventDecoder decoder;
};
ARA_MAP_HOST_REF (RemoteHostContentReader, ARAContentReaderHostRef)

/*******************************************************************************/

ARAAudioReaderHostRef AudioAccessController::createAudioReaderForSource (ARAAudioSourceHostRef audioSourceHostRef, bool use64BitSamples) noexcept
{
    auto remoteAudioReader { new RemoteAudioReader };
    remoteAudioReader->audioSource = fromHostRef (audioSourceHostRef);
    remoteAudioReader->use64BitSamples = use64BitSamples;
    remoteCallWithReply (remoteAudioReader->mainHostRef, HOST_METHOD_ID (ARAAudioAccessControllerInterface, createAudioReaderForSource), _remoteHostRef, remoteAudioReader->audioSource->mainHostRef, use64BitSamples);
    return toHostRef (remoteAudioReader);
}

void _swap (float* ptr)
{
    auto asIntPtr { reinterpret_cast<uint32_t*> (ptr) };
#if defined (_MSC_VER)
    *asIntPtr = _byteswap_ulong (*asIntPtr);
#elif defined (__GNUC__)
    *asIntPtr = __builtin_bswap32 (*asIntPtr);
#else
    #error "not implemented for this compiler."
#endif
}
void _swap (double* ptr)
{
    auto asIntPtr { reinterpret_cast<uint64_t*> (ptr) };
#if defined (_MSC_VER)
    *asIntPtr = _byteswap_uint64 (*asIntPtr);
#elif defined (__GNUC__)
    *asIntPtr = __builtin_bswap64 (*asIntPtr);
#else
    #error "not implemented for this compiler."
#endif
}

template<typename FloatT>
ARABool _readAudioSamples (const std::vector<uint8_t>& reply, ARASampleCount samplesPerChannel, ARAChannelCount channelCount, void* const buffers[], bool needSwap)
{
    const bool success { reply.size () > 0 };
    if (success)
        ARA_INTERNAL_ASSERT (reply.size () == sizeof (FloatT) * static_cast<size_t> (samplesPerChannel * channelCount));
    else
        ARA_INTERNAL_ASSERT (reply.size () == 0);

    auto sourcePtr = reply.data ();
    const auto channelSize { sizeof (FloatT) * static_cast<size_t> (samplesPerChannel) };
    for (auto i { 0 }; i < channelCount; ++i)
    {
        if (success != kARAFalse)
        {
            std::memcpy (buffers[i], sourcePtr, channelSize);
            if (needSwap)
            {
                for (auto f { 0 }; f < samplesPerChannel; ++f)
                    _swap (static_cast<FloatT*> (buffers[i]) + f);
            }
        }
        else
        {
            std::memset (buffers[i], 0, channelSize);
        }
        sourcePtr += channelSize;
    }
    return success;
}

bool AudioAccessController::readAudioSamples (ARAAudioReaderHostRef audioReaderHostRef, ARASamplePosition samplePosition,
                                              ARASampleCount samplesPerChannel, void* const buffers[]) noexcept
{
    auto remoteAudioReader { fromHostRef (audioReaderHostRef) };

    // recursively limit message size to keep IPC responsive
    if (samplesPerChannel > 8192)
    {
        const auto samplesPerChannel1 { samplesPerChannel / 2 };
        const auto result1 { readAudioSamples (audioReaderHostRef, samplePosition, samplesPerChannel1, buffers) };

        const auto sampleSize { (remoteAudioReader->use64BitSamples != kARAFalse) ? sizeof (double) : sizeof (float) };
        const auto samplesPerChannel2 { samplesPerChannel - samplesPerChannel1 };
        void* buffers2[32];
        ARA_INTERNAL_ASSERT(remoteAudioReader->audioSource->channelCount < 32);
        for (auto i { 0 }; i < remoteAudioReader->audioSource->channelCount; ++i)
            buffers2[i] = static_cast<uint8_t*> (buffers[i]) + static_cast<size_t> (samplesPerChannel1) * sampleSize;

        if (result1)
        {
            return readAudioSamples (audioReaderHostRef, samplePosition + samplesPerChannel1, samplesPerChannel2, buffers2);
        }
        else
        {
            for (auto i { 0 }; i < remoteAudioReader->audioSource->channelCount; ++i)
                std::memset (buffers2[i], 0, static_cast<size_t> (samplesPerChannel2) * sampleSize);
            return false;
        }
    }

    // local message copy to deal with float data memory ownership
    IPCMessage replyMsg;
    remoteCallWithReply (replyMsg, HOST_METHOD_ID (ARAAudioAccessControllerInterface, readAudioSamples),
                        _remoteHostRef, remoteAudioReader->mainHostRef, samplePosition, samplesPerChannel);
    std::vector<uint8_t> reply;
    BytesDecoder writer { reply };
    decodeReply (writer, replyMsg);
    const auto result { (remoteAudioReader->use64BitSamples != kARAFalse) ?
                        _readAudioSamples<double> (reply, samplesPerChannel, remoteAudioReader->audioSource->channelCount, buffers, !portEndianessMatches ()):
                        _readAudioSamples<float> (reply, samplesPerChannel, remoteAudioReader->audioSource->channelCount, buffers, !portEndianessMatches ()) };
    return (result != kARAFalse);
}

void AudioAccessController::destroyAudioReader (ARAAudioReaderHostRef audioReaderHostRef) noexcept
{
    auto remoteAudioReader { fromHostRef (audioReaderHostRef) };
    remoteCallWithoutReply (HOST_METHOD_ID (ARAAudioAccessControllerInterface, destroyAudioReader), _remoteHostRef, remoteAudioReader->mainHostRef);
    delete remoteAudioReader;
}

/*******************************************************************************/

ARASize ArchivingController::getArchiveSize (ARAArchiveReaderHostRef archiveReaderHostRef) noexcept
{
    ARASize size;
    remoteCallWithReply (size, HOST_METHOD_ID (ARAArchivingControllerInterface, getArchiveSize), _remoteHostRef, archiveReaderHostRef);
    return size;
}

bool ArchivingController::readBytesFromArchive (ARAArchiveReaderHostRef archiveReaderHostRef, ARASize position, ARASize length, ARAByte buffer[]) noexcept
{
    // recursively limit message size to keep IPC responsive
    if (length > 131072)
    {
        const auto length1 { length / 2 };
        const auto result1 { readBytesFromArchive (archiveReaderHostRef, position, length1, buffer) };

        const auto length2 { length - length1 };
        buffer += length1;
        if (result1)
        {
            return readBytesFromArchive (archiveReaderHostRef, position + length1, length2, buffer);
        }
        else
        {
            std::memset (buffer, 0, length2);
            return false;
        }
    }

    auto resultLength { length };
    BytesDecoder writer { buffer, resultLength };
    remoteCallWithReply (writer, HOST_METHOD_ID (ARAArchivingControllerInterface, readBytesFromArchive),
                        _remoteHostRef, archiveReaderHostRef, position, length);
    if (resultLength == length)
    {
        return true;
    }
    else
    {
        std::memset (buffer, 0, length);
        return false;
    }
}

bool ArchivingController::writeBytesToArchive (ARAArchiveWriterHostRef archiveWriterHostRef, ARASize position, ARASize length, const ARAByte buffer[]) noexcept
{
    // recursively limit message size to keep IPC responsive
    if (length > 131072)
    {
        const auto length1 { length / 2 };
        const auto result1 { writeBytesToArchive (archiveWriterHostRef, position, length1, buffer) };

        const auto length2 { length - length1 };
        buffer += length1;
        if (result1)
        {
            return writeBytesToArchive (archiveWriterHostRef, position + length1, length2, buffer);
        }
        else
        {
            return false;
        }
    }

    ARABool success;
    remoteCallWithReply (success, HOST_METHOD_ID (ARAArchivingControllerInterface, writeBytesToArchive),
                        _remoteHostRef, archiveWriterHostRef, position, BytesEncoder { buffer, length });
    return (success != kARAFalse);
}

void ArchivingController::notifyDocumentArchivingProgress (float value) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAArchivingControllerInterface, notifyDocumentArchivingProgress), _remoteHostRef, value);
}

void ArchivingController::notifyDocumentUnarchivingProgress (float value) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAArchivingControllerInterface, notifyDocumentUnarchivingProgress), _remoteHostRef, value);
}

ARAPersistentID ArchivingController::getDocumentArchiveID (ARAArchiveReaderHostRef archiveReaderHostRef) noexcept
{
    // local message copy to deal with string memory ownership
    IPCMessage replyMsg;
    remoteCallWithReply (replyMsg, HOST_METHOD_ID (ARAArchivingControllerInterface, getDocumentArchiveID), _remoteHostRef, archiveReaderHostRef);
    ARAPersistentID persistentID;
    decodeReply (persistentID, replyMsg);
    _archiveID.assign (persistentID);
    return _archiveID.c_str();
}

/*******************************************************************************/

bool ContentAccessController::isMusicalContextContentAvailable (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type) noexcept
{
    ARABool result;
    remoteCallWithReply (result, HOST_METHOD_ID (ARAContentAccessControllerInterface, isMusicalContextContentAvailable),
                        _remoteHostRef, musicalContextHostRef, type);
    return (result != kARAFalse);
}

ARAContentGrade ContentAccessController::getMusicalContextContentGrade (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type) noexcept
{
    ARAContentGrade grade;
    remoteCallWithReply (grade, HOST_METHOD_ID (ARAContentAccessControllerInterface, getMusicalContextContentGrade),
                        _remoteHostRef, musicalContextHostRef, type);
    return grade;
}

ARAContentReaderHostRef ContentAccessController::createMusicalContextContentReader (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type, const ARAContentTimeRange* range) noexcept
{
    ARAContentReaderHostRef contentReaderHostRef;
    remoteCallWithReply (contentReaderHostRef, HOST_METHOD_ID (ARAContentAccessControllerInterface, createMusicalContextContentReader),
                        _remoteHostRef, musicalContextHostRef, type, range);
    auto contentReader { new RemoteHostContentReader { contentReaderHostRef, type } };
    return toHostRef (contentReader);
}

bool ContentAccessController::isAudioSourceContentAvailable (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type) noexcept
{
    ARABool result;
    remoteCallWithReply (result, HOST_METHOD_ID (ARAContentAccessControllerInterface, isAudioSourceContentAvailable),
                        _remoteHostRef, fromHostRef (audioSourceHostRef)->mainHostRef, type);
    return (result != kARAFalse);
}

ARAContentGrade ContentAccessController::getAudioSourceContentGrade (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type) noexcept
{
    ARAContentGrade grade;
    remoteCallWithReply (grade, HOST_METHOD_ID (ARAContentAccessControllerInterface, getAudioSourceContentGrade),
                        _remoteHostRef, fromHostRef (audioSourceHostRef)->mainHostRef, type);
    return grade;
}

ARAContentReaderHostRef ContentAccessController::createAudioSourceContentReader (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type, const ARAContentTimeRange* range) noexcept
{
    ARAContentReaderHostRef contentReaderHostRef;
    remoteCallWithReply (contentReaderHostRef, HOST_METHOD_ID (ARAContentAccessControllerInterface, createAudioSourceContentReader),
                        _remoteHostRef, fromHostRef (audioSourceHostRef)->mainHostRef, type, range);
    auto contentReader { new RemoteHostContentReader { contentReaderHostRef, type } };
    return toHostRef (contentReader);
}

ARAInt32 ContentAccessController::getContentReaderEventCount (ARAContentReaderHostRef contentReaderHostRef) noexcept
{
    const auto contentReader { fromHostRef (contentReaderHostRef) };
    ARAInt32 count;
    remoteCallWithReply (count, HOST_METHOD_ID (ARAContentAccessControllerInterface, getContentReaderEventCount),
                        _remoteHostRef, contentReader->remoteHostRef);
    return count;
}

const void* ContentAccessController::getContentReaderDataForEvent (ARAContentReaderHostRef contentReaderHostRef, ARAInt32 eventIndex) noexcept
{
    const auto contentReader { fromHostRef (contentReaderHostRef) };
    IPCMessage reply;
    remoteCallWithReply (reply, HOST_METHOD_ID (ARAContentAccessControllerInterface, getContentReaderDataForEvent),
                        _remoteHostRef, contentReader->remoteHostRef, eventIndex);
    return contentReader->decoder.decode (reply);
}

void ContentAccessController::destroyContentReader (ARAContentReaderHostRef contentReaderHostRef) noexcept
{
    const auto contentReader { fromHostRef (contentReaderHostRef) };
    remoteCallWithoutReply (HOST_METHOD_ID (ARAContentAccessControllerInterface, destroyContentReader), _remoteHostRef, contentReader->remoteHostRef);
    delete contentReader;
}

/*******************************************************************************/

void ModelUpdateController::notifyAudioSourceAnalysisProgress (ARAAudioSourceHostRef audioSourceHostRef, ARAAnalysisProgressState state, float value) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioSourceAnalysisProgress), _remoteHostRef, fromHostRef (audioSourceHostRef)->mainHostRef, state, value);
}

void ModelUpdateController::notifyAudioSourceContentChanged (ARAAudioSourceHostRef audioSourceHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioSourceContentChanged), _remoteHostRef, fromHostRef (audioSourceHostRef)->mainHostRef, range, scopeFlags);
}

void ModelUpdateController::notifyAudioModificationContentChanged (ARAAudioModificationHostRef audioModificationHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyAudioModificationContentChanged), _remoteHostRef, audioModificationHostRef, range, scopeFlags);
}

void ModelUpdateController::notifyPlaybackRegionContentChanged (ARAPlaybackRegionHostRef playbackRegionHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAModelUpdateControllerInterface, notifyPlaybackRegionContentChanged), _remoteHostRef, playbackRegionHostRef, range, scopeFlags);
}

/*******************************************************************************/

void PlaybackController::requestStartPlayback () noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAPlaybackControllerInterface, requestStartPlayback), _remoteHostRef);
}

void PlaybackController::requestStopPlayback () noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAPlaybackControllerInterface, requestStopPlayback), _remoteHostRef);
}

void PlaybackController::requestSetPlaybackPosition (ARATimePosition timePosition) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAPlaybackControllerInterface, requestSetPlaybackPosition), _remoteHostRef, timePosition);
}

void PlaybackController::requestSetCycleRange (ARATimePosition startTime, ARATimeDuration duration) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAPlaybackControllerInterface, requestSetCycleRange), _remoteHostRef, startTime, duration);
}

void PlaybackController::requestEnableCycle (bool enable) noexcept
{
    remoteCallWithoutReply (HOST_METHOD_ID (ARAPlaybackControllerInterface, requestEnableCycle), _remoteHostRef, (enable) ? kARATrue : kARAFalse);
}

/*******************************************************************************/

const ARAFactory* _factory {};
IPCPort* _plugInCallbacksPort {};

void setupHostCommandHandler (const ARAFactory* factory, IPCPort* plugInCallbacksPort)
{
    ARA_INTERNAL_ASSERT(factory->highestSupportedApiGeneration >= kARAAPIGeneration_2_0_Final);

    _factory = factory;
    _plugInCallbacksPort = plugInCallbacksPort;
}

IPCMessage hostCommandHandler (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("_hostCommandHandler received message %s", decodePlugInMethodID (messageID));

    // ARAFactory
    if (messageID == kGetFactoryMethodID)
    {
        return encodeReply (*_factory);
    }
    else if (messageID == kCreateDocumentControllerMethodID)
    {
        ARAAudioAccessControllerHostRef audioAccessControllerHostRef;
        ARAArchivingControllerHostRef archivingControllerHostRef;
        ARABool provideContentAccessController;
        ARAContentAccessControllerHostRef contentAccessControllerHostRef;
        ARABool provideModelUpdateController;
        ARAModelUpdateControllerHostRef modelUpdateControllerHostRef;
        ARABool providePlaybackController;
        ARAPlaybackControllerHostRef playbackControllerHostRef;
        ARADocumentProperties properties;
        decodeArguments (message, audioAccessControllerHostRef, archivingControllerHostRef,
                                provideContentAccessController, contentAccessControllerHostRef,
                                provideModelUpdateController, modelUpdateControllerHostRef,
                                providePlaybackController, playbackControllerHostRef,
                                properties);

        const auto audioAccessController { new AudioAccessController { *_plugInCallbacksPort, audioAccessControllerHostRef } };
        const auto archivingController { new ArchivingController { *_plugInCallbacksPort, archivingControllerHostRef } };
        const auto contentAccessController { (provideContentAccessController != kARAFalse) ? new ContentAccessController { *_plugInCallbacksPort, contentAccessControllerHostRef } : nullptr };
        const auto modelUpdateController { (provideModelUpdateController != kARAFalse) ? new ModelUpdateController { *_plugInCallbacksPort, modelUpdateControllerHostRef } : nullptr };
        const auto playbackController { (providePlaybackController != kARAFalse) ? new PlaybackController { *_plugInCallbacksPort, playbackControllerHostRef } : nullptr };

        const auto hostInstance { new Host::DocumentControllerHostInstance { audioAccessController, archivingController,
                                                                                contentAccessController, modelUpdateController, playbackController } };

        auto documentControllerInstance { _factory->createDocumentControllerWithDocument (hostInstance, &properties) };
        ARA_VALIDATE_API_CONDITION (documentControllerInstance != nullptr);
        ARA_VALIDATE_API_INTERFACE (documentControllerInstance->documentControllerInterface, ARADocumentControllerInterface);
        auto documentController { new DocumentController (hostInstance, documentControllerInstance) };
        return encodeReply (ARADocumentControllerRef { toRef (documentController) });
    }

    //ARADocumentControllerInterface
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyDocumentController))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);
        auto documentController { fromRef (controllerRef) };
        documentController->destroyDocumentController ();

        delete documentController->getHostInstance ()->getPlaybackController ();
        delete documentController->getHostInstance ()->getModelUpdateController ();
        delete documentController->getHostInstance ()->getContentAccessController ();
        delete documentController->getHostInstance ()->getArchivingController ();
        delete documentController->getHostInstance ()->getAudioAccessController ();
        delete documentController->getHostInstance ();
        delete documentController;
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getFactory))
    {
        ARA_INTERNAL_ASSERT (false && "should never be queried here but instead cached from companion API upon setup");

        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);

        return encodeReply (*(fromRef (controllerRef)->getFactory ()));
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, beginEditing))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);

        fromRef (controllerRef)->beginEditing ();
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, endEditing))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);

        fromRef (controllerRef)->endEditing ();
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, notifyModelUpdates))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);

        fromRef (controllerRef)->notifyModelUpdates ();
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, restoreObjectsFromArchive))
    {
        ARADocumentControllerRef controllerRef;
        ARAArchiveReaderHostRef archiveReaderHostRef;
        OptionalArgument<ARARestoreObjectsFilter> filter;
        decodeArguments (message, controllerRef, archiveReaderHostRef, filter);

        return encodeReply (fromRef (controllerRef)->restoreObjectsFromArchive (archiveReaderHostRef, (filter.second) ? &filter.first : nullptr) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, storeObjectsToArchive))
    {
        ARADocumentControllerRef controllerRef;
        ARAArchiveWriterHostRef archiveWriterHostRef;
        OptionalArgument<ARAStoreObjectsFilter> filter;
        decodeArguments (message, controllerRef, archiveWriterHostRef, filter);

        std::vector<ARAAudioSourceRef> audioSourceRefs;
        if (filter.second && (filter.first.audioSourceRefsCount > 0))
        {
            audioSourceRefs.reserve (filter.first.audioSourceRefsCount);
            for (auto i { 0U }; i < filter.first.audioSourceRefsCount; ++i)
                audioSourceRefs.emplace_back (fromRef (filter.first.audioSourceRefs[i])->plugInRef);

            filter.first.audioSourceRefs = audioSourceRefs.data ();
        }

        return encodeReply (fromRef (controllerRef)->storeObjectsToArchive (archiveWriterHostRef, (filter.second) ? &filter.first : nullptr) ? kARATrue : kARAFalse);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateDocumentProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARADocumentProperties properties;
        decodeArguments (message, controllerRef, properties);

        fromRef (controllerRef)->updateDocumentProperties (&properties);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createMusicalContext))
    {
        ARADocumentControllerRef controllerRef;
        ARAMusicalContextHostRef hostRef;
        ARAMusicalContextProperties properties;
        decodeArguments (message, controllerRef, hostRef, properties);

        return encodeReply (fromRef (controllerRef)->createMusicalContext (hostRef, &properties));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateMusicalContextProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARAMusicalContextRef musicalContextRef;
        ARAMusicalContextProperties properties;
        decodeArguments (message, controllerRef, musicalContextRef, properties);

        fromRef (controllerRef)->updateMusicalContextProperties (musicalContextRef, &properties);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateMusicalContextContent))
    {
        ARADocumentControllerRef controllerRef;
        ARAMusicalContextRef musicalContextRef;
        OptionalArgument<ARAContentTimeRange*> range;
        ARAContentUpdateFlags flags;
        decodeArguments (message, controllerRef, musicalContextRef, range, flags);

        fromRef (controllerRef)->updateMusicalContextContent (musicalContextRef, (range.second) ? &range.first : nullptr, flags);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyMusicalContext))
    {
        ARADocumentControllerRef controllerRef;
        ARAMusicalContextRef musicalContextRef;
        decodeArguments (message, controllerRef, musicalContextRef);

        fromRef (controllerRef)->destroyMusicalContext (musicalContextRef);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createRegionSequence))
    {
        ARADocumentControllerRef controllerRef;
        ARARegionSequenceHostRef hostRef;
        ARARegionSequenceProperties properties;
        decodeArguments (message, controllerRef, hostRef, properties);

        return encodeReply (fromRef (controllerRef)->createRegionSequence (hostRef, &properties));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateRegionSequenceProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARARegionSequenceRef regionSequenceRef;
        ARARegionSequenceProperties properties;
        decodeArguments (message, controllerRef, regionSequenceRef, properties);

        fromRef (controllerRef)->updateRegionSequenceProperties (regionSequenceRef, &properties);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyRegionSequence))
    {
        ARADocumentControllerRef controllerRef;
        ARARegionSequenceRef regionSequenceRef;
        decodeArguments (message, controllerRef, regionSequenceRef);

        fromRef (controllerRef)->destroyRegionSequence (regionSequenceRef);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSource))
    {
        auto remoteAudioSource { new RemoteAudioSource };

        ARADocumentControllerRef controllerRef;
        ARAAudioSourceProperties properties;
        decodeArguments (message, controllerRef, remoteAudioSource->mainHostRef, properties);

        remoteAudioSource->channelCount = properties.channelCount;
        remoteAudioSource->plugInRef = fromRef (controllerRef)->createAudioSource (toHostRef (remoteAudioSource), &properties);

        return encodeReply (ARAAudioSourceRef { toRef (remoteAudioSource) });
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioSourceProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAAudioSourceProperties properties;
        decodeArguments (message, controllerRef, audioSourceRef, properties);

        fromRef (controllerRef)->updateAudioSourceProperties (fromRef (audioSourceRef)->plugInRef, &properties);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioSourceContent))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        OptionalArgument<ARAContentTimeRange*> range;
        ARAContentUpdateFlags flags;
        decodeArguments (message, controllerRef, audioSourceRef, range, flags);

        fromRef (controllerRef)->updateAudioSourceContent (fromRef (audioSourceRef)->plugInRef, (range.second) ? &range.first : nullptr, flags);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, enableAudioSourceSamplesAccess))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARABool enable;
        decodeArguments (message, controllerRef, audioSourceRef, enable);

        fromRef (controllerRef)->enableAudioSourceSamplesAccess (fromRef (audioSourceRef)->plugInRef, (enable) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, deactivateAudioSourceForUndoHistory))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARABool deactivate;
        decodeArguments (message, controllerRef, audioSourceRef, deactivate);

        fromRef (controllerRef)->deactivateAudioSourceForUndoHistory (fromRef (audioSourceRef)->plugInRef, (deactivate) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, storeAudioSourceToAudioFileChunk))
    {
        ARADocumentControllerRef controllerRef;
        ARAArchiveWriterHostRef archiveWriterHostRef;
        ARAAudioSourceRef audioSourceRef;
        decodeArguments (message, controllerRef, archiveWriterHostRef, audioSourceRef);

        ARAIPCStoreAudioSourceToAudioFileChunkReply reply;
        bool openAutomatically;
        reply.result = (fromRef (controllerRef)->storeAudioSourceToAudioFileChunk (archiveWriterHostRef, fromRef (audioSourceRef)->plugInRef,
                                                                    &reply.documentArchiveID, &openAutomatically)) ? kARATrue : kARAFalse;
        reply.openAutomatically = (openAutomatically) ? kARATrue : kARAFalse;
        return encodeReply (reply);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAnalysisIncomplete))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioSourceRef, contentType);

        return encodeReply (fromRef (controllerRef)->isAudioSourceContentAnalysisIncomplete (fromRef (audioSourceRef)->plugInRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestAudioSourceContentAnalysis))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        std::vector<ARAContentType> contentTypes;
        decodeArguments (message, controllerRef, audioSourceRef, contentTypes);

        fromRef (controllerRef)->requestAudioSourceContentAnalysis (fromRef (audioSourceRef)->plugInRef, contentTypes.size (), contentTypes.data ());
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAvailable))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioSourceRef, contentType);

        return encodeReply ((fromRef (controllerRef)->isAudioSourceContentAvailable (fromRef (audioSourceRef)->plugInRef, contentType)) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getAudioSourceContentGrade))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioSourceRef, contentType);

        return encodeReply (fromRef (controllerRef)->getAudioSourceContentGrade (fromRef (audioSourceRef)->plugInRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSourceContentReader))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> range;
        decodeArguments (message, controllerRef, audioSourceRef, contentType, range);

        auto remoteContentReader { new RemoteContentReader };
        remoteContentReader->plugInRef = fromRef (controllerRef)->createAudioSourceContentReader (fromRef (audioSourceRef)->plugInRef, contentType, (range.second) ? &range.first : nullptr);
        remoteContentReader->contentType = contentType;
        return encodeReply (ARAContentReaderRef { toRef (remoteContentReader) });
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioSource))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        decodeArguments (message, controllerRef, audioSourceRef);

        auto remoteAudioSource { fromRef (audioSourceRef) };
        fromRef (controllerRef)->destroyAudioSource (remoteAudioSource->plugInRef);

        delete remoteAudioSource;
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioModification))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAAudioModificationHostRef hostRef;
        ARAAudioModificationProperties properties;
        decodeArguments (message, controllerRef, audioSourceRef, hostRef, properties);

        return encodeReply (fromRef (controllerRef)->createAudioModification (fromRef (audioSourceRef)->plugInRef, hostRef, &properties));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, cloneAudioModification))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAAudioModificationHostRef hostRef;
        ARAAudioModificationProperties properties;
        decodeArguments (message, controllerRef, audioModificationRef, hostRef, properties);

        return encodeReply (fromRef (controllerRef)->cloneAudioModification (audioModificationRef, hostRef, &properties));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updateAudioModificationProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAAudioModificationProperties properties;
        decodeArguments (message, controllerRef, audioModificationRef, properties);

        fromRef (controllerRef)->updateAudioModificationProperties (audioModificationRef, &properties);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioModificationPreservingAudioSourceSignal))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        decodeArguments (message, controllerRef, audioModificationRef);

        return encodeReply ((fromRef (controllerRef)->isAudioModificationPreservingAudioSourceSignal (audioModificationRef)) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, deactivateAudioModificationForUndoHistory))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARABool deactivate;
        decodeArguments (message, controllerRef, audioModificationRef, deactivate);

        fromRef (controllerRef)->deactivateAudioModificationForUndoHistory (audioModificationRef, (deactivate) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioModificationContentAvailable))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioModificationRef, contentType);

        return encodeReply ((fromRef (controllerRef)->isAudioModificationContentAvailable (audioModificationRef, contentType)) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getAudioModificationContentGrade))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioModificationRef, contentType);

        return encodeReply (fromRef (controllerRef)->getAudioModificationContentGrade (audioModificationRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioModificationContentReader))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> range;
        decodeArguments (message, controllerRef, audioModificationRef, contentType, range);

        auto remoteContentReader { new RemoteContentReader };
        remoteContentReader->plugInRef = fromRef (controllerRef)->createAudioModificationContentReader (audioModificationRef, contentType, (range.second) ? &range.first : nullptr);
        remoteContentReader->contentType = contentType;
        return encodeReply (ARAContentReaderRef { toRef (remoteContentReader) });
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioModification))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        decodeArguments (message, controllerRef, audioModificationRef);

        fromRef (controllerRef)->destroyAudioModification (audioModificationRef);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createPlaybackRegion))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioModificationRef audioModificationRef;
        ARAPlaybackRegionHostRef hostRef;
        ARAPlaybackRegionProperties properties;
        decodeArguments (message, controllerRef, audioModificationRef, hostRef, properties);

        return encodeReply (fromRef (controllerRef)->createPlaybackRegion (audioModificationRef, hostRef, &properties));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, updatePlaybackRegionProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        ARAPlaybackRegionProperties properties;
        decodeArguments (message, controllerRef, playbackRegionRef, properties);

        fromRef (controllerRef)->updatePlaybackRegionProperties (playbackRegionRef, &properties);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getPlaybackRegionHeadAndTailTime))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        ARABool wantsHeadTime;
        ARABool wantsTailTime;
        decodeArguments (message, controllerRef, playbackRegionRef, wantsHeadTime, wantsTailTime);

        ARAIPCGetPlaybackRegionHeadAndTailTimeReply reply { 0.0, 0.0 };
        fromRef (controllerRef)->getPlaybackRegionHeadAndTailTime (playbackRegionRef, (wantsHeadTime != kARAFalse) ? &reply.headTime : nullptr,
                                                                                        (wantsTailTime != kARAFalse) ? &reply.tailTime : nullptr);
        return encodeReply (reply);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isPlaybackRegionContentAvailable))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, playbackRegionRef, contentType);

        return encodeReply ((fromRef (controllerRef)->isPlaybackRegionContentAvailable (playbackRegionRef, contentType)) ? kARATrue : kARAFalse);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getPlaybackRegionContentGrade))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, playbackRegionRef, contentType);

        return encodeReply (fromRef (controllerRef)->getPlaybackRegionContentGrade (playbackRegionRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createPlaybackRegionContentReader))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> range;
        decodeArguments (message, controllerRef, playbackRegionRef, contentType, range);

        auto remoteContentReader { new RemoteContentReader };
        remoteContentReader->plugInRef = fromRef (controllerRef)->createPlaybackRegionContentReader (playbackRegionRef, contentType, (range.second) ? &range.first : nullptr);
        remoteContentReader->contentType = contentType;
        return encodeReply (ARAContentReaderRef { toRef (remoteContentReader) });
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyPlaybackRegion))
    {
        ARADocumentControllerRef controllerRef;
        ARAPlaybackRegionRef playbackRegionRef;
        decodeArguments (message, controllerRef, playbackRegionRef);

        fromRef (controllerRef)->destroyPlaybackRegion (playbackRegionRef);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderEventCount))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        decodeArguments (message, controllerRef, contentReaderRef);

        return encodeReply (fromRef (controllerRef)->getContentReaderEventCount (fromRef (contentReaderRef)->plugInRef));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderDataForEvent))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        ARAInt32 eventIndex;
        decodeArguments (message, controllerRef, contentReaderRef, eventIndex);

        auto remoteContentReader { fromRef (contentReaderRef) };
        const void* eventData { fromRef (controllerRef)->getContentReaderDataForEvent (remoteContentReader->plugInRef, eventIndex) };
        return encodeContentEvent (remoteContentReader->contentType, eventData);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyContentReader))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        decodeArguments (message, controllerRef, contentReaderRef);

        auto remoteContentReader { fromRef (contentReaderRef) };
        fromRef (controllerRef)->destroyContentReader (remoteContentReader->plugInRef);

        delete remoteContentReader;
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmsCount))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);

        return encodeReply (fromRef (controllerRef)->getProcessingAlgorithmsCount ());
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmProperties))
    {
        ARADocumentControllerRef controllerRef;
        ARAInt32 algorithmIndex;
        decodeArguments (message, controllerRef, algorithmIndex);

        return encodeReply (*(fromRef (controllerRef)->getProcessingAlgorithmProperties (algorithmIndex)));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getProcessingAlgorithmForAudioSource))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        decodeArguments (message, controllerRef, audioSourceRef);

        return encodeReply (fromRef (controllerRef)->getProcessingAlgorithmForAudioSource (fromRef (audioSourceRef)->plugInRef));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestProcessingAlgorithmForAudioSource))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAInt32 algorithmIndex;
        decodeArguments (message, controllerRef, audioSourceRef, algorithmIndex);

        fromRef (controllerRef)->requestProcessingAlgorithmForAudioSource (fromRef (audioSourceRef)->plugInRef, algorithmIndex);
    }

    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isLicensedForCapabilities))
    {
        ARADocumentControllerRef controllerRef;
        ARABool runModalActivationDialogIfNeeded;
        std::vector<ARAContentType> types;
        ARAPlaybackTransformationFlags transformationFlags;
        decodeArguments (message, controllerRef, runModalActivationDialogIfNeeded, types, transformationFlags);

        return encodeReply ((fromRef (controllerRef)->isLicensedForCapabilities ((runModalActivationDialogIfNeeded != kARAFalse),
                                                                    types.size(), types.data (), transformationFlags)) ? kARATrue : kARAFalse);
    }

    // ARAPlaybackRendererInterface
    else if (messageID == PLUGIN_METHOD_ID (ARAPlaybackRendererInterface, addPlaybackRegion))
    {
        ARAPlaybackRendererRef playbackRendererRef;
        ARAPlaybackRegionRef playbackRegionRef;
        decodeArguments (message, playbackRendererRef, playbackRegionRef);

        fromRef (playbackRendererRef)->getPlaybackRenderer ()->addPlaybackRegion (playbackRegionRef);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARAPlaybackRendererInterface, removePlaybackRegion))
    {
        ARAPlaybackRendererRef playbackRendererRef;
        ARAPlaybackRegionRef playbackRegionRef;
        decodeArguments (message, playbackRendererRef, playbackRegionRef);

        fromRef (playbackRendererRef)->getPlaybackRenderer ()->removePlaybackRegion (playbackRegionRef);
    }

    // ARAEditorRendererInterface
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorRendererInterface, addPlaybackRegion))
    {
        ARAEditorRendererRef editorRendererRef;
        ARAPlaybackRegionRef playbackRegionRef;
        decodeArguments (message, editorRendererRef, playbackRegionRef);

        fromRef (editorRendererRef)->getEditorRenderer ()->addPlaybackRegion (playbackRegionRef);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorRendererInterface, removePlaybackRegion))
    {
        ARAEditorRendererRef editorRendererRef;
        ARAPlaybackRegionRef playbackRegionRef;
        decodeArguments (message, editorRendererRef, playbackRegionRef);

        fromRef (editorRendererRef)->getEditorRenderer ()->removePlaybackRegion (playbackRegionRef);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorRendererInterface, addRegionSequence))
    {
        ARAEditorRendererRef editorRendererRef;
        ARARegionSequenceRef regionSequenceRef;
        decodeArguments (message, editorRendererRef, regionSequenceRef);

        fromRef (editorRendererRef)->getEditorRenderer ()->addRegionSequence (regionSequenceRef);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorRendererInterface, removeRegionSequence))
    {
        ARAEditorRendererRef editorRendererRef;
        ARARegionSequenceRef regionSequenceRef;
        decodeArguments (message, editorRendererRef, regionSequenceRef);

        fromRef (editorRendererRef)->getEditorRenderer ()->removeRegionSequence (regionSequenceRef);
    }

    // ARAEditorViewInterface
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorViewInterface, notifySelection))
    {
        ARAEditorViewRef editorViewRef;
        ARAViewSelection selection;
        decodeArguments (message, editorViewRef, selection);

        fromRef (editorViewRef)->getEditorView ()->notifySelection (&selection);
    }
    else if (messageID == PLUGIN_METHOD_ID (ARAEditorViewInterface, notifyHideRegionSequences))
    {
        ARAEditorViewRef editorViewRef;
        std::vector<ARARegionSequenceRef> regionSequenceRefs;
        decodeArguments (message, editorViewRef, regionSequenceRefs);

        fromRef (editorViewRef)->getEditorView ()->notifyHideRegionSequences (regionSequenceRefs.size (), regionSequenceRefs.data ());
    }

    else
    {
        ARA_INTERNAL_ASSERT (false && "unhandled message ID");
    }
    return {};
}

}   // namespace ProxyHost
}   // namespace ARA
