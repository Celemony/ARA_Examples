//------------------------------------------------------------------------------
//! \file       TestAnalysis.cpp
//!             dummy implementation of audio source analysis for the ARA test plug-in
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
#include "TestPersistency.h"

#include "ARA_API/ARAInterface.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"

#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#include <chrono>
#include <thread>
#include <cmath>

// The test plug-in pretends to be able to do a kARAContentTypeNotes analysis:
// To simulate this, it reads all samples and creates a note with invalid pitch for each range of
// consecutive samples that are not 0. It also tracks the peak amplitude throughout each note and
// assumes this as note volume. (Note that actual plug-ins would rather use some calculation closer
// to RMS for determining volume.) This is no meaningful algorithm for real-world signals, but it
// was chosen so that the resulting note data can be easily verified both manually and via scripts
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


/*******************************************************************************/

void encodeTestNoteContent (const TestNoteContent* content, TestArchiver& archiver)
{
    archiver.writeBool (content != nullptr);
    if (content != nullptr)
    {
        const auto numNotes { content->size () };
        archiver.writeSize (numNotes);
        for (const auto& noteToPersist : *content)
        {
            archiver.writeDouble (noteToPersist._frequency);
            archiver.writeDouble (noteToPersist._volume);
            archiver.writeDouble (noteToPersist._startTime);
            archiver.writeDouble (noteToPersist._duration);
        }
    }
}

std::unique_ptr<TestNoteContent> decodeTestNoteContent (TestUnarchiver& unarchiver)
{
    std::unique_ptr<TestNoteContent> result;
    const bool hasNoteContent { unarchiver.readBool () };
    if (hasNoteContent)
    {
        const auto numNotes { unarchiver.readSize () };
        result = std::make_unique<TestNoteContent> (numNotes);
        for (TestNote& persistedNote : *result)
        {
            persistedNote._frequency = static_cast<float> (unarchiver.readDouble ());
            persistedNote._volume = static_cast<float> (unarchiver.readDouble ());
            persistedNote._startTime = unarchiver.readDouble ();
            persistedNote._duration = unarchiver.readDouble ();
        }
    }
    return result;
}

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

    std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisCallbacks* analysisCallbacks, const int64_t sampleCount, const double sampleRate, const uint32_t channelCount) const noexcept override
    {
        analysisCallbacks->notifyAnalysisProgressStarted ();

#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
        // helper variables to artificially slow down analysis as indicated by ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR
        const auto analysisStartTime { ARA_GET_CURRENT_TIME () };
        const auto analysisTargetDuration { ARA::timeAtSamplePosition (sampleCount, sampleRate) / ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR };
#endif

        // setup buffers and audio reader for reading samples
        constexpr auto blockSize { 64U };
        std::vector<float> buffer (channelCount * blockSize);
        std::vector<void*> dataPointers (channelCount);
        for (auto c { 0U }; c < channelCount; ++c)
            dataPointers[c] = &buffer[c * blockSize];

        // search the audio for silence and treat each region between silence as a note
        int64_t blockStartIndex { 0 };
        int64_t lastNoteStartIndex { 0 };
        bool wasZero { true };      // samples before the start of the file are 0
        float volume { 0.0f };
        std::vector<TestNote> foundNotes;
        while (true)
        {
            // check cancel
            if (analysisCallbacks->shouldCancel ())
            {
                analysisCallbacks->notifyAnalysisProgressCompleted ();
                return {};
            }

            // calculate size of current block and check if done
            const auto count { std::min (static_cast<int64_t> (blockSize), sampleCount - blockStartIndex) };
            if (count <= 0)
                break;

            // read samples - note that this test code ignores any errors that the reader might return here!
            analysisCallbacks->readAudioSamples (blockStartIndex, count, dataPointers.data ());

            // analyze current block
            for (int64_t i { 0 }; (i < count) && (foundNotes.size () < ARA_FAKE_NOTE_MAX_COUNT); ++i)
            {
                // check if current sample is zero on all channels
                bool isZero { true };
                for (int64_t c { 0 }; c < channelCount; ++c)
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
                        const double noteStartTime { static_cast<double> (lastNoteStartIndex) / sampleRate };
                        const double noteDuration { static_cast<double> (index - lastNoteStartIndex) / sampleRate };
                        TestNote foundNote { ARA::kARAInvalidFrequency, volume, noteStartTime, noteDuration };
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
            const float progress { 0.999f * static_cast<float> (blockStartIndex) / static_cast<float> (sampleCount) };
            analysisCallbacks->notifyAnalysisProgressUpdated (progress);

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
            const double noteStartTime { static_cast<double> (lastNoteStartIndex) / sampleRate };
            const double noteDuration { static_cast<double> (sampleCount - lastNoteStartIndex) / sampleRate };
            TestNote foundNote { ARA::kARAInvalidFrequency, volume, noteStartTime, noteDuration };
            foundNotes.push_back (foundNote);
        }

        // complete analysis and store result
        analysisCallbacks->notifyAnalysisProgressCompleted ();
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

    std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisCallbacks* analysisCallbacks, const int64_t sampleCount, const double sampleRate, uint32_t /*channelCount*/) const override
    {
        analysisCallbacks->notifyAnalysisProgressStarted ();

#if ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR != 0
        // for testing purposes only, sleep here until dummy analysis time has elapsed
        const auto analysisTargetDuration { ARA::timeAtSamplePosition (sampleCount, sampleRate) / ARA_FAKE_NOTE_ANALYSIS_SPEED_FACTOR };
        constexpr auto sliceDuration { 0.05 };
        const auto count { (analysisTargetDuration > sliceDuration) ? static_cast<int> (analysisTargetDuration / sliceDuration + 0.5) : 1 };
        for (auto i { 0 }; i < count; ++i)
        {
            // check cancel
            if (analysisCallbacks->shouldCancel ())
            {
                analysisCallbacks->notifyAnalysisProgressCompleted ();
                return {};
            }

            analysisCallbacks->notifyAnalysisProgressUpdated (static_cast<float> (i) / static_cast<float> (count));
            std::this_thread::sleep_for (std::chrono::milliseconds (std::llround (sliceDuration * 1000)));
        }
#endif

        TestNote foundNote { ARA::kARAInvalidFrequency, 1.0f, 0.0, ARA::timeAtSamplePosition (sampleCount, sampleRate) };
        analysisCallbacks->notifyAnalysisProgressCompleted ();
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
    const auto it { std::find_if (begin, end, [identifier] (const TestProcessingAlgorithm* algorithm)
                                                    { return algorithm->getIdentifier ().compare (identifier) == 0; } ) };
    return (it != end) ? *it : nullptr;
}
