//------------------------------------------------------------------------------
//! \file       ARATestDocumentController.cpp
//!             document controller implementation for the ARA test plug-in,
//!             customizing the document controller and related base classes of the ARA library
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "ARATestDocumentController.h"
#include "ARATestAudioSource.h"
#include "ARATestPlaybackRenderer.h"
#include "TestPersistency.h"
#include "TestPlugInConfig.h"

#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#include "ARA_Library/Debug/ARAContentLogger.h"

#include <array>
#include <map>
#include <atomic>
#include <future>


// In this test plug-in, we want assertions and logging to be always enabled, even in release builds.
// This needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif
#if !ARA_VALIDATE_API_CALLS
    #error "ARA_VALIDATE_API_CALLS not configured properly in the project"
#endif


// By default, the test plug-in only analyzes audio sources when explicitly requested by the host.
// The define below allows to always trigger audio source analysis when a new audio source instance
// is created (and the host does not provide all supported content for it), which is closer to the
// behavior of actual plug-ins like Melodyne, and also allows for testing analysis and related
// notifications in hosts that never request audio source analysis.
#if !defined (ARA_ALWAYS_PERFORM_ANALYSIS)
    #define ARA_ALWAYS_PERFORM_ANALYSIS 0
#endif


ARA_SETUP_DEBUG_MESSAGE_PREFIX (TEST_PLUGIN_NAME);


/*******************************************************************************/

// subclass of the SDK's content reader class to export our detected notes
class ARATestNoteContentReader : public ARA::PlugIn::ContentReader
{
public:
    explicit ARATestNoteContentReader (const ARATestAudioSource* audioSource, const ARA::ARAContentTimeRange* range)
    {
        ARA_INTERNAL_ASSERT (audioSource->getNoteContent () != nullptr);
        for (const auto& note : *audioSource->getNoteContent ())
        {
            // skip note if it does not intersect with the time range
            if (range && ((note._startTime + note._duration <= range->start) ||
                          (range->start + range->duration <= note._startTime)))
                continue;

            ARA::ARAContentNote exportedNote;
            exportedNote.frequency = note._frequency;
            if (exportedNote.frequency == ARA::kARAInvalidFrequency)
                exportedNote.pitchNumber = ARA::kARAInvalidPitchNumber;
            else
                exportedNote.pitchNumber = static_cast<ARA::ARAPitchNumber> (floor (0.5f + 69.0f + 12.0f * logf (exportedNote.frequency / 440.0f) / logf (2.0f)));
            exportedNote.volume = note._volume;
            exportedNote.startPosition = note._startTime;
            exportedNote.attackDuration = 0.0;
            exportedNote.noteDuration = note._duration;
            exportedNote.signalDuration = note._duration;
            _exportedNotes.emplace_back (exportedNote);
        }
    }

    // since our test plug-in makes no modifications to the audio source, it can simply forward the content reading to the source
    explicit ARATestNoteContentReader (const ARA::PlugIn::AudioModification* audioModification, const ARA::ARAContentTimeRange* range)
    : ARATestNoteContentReader { audioModification->getAudioSource<ARATestAudioSource> (), range }
    {}

    // since our test plug-in directly plays sections from the audio modification without any time stretching or other adoption,
    // it can simply copy the modification content and adjust it (and the optional filter range) to the actual playback position
    explicit ARATestNoteContentReader (const ARA::PlugIn::PlaybackRegion* playbackRegion, const ARA::ARAContentTimeRange* range)
    {
        // get filtered notes in modification time via a temporary modification reader
        const auto timeOffset { playbackRegion->getStartInPlaybackTime () - playbackRegion->getStartInAudioModificationTime () };
        const ARA::ARAContentTimeRange modificationRange { (range) ? range->start - timeOffset : playbackRegion->getStartInAudioModificationTime (),
                                                           (range) ? range->start - timeOffset : playbackRegion->getDurationInAudioModificationTime () };
        ARATestNoteContentReader tempModificationReader { playbackRegion->getAudioModification (), &modificationRange };

        // swap content with temp reader and adjust note starts from modification time to playback time
        _exportedNotes.swap (tempModificationReader._exportedNotes);
        for (auto& exportedNote : _exportedNotes)
            exportedNote.startPosition += timeOffset;
    }

    ARA::ARAInt32 getEventCount () noexcept override
    {
        return static_cast<ARA::ARAInt32> (_exportedNotes.size ());

    }

    const void* getDataForEvent (ARA::ARAInt32 eventIndex) noexcept override
    {
        return &_exportedNotes[static_cast<size_t> (eventIndex)];
    }

private:
    std::vector<ARA::ARAContentNote> _exportedNotes;
};

/*******************************************************************************/

// helper class to deal with string ownership for persistent IDs
// We're subclassing the actual ARA interface struct and add members that handle the ownership,
// which works because the host receiving the struct will not access any data beyond structSize.
class AlgorithmPropertiesWrapper
{
private:
    AlgorithmPropertiesWrapper (const TestProcessingAlgorithm* algorithm)
    : _algorithm { algorithm },
      _algorithmProperties { _algorithm->getIdentifier ().c_str (), _algorithm->getName ().c_str () }
    {}

public:
    static std::vector<AlgorithmPropertiesWrapper> const& getAlgorithmProperties ()
    {
        static std::vector<AlgorithmPropertiesWrapper> algorithms;
        if (algorithms.empty ())
        {
            for (const auto& algorithm : TestProcessingAlgorithm::getAlgorithms ())
                algorithms.emplace_back (AlgorithmPropertiesWrapper { algorithm });
        }
        return algorithms;
    }

    static ARA::ARAInt32 getIndexOfAlgorithm (const TestProcessingAlgorithm* algorithm)
    {
        const auto begin { getAlgorithmProperties ().begin () };
        const auto end { getAlgorithmProperties ().end () };
        const auto it { std::find_if (begin, end, [algorithm] (const AlgorithmPropertiesWrapper& props)
                                                        { return props._algorithm == algorithm; } ) };
        ARA_INTERNAL_ASSERT (it != end);
        return static_cast<ARA::ARAInt32> (it - begin);
    }

    const TestProcessingAlgorithm* getAlgorithm () const noexcept { return _algorithm; }

    inline operator const ARA::ARAProcessingAlgorithmProperties* () const noexcept
    {
        return &_algorithmProperties;
    }

private:
    const TestProcessingAlgorithm* const _algorithm;
    ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAProcessingAlgorithmProperties, name)> _algorithmProperties;
};

/*******************************************************************************/

class ARATestAnalysisTask : public TestAnalysisCallbacks
{
public:
    explicit ARATestAnalysisTask (ARATestAudioSource* audioSource, const TestProcessingAlgorithm* processingAlgorithm)
    : _audioSource { audioSource },
      _hostAudioReader { std::make_unique<ARA::PlugIn::HostAudioReader> (audioSource) },    // create audio reader on the main thread, before dispatching to analysis thread
      _processingAlgorithm { processingAlgorithm }
    {
        _future = std::async (std::launch::async, [this] ()
        {
            if (auto newNoteContent = _processingAlgorithm->analyzeNoteContent (this, _audioSource->getSampleCount (), _audioSource->getSampleRate (),
                                                                                static_cast<uint32_t> (_audioSource->getChannelCount ())))
                _noteContent = std::move (newNoteContent);
        });
    }

    ARATestAudioSource* getAudioSource () const noexcept
    {
        return _audioSource;
    }

    const TestProcessingAlgorithm* getProcessingAlgorithm () const noexcept
    {
        return _processingAlgorithm;
    }

    bool isDone () const
    {
        return _future.wait_for (std::chrono::milliseconds { 0 }) == std::future_status::ready;
    }

    void cancelSynchronously ()
    {
        _shouldCancel = true;
        _future.wait ();
        _noteContent.reset ();   // delete here in case our future completed before recognizing the cancel
    }

    std::unique_ptr<TestNoteContent>&& transferNoteContent ()
    {
        ARA_INTERNAL_ASSERT (isDone ());
        return std::move (_noteContent);
    }

    void notifyAnalysisProgressStarted () noexcept
    {
        _audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressStarted (_audioSource);
    }

    void notifyAnalysisProgressUpdated (float progress) noexcept
    {
        _audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressUpdated (_audioSource, progress);
    }

    void notifyAnalysisProgressCompleted () noexcept
    {
        _audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressCompleted (_audioSource);
    }

    bool readAudioSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[]) noexcept
    {
        return _hostAudioReader->readAudioSamples (samplePosition, samplesPerChannel, buffers);
    }

    bool shouldCancel () const noexcept
    {
        return _shouldCancel.load ();
    }

private:
    ARATestAudioSource* const _audioSource;
    const std::unique_ptr<ARA::PlugIn::HostAudioReader> _hostAudioReader;
    const TestProcessingAlgorithm* const _processingAlgorithm;
    std::unique_ptr<TestNoteContent> _noteContent;
    std::future<void> _future;
    std::atomic<bool> _shouldCancel { false };
};

/*******************************************************************************/

void ARATestDocumentController::startOrScheduleAnalysisOfAudioSource (ARATestAudioSource* audioSource)
{
    // test if already analyzing
    if (getActiveAnalysisTaskForAudioSource (audioSource) != nullptr)
        return;

    // postpone if host is currently editing or access is not enabled yet, otherwise start immediately
    if (isHostEditingDocument () || !audioSource->isSampleAccessEnabled ())
    {
        _audioSourcesScheduledForAnalysis.insert (audioSource);
    }
    else
    {
        _audioSourcesScheduledForAnalysis.erase (audioSource);
        startAnalysisTaskForAudioSource (audioSource);
    }
}

bool ARATestDocumentController::cancelAnalysisOfAudioSource (ARATestAudioSource* audioSource)
{
    if (cancelAnalysisTaskForAudioSource (audioSource))
        return true;

    return _audioSourcesScheduledForAnalysis.erase (audioSource) != 0;
}

void ARATestDocumentController::startAnalysisTaskForAudioSource (ARATestAudioSource* audioSource)
{
    ARA_INTERNAL_ASSERT (audioSource->isSampleAccessEnabled ());

    const auto algorithm = audioSource->getProcessingAlgorithm ();
    _activeAnalysisTasks.emplace_back (std::make_unique<ARATestAnalysisTask> (audioSource, algorithm));
}

bool ARATestDocumentController::cancelAnalysisTaskForAudioSource (ARATestAudioSource* audioSource)
{
    if (ARATestAnalysisTask* analysisTask { getActiveAnalysisTaskForAudioSource (audioSource) })
    {
        analysisTask->cancelSynchronously ();
        ARA::find_erase (_activeAnalysisTasks, analysisTask);
        return true;
    }

    return false;
}

ARATestAnalysisTask* ARATestDocumentController::getActiveAnalysisTaskForAudioSource (const ARATestAudioSource* audioSource) noexcept
{
    for (const auto& analysisTask : _activeAnalysisTasks)
    {
        if (analysisTask->getAudioSource () == audioSource)
            return analysisTask.get ();
    }
    return nullptr;
}

void ARATestDocumentController::processCompletedAnalysisTasks ()
{
    auto analysisTaskIt { _activeAnalysisTasks.begin () };
    while (analysisTaskIt != _activeAnalysisTasks.end ())
    {
        if (!(*analysisTaskIt)->isDone ())
        {
            ++analysisTaskIt;
            continue;
        }

        if (auto&& noteContent { (*analysisTaskIt)->transferNoteContent () })
        {
            auto audioSource { (*analysisTaskIt)->getAudioSource () };
            const auto algorithm { (*analysisTaskIt)->getProcessingAlgorithm () };
            audioSource->setProcessingAlgorithm (algorithm);
            audioSource->setNoteContent (std::move (noteContent), ARA::kARAContentGradeDetected, false);
            notifyAudioSourceContentChanged (audioSource, ARA::ContentUpdateScopes::notesAreAffected ());
            notifyAudioSourceDependentObjectsContentChanged (audioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        }

        analysisTaskIt = _activeAnalysisTasks.erase (analysisTaskIt);
    }
}

void ARATestDocumentController::notifyAudioSourceDependentObjectsContentChanged (ARATestAudioSource* audioSource, ARA::ContentUpdateScopes scopeFlags)
{
    for (auto& audioModification : audioSource->getAudioModifications ())
    {
        notifyAudioModificationContentChanged (audioModification, scopeFlags);

        for (auto& playbackRegion : audioModification->getPlaybackRegions ())
            notifyPlaybackRegionContentChanged (playbackRegion, scopeFlags);
    }
}

bool ARATestDocumentController::tryCopyHostNoteContent (ARATestAudioSource* audioSource)
{
    auto hostNoteReader { ARA::PlugIn::HostContentReader<ARA::kARAContentTypeNotes> (audioSource) };

    if (!hostNoteReader ||
        (hostNoteReader.getGrade () == ARA::kARAContentGradeInitial))
        return false;

    std::vector<TestNote> notes;
    notes.resize (static_cast<size_t> (hostNoteReader.getEventCount ()));
    for (const auto& hostNote : hostNoteReader)
        notes.emplace_back (TestNote { hostNote.frequency, hostNote.volume, hostNote.startPosition, hostNote.signalDuration });
    audioSource->setNoteContent (std::make_unique<TestNoteContent> (std::move (notes)), hostNoteReader.getGrade (), true);

    return true;
}

void ARATestDocumentController::updateAudioSourceAfterContentOrAlgorithmChanged (ARATestAudioSource* audioSource, bool hostChangedContent)
{
    // abort any currently ongoing analysis
#if !ARA_ALWAYS_PERFORM_ANALYSIS
    bool wasAnalyzing =
#endif
                        cancelAnalysisOfAudioSource (audioSource);

    // we only analyze note content, so if the host provides notes we can skip analysis
    bool notifyContentChanged;
    if (tryCopyHostNoteContent (audioSource))
    {
        notifyContentChanged = true;
    }
    else
    {
        // clear previous note content, triggering content change if data existed
        const bool hadNoteContent { audioSource->getNoteContent () != nullptr };
        audioSource->clearNoteContent ();
        notifyContentChanged = hadNoteContent;

        // (re-)start analysis if needed
#if !ARA_ALWAYS_PERFORM_ANALYSIS
        if (hadNoteContent || wasAnalyzing)
#endif
            startOrScheduleAnalysisOfAudioSource (audioSource);
    }

    if (notifyContentChanged)
    {
        if (!hostChangedContent)
            notifyAudioSourceContentChanged (audioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        notifyAudioSourceDependentObjectsContentChanged (audioSource, ARA::ContentUpdateScopes::notesAreAffected ());
    }
}

/*******************************************************************************/

void ARATestDocumentController::willNotifyModelUpdates () noexcept
{
    if (!isHostEditingDocument ())
        processCompletedAnalysisTasks ();
}

/*******************************************************************************/

void ARATestDocumentController::willBeginEditing () noexcept
{
    disableRendererModelGraphAccess ();
}

void ARATestDocumentController::didEndEditing () noexcept
{
    enableRendererModelGraphAccess ();

    auto audioSourceIt { _audioSourcesScheduledForAnalysis.begin () };
    while (audioSourceIt != _audioSourcesScheduledForAnalysis.end ())
    {
        if (!(*audioSourceIt)->isSampleAccessEnabled ())
        {
            ++audioSourceIt;
            continue;
        }

        startAnalysisTaskForAudioSource (*audioSourceIt);
        audioSourceIt = _audioSourcesScheduledForAnalysis.erase (audioSourceIt);
    }
}

/*******************************************************************************/

bool ARATestDocumentController::doRestoreObjectsFromArchive (ARA::PlugIn::HostArchiveReader* archiveReader, const ARA::PlugIn::RestoreObjectsFilter* filter) noexcept
{
    TestUnarchiver unarchiver { [archiveReader] (size_t position, size_t length, uint8_t buffer[]) noexcept -> bool
                                {
                                    return archiveReader->readBytesFromArchive (position, length, buffer);
                                }};

    const auto documentArchiveID { archiveReader->getDocumentArchiveID () };
    const bool isChunkArchive { (documentArchiveID != nullptr) ? std::strcmp (documentArchiveID, TEST_FILECHUNK_ARCHIVE_ID) == 0 : false };

    // loop over stored audio source data
    const auto numAudioSources { (isChunkArchive) ? 1 : unarchiver.readSize () };
    for (size_t i = 0; i < numAudioSources; ++i)
    {
        const float progressVal { static_cast<float> (i) / static_cast<float> (numAudioSources) };
        archiveReader->notifyDocumentUnarchivingProgress (progressVal);

        // read audio source persistent ID
        const auto persistentID { unarchiver.readString () };

        // read algorithm
        const auto algorithmID { unarchiver.readString () };

        // read note content (regarding file chunk content grade: storing a chunk for reuse implies "approving" it)
        const auto noteContentGrade { (isChunkArchive) ? ARA::kARAContentGradeApproved : static_cast<ARA::ARAContentGrade> (unarchiver.readInt64 ()) };
        const auto noteContentFromHost { (isChunkArchive) ? false : unarchiver.readBool () };
        std::unique_ptr<TestNoteContent> noteContent { decodeTestNoteContent (unarchiver) };

        // abort on reader error
        if (!unarchiver.didSucceed ())
            break;

        // find audio source to restore the state to (drop state if not to be loaded)
        auto testAudioSource { filter->getAudioSourceToRestoreStateWithID<ARATestAudioSource> (persistentID.c_str ()) };
        if (!testAudioSource)
            continue;

        // when restoring content, abort any currently running or scheduled analysis of the audio source
        cancelAnalysisOfAudioSource (testAudioSource);

        // set the algorithm from the restored persistent ID
        const auto algorithm { TestProcessingAlgorithm::getAlgorithmWithIdentifier (algorithmID) };
        ARA_INTERNAL_ASSERT (algorithm != nullptr);     // if we ever add or remove algorithms, we need some proper migration here
        testAudioSource->setProcessingAlgorithm (algorithm);

        // save restored result in model (no update notification to host sent here since this is expected upon successful restore)
        testAudioSource->setNoteContent (std::move (noteContent), noteContentGrade, noteContentFromHost);
    }

    archiveReader->notifyDocumentUnarchivingProgress (1.0f);

    return unarchiver.didSucceed ();
}

bool ARATestDocumentController::doStoreObjectsToArchive (ARA::PlugIn::HostArchiveWriter* archiveWriter, const ARA::PlugIn::StoreObjectsFilter* filter) noexcept
{
    // make sure to capture any pending analysis result
    processCompletedAnalysisTasks ();

    // create archiver
    TestArchiver archiver { [archiveWriter] (size_t position, size_t length, const uint8_t buffer[]) noexcept -> bool
                            {
                                return archiveWriter->writeBytesToArchive (position, length, buffer);
                            }};

    // this dummy implementation only deals with audio source states
    const auto& audioSourcesToPersist { filter->getAudioSourcesToStore<ARATestAudioSource> () };

    // write the number of audio sources we are persisting
    const auto numAudioSources { audioSourcesToPersist.size () };
    archiver.writeSize (numAudioSources);

    // loop over audio sources to persist
    for (size_t i { 0 }; i < numAudioSources; ++i)
    {
        const float progressVal { static_cast<float> (i) / static_cast<float> (numAudioSources) };
        archiveWriter->notifyDocumentArchivingProgress (progressVal);

        // write persistent ID
        archiver.writeString (audioSourcesToPersist[i]->getPersistentID ());

        // write algorithm
        archiver.writeString (audioSourcesToPersist[i]->getProcessingAlgorithm ()->getIdentifier ());

        // write note content
        archiver.writeInt64 (audioSourcesToPersist[i]->getNoteContentGrade ());
        archiver.writeBool (audioSourcesToPersist[i]->getNoteContentWasReadFromHost ());
        encodeTestNoteContent (audioSourcesToPersist[i]->getNoteContent (), archiver);
    }
    archiveWriter->notifyDocumentArchivingProgress (1.0f);

    return archiver.didSucceed ();
}

/*******************************************************************************/
void ARATestDocumentController::doUpdateMusicalContextContent (ARA::PlugIn::MusicalContext* musicalContext, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
#if ARA_ENABLE_DEBUG_OUTPUT
    ARA::ContentLogger::logUpdatedContent (*getHostContentAccessController (), musicalContext->getHostRef (), range, scopeFlags);
#endif
}

/*******************************************************************************/

ARA::PlugIn::AudioSource* ARATestDocumentController::doCreateAudioSource (ARA::PlugIn::Document* document, ARA::ARAAudioSourceHostRef hostRef) noexcept
{
    // create a new audio source, then check for host content and if that's not available start analysis
    auto testAudioSource { new ARATestAudioSource (document, hostRef) };
    if (!tryCopyHostNoteContent (testAudioSource))
    {
#if ARA_ALWAYS_PERFORM_ANALYSIS
        startOrScheduleAnalysisOfAudioSource (testAudioSource);
#endif
    }
    return testAudioSource;
}

void ARATestDocumentController::willUpdateAudioSourceProperties (ARA::PlugIn::AudioSource* audioSource, ARA::PlugIn::PropertiesPtr<ARA::ARAAudioSourceProperties> newProperties) noexcept
{
    if ((audioSource->getSampleRate () != newProperties->sampleRate) ||
        (audioSource->getSampleCount () != newProperties->sampleCount) ||
        (audioSource->getChannelCount () != newProperties->channelCount))
    {
        // no need to trigger updateRenderSampleCache () here, since host is required to
        // disable sample access when changing channel or sample count, which will always update the cache.
        // any potential analysis of the audio source also would have be cancelled already when disabling acces.

        // if we have a self-analyzed content, clear it and schedule reanalysis
        // (actual plug-ins may instead be able to create a new result based on the old one)
        auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };
        if (!testAudioSource->getNoteContentWasReadFromHost ())
            updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource, false);
    }
}

void ARATestDocumentController::doUpdateAudioSourceContent (ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
#if ARA_ENABLE_DEBUG_OUTPUT
    ARA::ContentLogger::logUpdatedContent (*getHostContentAccessController (), audioSource->getHostRef (), range, scopeFlags);
#endif

    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };

    if (scopeFlags.affectSamples () && testAudioSource->isSampleAccessEnabled ())
        testAudioSource->updateRenderSampleCache ();

    if (scopeFlags.affectNotes ())
        updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource, true);
}

void ARATestDocumentController::willEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    // if disabling access to the given audio source while analyzing,
    // we'll abort and restart the analysis when re-enabling access
    if (!enable)
    {
        auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };
        if (cancelAnalysisTaskForAudioSource (testAudioSource))
            _audioSourcesScheduledForAnalysis.insert (testAudioSource);
    }

    // make sure renderers will not access the audio source while its state changes -
    // if being edited, renderers have already been disabled, otherwise do so now.
    if (!isHostEditingDocument ())
        disableRendererModelGraphAccess ();
}

void ARATestDocumentController::didEnableAudioSourceSamplesAccess (ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept
{
    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };

    if (enable)
        testAudioSource->updateRenderSampleCache ();

    if (!isHostEditingDocument ())
        enableRendererModelGraphAccess ();

    // if enabling access, restart any pending analysis if host is not currently editing
    // the document (otherwise done in didEndEditing ())
    if (!isHostEditingDocument () && enable)
    {
        if (_audioSourcesScheduledForAnalysis.count (testAudioSource) != 0)
        {
            startAnalysisTaskForAudioSource (testAudioSource);
            _audioSourcesScheduledForAnalysis.erase (testAudioSource);
        }
    }
}

void ARATestDocumentController::didDeactivateAudioSourceForUndoHistory (ARA::PlugIn::AudioSource* audioSource, bool deactivate) noexcept
{
    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };
    if (deactivate)
    {
        cancelAnalysisOfAudioSource (testAudioSource);
        testAudioSource->destroyRenderSampleCache ();
    }
    else
    {
        if (testAudioSource->isSampleAccessEnabled ())
            testAudioSource->updateRenderSampleCache ();
    }
}

void ARATestDocumentController::willDestroyAudioSource (ARA::PlugIn::AudioSource* audioSource) noexcept
{
    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };

    cancelAnalysisOfAudioSource (testAudioSource);
}

/*******************************************************************************/

bool ARATestDocumentController::doIsAudioSourceContentAvailable (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept
{
    if (type == ARA::kARAContentTypeNotes)
    {
        processCompletedAnalysisTasks ();
        //! \todo is there any elegant way to avoid all those up-casts from ARA::PlugIn::AudioSource* to ARATestAudioSource* in this file?
        return (static_cast<const ARATestAudioSource*> (audioSource)->getNoteContent () != nullptr);
    }
    return false;
}

ARA::ARAContentGrade ARATestDocumentController::doGetAudioSourceContentGrade (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type) noexcept
{
    if (doIsAudioSourceContentAvailable (audioSource, type))
        return static_cast<const ARATestAudioSource*> (audioSource)->getNoteContentGrade ();
    return ARA::kARAContentGradeInitial;
}

ARA::PlugIn::ContentReader* ARATestDocumentController::doCreateAudioSourceContentReader (ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept
{
    if (type == ARA::kARAContentTypeNotes)
        return new ARATestNoteContentReader (static_cast<const ARATestAudioSource*> (audioSource), range);
    return nullptr;
}

bool ARATestDocumentController::doIsAudioModificationContentAvailable (const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept
{
    // since this demo plug-in does not allow for modifying the content, we can directly forward the audio source data
    return doIsAudioSourceContentAvailable (audioModification->getAudioSource (), type);
}

ARA::ARAContentGrade ARATestDocumentController::doGetAudioModificationContentGrade (const ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type) noexcept
{
    // since this demo plug-in does not allow for modifying the content, we can directly forward the audio source data
    return doGetAudioSourceContentGrade (audioModification->getAudioSource (), type);
}

ARA::PlugIn::ContentReader* ARATestDocumentController::doCreateAudioModificationContentReader (ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept
{
    if (type == ARA::kARAContentTypeNotes)
        return new ARATestNoteContentReader (audioModification, range);
    return nullptr;
}

bool ARATestDocumentController::doIsPlaybackRegionContentAvailable (const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept
{
    // since this demo plug-in plays back all modification data as is (no time stretching etc.),
    // we can directly forward the audio modification data
    return doIsAudioModificationContentAvailable (playbackRegion->getAudioModification (), type);
}

ARA::ARAContentGrade ARATestDocumentController::doGetPlaybackRegionContentGrade (const ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type) noexcept
{
    // since this demo plug-in plays back all modification data as is (no time stretching etc.),
    // we can directly forward the audio modification data
    return doGetAudioModificationContentGrade (playbackRegion->getAudioModification (), type);
}

ARA::PlugIn::ContentReader* ARATestDocumentController::doCreatePlaybackRegionContentReader (ARA::PlugIn::PlaybackRegion* playbackRegion, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept
{
    if (type == ARA::kARAContentTypeNotes)
        return new ARATestNoteContentReader (playbackRegion, range);
    return nullptr;
}

/*******************************************************************************/

void ARATestDocumentController::doRequestAudioSourceContentAnalysis (ARA::PlugIn::AudioSource* audioSource, std::vector<ARA::ARAContentType> const& ARA_MAYBE_UNUSED_ARG (contentTypes)) noexcept
{
    ARA_INTERNAL_ASSERT (contentTypes.size () == 1);
    ARA_INTERNAL_ASSERT (contentTypes[0] == ARA::kARAContentTypeNotes);

    processCompletedAnalysisTasks ();

    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };

    if (testAudioSource->getNoteContentWasReadFromHost ())
        testAudioSource->clearNoteContent ();

    if ((testAudioSource->getNoteContent () == nullptr) ||
        (testAudioSource->getNoteContentGrade () == ARA::kARAContentGradeInitial))
        startOrScheduleAnalysisOfAudioSource (testAudioSource);
}

bool ARATestDocumentController::doIsAudioSourceContentAnalysisIncomplete (const ARA::PlugIn::AudioSource* audioSource, ARA::ARAContentType ARA_MAYBE_UNUSED_ARG (type)) noexcept
{
    ARA_INTERNAL_ASSERT (type == ARA::kARAContentTypeNotes);

    processCompletedAnalysisTasks ();

    const auto testAudioSource { static_cast<const ARATestAudioSource*> (audioSource) };
    return testAudioSource->getNoteContent () == nullptr;
}

ARA::ARAInt32 ARATestDocumentController::doGetProcessingAlgorithmsCount () noexcept
{
    return static_cast<ARA::ARAInt32> (AlgorithmPropertiesWrapper::getAlgorithmProperties ().size ());
}

const ARA::ARAProcessingAlgorithmProperties* ARATestDocumentController::doGetProcessingAlgorithmProperties (ARA::ARAInt32 algorithmIndex) noexcept
{
    return AlgorithmPropertiesWrapper::getAlgorithmProperties ()[static_cast<uint32_t> (algorithmIndex)];
}

ARA::ARAInt32 ARATestDocumentController::doGetProcessingAlgorithmForAudioSource (const ARA::PlugIn::AudioSource* audioSource) noexcept
{
    return AlgorithmPropertiesWrapper::getIndexOfAlgorithm (static_cast<const ARATestAudioSource*> (audioSource)->getProcessingAlgorithm ());
}

void ARATestDocumentController::doRequestProcessingAlgorithmForAudioSource (ARA::PlugIn::AudioSource* audioSource, ARA::ARAInt32 algorithmIndex) noexcept
{
    auto testAudioSource { static_cast<ARATestAudioSource*> (audioSource) };
    const auto algorithm = AlgorithmPropertiesWrapper::getAlgorithmProperties ()[static_cast<uint32_t> (algorithmIndex)].getAlgorithm ();

    if (testAudioSource->getProcessingAlgorithm () != algorithm)
    {
        testAudioSource->setProcessingAlgorithm (algorithm);

        // if we have a self-analyzed content, clear it and schedule reanalysis with new algorithm if needed
        // (actual plug-ins may instead always need to perform a new analysis if their internal result
        // representation depends on the processing algorithm)
        if (!testAudioSource->getNoteContentWasReadFromHost ())
            updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource, false);
    }
}

/*******************************************************************************/

ARA::PlugIn::PlaybackRenderer* ARATestDocumentController::doCreatePlaybackRenderer () noexcept
{
    return new ARATestPlaybackRenderer (this);
}

/*******************************************************************************/

bool ARATestDocumentController::rendererWillAccessModelGraph (ARATestPlaybackRenderer* /*playbackRenderer*/) noexcept
{
    ++_countOfRenderersCurrentlyAccessingModelGraph;
    return _renderersCanAccessModelGraph;
}

void ARATestDocumentController::rendererDidAccessModelGraph (ARATestPlaybackRenderer* /*playbackRenderer*/) noexcept
{
    ARA_INTERNAL_ASSERT (_countOfRenderersCurrentlyAccessingModelGraph > 0);
    --_countOfRenderersCurrentlyAccessingModelGraph;
}

void ARATestDocumentController::disableRendererModelGraphAccess () noexcept
{
#if (__cplusplus >= 201703L)
    static_assert (decltype (_renderersCanAccessModelGraph)::is_always_lock_free);
    static_assert (decltype (_countOfRenderersCurrentlyAccessingModelGraph)::is_always_lock_free);
#else
    ARA_INTERNAL_ASSERT (_renderersCanAccessModelGraph.is_lock_free ());
    ARA_INTERNAL_ASSERT (_countOfRenderersCurrentlyAccessingModelGraph.is_lock_free ());
#endif

    ARA_INTERNAL_ASSERT (_renderersCanAccessModelGraph);
    _renderersCanAccessModelGraph = false;

    while (_countOfRenderersCurrentlyAccessingModelGraph)
        {};    // spin until all concurrent renderer calls have completed
}

void ARATestDocumentController::enableRendererModelGraphAccess () noexcept
{
    ARA_INTERNAL_ASSERT (!_renderersCanAccessModelGraph);
    _renderersCanAccessModelGraph = true;
}

/*******************************************************************************/

static constexpr std::array<ARA::ARAContentType, 1> analyzeableContentTypes { ARA::kARAContentTypeNotes };

class ARATestFactoryConfig : public ARA::PlugIn::FactoryConfig
{
public:
    const char* getFactoryID () const noexcept override { return TEST_FACTORY_ID; }
    const char* getPlugInName () const noexcept override { return TEST_PLUGIN_NAME; }
    const char* getManufacturerName () const noexcept override { return TEST_MANUFACTURER_NAME; }
    const char* getInformationURL () const noexcept override { return TEST_INFORMATION_URL; }
    const char* getVersion () const noexcept override { return TEST_VERSION_STRING; }

    const char* getDocumentArchiveID () const noexcept override { return TEST_DOCUMENT_ARCHIVE_ID; }

    ARA::ARASize getAnalyzeableContentTypesCount () const noexcept override  { return analyzeableContentTypes.size (); }
    const ARA::ARAContentType* getAnalyzeableContentTypes () const noexcept override { return  analyzeableContentTypes.data (); }
    ARA::ARASize getCompatibleDocumentArchiveIDsCount () const noexcept override { return 1; }
    const ARA::ARAPersistentID* getCompatibleDocumentArchiveIDs () const noexcept override { static const auto id { TEST_FILECHUNK_ARCHIVE_ID }; return &id; }

    bool supportsStoringAudioFileChunks () const noexcept override { return true; }
};

const ARA::ARAFactory* ARATestDocumentController::getARAFactory () noexcept
{
    return ARA::PlugIn::PlugInEntry::getPlugInEntry<ARATestFactoryConfig, ARATestDocumentController> ()->getFactory ();
}
