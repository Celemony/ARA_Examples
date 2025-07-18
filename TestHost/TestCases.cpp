//------------------------------------------------------------------------------
//! \file       TestCases.cpp
//!             various tests simulating user interaction with the TestHost
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2025, Celemony Software GmbH, All Rights Reserved.
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

#include "TestCases.h"
#include "TestHost.h"
#include "ARAHostInterfaces/ARAAudioAccessController.h"

#include "ARA_Library/Utilities/ARASamplePositionConversion.h"
#include "ARA_Library/Utilities/ARAStdVectorUtilities.h"

#include "ARA_API/ARAAudioFileChunks.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <thread>


// in this test application, we want assertions and logging to be always enabled, even in release builds.
// this needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif
#if !ARA_VALIDATE_API_CALLS
    #error "ARA_VALIDATE_API_CALLS not configured properly in the project"
#endif

#define ARA_LOG_TEST_HOST_FUNC(funcName) do { ARA_LOG (""); ARA_LOG ("*** testing %s ***", funcName); ARA_LOG (""); } while (0)


// Helper function to create dummy audio file representations that play back a pulsed sine signal.
AudioFileList createDummyAudioFiles (size_t numFiles)
{
    // add an audio source with 5 seconds of single channel audio with a sample rate of 44100
    AudioFileList dummyFiles;
    for (size_t i { 0 }; i < numFiles; ++i)
        dummyFiles.emplace_back (new SineAudioFile ("Sin Source " + std::to_string (i), 5.0, 44100.0, 1));
    return dummyFiles;
}

/*******************************************************************************/
// Using the supplied binary, this function creates an instance of the TestHost with a document
// that contains a musical context with one region sequence. Per file provided in the file list,
// an audio source with a single audio modification is created, and a playback region covering
// the entire audio modification is placed on the region sequence.
// We can optionally request the plug-in to perform its audio source analysis immediately and
// block until analysis completes, or time-stretch the region if the plug-in supports this.
ARADocumentController* createHostAndBasicDocument (PlugInEntry* plugInEntry, std::unique_ptr<TestHost>& testHost, std::string documentName, bool requestPlugInAnalysisAndBlock, const AudioFileList& audioFiles)
{
    // create our ARA host and document
    if (testHost == nullptr)
        testHost = std::make_unique<TestHost> ();

    auto document { testHost->addDocument (documentName, plugInEntry) };
    auto araDocumentController { testHost->getDocumentController (document) };

    if (requestPlugInAnalysisAndBlock)
        araDocumentController->setMinimalContentUpdateLogging (true);

    // begin the document edit cycle to configure the document
    araDocumentController->beginEditing ();

    // add a musical context and describe our timeline
    auto musicalContext { testHost->addMusicalContext (document, "ARA Test Musical Context", { 1.0f, 0.0f, 0.0f }) };

    // add a region sequence to describe our arrangement with a single track
    auto regionSequence { testHost->addRegionSequence (document, "Track 1", musicalContext, { 0.0f, 1.0f, 0.0f }) };

    double position { 0.0 };
    for (size_t i { 0 }; i < audioFiles.size (); ++i)
    {
        // add an audio source based on the audio file
        const std::string audioSourcePersistentID { "audioSourceTestPersistentID " + std::to_string (i) };
        auto audioSource { testHost->addAudioSource (document, audioFiles[i].get (), audioSourcePersistentID) };

        // add an audio modification associated with the audio source
        const std::string audioModificationName { "Test audio modification " + std::to_string (i) };
        const std::string audioModificationPersistentID { "audioModificationTestPersistentID " + std::to_string (i) };
        auto audioModification { testHost->addAudioModification (document, audioSource, audioModificationName, audioModificationPersistentID) };

        // add a playback region encompassing the entire audio source to render modifications in our musical context,
        // enabling time stretching if requested & supported
        const auto duration { audioSource->getDuration () };
        testHost->addPlaybackRegion (document, audioModification, ARA::kARAPlaybackTransformationNoChanges, 0.0, duration, position, duration, regionSequence, "Test playback region", { 0.0f, 0.0f, 1.0f });
        position += duration;
    }

    // end the document edit cycle
    araDocumentController->endEditing ();

    // enable audio source samples access and
    // request the analysis for all available content types if this plug-in has any
    const auto araFactory { plugInEntry->getARAFactory () };
    for (auto& audioSource : document->getAudioSources ())
    {
        araDocumentController->enableAudioSourceSamplesAccess (audioSource.get (), true);

        if (requestPlugInAnalysisAndBlock && araFactory->analyzeableContentTypesCount > 0)
            araDocumentController->requestAudioSourceContentAnalysis (audioSource.get (), araFactory->analyzeableContentTypesCount, araFactory->analyzeableContentTypes, true);
    }

    if (requestPlugInAnalysisAndBlock)
        araDocumentController->setMinimalContentUpdateLogging (false);

    return araDocumentController;
}

/*******************************************************************************/
// Demonstrates updating several properties of ARA model graph objects within an edit cycle
// (note: in an actual application, these updates would likely be spread across individual cycles)
void testPropertyUpdates (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("property updates");

    // create basic ARA model graph
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testPropertyUpdates", false, audioFiles) };
    auto document { araDocumentController->getDocument () };

    // begin an ARA document edit cycle
    araDocumentController->beginEditing ();

    // update the name of the first audio source
    // flush the updated properties to the ARA graph using the document controller
    auto& audioSource { document->getAudioSources ().front () };
    ARA_LOG ("Updating the name of audio source %p (ARAAudioSourceRef %p)", audioSource.get (), araDocumentController->getRef (audioSource.get ()));
    audioSource->setName ("Updated Audio Source Name");
    araDocumentController->updateAudioSourceProperties (audioSource.get ());

    // update the color of the first region sequence
    auto& regionSequence { document->getRegionSequences ().front () };
    ARA_LOG ("Updating the color of region sequence %p (ARARegionSequenceRef %p)", regionSequence.get (), araDocumentController->getRef (regionSequence.get ()));
    regionSequence->setColor ({ 1.0f, 1.0f, 0.0f });
    araDocumentController->updateRegionSequenceProperties (regionSequence.get ());

    // move the start time of the first playback region in regionSequence ahead by one second
    auto& playbackRegion { regionSequence->getPlaybackRegions ().front () };
    const auto newStartTime { 1.0 + playbackRegion->getStartInPlaybackTime () };
    ARA_LOG ("Updating the start time of playback region %p (ARAPlaybackRegionRef %p)", playbackRegion, araDocumentController->getRef (playbackRegion));
    playbackRegion->setStartInPlaybackTime (newStartTime);
    araDocumentController->updatePlaybackRegionProperties (playbackRegion);

    // end the edit cycle once we're done updating the properties
    araDocumentController->endEditing ();
}

/*******************************************************************************/
// Demonstrates how to update content information if changed in the host
// The plug-in will call back into the host's ARAContentAccessController implementation
// to read the updated data - see ARAContentAccessController
void testContentUpdates (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("content updates");

    // create basic ARA model graph
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testContentUpdates", false, audioFiles) };
    const auto document { araDocumentController->getDocument () };

    // give our musical context some tempo and bar signature entries
    auto& musicalContext { document->getMusicalContexts ().front () };
    auto& audioSource { document->getAudioSources ().front () };

    const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { 0.0, 0.0 }, { 0.5, 1.0 } };
    // here are some more valid timelines you can use for testing your implementation:
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -0.5, -1.0 }, { 0.0, 0.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -1.0, -2.0 }, { -0.5, -1.0 }, { 0.0, 0.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -0.5, -1.0 }, { 0.0, 0.0 }, { 0.5, 1.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 2.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -1.0, -2.0 }, { -0.5, -1.0 }, { 0.0, 0.0 }, { 0.5, 1.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -0.5, -1.0 }, { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 2.0 } };
    //const std::vector<ARA::ARAContentTempoEntry> tempoEntries { { -1.0, -2.0 }, { -0.5, -1.0 }, { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 2.0 } };

    const std::vector<ARA::ARAContentBarSignature> barSignatures { { 4, 4, 0.0 } };
    //const std::vector<ARA::ARAContentBarSignature> barSignatures { { 3, 4, -5.0 }, { 7, 8, 10.0 } };

    const ARA::ARAContentTuning tuning { 442.0f, 2, { 0.0f, 0.0f, 0.0f, 0.0f, -50.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -50.0f }, "Arabian Rast" };
    //const ARA::ARAContentTuning tuning { 440.0f, 0, { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, "Equal Temperament" };

    const std::vector<ARA::ARAContentKeySignature> keySignatures { { -1, { 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00 }, "F Minor", 0.0 },
                                                                   {  2, { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF }, "D Major", 4.0 },
                                                                   { -2, { 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00 }, "Bb Dorian", 8.0 },
                                                                   {  0, { 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00 }, "C Phrygian", 12.0 },
                                                                   {  3, { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF }, "A Lydian", 16.0 },
                                                                   {  1, { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00 }, "G Mixolydian", 20.0 },
                                                                   {  4, { 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00 }, "E Locrian", 24.0 },
                                                                   { -3, { 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF }, "Eb Harmonic Minor", 28.0 },
                                                                   { -4, { 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF }, "Ab Melodic Minor", 32.0 },
                                                                   {  0, { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 }, "C Pentatonic Major", 36.0 },
                                                                   {  0, { 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00 }, "C Pentatonic Minor", 40.0 },
                                                                   { -1, { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00 }, "F Whole Tone", 44.0 },
                                                                   {  0, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, "Chromatic",  48.0 } };

    const std::vector<ARA::ARAContentChord> chords { {  1,  2, { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00 }, "G/D", 0.0 },
                                                     { -1, -1, { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF }, "Fmaj7", 4.0 },
                                                     {  0,  4, { 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00 }, "Csus2/E", 8.0 },
                                                     {  0,  4, { 0x01, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00 }, "C5add9/E", 12.0 },
                                                     {  0,  4, { 0x01, 0x00, 0x09, 0x00, 0x03, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00 }, "Cadd9/E", 16.0 },
                                                     {  6,  6, { 0x01, 0x00, 0x09, 0x00, 0x03, 0x00, 0x00, 0x05, 0x00, 0x0D, 0x07, 0x00 }, "F#13", 20.0 },
                                                     {  6,  6, { 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x05, 0x00, 0x0D, 0x00, 0x00 }, "F#add13", 24.0 },
                                                     {  6,  6, { 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00 }, "F#6", 28.0 },
                                                     {  6,  6, { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 }, "F#6", 32.0 } };

    std::vector<ARA::ARAContentNote> notes (12);
    for (size_t i { 0 }; i < notes.size (); ++i)
    {
        notes[i].pitchNumber = static_cast<ARA::ARAPitchNumber> (60 + i);
        notes[i].frequency = (440.0f * powf (2.0f, (static_cast<float> (notes[i].pitchNumber) - 69.0f) / 12.0f));
        notes[i].volume = 0.5f + static_cast<float> (i) * 0.05f;
        notes[i].startPosition = (static_cast<double> (i) * audioSource->getDuration ()) / static_cast<double> (notes.size ());
        notes[i].signalDuration = audioSource->getDuration () / static_cast<double> (notes.size ());
        notes[i].noteDuration = notes[i].signalDuration / 2.0;
    }

    ARA_LOG ("Updating musical context %p (ARAMusicalContextRef %p) with new tempo, bar signature, tuning, key signature, and chord data", musicalContext.get (), araDocumentController->getRef (musicalContext.get ()));
    constexpr auto musicalContextUpdateScope { ARA::ContentUpdateScopes::timelineIsAffected () +
                                               ARA::ContentUpdateScopes::harmoniesAreAffected () +
                                               ARA::ContentUpdateScopes::tuningIsAffected () };
    araDocumentController->beginEditing ();
    musicalContext->setTempoEntries (tempoEntries);
    musicalContext->setBarSignatures (barSignatures);
    musicalContext->setTuning (tuning);
    musicalContext->setKeySignatures (keySignatures);
    musicalContext->setChords (chords);
    araDocumentController->updateMusicalContextContent (musicalContext.get (), nullptr, musicalContextUpdateScope);
    araDocumentController->endEditing ();

    ARA_LOG ("Updating audio source %p (ARAAudioSourceRef %p) with new notes, tempo, bar signature, tuning, key signature, and chord data", audioSource.get (), araDocumentController->getRef (audioSource.get ()));
    constexpr auto audioSourceUpdateScope { musicalContextUpdateScope + ARA::ContentUpdateScopes::notesAreAffected () };
    araDocumentController->beginEditing ();
    audioSource->setNotes (notes);
    audioSource->setTempoEntries (tempoEntries);
    audioSource->setBarSignatures (barSignatures);
    audioSource->setTuning (tuning);
    audioSource->setKeySignatures (keySignatures);
    audioSource->setChords (chords);
    araDocumentController->updateAudioSourceContent (audioSource.get (), nullptr, audioSourceUpdateScope);
    araDocumentController->endEditing ();
}

/*******************************************************************************/
// Demonstrates how to read ARAContentTypes from a plug-in -
// see ContentLogger::log () for implementation of the actual content reading
void testContentReading (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("content reading");

    // create basic ARA model graph and perform analysis
    std::unique_ptr<TestHost> testHost;
    const auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testContentReading", true, audioFiles) };

    // read back all content types
    for (const auto& audioSource : araDocumentController->getDocument ()->getAudioSources ())
    {
        araDocumentController->logAllContent (audioSource.get ());
        for (const auto& audioModification : audioSource->getAudioModifications ())
        {
            araDocumentController->logAllContent (audioModification.get ());
            for (const auto& playbackRegion : audioModification->getPlaybackRegions ())
                araDocumentController->logAllContent (playbackRegion.get ());
        }
    }
}

/*******************************************************************************/
// Demonstrates how to clone an audio modification to enable two separate edits of the same audio source
void testModificationCloning (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("modification cloning");

    // create basic ARA model graph and perform analysis
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testModificationCloning", true, audioFiles) };
    auto document { araDocumentController->getDocument () };

    // read all content for the original audio modification and playback region
    // and construct a vector of audio modifications to clone
    std::vector<AudioModification*> audioModificationsToClone;
    for (const auto& audioSource : document->getAudioSources ())
    {
        for (const auto& audioModification : audioSource->getAudioModifications ())
        {
            araDocumentController->logAvailableContent (audioModification.get ());
            araDocumentController->logAudioModificationPreservesAudioSourceSignalIfSupported (audioModification.get ());

            audioModificationsToClone.push_back (audioModification.get ());
        }
    }

    // clone the audio modifications while editing, storing a vector of the clones
    araDocumentController->beginEditing ();
    std::vector<AudioModification*> audioModificationClones;
    for (const auto& audioModification : audioModificationsToClone)
    {
        auto audioModificationClone { testHost->cloneAudioModification (document, audioModification, audioModification->getName () + " (cloned)",
                                                                           audioModification->getPersistentID () + " (cloned)") };
        for (const auto& playbackRegion : audioModification->getPlaybackRegions ())
        {
            testHost->addPlaybackRegion (document, audioModificationClone,
                                          playbackRegion->getTransformationFlags (),
                                          playbackRegion->getStartInModificationTime (),
                                          playbackRegion->getDurationInModificationTime (),
                                          playbackRegion->getStartInPlaybackTime () + playbackRegion->getDurationInPlaybackTime (), // Place cloned region just after original
                                          playbackRegion->getDurationInPlaybackTime (),
                                          playbackRegion->getRegionSequence (),
                                          playbackRegion->getName (),
                                          playbackRegion->getColor ());
        }
        audioModificationClones.push_back (audioModificationClone);

        const auto origRef { araDocumentController->getRef (audioModification) };
        const auto cloneRef { araDocumentController->getRef (audioModificationClone) };
        ARA_LOG ("Cloned source audio modification %p (ARAAudioModificationRef %p) into new modification %p (ARAAudioModificationRef %p)", audioModification, origRef, audioModificationClone, cloneRef);
    }
    araDocumentController->endEditing ();

    // read back all the cloned audio modification content
    for (const auto& audioModificationClone : audioModificationClones)
    {
        araDocumentController->logAvailableContent (audioModificationClone);
        araDocumentController->logAudioModificationPreservesAudioSourceSignalIfSupported (audioModificationClone);
    }
}

/*******************************************************************************/
// Demonstrates how to store and restore plug-in document archives
void testArchiving (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("archiving");

    bool supportsARA2Persistency { false };                 // will be properly set after creating document controller

    MemoryArchive archive { plugInEntry->getARAFactory ()->documentArchiveID };

    // create and archive the document,
    // caching the audio source / modification persistent IDs
    std::vector<std::string> audioSourcePersistentIDs;
    std::map<std::string, std::vector<std::string>> audioModificationPersistentIDs;
    {
        // create basic ARA model graph and perform analysis
        std::unique_ptr<TestHost> testHost;
        auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testArchiving", true, audioFiles) };
        supportsARA2Persistency = araDocumentController->supportsPartialPersistency ();

        // log the audio source and modification content as reference
        for (const auto& audioSource : araDocumentController->getDocument ()->getAudioSources ())
        {
            ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) will be stored with persistent ID \"%s\"", audioSource.get (), araDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
            araDocumentController->logAvailableContent (audioSource.get ());

            for (const auto& audioModification : audioSource->getAudioModifications ())
            {
                ARA_LOG ("Audio modification %p (ARAAudioModificationRef %p) be stored with persistent ID \"%s\"", audioModification.get (), araDocumentController->getRef (audioModification.get ()), audioModification->getPersistentID ().c_str ());
                araDocumentController->logAvailableContent (audioModification.get ());
            }
        }

        // cache the audio source and modification persistent IDs
        for (const auto& audioSource : araDocumentController->getDocument ()->getAudioSources ())
        {
            audioSourcePersistentIDs.push_back (audioSource->getPersistentID ());
            for (const auto& audioModification : audioSource->getAudioModifications ())
                audioModificationPersistentIDs[audioSource->getPersistentID ()].push_back (audioModification->getPersistentID ());
        }

        // store our analysis results
        bool archivingSuccess { false };
        if (supportsARA2Persistency)
            archivingSuccess = araDocumentController->storeObjectsToArchive (&archive);
        else
            archivingSuccess = araDocumentController->storeDocumentToArchive (&archive);
        ARA_VALIDATE_API_STATE (archivingSuccess);      // our archive writer implementation never returns false, so this must always succeed
    }

    // use the archive to restore the entire document
    {
        // When restoring, we avoid using createHostAndBasicDocument () in order to
        // A) use the cached audio source / modification persistent IDs from our previous graph, and
        // B) perform the restore operation within a single edit cycle
        auto testHost { std::make_unique<TestHost> () };
        auto documentName { "testHostUnarchiving" };
        auto document { testHost->addDocument (documentName, plugInEntry) };
        auto araDocumentController { testHost->getDocumentController (document) };

        // begin the document edit cycle to configure and restore the document
        bool unarchivingSuccess { true };
        if (supportsARA2Persistency)
            araDocumentController->beginEditing ();
        else
            unarchivingSuccess = araDocumentController->beginRestoringDocumentFromArchive (&archive);

        // add a musical context and describe our timeline
        auto musicalContext { testHost->addMusicalContext (document, "ARA Test Musical Context", { 1.0f, 0.0f, 0.0f }) };

        // add a region sequence to describe our arrangement with a single track
        auto regionSequence { testHost->addRegionSequence (document, "Track 1", musicalContext, { 0.0f, 1.0f, 0.0f }) };

        // recreate the audio sources / modifications based on our cached persistent IDs
        for (size_t i { 0 }; i < audioFiles.size (); ++i)
        {
            auto audioSource { testHost->addAudioSource (document, audioFiles[i].get (), audioSourcePersistentIDs[i]) };
            araDocumentController->enableAudioSourceSamplesAccess (audioSource, true);

            for (size_t j { 0 }; j < audioModificationPersistentIDs[audioSource->getPersistentID ()].size (); ++j)
            {
                const std::string audioModificationName { "Test audio modification " + std::to_string (i) + " " + std::to_string (j) };
                auto audioModification { testHost->addAudioModification (document, audioSource, audioModificationName, audioModificationPersistentIDs[audioSource->getPersistentID ()][j]) };

                // add a playback region encompassing the entire audio source to render modifications in our musical context
                const auto playbackDuration { audioSource->getDuration () };
                testHost->addPlaybackRegion (document, audioModification, ARA::kARAPlaybackTransformationNoChanges,
                                                0.0, audioSource->getDuration (), static_cast<double> (i) * playbackDuration, playbackDuration,
                                                regionSequence, "Test playback region", { 0.0f, 0.0f, 1.0f });
            }
        }

        // inject state and end the document edit cycle
        if (supportsARA2Persistency)
        {
            unarchivingSuccess = araDocumentController->restoreObjectsFromArchive (&archive);
            araDocumentController->endEditing ();
        }
        else
        {
            unarchivingSuccess = araDocumentController->endRestoringDocumentFromArchive (&archive) && unarchivingSuccess;
        }
        ARA_VALIDATE_API_STATE (unarchivingSuccess);    // our archive reader implementation never returns false, and the archive
                                                        // was created on the same machine, so this call must always succeed

        // log the restored audio source and modification content to verify proper restoration
        for (const auto& audioSource : document->getAudioSources ())
        {
            ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) with persistent ID \"%s\" has been restored", audioSource.get (), araDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
            araDocumentController->logAvailableContent (audioSource.get ());

            for (const auto& audioModification : audioSource->getAudioModifications ())
            {
                ARA_LOG ("Audio modification %p (ARAAudioModificationRef %p) with persistent ID \"%s\" has been restored", audioModification.get (), araDocumentController->getRef (audioModification.get ()), audioModification->getPersistentID ().c_str ());
                araDocumentController->logAvailableContent (audioModification.get ());
            }
        }

        // plug-ins must deal with archives containing more data than actually being restored.
        // to test this, we delete our first source, then restore again.
        if (supportsARA2Persistency)
        {
            araDocumentController->beginEditing ();
            auto sourceToRemove { document->getAudioSources().front ().get () };
            auto modificationToRemove { sourceToRemove->getAudioModifications ().front ().get () };
            auto regionToRemove { modificationToRemove->getPlaybackRegions ().front ().get () };
            testHost->removePlaybackRegion (document, regionToRemove);
            testHost->removeAudioModification (document, modificationToRemove);
            testHost->removeAudioSource (document, sourceToRemove);

            unarchivingSuccess = araDocumentController->restoreObjectsFromArchive (&archive);
            ARA_VALIDATE_API_STATE (unarchivingSuccess);
            araDocumentController->endEditing ();

            // log the restored audio source and modification content to verify proper restoration
            for (const auto& audioSource : document->getAudioSources ())
            {
                ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) with persistent ID \"%s\" has been restored", audioSource.get (), araDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
                araDocumentController->logAvailableContent (audioSource.get ());

                for (const auto& audioModification : audioSource->getAudioModifications ())
                {
                    ARA_LOG ("Audio modification %p (ARAAudioModificationRef %p) with persistent ID \"%s\" has been restored", audioModification.get (), araDocumentController->getRef (audioModification.get ()), audioModification->getPersistentID ().c_str ());
                    araDocumentController->logAvailableContent (audioModification.get ());
                }
            }
        }
    }
}

/*******************************************************************************/
// For ARA 2 plug-ins, instead of a monolithic archive for the entire document this test uses
// multiple smaller archives, each containing specific parts of the graph.
void testSplitArchives (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("split archives");

    // create and archive the document,
    // caching the audio source / modification persistent IDs
    MemoryArchive documentDataArchive { plugInEntry->getARAFactory ()->documentArchiveID };
    std::vector<std::string> audioSourcePersistentIDs;
    std::vector<std::unique_ptr<MemoryArchive>> audioSourceArchives;
    std::map<std::string, std::vector<std::string>> audioModificationPersistentIDs;
    std::vector<std::unique_ptr<MemoryArchive>> audioModificationArchives;
    {
        // create basic ARA model graph and perform analysis
        std::unique_ptr<TestHost> testHost;
        auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testSplitArchives", true, audioFiles) };
        if (!araDocumentController->supportsPartialPersistency ())
        {
            ARA_LOG ("ARA2 Partial Persistency not supported by plug-in %s, skipping split archives test", plugInEntry->getARAFactory ()->plugInName);
            return;
        }

        // log the audio source and modification content as reference
        for (const auto& audioSource : araDocumentController->getDocument ()->getAudioSources ())
        {
            ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) will be stored with persistent ID \"%s\"",
                        audioSource.get (), araDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
            araDocumentController->logAvailableContent (audioSource.get ());

            for (const auto& audioModification : audioSource->getAudioModifications ())
            {
                ARA_LOG ("Audio modification %p (ARAAudioModificationRef %p) be stored with persistent ID \"%s\"",
                            audioModification.get (), araDocumentController->getRef (audioModification.get ()), audioModification->getPersistentID ().c_str ());
                araDocumentController->logAvailableContent (audioModification.get ());
            }
        }

        // store document data
        const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAStoreObjectsFilter, audioModificationRefs)> storeDocumentDataFilter { ARA::kARATrue,
                                                                                                                           0U, nullptr,
                                                                                                                           0U, nullptr
                                                                                                                         };
        auto archivingSuccess { araDocumentController->storeObjectsToArchive (&documentDataArchive, &storeDocumentDataFilter) };
        ARA_VALIDATE_API_STATE (archivingSuccess);          // our archive writer implementation never returns false, so this must always succeed

        // store each audio source and audio modification into an individual archive, and store their persistent IDs
        for (const auto& audioSource : araDocumentController->getDocument ()->getAudioSources ())
        {
            audioSourcePersistentIDs.push_back (audioSource->getPersistentID ());
            const auto audioSourceRef { araDocumentController->getRef (audioSource.get ()) };
            const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAStoreObjectsFilter, audioModificationRefs)> storeAudioSourceFilter { ARA::kARAFalse,
                                                                                                                              1U, &audioSourceRef,
                                                                                                                              0U, nullptr
                                                                                                                            };
            audioSourceArchives.emplace_back (new MemoryArchive { plugInEntry->getARAFactory ()->documentArchiveID });
            archivingSuccess = araDocumentController->storeObjectsToArchive (audioSourceArchives.back ().get (), &storeAudioSourceFilter);
            ARA_VALIDATE_API_STATE (archivingSuccess);      // our archive writer implementation never returns false, so this must always succeed

            for (const auto& audioModification : audioSource->getAudioModifications ())
            {
                audioModificationPersistentIDs[audioSource->getPersistentID ()].push_back (audioModification->getPersistentID ());
                const auto audioModificationRef { araDocumentController->getRef (audioModification.get ()) };
                const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAStoreObjectsFilter, audioModificationRefs)> storeAudioModificationFilter { ARA::kARAFalse,
                                                                                                                                        0U, nullptr,
                                                                                                                                        1U, &audioModificationRef
                                                                                                                                      };
                audioModificationArchives.emplace_back (new MemoryArchive { plugInEntry->getARAFactory ()->documentArchiveID });
                archivingSuccess = araDocumentController->storeObjectsToArchive (audioModificationArchives.back ().get (), &storeAudioModificationFilter);
                ARA_VALIDATE_API_STATE (archivingSuccess);  // our archive writer implementation never returns false, so this must always succeed
            }
        }
    }

    // use the archives to restore the entire document
    {
        // When restoring, we avoid using createHostAndBasicDocument () in order to
        // a) use the cached audio source / modification persistent IDs from our previous graph, and
        // b) perform the restore operation within a single edit cycle
        auto testHost { std::make_unique<TestHost> () };
        auto documentName { "testHostUnarchiving" };
        auto document { testHost->addDocument (documentName, plugInEntry) };
        auto araDocumentController { testHost->getDocumentController (document) };

        // begin the document edit cycle to configure and restore the document
        araDocumentController->beginEditing ();

        // add a musical context and describe our timeline
        auto musicalContext { testHost->addMusicalContext (document, "ARA Test Musical Context", { 1.0f, 0.0f, 0.0f }) };

        // add a region sequence to describe our arrangement with a single track
        auto regionSequence { testHost->addRegionSequence (document, "Track 1", musicalContext, { 0.0f, 1.0f, 0.0f }) };

        // recreate the audio sources / modifications based on our cached persistent IDs, immediately injecting the respective state
        bool unarchivingSuccess { false };
        for (size_t i { 0 }; i < audioFiles.size (); ++i)
        {
            // recreate audio source
            auto audioSource { testHost->addAudioSource (document, audioFiles[i].get (), audioSourcePersistentIDs[i]) };

            // inject audio source state
            const auto audioSourcePersistentID { audioSource->getPersistentID ().c_str () };
            const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> restoreAudioSourceFilter { ARA::kARAFalse,
                                                                                                                                        1U, &audioSourcePersistentID, nullptr,
                                                                                                                                        0U, nullptr, nullptr
                                                                                                                                       };
            unarchivingSuccess = araDocumentController->restoreObjectsFromArchive (audioSourceArchives[i].get (), &restoreAudioSourceFilter);
            ARA_VALIDATE_API_STATE (unarchivingSuccess);        // our archive reader implementation never returns false, and the archive
                                                                // was created on the same machine, so this call must always succeed

            for (size_t j { 0 }; j < audioModificationPersistentIDs[audioSource->getPersistentID ()].size (); ++j)
            {
                // recreate audio modification
                const std::string audioModificationName { "Test audio modification " + std::to_string (i) + " " + std::to_string (j) };
                auto audioModification { testHost->addAudioModification (document, audioSource, audioModificationName, audioModificationPersistentIDs[audioSource->getPersistentID ()][j]) };

                // inject audio modification state
                const auto audioModificationPersistentID { audioModification->getPersistentID ().c_str () };
                const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> restoreAudioModificationFilter { ARA::kARAFalse,
                                                                                                                                                  0U, nullptr, nullptr,
                                                                                                                                                  1U, &audioModificationPersistentID, nullptr
                                                                                                                                                };
                unarchivingSuccess = araDocumentController->restoreObjectsFromArchive (audioModificationArchives[i].get (), &restoreAudioModificationFilter);
                ARA_VALIDATE_API_STATE (unarchivingSuccess);    // our archive reader implementation never returns false, and the archive
                                                                // was created on the same machine, so this call must always succeed

                // add a playback region encompassing the entire audio source to render modifications in our musical context
                const auto playbackDuration { audioSource->getDuration () };
                testHost->addPlaybackRegion (document, audioModification, ARA::kARAPlaybackTransformationNoChanges,
                                                0.0, audioSource->getDuration (), static_cast<double> (i) * playbackDuration, playbackDuration,
                                                regionSequence, "Test playback region", { 0.0f, 0.0f, 1.0f });
            }

            // enable audio source access
            araDocumentController->enableAudioSourceSamplesAccess (audioSource, true);
        }

        // finally, inject document data and end the document edit cycle
        ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> restoreDocumentDataFilter { ARA::kARATrue,
                                                                                                                               0U, nullptr, nullptr,
                                                                                                                               0U, nullptr, nullptr
                                                                                                                             };
        unarchivingSuccess = araDocumentController->restoreObjectsFromArchive (&documentDataArchive, &restoreDocumentDataFilter);
        ARA_VALIDATE_API_STATE (unarchivingSuccess);    // our archive reader implementation never returns false, and the archive
                                                        // was created on the same machine, so this call must always succeed
        araDocumentController->endEditing ();

        // log the restored audio source and modification content to verify proper restoration
        for (const auto& audioSource : document->getAudioSources ())
        {
            ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) with persistent ID \"%s\" has been restored",
                        audioSource.get (), araDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
            araDocumentController->logAvailableContent (audioSource.get ());

            for (const auto& audioModification : audioSource->getAudioModifications ())
            {
                ARA_LOG ("Audio modification %p (ARAAudioModificationRef %p) with persistent ID \"%s\" has been restored",
                            audioModification.get (), araDocumentController->getRef (audioModification.get ()), audioModification->getPersistentID ().c_str ());
                araDocumentController->logAvailableContent (audioModification.get ());
            }
        }
    }
}

/*******************************************************************************/
// Simulates a "drag & drop" operation by archiving one source and its modification in a
// two source/modification document with a StoreObjectsFilter, and restoring them in another document
void testDragAndDrop (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("drag and drop");

    std::unique_ptr<TestHost> testHost;
    auto testDocController { createHostAndBasicDocument (plugInEntry, testHost, "ARA2PersistencyTestDoc", false, {}) };
    if (!testDocController->supportsPartialPersistency ())
    {
        ARA_LOG ("ARA2 Partial Persistency not supported by plug-in %s, skipping drag and drop test", plugInEntry->getARAFactory ()->plugInName);
        return;
    }

    // create our "drag" document with two audio sources and perform analysis
    auto dragDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "Drag Document", true, audioFiles) };
    const auto dragDocument { dragDocumentController->getDocument () };

    // read the audio source content
    ARA_LOG ("Logging audio source content for document \"%s\"", dragDocument->getName ().c_str ());
    for (const auto& audioSource : dragDocument->getAudioSources ())
    {
        ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) has persistent ID \"%s\"", audioSource.get (), dragDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
        dragDocumentController->logAvailableContent (audioSource.get ());
    }

    // we simulate dragging the first source and its modification
    auto draggedAudioSource { dragDocument->getAudioSources ().front ().get () };
    auto draggedAudioModification { draggedAudioSource->getAudioModifications ().front ().get () };

    // use a StoreObjectsFilter to create a "drag" archive containing only draggedAudioSource / Modification
    auto draggedAudioSourceRef { dragDocumentController->getRef (draggedAudioSource) };
    auto draggedAudioModificationRef { dragDocumentController->getRef (draggedAudioModification) };
    const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAStoreObjectsFilter, audioModificationRefs)> storeObjectsFilter { ARA::kARATrue,
                                                                                                                  1U, &draggedAudioSourceRef,
                                                                                                                  1U, &draggedAudioModificationRef
                                                                                                                };

    // store only the dragged audio source's data in the archive
    ARA_LOG ("Dragging audio source with persistent ID \"%s\" from %s", draggedAudioSource->getPersistentID ().c_str (), dragDocument->getName ().c_str ());
    MemoryArchive clipBoardArchive { plugInEntry->getARAFactory ()->documentArchiveID };
    const bool archivingSuccess { dragDocumentController->storeObjectsToArchive (&clipBoardArchive, &storeObjectsFilter) };
    ARA_VALIDATE_API_STATE (archivingSuccess);      // our archive writer implementation never returns false, so this must always succeed

    // now create a new document that we'll "drop" the archive data on to
    auto dropDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "Drop Document", false, audioFiles) };
    const auto dropDocument { dropDocumentController->getDocument () };

    // add new audio source and modification with unique persistent IDs
    dropDocumentController->beginEditing ();

    const std::string dropAudioSourcePersistentID { "audioSourceTestPersistentID " + std::to_string (audioFiles.size ()) };
    auto dropAudioSource { testHost->addAudioSource (dropDocument, draggedAudioSource->getAudioFile (), dropAudioSourcePersistentID) };

    const std::string dropAudioModificationPersistentID { "audioModificationTestPersistentID " + std::to_string (audioFiles.size ()) };
    auto dropAudioModification { testHost->addAudioModification (dropDocument, dropAudioSource, draggedAudioModification->getName (), dropAudioModificationPersistentID) };

    // construct a ARARestoreObjectsFilter containing the objects we want to restore
    const auto audioSourceArchiveID { draggedAudioSource->getPersistentID ().c_str () };
    const auto audioModificationArchiveID { draggedAudioModification->getPersistentID ().c_str () };
    const auto audioSourceCurrentID { dropAudioSource->getPersistentID ().c_str () };
    const auto audioModificationCurrentID { dropAudioModification->getPersistentID ().c_str () };
    const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> restoreObjectsFilter { ARA::kARATrue,
                                                                                                                            1U, &audioSourceArchiveID, &audioSourceCurrentID,
                                                                                                                            1U, &audioModificationArchiveID, &audioModificationCurrentID
                                                                                                                          };

    ARA_LOG ("Dropping dragged data into audio source with persistent ID \"%s\" to %s", audioSourceCurrentID, dropDocument->getName ().c_str ());
    const bool unarchivingSuccess { dropDocumentController->restoreObjectsFromArchive (&clipBoardArchive, &restoreObjectsFilter) };

    dropDocumentController->endEditing ();

    ARA_VALIDATE_API_STATE (unarchivingSuccess);

    // verify the restored content
    ARA_LOG ("Logging audio source content for document \"%s\"", dropDocument->getName ().c_str ());
    for (const auto& audioSource : dropDocument->getAudioSources ())
    {
        ARA_LOG ("Audio source %p (ARAAudioSourceRef %p) has persistent ID \"%s\"", audioSource.get (), dropDocumentController->getRef (audioSource.get ()), audioSource->getPersistentID ().c_str ());
        dropDocumentController->logAvailableContent (audioSource.get ());
    }
}

/*******************************************************************************/
// Demonstrates using a plug-in playback renderer instance to process audio for a playback region,
// using the companion API rendering methods
// Can optionally use an ARA plug-in's time stretching capabilities to stretch a playback region -
// try loading Melodyne to see this feature in action
void testPlaybackRendering (PlugInEntry* plugInEntry, bool enableTimeStretchingIfSupported, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("playback rendering (with time stretching if supported)");

    // create basic ARA model graph and perform analysis
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testPlaybackRendering", false, audioFiles) };
    const auto document { araDocumentController->getDocument () };

    // instantiate the plug-in with the PlaybackRenderer role and verify that it's a valid playback renderer instance
    auto plugInInstance { plugInEntry->createPlugInInstance () };
    plugInInstance->bindToDocumentControllerWithRoles (araDocumentController->getDocumentController ()->getRef (), ARA::kARAPlaybackRendererRole);
    auto playbackRenderer { plugInInstance->getPlaybackRenderer () };

    // for testing purposes, we take the sample rate of the first audio source as our renderer sample rate
    const auto renderSampleRate { (!document->getAudioSources ().empty ()) ? document->getAudioSources ().front ()->getSampleRate () : 44100.0 };

    // add all regions to the renderer and also find the sample boundaries of our document's playback regions
    std::vector<PlaybackRegion*> playbackRegions;
    auto startOfPlaybackRegions { std::numeric_limits<double>::max () };
    auto endOfPlaybackRegions { std::numeric_limits<double>::min () };
    for (const auto& regionSequence : document->getRegionSequences ())
    {
        for (const auto& playbackRegion : regionSequence->getPlaybackRegions ())
        {
            ARA_LOG ("Adding playback region %p (ARAPlaybackRegionRef %p) to playback renderer %p", playbackRegion, araDocumentController->getRef (playbackRegion), playbackRenderer.getRef ());
            playbackRenderer.addPlaybackRegion (araDocumentController->getRef (playbackRegion));

            auto headTime { 0.0 }, tailTime { 0.0 };
            araDocumentController->getPlaybackRegionHeadAndTailTime (playbackRegion, &headTime, &tailTime);

            startOfPlaybackRegions = std::min (playbackRegion->getStartInPlaybackTime () - headTime, startOfPlaybackRegions);
            endOfPlaybackRegions = std::max (playbackRegion->getEndInPlaybackTime () + tailTime, endOfPlaybackRegions);

            playbackRegions.push_back (playbackRegion);
        }
    }

    // bail if no region samples to render
    if (startOfPlaybackRegions < endOfPlaybackRegions)
    {
        ARA_LOG ("Rendering %lu region(s) assigned to playback renderer %p with sample rate %lgHz", playbackRegions.size (), playbackRenderer.getRef (), renderSampleRate);

        auto startOfPlaybackRegionSamples { ARA::samplePositionAtTime (startOfPlaybackRegions, renderSampleRate) };
        auto endOfPlaybackRegionSamples { ARA::samplePositionAtTime (endOfPlaybackRegions, renderSampleRate) };

        // create a buffer of output samples
        std::vector<float> outputData (static_cast<size_t> (endOfPlaybackRegionSamples - startOfPlaybackRegionSamples));

        // ARA plug-ins should be rendered with large buffer sizes for playback (and ahead-of-time in
        // actual hosts) since they do not depend on any realtime input.
        constexpr auto renderBlockSize { 2048 };

        // render all playback region samples
        plugInInstance->startRendering (renderBlockSize, renderSampleRate);

        bool renderingCompleted { false };
        auto renderOnOtherThread = [&] () {
            std::thread renderthread { [&] () {
                ARAAudioAccessController::registerRenderThread ();
                for (auto samplePosition { startOfPlaybackRegionSamples }; samplePosition < endOfPlaybackRegionSamples; samplePosition += renderBlockSize)
                {
                    const auto samplesToRender { std::min (renderBlockSize, static_cast<int> (endOfPlaybackRegionSamples - samplePosition)) };
                    const auto outputPosition { samplePosition - startOfPlaybackRegionSamples };
                    plugInInstance->renderSamples (samplesToRender, samplePosition, &outputData[static_cast<size_t> (outputPosition)]);
                }
                ARAAudioAccessController::unregisterRenderThread ();
                renderingCompleted = true;
            } };

            while (!renderingCompleted)
                plugInEntry->idleThreadForDuration (10);

            renderthread.join ();
        };

        renderOnOtherThread ();

        // optionally perform the render again if the plug-in supports time stretching
        if (enableTimeStretchingIfSupported)
        {
            const auto supportedTransformationFlags { plugInEntry->getARAFactory ()->supportedPlaybackTransformationFlags };
            if ((supportedTransformationFlags & ARA::kARAPlaybackTransformationTimestretch) != 0)
            {
                constexpr double timeStretchFactor { 0.75 };
                ARA_LOG ("Applying time stretch factor of %lg to all playback regions assigned to playback renderer %p", timeStretchFactor, playbackRenderer.getRef ());

                araDocumentController->beginEditing ();
                for (auto& playbackRegion : playbackRegions)
                {
                    playbackRegion->setTransformationFlags (ARA::kARAPlaybackTransformationTimestretch + playbackRegion->getTransformationFlags ());
                    playbackRegion->setDurationInPlaybackTime (timeStretchFactor * playbackRegion->getDurationInPlaybackTime ());
                    araDocumentController->updatePlaybackRegionProperties (playbackRegion);
                }
                araDocumentController->endEditing ();

                endOfPlaybackRegions *= timeStretchFactor;
                endOfPlaybackRegionSamples = ARA::samplePositionAtTime (endOfPlaybackRegions, renderSampleRate);

                ARA_LOG ("Rendering %lu region(s) assigned to playback renderer %p with sample rate %lgHz", playbackRegions.size (), playbackRenderer.getRef (), renderSampleRate);

                renderOnOtherThread ();
            }
            else
            {
                ARA_LOG ("Time stretching requested, but plug-in doesn't support kARAPlaybackTransformationTimestretch");
            }
        }

        plugInInstance->stopRendering ();
        for (const auto& regionSequence : document->getRegionSequences ())
        {
            for (const auto& playbackRegion : regionSequence->getPlaybackRegions ())
                playbackRenderer.removePlaybackRegion (araDocumentController->getRef (playbackRegion));
        }
    }
}

/*******************************************************************************/
// Demonstrates how to communicate view selection and region sequence hiding
// (albeit this is of rather limited use in a non-UI application)
void testEditorView (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("editor view communication");

    // create basic ARA model graph
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testEditorView", false, audioFiles) };
    const auto document { araDocumentController->getDocument () };

    // instantiate the plug-in with the EditorView role and verify that it's a valid editor view instance
    auto plugInInstance { plugInEntry->createPlugInInstance () };
    plugInInstance->bindToDocumentControllerWithRoles (araDocumentController->getDocumentController ()->getRef (), ARA::kARAEditorViewRole);
    auto editorView { plugInInstance->getEditorView () };

    // Selection demonstration
    using Selection = ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAViewSelection, timeRange)>;

    // create a "selection" containing all playback regions in the document and notify the editor view
    std::vector<ARA::ARAPlaybackRegionRef> playbackRegionRefs;
    for (const auto& regionSequence : document->getRegionSequences ())
        for (const auto& playbackRegion : regionSequence->getPlaybackRegions ())
            playbackRegionRefs.push_back (araDocumentController->getRef (playbackRegion));
    ARA_LOG ("Notifying editor view %p of %lu selected playback region(s)", editorView.getRef (), playbackRegionRefs.size ());
    const Selection selection1 { playbackRegionRefs.size (), playbackRegionRefs.data (), 0U, nullptr, nullptr };
    editorView.notifySelection (&selection1);
    plugInEntry->idleThreadForDuration (50);    // idle a bit here to enable ARATestPlugIn's UI interaction simulation, see ARA_SIMULATE_USER_INTERACTION

    // we can also select all region sequences and limit the selection to a specific time range
    std::vector<ARA::ARARegionSequenceRef> regionSequenceRefs;
    for (const auto& regionSequence : document->getRegionSequences ())
        regionSequenceRefs.push_back (araDocumentController->getRef (regionSequence.get ()));
    ARA::ARAContentTimeRange timeRange { 0.0, 5.0 };
    ARA_LOG ("Notifying editor view %p of %lu selected region sequence(s)", editorView.getRef (), regionSequenceRefs.size ());
    const Selection selection2 { 0U, nullptr, regionSequenceRefs.size (), regionSequenceRefs.data (), &timeRange };
    editorView.notifySelection (&selection2);
    plugInEntry->idleThreadForDuration (50);    // idle a bit here to enable ARATestPlugIn's UI interaction simulation, see ARA_SIMULATE_USER_INTERACTION

    // we can also mix playback region and region sequence selection, if this is a valid pattern in the host
    ARA_LOG ("Notifying editor view %p of %lu selected playback region(s) and %lu selected region sequence(s)", editorView.getRef (), playbackRegionRefs.size (), regionSequenceRefs.size ());
    const Selection selection3 { playbackRegionRefs.size (), playbackRegionRefs.data (), regionSequenceRefs.size (), regionSequenceRefs.data (), &timeRange };
    editorView.notifySelection (&selection3);
    plugInEntry->idleThreadForDuration (50);    // idle a bit here to enable ARATestPlugIn's UI interaction simulation, see ARA_SIMULATE_USER_INTERACTION

    // Region sequence hiding demonstration
    // "hide" the region sequences and inform the plug-in editor view
    ARA_LOG ("Notifying editor view %p of %lu hidden region sequence(s)", editorView.getRef (), regionSequenceRefs.size ());
    editorView.notifyHideRegionSequences (regionSequenceRefs.size (), regionSequenceRefs.data ());

    // "unhide" the region sequences
    ARA_LOG ("Notifying editor view %p that all region sequences are now un-hidden", editorView.getRef ());
    editorView.notifyHideRegionSequences (0, nullptr);
}

/*******************************************************************************/
// Requests plug-in analysis, using every processing algorithm published by the plug-in.
void testProcessingAlgorithms (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("processing algorithms");

    // create basic ARA model graph
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testProcessingAlgorithm", false, audioFiles) };
    araDocumentController->setMinimalContentUpdateLogging (true);
    const auto document { araDocumentController->getDocument () };
    const auto araFactory { plugInEntry->getARAFactory () };

    // run analysis, log content for each available processing algorithm
    const auto algorithmCount { araDocumentController->getProcessingAlgorithmsCount () };
    if (algorithmCount == 0)
    {
        ARA_LOG ("No processing algorithms available for plug-in %s", araFactory->plugInName);
        return;
    }

    for (auto i { 0 }; i < algorithmCount; ++i)
    {
        const auto algorithmProperties { araDocumentController->getProcessingAlgorithmProperties (i) };
        ARA_LOG ("analyzing audio source content using analysis algorithm %i \"%s\"", i, algorithmProperties->name);

        // first request new algorithm for all sources
        araDocumentController->beginEditing ();
        for (auto& audioSource : document->getAudioSources ())
            araDocumentController->requestProcessingAlgorithmForAudioSource (audioSource.get (), i);
        araDocumentController->endEditing ();

        // now request analysis for each source and wait for completion
        for (auto& audioSource : document->getAudioSources ())
        {
            araDocumentController->requestAudioSourceContentAnalysis (audioSource.get (), araFactory->analyzeableContentTypesCount, araFactory->analyzeableContentTypes, true);
            const auto actualIndex { araDocumentController->getProcessingAlgorithmForAudioSource (audioSource.get ()) };
            if (actualIndex != i)
                ARA_LOG ("algorithm actually differs from requested algorithm, is %i \"%s\"", actualIndex, araDocumentController->getProcessingAlgorithmProperties (actualIndex)->name);
            araDocumentController->logAvailableContent (audioSource.get ());
        }
    }
}

/*******************************************************************************/
// Loads an `iXML` ARA audio file chunk from a supplied .WAV or .AIFF file
void testAudioFileChunkLoading (PlugInEntry* plugInEntry, const AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("ARA audio file loading XML chunks");

    // create basic ARA model graph with no audio sources - we'll create one for each wav file
    std::unique_ptr<TestHost> testHost;
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testAudioFileChunkLoading", false, {}) };
    const auto araFactory { plugInEntry->getARAFactory () };
    const auto document { araDocumentController->getDocument () };

    auto index { 0 };
    for (const auto& audioFile : audioFiles)
    {
        // find matching ARA archive
        bool openAutomatically { false };
        std::string plugInName, plugInVersion, manufacturer, informationURL, persistentID;
        auto documentArchiveID { araFactory->documentArchiveID };
        auto data { audioFile->getiXMLARAAudioSourceData (documentArchiveID, openAutomatically,
                                                          plugInName, plugInVersion, manufacturer, informationURL, persistentID) };
        for (auto i { 0U }; data.empty () && (i < araFactory->compatibleDocumentArchiveIDsCount); ++i)
        {
            documentArchiveID = araFactory->compatibleDocumentArchiveIDs[i];
            data = audioFile->getiXMLARAAudioSourceData (documentArchiveID, openAutomatically,
                                                         plugInName, plugInVersion, manufacturer, informationURL, persistentID);
        }
        if (data.empty ())
        {
            ARA_LOG ("No matching ARA archive chunk found in iXML chunk in audio file %s", audioFile->getName ().c_str ());
            continue;
        }
        ARA_LOG ("Found matching ARA archive in audio file %s:", audioFile->getName ().c_str ());
        ARA_LOG ("Open automatically: %s", (openAutomatically) ? "true" : "false");
        ARA_LOG ("Suggested plug-in for loading the chunk:");
        ARA_VALIDATE_API_STATE (!plugInName.empty ());
        ARA_LOG ("    name: %s", plugInName.c_str ());
        ARA_VALIDATE_API_STATE (!plugInVersion.empty ());
        ARA_LOG ("    minimum version: %s", plugInVersion.c_str ());
        ARA_VALIDATE_API_STATE (!manufacturer.empty ());
        ARA_LOG ("    manufacturer: %s", manufacturer.c_str ());
        ARA_VALIDATE_API_STATE (!informationURL.empty ());
        ARA_LOG ("    website: %s", informationURL.c_str ());

        MemoryArchive archive { data, documentArchiveID };

        // begin loading chunk
        araDocumentController->beginEditing ();

        // create audio source and load its state
        const std::string newPersistentID { "audioSourceTestPersistentID " + std::to_string (index) };
        auto audioSource { testHost->addAudioSource (document, audioFile.get (), newPersistentID) };

        // partial persistence - restore this audio source using the archive stored in the XML data
        const auto oldID { persistentID.c_str () };
        const auto newID { newPersistentID.c_str () };
        const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> restoreObjectsFilter { ARA::kARAFalse,
                                                                                                                                1U, &oldID, &newID,
                                                                                                                                0U, nullptr, nullptr
                                                                                                                              };
        // load chunk and enable sample access
        const auto unarchivingSuccess { araDocumentController->restoreObjectsFromArchive (&archive, &restoreObjectsFilter) };
        ARA_VALIDATE_API_STATE (unarchivingSuccess);

        araDocumentController->enableAudioSourceSamplesAccess (audioSource, true);

        // add audio modification and playback region
        const std::string audioModificationPersistentID { "audioModificationTestPersistentID " + std::to_string (index) };
        const auto duration { audioSource->getDuration () };
        auto audioModification { testHost->addAudioModification (document, audioSource, audioFile->getName () + " Modification", audioModificationPersistentID.c_str ()) };
        testHost->addPlaybackRegion (document, audioModification, ARA::kARAPlaybackTransformationNoChanges, 0.0, duration, 0.0, duration, document->getRegionSequences ()[0].get (), audioFile->getName () + "Playback Region", ARA::ARAColor {});

        // conclude loading chunk
        araDocumentController->endEditing ();

        // log the restored audio source content
        for (auto i { 0U }; i < araFactory->analyzeableContentTypesCount; ++i)
            araDocumentController->logAvailableContent (audioSource);

        ++index;
    }
}

/*******************************************************************************/
// Requests plug-in analysis and saves audio source state into an `iXML` data chunk in each audio file
// (if chunk authoring is supported by the plug-in) -
// overwrites any current iXML chunk in the files (but only in-memory)
void testAudioFileChunkSaving (PlugInEntry* plugInEntry, AudioFileList& audioFiles)
{
    ARA_LOG_TEST_HOST_FUNC ("ARA audio file saving XML chunks");

    std::unique_ptr<TestHost> testHost;
    auto testDocController { createHostAndBasicDocument (plugInEntry, testHost, "ChunkSavingTestDoc", false, {}) };
    if (!testDocController->supportsStoringAudioFileChunks ())
    {
        ARA_LOG ("ARA audio file chunk authoring is not supported by plug-in %s", plugInEntry->getARAFactory ()->plugInName);
        return;
    }

    // create basic ARA model graph
    auto araDocumentController { createHostAndBasicDocument (plugInEntry, testHost, "testAudioFileChunkSaving", true, audioFiles) };
    const auto araFactory { plugInEntry->getARAFactory () };
    const auto document { araDocumentController->getDocument () };

    // store the XML data chunk for each audio source
    for (const auto& audioSource : document->getAudioSources ())
    {
        // log the audio source content to store
        for (auto i { 0U }; i < araFactory->analyzeableContentTypesCount; ++i)
            araDocumentController->logAvailableContent (audioSource.get ());

        // store archive for this audio source
        MemoryArchive archive { araFactory->documentArchiveID };
        ARA::ARAPersistentID documentArchiveID { nullptr };
        bool openAutomatically { false };
        const auto archivingSuccess { araDocumentController->storeAudioSourceToAudioFileChunk (&archive, audioSource.get (), &documentArchiveID, &openAutomatically) };
        ARA_VALIDATE_API_STATE (archivingSuccess);      // our archive writer implementation never returns false, so this must always succeed
        ARA_VALIDATE_API_ARGUMENT (documentArchiveID, documentArchiveID != nullptr);
        bool isValidID { documentArchiveID == araFactory->documentArchiveID };
        for (auto i { 0U }; !isValidID && (i < araFactory->compatibleDocumentArchiveIDsCount); ++i)
            isValidID = (documentArchiveID == araFactory->compatibleDocumentArchiveIDs[i]);
        ARA_VALIDATE_API_ARGUMENT (documentArchiveID, isValidID);

        // store ARA audio file XML chunk
        audioSource->getAudioFile ()->setiXMLARAAudioSourceData (documentArchiveID, openAutomatically,
                                              araFactory->plugInName, araFactory->version, araFactory->manufacturerName, araFactory->informationURL,
                                              audioSource->getPersistentID (), archive);

// enable this to update the audio file also on disk
//      const auto success { audioSource->getAudioFile ()->saveToFile (audioSource->getName ()) };
//      ARA_INTERNAL_ASSERT (success);
    }
}
