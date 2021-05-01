//------------------------------------------------------------------------------
//! \file       AudioFiles.h
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

#pragma once

#include <stdio.h>
#include "3rdParty/ICST_AudioFile/AudioFile.h"

#include <cstdint>
#include <string>

/*******************************************************************************/

// Abstract interface for audio files.
class AudioFileBase
{
public:
    AudioFileBase (const std::string& name) : _name { name } {}
    virtual ~AudioFileBase () = default;

    virtual int64_t getSampleCount () const noexcept = 0;
    virtual double getSampleRate () const noexcept = 0;
    virtual int getChannelCount () const noexcept = 0;
    virtual bool merits64BitSamples () const noexcept = 0;

    virtual bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                              void* const buffers[], bool use64BitSamples) noexcept = 0;

    virtual const uint8_t* getiXMLChunk (size_t* length) { *length = 0; return nullptr; }

    virtual bool saveToFile (const std::string& path) = 0;

    const std::string& getName () const noexcept { return _name; }
    void setName (const std::string& name) noexcept { _name = name; }

private:
    std::string _name;
};

/*******************************************************************************/

// Dummy in-memory audio file based on a generated pulsed sine wave.
class SineAudioFile : public AudioFileBase
{
public:
    SineAudioFile (const std::string& name, double duration, double sampleRate, int channelCount);
    SineAudioFile (const std::string& name, int64_t sampleCount, double sampleRate, int channelCount);

    int64_t getSampleCount () const noexcept override { return _sampleCount; }
    double getSampleRate () const noexcept override { return _sampleRate; }
    int getChannelCount () const noexcept override { return _channelCount; }
    bool merits64BitSamples () const noexcept override { return true; }

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                      void* const buffers[], bool use64BitSamples) noexcept override;

    bool saveToFile (const std::string& path) override;

private:
    int64_t _sampleCount;
    double _sampleRate;
    int _channelCount;
};

/*******************************************************************************/

// Encapsulation of a WAVE or AIFF audio file.
class AudioDataFile : public AudioFileBase
{
public:
    AudioDataFile (const std::string& name, icstdsp::AudioFile&& audioFile);

    int64_t getSampleCount () const noexcept override { return _audioFile.GetSize (); }
    double getSampleRate () const noexcept override { return _audioFile.GetRate (); }
    int getChannelCount () const noexcept override { return static_cast<int> (_audioFile.GetChannels ()); }
    bool merits64BitSamples () const noexcept override { return false; }

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                      void* const buffers[], bool use64BitSamples) noexcept override;

    const uint8_t* getiXMLChunk (size_t* length) override;

    bool saveToFile (const std::string& path) override;

private:
    icstdsp::AudioFile _audioFile;
};
