//------------------------------------------------------------------------------
//! \file       ARATestDocumentController.cpp
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

#include "ARATestDocumentController.h"
#include "ARATestAudioSource.h"
#include "ARATestPlaybackRenderer.h"
#include "TestPersistency.h"
#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#include "ARA_Library/Debug/ARAContentLogger.h"

#include <array>
#include <map>


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
// behaviour of actual plug-ins like Melodyne, and also allows for testing analysis and related
// notifications in hosts that never request audio source analysis.
#if !defined (ARA_ALWAYS_PERFORM_ANALYSIS)
    #define ARA_ALWAYS_PERFORM_ANALYSIS 0
#endif


ARA_SETUP_DEBUG_MESSAGE_PREFIX(ARA_PLUGIN_NAME);

/*******************************************************************************/

ARATestNoteContentReader::ARATestNoteContentReader (const ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* range)
: ARATestNoteContentReader { audioSource, (range) ? *range : ARA::ARAContentTimeRange { 0.0, audioSource->getDuration () } }
{}

ARATestNoteContentReader::ARATestNoteContentReader (const ARA::PlugIn::AudioModification* audioModification, const ARA::ARAContentTimeRange* range)
// actual plug-ins will take the modification data into account instead of simply forwarding to the audio source detection data
: ARATestNoteContentReader { audioModification->getAudioSource (), (range) ? *range : ARA::ARAContentTimeRange { 0.0, audioModification->getAudioSource ()->getDuration () } }
{}

ARATestNoteContentReader::ARATestNoteContentReader (const ARA::PlugIn::PlaybackRegion* playbackRegion, const ARA::ARAContentTimeRange* range)
// actual plug-ins will take the modification data and the full region transformation into account instead of simply forwarding to the audio source detection data
: ARATestNoteContentReader { playbackRegion->getAudioModification ()->getAudioSource (),
                             (range) ? *range : ARA::ARAContentTimeRange { playbackRegion->getStartInPlaybackTime (), playbackRegion->getDurationInPlaybackTime () },
                             playbackRegion->getStartInPlaybackTime () - playbackRegion->getStartInAudioModificationTime () }
{}

ARATestNoteContentReader::ARATestNoteContentReader (const ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange& range, double timeOffset)
{
    //! \todo is there any elegant way to avoid all those up-casts from ARA::PlugIn::AudioSource* to ARATestAudioSource* in this file?
    auto testAudioSource { static_cast<const ARATestAudioSource*> (audioSource) };
    ARA_INTERNAL_ASSERT (testAudioSource->getNoteContent () != nullptr);
    for (const auto& note : *testAudioSource->getNoteContent ())
    {
        if ((range.start - timeOffset < note._startTime + note._duration) &&
            (range.start + range.duration - timeOffset >= note._startTime))
        {
            ARA::ARAContentNote exportedNote;
            exportedNote.frequency = note._frequency;
            if (exportedNote.frequency == ARA::kARAInvalidFrequency)
                exportedNote.pitchNumber = ARA::kARAInvalidPitchNumber;
            else
                exportedNote.pitchNumber = static_cast<ARA::ARAPitchNumber> (floor (0.5f + 69.0f + 12.0f * logf (exportedNote.frequency / 440.0f) / logf (2.0f)));
            exportedNote.volume = note._volume;
            exportedNote.startPosition = note._startTime + timeOffset;
            exportedNote.attackDuration = 0.0;
            exportedNote.noteDuration = note._duration;
            exportedNote.signalDuration = note._duration;
            _exportedNotes.push_back (exportedNote);
        }
    }
}

ARA::ARAInt32 ARATestNoteContentReader::getEventCount () noexcept
{
    return static_cast<ARA::ARAInt32> (_exportedNotes.size ());
}

const void* ARATestNoteContentReader::getDataForEvent (ARA::ARAInt32 eventIndex) noexcept
{
    return &_exportedNotes[static_cast<size_t> (eventIndex)];
}

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
        const auto it { std::find_if (begin, end,   [algorithm](const AlgorithmPropertiesWrapper& props)
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
    _activeAnalysisTasks.emplace_back (std::make_unique<TestAnalysisTask> (audioSource, algorithm));
}

bool ARATestDocumentController::cancelAnalysisTaskForAudioSource (ARATestAudioSource* audioSource)
{
    if (TestAnalysisTask* analysisTask { getActiveAnalysisTaskForAudioSource (audioSource) })
    {
        analysisTask->cancelSynchronously ();
        ARA::find_erase (_activeAnalysisTasks, analysisTask);
        return true;
    }

    return false;
}

TestAnalysisTask* ARATestDocumentController::getActiveAnalysisTaskForAudioSource (const ARATestAudioSource* audioSource) noexcept
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
            notifyAudioSourceDependendObjectsContentChanged (audioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        }

        analysisTaskIt = _activeAnalysisTasks.erase (analysisTaskIt);
    }
}

void ARATestDocumentController::notifyAudioSourceDependendObjectsContentChanged (ARATestAudioSource* audioSource, ARA::ContentUpdateScopes scopeFlags)
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
        notes.emplace_back ( TestNote { hostNote.frequency, hostNote.volume, hostNote.startPosition, hostNote.signalDuration } );
    audioSource->setNoteContent (std::make_unique<TestNoteContent> (std::move (notes)), hostNoteReader.getGrade (), true);

    return true;
}

bool ARATestDocumentController::updateAudioSourceAfterContentOrAlgorithmChanged (ARATestAudioSource* audioSource)
{
    // if already analyzing this audio source, abort and flag the need to restart analysis
    bool reanalyze { cancelAnalysisOfAudioSource (audioSource) };

    // clear previous note content but store whether it was present
    const bool hadNoteContent { audioSource->getNoteContent () != nullptr };
    audioSource->clearNoteContent ();

    bool notifyContentChanged = hadNoteContent;

    // we only analyze note content, so if the host provides notes we can skip analysis
    if (tryCopyHostNoteContent (audioSource))
    {
        notifyContentChanged = true;
        reanalyze = false;
    }
    else
    {
#if !ARA_ALWAYS_PERFORM_ANALYSIS
        if (hadNoteContent)
#endif
            reanalyze = true;
    }

    if (reanalyze)
        startOrScheduleAnalysisOfAudioSource (audioSource);

    return notifyContentChanged;
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
    // start by reading the number of audio sources stored in the archive
    TestUnarchiver unarchiver (archiveReader);
    const auto numAudioSources { unarchiver.readSize () };

    // loop over stored audio source data
    for (size_t i = 0; i < numAudioSources; ++i)
    {
        const float progressVal { static_cast<float> (i) / static_cast<float> (numAudioSources) };
        archiveReader->notifyDocumentUnarchivingProgress (progressVal);

        // read audio source persistent ID
        const auto persistentID { unarchiver.readString () };

        // read algorithm
        const auto algorithmID { unarchiver.readString () };

        // read note content
        const auto noteContentGrade { static_cast<ARA::ARAContentGrade> (unarchiver.readInt64 ()) };
        const auto noteContentFromHost { unarchiver.readBool () };

        std::unique_ptr<TestNoteContent> noteContent;
        const bool hasNoteContent { unarchiver.readBool () };
        if (hasNoteContent)
        {
            const auto numNotes { unarchiver.readSize () };
            noteContent = std::make_unique<TestNoteContent> (numNotes);
            for (TestNote& persistedNote : *noteContent)
            {
                persistedNote._frequency = static_cast<float> (unarchiver.readDouble ());
                persistedNote._volume = static_cast<float> (unarchiver.readDouble ());
                persistedNote._startTime = unarchiver.readDouble ();
                persistedNote._duration = unarchiver.readDouble ();
            }
        }

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
    TestArchiver archiver (archiveWriter);

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

        const auto analysisResult { audioSourcesToPersist[i]->getNoteContent () };
        archiver.writeBool (analysisResult != nullptr);
        if (analysisResult != nullptr)
        {
            const auto numNotes { analysisResult->size () };
            archiver.writeSize (numNotes);
            for (const auto& noteToPersist : *analysisResult)
            {
                archiver.writeDouble (noteToPersist._frequency);
                archiver.writeDouble (noteToPersist._volume);
                archiver.writeDouble (noteToPersist._startTime);
                archiver.writeDouble (noteToPersist._duration);
            }
        }
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
        if (!testAudioSource->getNoteContentWasReadFromHost () &&
            updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource))
        {
            notifyAudioSourceContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
            notifyAudioSourceDependendObjectsContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        }
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

    if (scopeFlags.affectNotes () &&
        updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource))
    {
        if (!scopeFlags.affectNotes ())
            notifyAudioSourceContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        notifyAudioSourceDependendObjectsContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
    }
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
        return new ARATestNoteContentReader (audioSource, range);
    return nullptr;
}

ARA::PlugIn::ContentReader* ARATestDocumentController::doCreateAudioModificationContentReader (ARA::PlugIn::AudioModification* audioModification, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept
{
    if (type == ARA::kARAContentTypeNotes)
        return new ARATestNoteContentReader (audioModification, range);
    return nullptr;
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
        if (!testAudioSource->getNoteContentWasReadFromHost () &&
            updateAudioSourceAfterContentOrAlgorithmChanged (testAudioSource))
        {
            notifyAudioSourceContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
            notifyAudioSourceDependendObjectsContentChanged (testAudioSource, ARA::ContentUpdateScopes::notesAreAffected ());
        }
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
    const char* getFactoryID () const noexcept override { return "com.arademocompany.testplugin.arafactory"; }
    const char* getPlugInName () const noexcept override { return ARA_PLUGIN_NAME; }
    const char* getManufacturerName () const noexcept override { return ARA_MANUFACTURER_NAME; }
    const char* getInformationURL () const noexcept override { return ARA_INFORMATION_URL; }
    const char* getVersion () const noexcept override { return ARA_VERSION_STRING; }

    const char* getDocumentArchiveID () const noexcept override { return "com.arademocompany.testplugin.aradocumentarchive.version1"; }

    ARA::ARASize getAnalyzeableContentTypesCount () const noexcept override  { return analyzeableContentTypes.size (); }
    const ARA::ARAContentType* getAnalyzeableContentTypes () const noexcept override { return  analyzeableContentTypes.data (); }
};

const ARA::ARAFactory* ARATestDocumentController::getARAFactory () noexcept
{
    return ARA::PlugIn::PlugInEntry::getPlugInEntry<ARATestFactoryConfig, ARATestDocumentController> ()->getFactory ();
}
