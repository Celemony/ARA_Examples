//------------------------------------------------------------------------------
//! \file       AudioFiles.cpp
//!             classes representing audio files
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

#include "AudioFiles.h"

#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

/*******************************************************************************/

SineAudioFile::SineAudioFile (std::string name, int64_t sampleCount, double sampleRate, int32_t channelCount)
: AudioFileBase { name },
  _sampleCount { sampleCount },
  _sampleRate { sampleRate },
  _channelCount { channelCount }
{}

bool SineAudioFile::readSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[], bool use64BitSamples) noexcept
{
    RenderPulsedSineSignal (samplePosition, getSampleRate (), getSampleCount (),
                            getChannelCount (), samplesPerChannel, buffers, use64BitSamples);
    return true;
}

/*******************************************************************************/

AudioDataFile::AudioDataFile (std::string name, icstdsp::AudioFile&& audioFile)
: AudioFileBase (name),
  _audioFile (std::move (audioFile))
{}

bool AudioDataFile::readSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[], bool use64BitSamples) noexcept
{
    auto index { 0L };
    while (samplesPerChannel--)
    {
        for (auto i { 0 }; i < getChannelCount (); ++i)
        {
            auto value = _audioFile.GetSafePt (static_cast<unsigned int> (i))[samplePosition];
            if (use64BitSamples)
                static_cast<double*> (buffers[i])[index] = value;
            else
                static_cast<float*> (buffers[i])[index] = static_cast<float> (value);
        }
        ++samplePosition;
        ++index;
    }

    return true;
}

const uint8_t* AudioDataFile::getiXMLChunk (size_t* length)
{
    unsigned int size { 0 };
    auto result = _audioFile.GetiXMLData (&size);
    *length = size;
    return result;
}
