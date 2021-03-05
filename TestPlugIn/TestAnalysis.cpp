//------------------------------------------------------------------------------
//! \file       TestAnalysis.cpp
//!             dummy implementation of audio source analysis for the ARA test plug-in
//!             Actual plug-ins will typically have an analysis implementation which is
//!             independent of ARA - this code is also largely decoupled from ARA.
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

#include "TestAnalysis.h"
#include "ARATestAudioSource.h"

#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#include <chrono>
#include <cmath>

// The test plug-in pretends to be able to do a kARAContentTypeNotes analysis:
// To simulate this, it reads all samples and creates a note with invalid pitch for each range of
// consecutive samples that are not 0. It also tracks the peak amplitude throughout each note and
// assumes this as note volume. (Note that actual plug-ins would rather use some calculation closer
// to RMS for determining volume.) This is no meaningful algorithm for real-world signals, but it
// was choosen so that the resulting note data can be easily verified both manually and via scripts
// parsing the debug output of the various examples (which generate a pulsed sine wave whenever no
// actual audio file is used).
// The time consumed by the fake analysis is the duration of the audio source scaled down by
// ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR - if this is set to 0, the artificial delays are supressed.
#if !defined (ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR)
    #define ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR 20
#endif

// if desired, a custom timer for calculating the analysis delay can be injected by defining ARA_GET_CURRENT_TIME accordingly.
#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
    #if defined (ARA_GET_CURRENT_TIME)
        double ARA_GET_CURRENT_TIME ();    /* declare custom time getter function */
    #else
        #define ARA_GET_CURRENT_TIME() (0.000001 * static_cast<double> (std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::high_resolution_clock::now ().time_since_epoch ()).count ()))
    #endif
#endif

#if !defined (ARA_FAKE_NOTE_MAX_COUNT)
    #define ARA_FAKE_NOTE_MAX_COUNT 100
#endif


using namespace ARA;
using namespace PlugIn;

/*******************************************************************************/

class DefaultProcessingAlgorithm : public TestProcessingAlgorithm
{
public:
    using TestProcessingAlgorithm::TestProcessingAlgorithm;

    const std::string& getName () const override
    {
        static const std::string name { "default algorithm" };
        return name;
    }

    const std::string& getIdentifier () const override
    {
        static const std::string identifier { "com.arademocompany.testplugin.algorithm.default" };
        return identifier;
    }

    std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisTask* analysisTask) const noexcept override
    {
        auto audioSource = analysisTask->getAudioSource ();
        audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressStarted (audioSource);

#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
        // helper variables to artificially slow down analysis as indicated by ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR
        const auto analysisStartTime { ARA_GET_CURRENT_TIME () };
        const auto analysisTargetDuration { audioSource->getDuration () / ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR };
#endif

        // setup buffers and audio reader for reading samples
        constexpr auto blockSize { 64U };
        const auto channelCount { static_cast<uint32_t> (audioSource->getChannelCount ()) };
        std::vector<float> buffer (channelCount * blockSize);
        std::vector<void*> dataPointers (channelCount);
        for (auto c { 0U }; c < channelCount; ++c)
            dataPointers[c] = &buffer[c * blockSize];

        // search the audio for silence and treat each region between silence as a note
        ARASamplePosition blockStartIndex { 0 };
        ARASamplePosition lastNoteStartIndex { 0 };
        bool wasZero { true };      // samples before the start of the file are 0
        float volume { 0.0f };
        std::vector<TestNote> foundNotes;
        while (true)
        {
            // check cancel
            if (analysisTask->shouldCancel ())
            {
                audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressCompleted (audioSource);
                return {};
            }

            // calculate size of current block and check if done
            const auto count { std::min (static_cast<ARASamplePosition> (blockSize), audioSource->getSampleCount () - blockStartIndex) };
            if (count <= 0)
                break;

            // read samples - note that this test code ignores any errors that the reader might return here!
            analysisTask->getHostAudioReader ()->readAudioSamples (blockStartIndex, count, dataPointers.data ());

            // analyze current block
            for (ARASamplePosition i { 0 }; (i < count) && (foundNotes.size () < ARA_FAKE_NOTE_MAX_COUNT); ++i)
            {
                // check if current sample is zero on all channels
                bool isZero { true };
                for (ARASampleCount c { 0 }; c < channelCount; ++c)
                {
                    const auto sample = buffer[static_cast<size_t> (i + c * blockSize)];
                    isZero &= (sample == 0.0f);
                    volume = std::max (volume, std::abs (sample));
                }

                // check if consecutive range of (non)zero samples ends
                if (isZero != wasZero)
                {
                    wasZero = isZero;
                    const auto index { blockStartIndex + i };
                    if (isZero)
                    {
                        // found end of note - construct note
                        const double noteStartTime { static_cast<double> (lastNoteStartIndex) / audioSource->getSampleRate () };
                        const double noteDuration { static_cast<double> (index - lastNoteStartIndex) / audioSource->getSampleRate () };
                        TestNote foundNote { kARAInvalidFrequency, volume, noteStartTime, noteDuration };
                        foundNotes.push_back (foundNote);
                        volume = 0.0f;
                    }
                    else
                    {
                        // found start of note - store start index
                        lastNoteStartIndex = index;
                    }
                }
            }

            // go to next block and set progress
            // (in the progress calculation, we're scaling by 0.999 to account for the time needed
            // to store the result after this loop has completed)
            blockStartIndex += count;
            const float progress { 0.999f * static_cast<float> (blockStartIndex) / static_cast<float> (audioSource->getSampleCount ()) };
            audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressUpdated (audioSource, progress);

#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
            // for testing purposes only, sleep here until dummy analysis time has elapsed -
            // actual plug-ins will process as fast as possible, without arbitrary sleeping
            const auto analysisTargetTime { analysisStartTime + progress * analysisTargetDuration };
            const auto timeToSleep { analysisTargetTime - ARA_GET_CURRENT_TIME () };
            if (timeToSleep > 0.0)
                std::this_thread::sleep_for (std::chrono::milliseconds (std::llround (timeToSleep * 1000)));
#endif
        }

        if (!wasZero)
        {
            // last note continued until the end of the audio source - construct last note
            const double noteStartTime { static_cast<double> (lastNoteStartIndex) / audioSource->getSampleRate () };
            const double noteDuration { static_cast<double> (audioSource->getSampleCount () - lastNoteStartIndex) / audioSource->getSampleRate () };
            TestNote foundNote { kARAInvalidFrequency, volume, noteStartTime, noteDuration };
            foundNotes.push_back (foundNote);
        }

        // complete analysis and store result
        audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressCompleted (audioSource);
        return std::make_unique<TestNoteContent> (foundNotes);
    }
};

/*******************************************************************************/

class SingleNoteProcessingAlgorithm : public TestProcessingAlgorithm
{
public:
    using TestProcessingAlgorithm::TestProcessingAlgorithm;

    const std::string& getName () const override
    {
        static const std::string name { "single note algorithm" };
        return name;
    }

    const std::string& getIdentifier () const override
    {
        static const std::string identifier { "com.arademocompany.testplugin.algorithm.singlenote" };
        return identifier;
    }

    std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisTask* analysisTask) const noexcept override
    {
        auto audioSource = analysisTask->getAudioSource ();
        audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressStarted (audioSource);

#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
        // for testing purposes only, sleep here until dummy analysis time has elapsed
        const auto analysisTargetDuration { audioSource->getDuration () / ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR };
        constexpr auto sliceDuration { 0.05 };
        const auto count { (analysisTargetDuration > sliceDuration) ? static_cast<int> (analysisTargetDuration / sliceDuration + 0.5) : 1 };
        for (auto i { 0 }; i < count; ++i)
        {
            // check cancel
            if (analysisTask->shouldCancel ())
            {
                audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressCompleted (audioSource);
                return {};
            }

            audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressUpdated (audioSource, static_cast<float> (i) / static_cast<float> (count));
            std::this_thread::sleep_for (std::chrono::milliseconds (std::llround (sliceDuration * 1000)));
        }
#endif

        TestNote foundNote { kARAInvalidFrequency, 1.0f, 0.0, analysisTask->getAudioSource ()->getDuration () };
        audioSource->getDocumentController ()->notifyAudioSourceAnalysisProgressCompleted (audioSource);
        return std::make_unique<TestNoteContent> (std::vector<TestNote> { foundNote });
    }
};

/*******************************************************************************/

static const DefaultProcessingAlgorithm defaultAlgorithm;
static const SingleNoteProcessingAlgorithm singleNoteAlgorithm;

std::vector<const TestProcessingAlgorithm*> const& TestProcessingAlgorithm::getAlgorithms ()
{
    static const std::vector<const TestProcessingAlgorithm*> algorithms { &defaultAlgorithm, &singleNoteAlgorithm };
    return algorithms;
}

const TestProcessingAlgorithm* TestProcessingAlgorithm::getDefaultAlgorithm ()
{
    return getAlgorithmWithIdentifier (defaultAlgorithm.getIdentifier ());
}

const TestProcessingAlgorithm* TestProcessingAlgorithm::getAlgorithmWithIdentifier (const std::string& identifier)
{
    const auto begin { getAlgorithms ().begin () };
    const auto end { getAlgorithms ().end () };
    const auto it { std::find_if (begin, end,   [identifier](const TestProcessingAlgorithm* algorithm)
                                                    { return algorithm->getIdentifier ().compare (identifier) == 0; } ) };
    return (it != end) ? *it : nullptr;
}

/*******************************************************************************/

TestAnalysisTask::TestAnalysisTask (ARATestAudioSource* audioSource, const TestProcessingAlgorithm* processingAlgorithm)
: _audioSource { audioSource },
  _processingAlgorithm { processingAlgorithm },
  _hostAudioReader { std::make_unique<HostAudioReader> (audioSource) }  // create audio reader on the main thread, before dispatching to analysis thread
{
    _future = std::async (std::launch::async, [this]()
    {
        if (auto newNoteContent = _processingAlgorithm->analyzeNoteContent (this))
            _noteContent = std::move (newNoteContent);
    });
}

bool TestAnalysisTask::isDone () const
{
    return _future.wait_for (std::chrono::milliseconds { 0 }) == std::future_status::ready;
}

bool TestAnalysisTask::shouldCancel () const
{
    return _shouldCancel.load ();
}

void TestAnalysisTask::cancelSynchronously ()
{
    _shouldCancel = true;
    _future.wait ();
    _noteContent.reset ();   // delete here in case our future completed before recognizing the cancel
}

std::unique_ptr<TestNoteContent>&& TestAnalysisTask::transferNoteContent ()
{
    ARA_INTERNAL_ASSERT (isDone ());
    return std::move (_noteContent);
}
