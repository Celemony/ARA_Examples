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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#pragma once

#include "ARA_API/ARAInterface.h"

#include "ARA_Library/Utilities/ARAStdVectorUtilities.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"

#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#include <string>

#include "3rdParty/ICST_AudioFile/AudioFile.h"

class Document;
class MusicalContext;
class RegionSequence;
class AudioSource;
class AudioModification;
class PlaybackRegion;

/*******************************************************************************/

class AudioFileBase
{
public:
    AudioFileBase (std::string name)
    : _name { name }
    {}
    virtual ~AudioFileBase () = default;

    virtual int64_t getSampleCount () const noexcept = 0;
    virtual double getSampleRate () const noexcept = 0;
    virtual int getChannelCount () const noexcept = 0;
    virtual bool merits64BitSamples () const noexcept = 0;

    virtual bool readSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[], bool use64BitSamples) noexcept = 0;

    virtual const uint8_t* getiXMLChunk (size_t* length) { *length = 0; return nullptr; }

    void setName (std::string name) noexcept { _name = name; }
    const std::string& getName () const noexcept { return _name; }

private:
    std::string _name;
};

class SineAudioFile : public AudioFileBase
{
public:
    SineAudioFile (std::string name, int64_t sampleCount, double sampleRate, int channelCount);

    int64_t getSampleCount () const noexcept override { return _sampleCount; }
    double getSampleRate () const noexcept override { return _sampleRate; }
    int getChannelCount () const noexcept override { return _channelCount; }
    bool merits64BitSamples () const noexcept override { return true; }

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[], bool use64BitSamples) noexcept override;

private:
    int64_t _sampleCount;
    double _sampleRate;
    int _channelCount;
};

class AudioDataFile : public AudioFileBase
{
public:
    AudioDataFile (std::string name, icstdsp::AudioFile&& audioFile);

    int64_t getSampleCount () const noexcept override { return _audioFile.GetSize (); }
    double getSampleRate () const noexcept override { return _audioFile.GetRate (); }
    int getChannelCount () const noexcept override { return static_cast<int> (_audioFile.GetChannels ()); }
    bool merits64BitSamples () const noexcept override { return false; }

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[], bool use64BitSamples) noexcept override;

    const uint8_t* getiXMLChunk (size_t* length) override;

private:
    icstdsp::AudioFile _audioFile;
};
