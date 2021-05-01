//------------------------------------------------------------------------------
//! \file       ARATestDocumentController.h
//!             document controller implementation for the ARA test plug-in,
//!             customizing the document controller and related base classes of the ARA library
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2021, Celemony Software GmbH, All Rights Reserved.
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

#pragma once

#include "ARA_Library/PlugIn/ARAPlug.h"

#include "TestAnalysis.h"

#include <unordered_set>


class ARATestAudioSource;
class ARATestPlaybackRenderer;

/*******************************************************************************/
class ARATestDocumentController : public ARA::PlugIn::DocumentController
{
public:
    // publish inherited constructor
    using ARA::PlugIn::DocumentController::DocumentController;

    // getter for the Companion API implementations
    static const ARA::ARAFactory* getARAFactory () noexcept;

protected:
    // Document Management
    void willBeginEditing () noexcept override;
    void didEndEditing () noexcept override;

    // Hack to keep this test plug-in simple:
    // In an actual implementation, we would use some condition or timer to trigger integrating
    // the output of a completed analysis task into the model on the main thread.
    // In this dummy implementation however, we rely upon the host polling model updates or
    // analysis completion to act like a timer on the main thread.
    void willNotifyModelUpdates () noexcept override;

    bool doRestoreObjectsFromArchive (ARA::PlugIn::HostArchiveReader* archiveReader, const ARA::PlugIn::RestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToArchive (ARA::PlugIn::HostArchiveWriter* archiveWriter, const ARA::PlugIn::StoreObjectsFilter* filter) noexcept override;

    // Musical Context Management
    void doUpdateMusicalContextContent (ARA::PlugIn::MusicalContext* musicalContext, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

    // Audio Source Management
    ARA::PlugIn::AudioSource* doCreateAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override;
    void willUpdateAudioSourceProperties (ARA::PlugIn::AudioSource* audioSource, ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept override;
    void doUpdateAudioSourceContent (ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;
    void willEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept override;
    void didEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept override;
    void didDeactivateAudioSourceForUndoHistory (ARA::PlugIn::AudioSource* audioSource, bool deactivate) noexcept override;
    void willDestroyAudioSource (ARA::PlugIn::AudioSource* audioSource) noexcept override;

    // Content Reader Management
    bool doIsAudioSourceContentAvailable (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetAudioSourceContentGrade (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreateAudioSourceContentReader (ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept override;

    bool doIsAudioModificationContentAvailable (const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetAudioModificationContentGrade (const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreateAudioModificationContentReader (ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept override;

    bool doIsPlaybackRegionContentAvailable (const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade doGetPlaybackRegionContentGrade (const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept override;
    ARA::PlugIn::ContentReader* doCreatePlaybackRegionContentReader (ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept override;

    // Controlling Analysis
    void doRequestAudioSourceContentAnalysis (ARA::PlugIn::AudioSource* audioSource, std::vector<ARA::ARAContentType> const& contentTypes) noexcept override;
    bool doIsAudioSourceContentAnalysisIncomplete (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept override;

    ARA::ARAInt32 doGetProcessingAlgorithmsCount () noexcept override;
    const ARA::ARAProcessingAlgorithmProperties* doGetProcessingAlgorithmProperties (ARA::ARAInt32 algorithmIndex) noexcept override;
    ARA::ARAInt32 doGetProcessingAlgorithmForAudioSource (const ARA::PlugIn::AudioSource* audioSource) noexcept override;
    void doRequestProcessingAlgorithmForAudioSource (ARA::PlugIn::AudioSource* audioSource, ARA::ARAInt32 algorithmIndex) noexcept override;

    // Plug-In Instance Management
    ARA::PlugIn::PlaybackRenderer* doCreatePlaybackRenderer () noexcept override;

public:
    // Render thread synchronization:
    // This is just a test code implementation of handling the threading - proper code will use a
    // more sophisticated threading implementation, which is needed regardless of ARA.
    // The test code simply blocks renderer access to the model while it is being modified.
    // This includes waiting until concurrent renderer model access has completed before starting modifications.
    bool rendererWillAccessModelGraph (ARATestPlaybackRenderer* playbackRenderer) noexcept;
    void rendererDidAccessModelGraph (ARATestPlaybackRenderer* playbackRenderer) noexcept;

private:
    void disableRendererModelGraphAccess () noexcept;
    void enableRendererModelGraphAccess () noexcept;

    void startOrScheduleAnalysisOfAudioSource (ARATestAudioSource* audioSource);    // does nothing if already analyzing
    bool cancelAnalysisOfAudioSource (ARATestAudioSource* audioSource);

    void startAnalysisTaskForAudioSource (ARATestAudioSource* audioSource);
    bool cancelAnalysisTaskForAudioSource (ARATestAudioSource* audioSource);
    TestAnalysisTask* getActiveAnalysisTaskForAudioSource (const ARATestAudioSource* audioSource) noexcept; // returns nullptr if no active analysis for given audio source
    void processCompletedAnalysisTasks ();

    // because our modifications and playback regions pull their content from the audio sources,
    // we always must notify their changes when changing the audio source content.
    void notifyAudioSourceDependentObjectsContentChanged (ARATestAudioSource* audioSource, ARA::ContentUpdateScopes scopeFlags);

    bool tryCopyHostNoteContent (ARATestAudioSource* audioSource);

    // if audio samples or note content or processing algorithm changes, we need to:
    // - stop a potentially ongoing analysis
    // - clear our current analysis result
    // - try read analysis from the host, or else start a new analysis
    // - notify the host about the resulting content changes, also for the
    //   dependent audio modifications and playback regions
    void updateAudioSourceAfterContentOrAlgorithmChanged (ARATestAudioSource* audioSource, bool hostChangedContent);

private:
    std::unordered_set<ARATestAudioSource*> _audioSourcesScheduledForAnalysis;
    std::vector<std::unique_ptr<TestAnalysisTask>> _activeAnalysisTasks;

    std::atomic<bool> _renderersCanAccessModelGraph { true };
    std::atomic<int> _countOfRenderersCurrentlyAccessingModelGraph { 0 };
};
