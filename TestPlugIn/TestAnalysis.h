//------------------------------------------------------------------------------
//! \file       TestAnalysis.h
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

#pragma once

#include <string>
#include <vector>
#include <memory>

class TestArchiver;
class TestUnarchiver;

/*******************************************************************************/
struct TestNote
{
    float _frequency;
    float _volume;
    double _startTime;
    double _duration;
};

/*******************************************************************************/
using TestNoteContent = std::vector<TestNote>;

void encodeTestNoteContent (const TestNoteContent* content, TestArchiver& archiver);
std::unique_ptr<TestNoteContent> decodeTestNoteContent (TestUnarchiver& unarchiver);

/*******************************************************************************/
class TestAnalysisCallbacks
{
public:
    virtual ~TestAnalysisCallbacks () = default;
    virtual void notifyAnalysisProgressStarted () noexcept {}
    virtual void notifyAnalysisProgressUpdated (float /*progress*/) noexcept {}
    virtual void notifyAnalysisProgressCompleted () noexcept {}
    virtual bool readAudioSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[]) noexcept = 0;
    virtual bool shouldCancel () const noexcept { return false; }
};

/*******************************************************************************/
class TestProcessingAlgorithm
{
protected:
    inline TestProcessingAlgorithm () = default;

public:
    virtual ~TestProcessingAlgorithm () = default;

    static std::vector<const TestProcessingAlgorithm*> const& getAlgorithms ();
    static const TestProcessingAlgorithm* getDefaultAlgorithm ();
    static const TestProcessingAlgorithm* getAlgorithmWithIdentifier (const std::string& identifier);

    virtual const std::string& getName () const = 0;
    virtual const std::string& getIdentifier () const = 0;

    virtual std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisCallbacks* analysisCallbacks, int64_t sampleCount, double sampleRate, uint32_t channelCount) const = 0;
};
