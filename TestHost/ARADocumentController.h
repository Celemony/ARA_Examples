//------------------------------------------------------------------------------
//! \file       ARADocumentController.h
//!             provides access the plug-in document controller
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2021, Celemony Software GmbH, All Rights Reserved.
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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#pragma once

#include "ARA_Library/Dispatch/ARAHostDispatch.h"
#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Debug/ARAContentLogger.h"

#include "ExamplesCommon/Archives/Archives.h"
#include "ModelObjects.h"

#include <map>

// These macros allow us to use pointers to host side model objects as
// ARA host reference types that will be passed to the ARA APIs
ARA_MAP_HOST_REF (MusicalContext, ARA::ARAMusicalContextHostRef)
ARA_MAP_HOST_REF (RegionSequence, ARA::ARARegionSequenceHostRef)
ARA_MAP_HOST_REF (AudioSource, ARA::ARAAudioSourceHostRef)
ARA_MAP_HOST_REF (AudioModification, ARA::ARAAudioModificationHostRef)
ARA_MAP_HOST_REF (PlaybackRegion, ARA::ARAPlaybackRegionHostRef)

ARA_MAP_HOST_REF (ArchiveBase, ARA::ARAArchiveReaderHostRef, ARA::ARAArchiveWriterHostRef)

// These property typedefs implicity version our properties structs according to the last member
using DocumentProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARADocumentProperties, name)>;
using MusicalContextProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAMusicalContextProperties, color)>;
using RegionSequenceProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARegionSequenceProperties, color)>;
using AudioSourceProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)>;
using AudioModificationProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAAudioModificationProperties, persistentID)>;
using PlaybackRegionProperties = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAPlaybackRegionProperties, color)>;

// Forward declarations of our controller implementations
class ARAAudioAccessController;
class ArchiveReaderWriter;
class ARAArchivingController;
class ARAContentAccessController;
class ARAModelUpdateController;
class ARAPlaybackController;

/*******************************************************************************/
// Our test host document controller class.
// This function creates a plug-in side document and controller and
// provides handles to the othe ARA host controller interfaces.
class ARADocumentController
{
public:
    ARADocumentController (Document* document, const ARA::ARAFactory* araFactory);
    virtual ~ARADocumentController ();

    /*******************************************************************************/
    // Document Control (ARA model graph)

    // these functions define an "edit cycle" - edits to the
    // ARA document must only be made between begin and endEditing
    void beginEditing ();
    void endEditing ();

    // ARA model graph changes must go through these APIs in order
    // to synchronize the plug-in's representation with our own
    void updateDocumentProperties ();

    void addMusicalContext (MusicalContext* musicalContext);
    void removeMusicalContext (MusicalContext* musicalContext);
    void updateMusicalContextProperties (MusicalContext* musicalContext);
    void updateMusicalContextContent (MusicalContext* musicalContext, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags);

    void addRegionSequence (RegionSequence* regionSequence);
    void removeRegionSequence (RegionSequence* regionSequence);
    void updateRegionSequenceProperties (RegionSequence* regionSequence);

    void addAudioSource (AudioSource* audioSource);
    void removeAudioSource (AudioSource* audioSource);
    void updateAudioSourceProperties (AudioSource* audioSource);
    void updateAudioSourceContent (AudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags);

    void addAudioModification (AudioModification* audioModification);
    void cloneAudioModification (AudioModification* sourceAudioModification, AudioModification* clonedAudioModification);
    void removeAudioModification (AudioModification* audioModification);
    void updateAudioModificationProperties (AudioModification* audioModification);

    void addPlaybackRegion (PlaybackRegion* playbackRegion);
    void removePlaybackRegion (PlaybackRegion* playbackRegion);
    void updatePlaybackRegionProperties (PlaybackRegion* playbackRegion);

    /*******************************************************************************/
    // Archiving functions

    // ARA2 style archiving (aka "partial persistency")
    bool supportsPartialPersistency ();
    bool storeObjectsToArchive (ArchiveBase* archive, const ARA::ARAStoreObjectsFilter* filter = nullptr);
    bool restoreObjectsFromArchive (const ArchiveBase* archive, const ARA::ARARestoreObjectsFilter* filter = nullptr);

    // ARA1 style monolithic document archiving functions
    bool storeDocumentToArchive (ArchiveBase* archive);
    bool beginRestoringDocumentFromArchive (const ArchiveBase* archive);
    bool endRestoringDocumentFromArchive (const ArchiveBase* archive);

    // audio file chunk authoring
    bool supportsStoringAudioFileChunks ();
    bool storeAudioSourceToAudioFileChunk (ArchiveBase* archive, AudioSource* audioSource, ARA::ARAPersistentID* documentArchiveID, bool* openAutomatically);

    // debug support: used by ARAArchivingController only, to validate the time slots when
    // the plug-in may actually call into the interfaces for reading or writing.
    bool isUsingArchive (const ArchiveBase* archive = nullptr);

    /*******************************************************************************/
    // Functions to enable audio source access and read head/tail time

    void enableAudioSourceSamplesAccess (AudioSource* audioSource, bool enable);

    void getPlaybackRegionHeadAndTailTime (PlaybackRegion* playbackRegion, double* headTime, double* tailTime);

    /*******************************************************************************/
    // Functions to trigger audio source analysis and deal with processing algorithm selection

    void requestAudioSourceContentAnalysis (AudioSource* audioSource, size_t contentTypesCount, const ARA::ARAContentType contentTypes[], bool bWaitUntilFinish);

    int getProcessingAlgorithmsCount ();
    const ARA::ARAProcessingAlgorithmProperties* getProcessingAlgorithmProperties (int algorithmIndex);
    int getProcessingAlgorithmForAudioSource (AudioSource* audioSource);
    void requestProcessingAlgorithmForAudioSource (AudioSource* audioSource, int algorithmIndex);

    /*******************************************************************************/
    // Functions for reading and logging available content data

    template <typename ModelObjectPtrType>
    inline void logAllContent (ModelObjectPtrType modelObjectPtr, const ARA::ARAContentTimeRange* range = nullptr)
    {
        ARA::ContentLogger::logAllContent (*_documentController, getRef (modelObjectPtr), range);
    }
    template <typename ModelObjectPtrType>
    inline void logAvailableContent (ModelObjectPtrType modelObjectPtr, const ARA::ARAContentTimeRange* range = nullptr)
    {
        ARA::ContentLogger::logAvailableContent (*_documentController, getRef (modelObjectPtr), range);
    }

    void setMinimalContentUpdateLogging (bool flag);

    /*******************************************************************************/
    // Public accessors
    Document* getDocument () const noexcept { return _document; }
    ARA::Host::DocumentController* getDocumentController () const noexcept { return _documentController.get (); }

    // If the host and plug-in documents are in sync then each document object
    // will have a reference to its plug-in side representation, accessible here
    ARA::ARAMusicalContextRef getRef (MusicalContext* musicalContext) const { return _musicalContextRefs.at (musicalContext); }
    ARA::ARARegionSequenceRef getRef (RegionSequence* regionSequence) const { return _regionSequenceRefs.at (regionSequence); }
    ARA::ARAAudioSourceRef getRef (AudioSource* audioSource) const { return _audioSourceRefs.at (audioSource); }
    ARA::ARAAudioModificationRef getRef (AudioModification* audioModification) const { return _audioModificationRefs.at (audioModification); }
    ARA::ARAPlaybackRegionRef getRef (PlaybackRegion* playbackRegion) const { return _playbackRegionRefs.at (playbackRegion); }

private:
    const DocumentProperties getDocumentProperties () const;
    const MusicalContextProperties getMusicalContextProperties (const MusicalContext* musicalContext) const;
    const RegionSequenceProperties getRegionSequenceProperties (const RegionSequence* regionSequence) const;
    const AudioSourceProperties getAudioSourceProperties (const AudioSource* audioSource) const;
    const AudioModificationProperties getAudioModificationProperties (const AudioModification* audioModification) const;
    const PlaybackRegionProperties getPlaybackRegionProperties (const PlaybackRegion* playbackRegion) const;

    ARAAudioAccessController* getAudioAccessController () const noexcept;
    ARAArchivingController* getArchivingController () const noexcept;
    ARAContentAccessController* getContentAccessController () const noexcept;
    ARAModelUpdateController* getModelUpdateController () const noexcept;
    ARAPlaybackController* getPlaybackController () const noexcept;

private:
    Document* _document;
    bool _isEditingDocument { false };
    ARA::Host::DocumentControllerHostInstance _documentControllerHostInstance;
    std::unique_ptr<ARA::Host::DocumentController> _documentController;

    // These maps are used to associate objects in our document with their plug-in side counterparts
    std::map<MusicalContext*, ARA::ARAMusicalContextRef> _musicalContextRefs;
    std::map<RegionSequence*, ARA::ARARegionSequenceRef> _regionSequenceRefs;
    std::map<AudioSource*, ARA::ARAAudioSourceRef> _audioSourceRefs;
    std::map<AudioModification*, ARA::ARAAudioModificationRef> _audioModificationRefs;
    std::map<PlaybackRegion*, ARA::ARAPlaybackRegionRef> _playbackRegionRefs;

    // for debugging only, see isUsingArchive ()
    const ArchiveBase* _currentArchive { nullptr };
};
