//------------------------------------------------------------------------------
//! \file       ARAIPCProxyPlugIn.h
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

#ifndef ARAIPCProxyPlugIn_h
#define ARAIPCProxyPlugIn_h

#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARAHostDispatch.h"
#include "ARA_Library/Dispatch/ARAPlugInDispatch.h"

#include "IPCPort.h"
#include "ARAIPCEncoding.h"

#include <memory>
#include <set>
#include <string>
#include <thread>

#if ARA_VALIDATE_API_CALLS
    #include "ARA_Library/Debug/ARAContentValidator.h"
#endif

#if ARA_SUPPORT_VERSION_1
    #error "This proxy does not support ARA 1."
#endif


namespace ARA
{
namespace ProxyPlugIn
{

struct AudioSource;
struct ContentReader;
struct HostContentReader;
struct HostAudioReader;
class DocumentController;
class PlaybackRenderer;
class EditorRenderer;
class EditorView;
class PlugInExtension;
class Factory;

/*******************************************************************************/
//! ObjectRef validation helper class - empty class unless ARA_VALIDATE_API_CALLS is enabled
template<typename SubclassT>
class InstanceValidator
{
#if ARA_VALIDATE_API_CALLS
protected:
    inline InstanceValidator () noexcept
    {
        auto result { _instances.insert (this) };
        ARA_INTERNAL_ASSERT (result.second);
    }

    inline ~InstanceValidator ()
    {
        auto it { _instances.find (this) };
        ARA_INTERNAL_ASSERT (it != _instances.end ());
        _instances.erase (it);
    }

public:
    static inline bool isValid (const InstanceValidator* instance)
    {
        return _instances.find (instance) != _instances.end ();
    }

private:
    static std::set<const InstanceValidator*> _instances;
#endif
};

/*******************************************************************************/
//! Implementation of DocumentControllerInterface that channels all calls through IPC
class DocumentController : public PlugIn::DocumentControllerInterface, protected ARAIPCMessageSender, public InstanceValidator<DocumentController>
{
public:
    DocumentController (IPCPort& port, const ARAFactory* factory, const ARADocumentControllerHostInstance* instance, const ARADocumentProperties* properties) noexcept;

public:
    template <typename StructType>
    using PropertiesPtr = PlugIn::PropertiesPtr<StructType>;

    // Destruction
    void destroyDocumentController () noexcept override;

    // Factory
    const ARAFactory* getFactory () const noexcept override;

    // Update Management
    void beginEditing () noexcept override;
    void endEditing () noexcept override;
    void notifyModelUpdates () noexcept override;

    // Archiving
    bool restoreObjectsFromArchive (ARAArchiveReaderHostRef archiveReaderHostRef, const ARARestoreObjectsFilter* filter) noexcept override;
    bool storeObjectsToArchive (ARAArchiveWriterHostRef archiveWriterHostRef, const ARAStoreObjectsFilter* filter) noexcept override;
    bool storeAudioSourceToAudioFileChunk (ARAArchiveWriterHostRef archiveWriterHostRef, ARAAudioSourceRef audioSourceRef, ARAPersistentID* documentArchiveID, bool* openAutomatically) noexcept override;

    // Document Management
    void updateDocumentProperties (PropertiesPtr<ARADocumentProperties> properties) noexcept override;

    // Musical Context Management
    ARAMusicalContextRef createMusicalContext (ARAMusicalContextHostRef hostRef, PropertiesPtr<ARAMusicalContextProperties> properties) noexcept override;
    void updateMusicalContextProperties (ARAMusicalContextRef musicalContextRef, PropertiesPtr<ARAMusicalContextProperties> properties) noexcept override;
    void updateMusicalContextContent (ARAMusicalContextRef musicalContextRef, const ARAContentTimeRange* range, ContentUpdateScopes flags) noexcept override;
    void destroyMusicalContext (ARAMusicalContextRef musicalContextRef) noexcept override;

    // Region Sequence Management
    ARARegionSequenceRef createRegionSequence (ARARegionSequenceHostRef hostRef, PropertiesPtr<ARARegionSequenceProperties> properties) noexcept override;
    void updateRegionSequenceProperties (ARARegionSequenceRef regionSequence, PropertiesPtr<ARARegionSequenceProperties> properties) noexcept override;
    void destroyRegionSequence (ARARegionSequenceRef regionSequence) noexcept override;

    // Audio Source Management
    ARAAudioSourceRef createAudioSource (ARAAudioSourceHostRef hostRef, PropertiesPtr<ARAAudioSourceProperties> properties) noexcept override;
    void updateAudioSourceProperties (ARAAudioSourceRef audioSourceRef, PropertiesPtr<ARAAudioSourceProperties> properties) noexcept override;
    void updateAudioSourceContent (ARAAudioSourceRef audioSourceRef, const ARAContentTimeRange* range, ContentUpdateScopes flags) noexcept override;
    void enableAudioSourceSamplesAccess (ARAAudioSourceRef audioSourceRef, bool enable) noexcept override;
    void deactivateAudioSourceForUndoHistory (ARAAudioSourceRef audioSourceRef, bool deactivate) noexcept override;
    void destroyAudioSource (ARAAudioSourceRef audioSourceRef) noexcept override;

    // Audio Modification Management
    ARAAudioModificationRef createAudioModification (ARAAudioSourceRef audioSourceRef, ARAAudioModificationHostRef hostRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept override;
    ARAAudioModificationRef cloneAudioModification (ARAAudioModificationRef audioModificationRef, ARAAudioModificationHostRef hostRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept override;
    void updateAudioModificationProperties (ARAAudioModificationRef audioModificationRef, PropertiesPtr<ARAAudioModificationProperties> properties) noexcept override;
    bool isAudioModificationPreservingAudioSourceSignal (ARAAudioModificationRef audioModificationRef) noexcept override;
    void deactivateAudioModificationForUndoHistory (ARAAudioModificationRef audioModificationRef, bool deactivate) noexcept override;
    void destroyAudioModification (ARAAudioModificationRef audioModificationRef) noexcept override;

    // Playback Region Management
    ARAPlaybackRegionRef createPlaybackRegion (ARAAudioModificationRef audioModificationRef, ARAPlaybackRegionHostRef hostRef, PropertiesPtr<ARAPlaybackRegionProperties> properties) noexcept override;
    void updatePlaybackRegionProperties (ARAPlaybackRegionRef playbackRegionRef, PropertiesPtr<ARAPlaybackRegionProperties> properties) noexcept override;
    void getPlaybackRegionHeadAndTailTime (ARAPlaybackRegionRef playbackRegionRef, ARATimeDuration* headTime, ARATimeDuration* tailTime) noexcept override;
    void destroyPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept override;

    // Content Reader Management
    bool isAudioSourceContentAvailable (ARAAudioSourceRef audioSourceRef, ARAContentType type) noexcept override;
    ARAContentGrade getAudioSourceContentGrade (ARAAudioSourceRef audioSourceRef, ARAContentType type) noexcept override;
    ARAContentReaderRef createAudioSourceContentReader (ARAAudioSourceRef audioSourceRef, ARAContentType type, const ARAContentTimeRange* range) noexcept override;

    bool isAudioModificationContentAvailable (ARAAudioModificationRef audioModificationRef, ARAContentType type) noexcept override;
    ARAContentGrade getAudioModificationContentGrade (ARAAudioModificationRef audioModificationRef, ARAContentType type) noexcept override;
    ARAContentReaderRef createAudioModificationContentReader (ARAAudioModificationRef audioModificationRef, ARAContentType type, const ARAContentTimeRange* range) noexcept override;

    bool isPlaybackRegionContentAvailable (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type) noexcept override;
    ARAContentGrade getPlaybackRegionContentGrade (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type) noexcept override;
    ARAContentReaderRef createPlaybackRegionContentReader (ARAPlaybackRegionRef playbackRegionRef, ARAContentType type, const ARAContentTimeRange* range) noexcept override;

    ARAInt32 getContentReaderEventCount (ARAContentReaderRef contentReaderRef) noexcept override;
    const void* getContentReaderDataForEvent (ARAContentReaderRef contentReaderRef, ARAInt32 eventIndex) noexcept override;
    void destroyContentReader (ARAContentReaderRef contentReaderRef) noexcept override;

    // Controlling Analysis
    bool isAudioSourceContentAnalysisIncomplete (ARAAudioSourceRef audioSourceRef, ARAContentType contentType) noexcept override;
    void requestAudioSourceContentAnalysis (ARAAudioSourceRef audioSourceRef, ARASize contentTypesCount, const ARAContentType contentTypes[]) noexcept override;

    ARAInt32 getProcessingAlgorithmsCount () noexcept override;
    const ARAProcessingAlgorithmProperties* getProcessingAlgorithmProperties (ARAInt32 algorithmIndex) noexcept override;
    ARAInt32 getProcessingAlgorithmForAudioSource (ARAAudioSourceRef audioSourceRef) noexcept override;
    void requestProcessingAlgorithmForAudioSource (ARAAudioSourceRef audioSourceRef, ARAInt32 algorithmIndex) noexcept override;

    // License Management
    bool isLicensedForCapabilities (bool runModalActivationDialogIfNeeded, ARASize contentTypesCount, const ARAContentType contentTypes[], ARAPlaybackTransformationFlags transformationFlags) noexcept override;

    // Accessors for Proxy
    const ARADocumentControllerInstance* getInstance () const noexcept { return &_instance; }
    ARADocumentControllerRef getRemoteRef () const noexcept { return _remoteRef; }

    // Host Interface Access
    PlugIn::HostAudioAccessController* getHostAudioAccessController () noexcept { return &_hostAudioAccessController; }
    PlugIn::HostArchivingController* getHostArchivingController () noexcept { return &_hostArchivingController; }
    PlugIn::HostContentAccessController* getHostContentAccessController () noexcept { return (_hostContentAccessController.isProvided ()) ? &_hostContentAccessController : nullptr; }
    PlugIn::HostModelUpdateController* getHostModelUpdateController () noexcept { return (_hostModelUpdateController.isProvided ()) ? &_hostModelUpdateController : nullptr; }
    PlugIn::HostPlaybackController* getHostPlaybackController () noexcept { return (_hostPlaybackController.isProvided ()) ? &_hostPlaybackController : nullptr; }

private:
    void destroyIfUnreferenced () noexcept;

    friend class PlugInExtension;
    void addPlugInExtension (PlugInExtension* plugInExtension) noexcept { _plugInExtensions.insert (plugInExtension); }
    void removePlugInExtension (PlugInExtension* plugInExtension) noexcept { _plugInExtensions.erase (plugInExtension); if (_plugInExtensions.empty ()) destroyIfUnreferenced (); }

private:
    const ARAFactory* const _factory;

    PlugIn::HostAudioAccessController _hostAudioAccessController;
    PlugIn::HostArchivingController _hostArchivingController;
    PlugIn::HostContentAccessController _hostContentAccessController;
    PlugIn::HostModelUpdateController _hostModelUpdateController;
    PlugIn::HostPlaybackController _hostPlaybackController;

    PlugIn::DocumentControllerInstance _instance;

    ARADocumentControllerRef _remoteRef;

    bool _hasBeenDestroyed { false };

    ARAProcessingAlgorithmProperties _processingAlgorithmData { 0, nullptr, nullptr };
    struct
    {
        std::string persistentID;
        std::string name;
    } _processingAlgorithmStrings;

    std::set<PlugInExtension*> _plugInExtensions;

    ARA_HOST_MANAGED_OBJECT (DocumentController)
};
ARA_MAP_HOST_REF (DocumentController, ARAAudioAccessControllerHostRef, ARAArchivingControllerHostRef,
                    ARAContentAccessControllerHostRef, ARAModelUpdateControllerHostRef, ARAPlaybackControllerHostRef)


/*******************************************************************************/
//! Extensible plug-in instance role class implementing an ARA \ref Playback_Renderer_Interface.
class PlaybackRenderer : public PlugIn::PlaybackRendererInterface, protected ARAIPCMessageSender, public InstanceValidator<PlaybackRenderer>
{
public:
    explicit PlaybackRenderer (IPCPort& port, ARAPlaybackRendererRef remoteRef) noexcept;

    // Inherited public interface used by the C++ dispatcher, to be called by the ARAPlugInDispatch code exclusively.
    void addPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept override;
    void removePlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept override;

private:
    ARAPlaybackRendererRef const _remoteRef;

    ARA_HOST_MANAGED_OBJECT (PlaybackRenderer)
};


/*******************************************************************************/
//! Extensible plug-in instance role class implementing an ARA \ref Editor_Renderer_Interface.
class EditorRenderer : public PlugIn::EditorRendererInterface, protected ARAIPCMessageSender, public InstanceValidator<EditorRenderer>
{
public:
    explicit EditorRenderer (IPCPort& port, ARAEditorRendererRef remoteRef) noexcept;

    // Inherited public interface used by the C++ dispatcher, to be called by the ARAPlugInDispatch code exclusively.
    void addPlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept override;
    void removePlaybackRegion (ARAPlaybackRegionRef playbackRegionRef) noexcept override;

    void addRegionSequence (ARARegionSequenceRef regionSequenceRef) noexcept override;
    void removeRegionSequence (ARARegionSequenceRef regionSequenceRef) noexcept override;

private:
    ARAEditorRendererRef const _remoteRef;

    ARA_HOST_MANAGED_OBJECT (EditorRenderer)
};


/*******************************************************************************/
//! Extensible plug-in instance role class implementing an ARA \ref Editor_View_Interface.
class EditorView : public PlugIn::EditorViewInterface, protected ARAIPCMessageSender, public InstanceValidator<EditorView>
{
public:
    explicit EditorView (IPCPort& port, ARAEditorViewRef remoteRef) noexcept;

    // Inherited public interface used by the C++ dispatcher, to be called by the ARAPlugInDispatch code exclusively.
    void notifySelection (SizedStructPtr<ARAViewSelection> selection) noexcept override;
    void notifyHideRegionSequences (ARASize regionSequenceRefsCount, const ARARegionSequenceRef regionSequenceRefs[]) noexcept override;

private:
    ARAEditorViewRef const _remoteRef;

    ARA_HOST_MANAGED_OBJECT (EditorView)
};


/*******************************************************************************/
//! Utility class that wraps an ARAPlugInExtensionInstance.
//! Each companion API plug-in instance owns one PlugInExtension (or custom subclass thereof).
class PlugInExtension
{
public:
    PlugInExtension (IPCPort& port, ARADocumentControllerRef documentControllerRef,
                     ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles,
                     size_t remotePlugInExtensionRef) noexcept;
    ~PlugInExtension () noexcept;

    const ARAPlugInExtensionInstance* getInstance () noexcept { return &_instance; }

    PlugIn::PlaybackRendererInterface* getPlaybackRenderer () const noexcept { return _instance.getPlaybackRenderer (); }
    PlugIn::EditorRendererInterface* getEditorRenderer () const noexcept { return _instance.getEditorRenderer (); }
    PlugIn::EditorViewInterface* getEditorView () const noexcept { return _instance.getEditorView (); }

private:
    DocumentController* const _documentController;
    PlugIn::PlugInExtensionInstance _instance;

    ARA_HOST_MANAGED_OBJECT (PlugInExtension)
};

/*******************************************************************************/

class Factory
{
public:
    Factory (IPCPort& hostCommandsPort);

    // proxy document controller creation call, to be used instead of getFactory ()->createDocumentControllerWithDocument ()
    const ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARADocumentControllerHostInstance* hostInstance,
                                                                               const ARADocumentProperties* properties);

    // copy of the remote factory data, but with all function calls removed
    const ARAFactory* getFactory () const { return &_factory; }

    // handler of received messages
    static IPCMessage plugInCallbacksDispatcher (const int32_t messageID, const IPCMessage& message);

    // \todo to perform the binding, the host needs access to this translation...
    static ARADocumentControllerRef getDocumentControllerRemoteRef (ARADocumentControllerRef documentControllerRef)
    { return static_cast<DocumentController*> (PlugIn::fromRef (documentControllerRef))->getRemoteRef (); }

    static std::unique_ptr<PlugInExtension> createPlugInExtension (size_t remoteExtensionRef, IPCPort& port, ARADocumentControllerRef documentControllerRef,
                                                                   ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles);

private:
    IPCPort& _hostCommandsPort;

    ARAFactory _factory;
    struct
    {
        std::string factoryID;
        std::string plugInName;
        std::string manufacturerName;
        std::string informationURL;
        std::string version;
        std::string documentArchiveID;
    } _factoryStrings;
    std::vector<std::string> _factoryCompatibleIDStrings;
    std::vector<const char*> _factoryCompatibleIDs;
    std::vector<ARAContentType> _factoryAnalyzableTypes;
};


}   // namespace ProxyPlugIn
}   // namespace ARA

#endif // ARAIPCProxyPlugIn_h