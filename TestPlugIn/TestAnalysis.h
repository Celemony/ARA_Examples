//------------------------------------------------------------------------------
//! \file       TestAnalysis.h
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

#pragma once

#include <atomic>
#include <future>
#include <string>
#include <vector>

namespace ARA
{
namespace PlugIn
{
    class HostAudioReader;
}
}

class ARATestAudioSource;
class TestAnalysisTask;

/*******************************************************************************/
struct TestNote
{
    float _frequency;
    float _volume;
    double _startTime;
    double _duration;
};

using TestNoteContent = std::vector<TestNote>;

/*******************************************************************************/
class TestProcessingAlgorithm
{
protected:
    inline TestProcessingAlgorithm () = default;

public:
    virtual ~TestProcessingAlgorithm () {}

    static std::vector<const TestProcessingAlgorithm*> const& getAlgorithms ();
    static const TestProcessingAlgorithm* getDefaultAlgorithm ();
    static const TestProcessingAlgorithm* getAlgorithmWithIdentifier (const std::string& identifier);

    virtual const std::string& getName () const = 0;
    virtual const std::string& getIdentifier () const = 0;

    virtual std::unique_ptr<TestNoteContent> analyzeNoteContent (TestAnalysisTask* analysisTask) const = 0;
};

/*******************************************************************************/
class TestAnalysisTask
{
public:
    explicit TestAnalysisTask (ARATestAudioSource* audioSource, const TestProcessingAlgorithm* processingAlgorithm);

    ARATestAudioSource* getAudioSource () const noexcept { return _audioSource; }
    const TestProcessingAlgorithm* getProcessingAlgorithm () const noexcept { return _processingAlgorithm; }
    ARA::PlugIn::HostAudioReader* getHostAudioReader () const noexcept { return _hostAudioReader.get (); }

    bool isDone () const;
    bool shouldCancel () const;
    void cancelSynchronously ();

    std::unique_ptr<TestNoteContent>&& transferNoteContent ();

private:
    ARATestAudioSource* const _audioSource;
    const TestProcessingAlgorithm* const _processingAlgorithm;
    const std::unique_ptr<ARA::PlugIn::HostAudioReader> _hostAudioReader;
    std::unique_ptr<TestNoteContent> _noteContent;
    std::future<void> _future;
    std::atomic<bool> _shouldCancel { false };
};
