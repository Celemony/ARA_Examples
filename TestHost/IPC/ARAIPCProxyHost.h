//------------------------------------------------------------------------------
//! \file       ARAIPCProxyHost.h
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

#ifndef ARAIPCProxyHost_h
#define ARAIPCProxyHost_h

#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARAHostDispatch.h"
#include "ARA_Library/Dispatch/ARAPlugInDispatch.h"
#include "ARA_Library/Utilities/ARAStdVectorUtilities.h"

#if ARA_VALIDATE_API_CALLS
    #include "ARA_Library/Debug/ARAContentValidator.h"
#endif

#include "IPCPort.h"
#include "ARAIPCEncoding.h"

#include <string>
#include <vector>

#if ARA_SUPPORT_VERSION_1
    #error "This proxy does not support ARA 1."
#endif


namespace ARA
{
namespace ProxyHost
{

class AudioAccessController;
class ArchivingController;
class ContentAccessController;
class ModelUpdateController;
class PlaybackController;
class DocumentController;

/*******************************************************************************/
//! Implementation of AudioAccessControllerInterface that channels all calls through IPC
class AudioAccessController : public Host::AudioAccessControllerInterface, public ARAIPCMessageSender
{
public:
    AudioAccessController (IPCPort& port, ARAAudioAccessControllerHostRef remoteHostRef) noexcept
    : ARAIPCMessageSender { port }, _remoteHostRef { remoteHostRef } {}

    ARAAudioReaderHostRef createAudioReaderForSource (ARAAudioSourceHostRef audioSourceHostRef, bool use64BitSamples) noexcept override;
    bool readAudioSamples (ARAAudioReaderHostRef audioReaderHostRef, ARASamplePosition samplePosition, ARASampleCount samplesPerChannel, void* const buffers[]) noexcept override;
    void destroyAudioReader (ARAAudioReaderHostRef audioReaderHostRef) noexcept override;

private:
    ARAAudioAccessControllerHostRef _remoteHostRef;
};

/*******************************************************************************/
//! Implementation of ArchivingControllerInterface that channels all calls through IPC
class ArchivingController : public Host::ArchivingControllerInterface, public ARAIPCMessageSender
{
public:
    ArchivingController (IPCPort& port, ARAArchivingControllerHostRef remoteHostRef) noexcept
    : ARAIPCMessageSender { port }, _remoteHostRef { remoteHostRef } {}

    ARASize getArchiveSize (ARAArchiveReaderHostRef archiveReaderHostRef) noexcept override;
    bool readBytesFromArchive (ARAArchiveReaderHostRef archiveReaderHostRef, ARASize position, ARASize length, ARAByte buffer[]) noexcept override;
    bool writeBytesToArchive (ARAArchiveWriterHostRef archiveWriterHostRef, ARASize position, ARASize length, const ARAByte buffer[]) noexcept override;
    void notifyDocumentArchivingProgress (float value) noexcept override;
    void notifyDocumentUnarchivingProgress (float value) noexcept override;
    ARAPersistentID getDocumentArchiveID (ARAArchiveReaderHostRef archiveReaderHostRef) noexcept override;

private:
    ARAArchivingControllerHostRef _remoteHostRef;
    std::string _archiveID;
};

/*******************************************************************************/
//! Implementation of ArchivingControllerInterface that channels all calls through IPC
class ContentAccessController : public Host::ContentAccessControllerInterface, public ARAIPCMessageSender
{
public:
    ContentAccessController (IPCPort& port, ARAContentAccessControllerHostRef remoteHostRef) noexcept
    : ARAIPCMessageSender { port }, _remoteHostRef { remoteHostRef } {}

    bool isMusicalContextContentAvailable (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type) noexcept override;
    ARAContentGrade getMusicalContextContentGrade (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type) noexcept override;
    ARAContentReaderHostRef createMusicalContextContentReader (ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type, const ARAContentTimeRange* range) noexcept override;
    bool isAudioSourceContentAvailable (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type) noexcept override;
    ARAContentGrade getAudioSourceContentGrade (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type) noexcept override;
    ARAContentReaderHostRef createAudioSourceContentReader (ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type, const ARAContentTimeRange* range) noexcept override;
    ARAInt32 getContentReaderEventCount (ARAContentReaderHostRef contentReaderHostRef) noexcept override;
    const void* getContentReaderDataForEvent (ARAContentReaderHostRef contentReaderHostRef, ARAInt32 eventIndex) noexcept override;
    void destroyContentReader (ARAContentReaderHostRef contentReaderHostRef) noexcept override;

private:
    ARAContentAccessControllerHostRef _remoteHostRef;
};

/*******************************************************************************/
//! Implementation of ModelUpdateControllerInterface that channels all calls through IPC
class ModelUpdateController : public Host::ModelUpdateControllerInterface, public ARAIPCMessageSender
{
public:
    ModelUpdateController (IPCPort& port, ARAModelUpdateControllerHostRef remoteHostRef) noexcept
    : ARAIPCMessageSender { port }, _remoteHostRef { remoteHostRef } {}

    void notifyAudioSourceAnalysisProgress (ARAAudioSourceHostRef audioSourceHostRef, ARAAnalysisProgressState state, float value) noexcept override;
    void notifyAudioSourceContentChanged (ARAAudioSourceHostRef audioSourceHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept override;
    void notifyAudioModificationContentChanged (ARAAudioModificationHostRef audioModificationHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept override;
    void notifyPlaybackRegionContentChanged (ARAPlaybackRegionHostRef playbackRegionHostRef, const ARAContentTimeRange* range, ContentUpdateScopes scopeFlags) noexcept override;

private:
    ARAModelUpdateControllerHostRef _remoteHostRef;
};

/*******************************************************************************/
//! Implementation of PlaybackControllerInterface that channels all calls through IPC
class PlaybackController : public Host::PlaybackControllerInterface, public ARAIPCMessageSender
{
public:
    PlaybackController (IPCPort& port, ARAPlaybackControllerHostRef remoteHostRef) noexcept
    : ARAIPCMessageSender { port }, _remoteHostRef { remoteHostRef } {}

    void requestStartPlayback () noexcept override;
    void requestStopPlayback () noexcept override;
    void requestSetPlaybackPosition (ARATimePosition timePosition) noexcept override;
    void requestSetCycleRange (ARATimePosition startTime, ARATimeDuration duration) noexcept override;
    void requestEnableCycle (bool enable) noexcept override;

private:
    ARAPlaybackControllerHostRef _remoteHostRef;
};

/*******************************************************************************/
//! extension of Host::DocumentController that also stores the host instance visible to the plug-in
class DocumentController : public Host::DocumentController
{
public:
    explicit DocumentController (const Host::DocumentControllerHostInstance* hostInstance, const ARADocumentControllerInstance* instance) noexcept
      : Host::DocumentController { instance },
        _hostInstance { hostInstance }
    {}

    const Host::DocumentControllerHostInstance* getHostInstance () { return _hostInstance; }

private:
    const Host::DocumentControllerHostInstance* _hostInstance;
};
ARA_MAP_REF (DocumentController, ARADocumentControllerRef)


/*******************************************************************************/
//! Wrapper class for a plug-in extension instance that can forward remote calls to its sub-interfaces
class PlugInExtension
{
public:
    explicit PlugInExtension (const ARA::ARAPlugInExtensionInstance* instance)
    : _playbackRenderer { instance },
      _editorRenderer { instance },
      _editorView { instance }
    {}

    // Getters for ARA specific plug-in role interfaces
    Host::PlaybackRenderer* getPlaybackRenderer () { return &_playbackRenderer; }
    Host::EditorRenderer* getEditorRenderer () { return &_editorRenderer; }
    Host::EditorView* getEditorView () { return &_editorView; }

private:
    Host::PlaybackRenderer _playbackRenderer;
    Host::EditorRenderer _editorRenderer;
    Host::EditorView _editorView;
};
ARA_MAP_REF (PlugInExtension, ARAPlugInExtensionRef, ARAPlaybackRendererRef, ARAEditorRendererRef, ARAEditorViewRef)


/*******************************************************************************/

void runHost (const ARAFactory& factory, const char* hostCommandsPortID, const char* plugInCallbacksPortID);


}   // namespace ProxyHost
}   // namespace ARA

#endif // ARAIPCProxyHost_h
