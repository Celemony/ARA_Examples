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

#include "AudioFiles.h"

#include "ARA_API/ARAInterface.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"

#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

/*******************************************************************************/

SineAudioFile::SineAudioFile (const std::string& name, double duration, double sampleRate, int32_t channelCount)
: SineAudioFile { name, ARA::samplePositionAtTime (duration, sampleRate), sampleRate, channelCount }
{}

SineAudioFile::SineAudioFile (const std::string& name, int64_t sampleCount, double sampleRate, int32_t channelCount)
: AudioFileBase { name },
  _sampleCount { sampleCount },
  _sampleRate { sampleRate },
  _channelCount { channelCount }
{}

bool SineAudioFile::readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                                 void* const buffers[], bool use64BitSamples) noexcept
{
    RenderPulsedSineSignal (samplePosition, getSampleRate (), getSampleCount (),
                            getChannelCount (), samplesPerChannel, buffers, use64BitSamples);
    return true;
}

bool SineAudioFile::saveToFile (const std::string& path)
{
    // first we copy our sample data to a new icstdsp::AudioFile
    icstdsp::AudioFile audioFile;
    audioFile.Create (static_cast<unsigned int> (getSampleCount ()),
                      static_cast<unsigned int> (getChannelCount ()),
                      static_cast<unsigned int> (getSampleRate () + 0.5));

    std::vector<float*> audioSampleBuffers;
    for (auto c { 0 }; c < getChannelCount (); c++)
        audioSampleBuffers.push_back (audioFile.GetSafePt (static_cast<unsigned int> (c)));
    readSamples (0, getSampleCount (), reinterpret_cast<void**> (audioSampleBuffers.data ()), false);

    // now we create an AudioDataFile from that
    AudioDataFile dataFile { {}, std::move (audioFile) };
    return dataFile.saveToFile (path);
}

/*******************************************************************************/

AudioDataFile::AudioDataFile (const std::string& name, icstdsp::AudioFile&& audioFile)
: AudioFileBase (name),
  _audioFile (std::move (audioFile))
{}

bool AudioDataFile::readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                                 void* const buffers[], bool use64BitSamples) noexcept
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

const uint8_t* AudioDataFile::getiXMLChunk (size_t* size)
{
    unsigned int dataLength { 0 };
    auto result = _audioFile.GetiXMLData (&dataLength);
    *size = dataLength;
    return result;
}

bool AudioDataFile::saveToFile (const std::string& path)
{
    auto validatedPath { path };
    const auto extension { (path.length () < 4) ? "" : path.substr (path.length () - 4) };
    if ((extension != ".wav") && (extension != ".aif"))
        validatedPath += ".wav";

    return (_audioFile.SaveWave (validatedPath.c_str ()) == 0);
}
