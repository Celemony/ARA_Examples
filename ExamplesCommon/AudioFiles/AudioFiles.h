//------------------------------------------------------------------------------
//! \file       AudioFiles.h
//!             classes representing audio files
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2026, Celemony Software GmbH, All Rights Reserved.
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
#include "3rdParty/midifile/include/MidiFile.h"

#include <cstdint>
#include <string>

class ARAiXMLChunk;

/*******************************************************************************/

// Abstract interface for audio files.
class AudioFileBase
{
protected:
    AudioFileBase (const std::string& name, int64_t sampleCount, double sampleRate,
                   double duration, int channelCount, bool merits64BitSamples)
    : _name { name },
      _sampleCount { sampleCount },
      _sampleRate { sampleRate },
      _duration { duration },
      _channelCount { channelCount },
      _merits64BitSamples { merits64BitSamples }
    {}

public:
    virtual ~AudioFileBase () { setiXMLChunk (nullptr); }

    const std::string& getName () const noexcept { return _name; }
    void setName (const std::string& name) noexcept { _name = name; }

    int64_t getSampleCount () const noexcept { return _sampleCount; }
    double getSampleRate () const noexcept { return _sampleRate; }
    double getDuration () const noexcept { return _duration; }
    int getChannelCount () const noexcept { return _channelCount; }
    bool merits64BitSamples () const noexcept { return _merits64BitSamples; }

    virtual bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                              void* const buffers[], bool use64BitSamples) noexcept = 0;

    std::string getiXMLARAAudioSourceData (const std::string& documentArchiveID, bool& openAutomatically,
                                           std::string& plugInName, std::string& plugInVersion,
                                           std::string& manufacturer, std::string& informationURL,
                                           std::string& persistentID);
    void setiXMLARAAudioSourceData (const std::string& documentArchiveID, bool openAutomatically,
                                    const std::string& plugInName, const std::string& plugInVersion,
                                    const std::string& manufacturer, const std::string& informationURL,
                                    const std::string& persistentID, const std::string& data);

    struct MIDINote
    {
        uint8_t noteNumber;
        uint8_t velocity;
        double startTime;
        double duration;
    };
    const std::vector<MIDINote>& getMIDINotes () const noexcept { return _midiNotes; }
    void setMIDINotes (const std::vector<MIDINote>& midiNotes) noexcept { _midiNotes = midiNotes; }

    virtual bool saveToFile (const std::string& path) = 0;

protected:
    const ARAiXMLChunk* getiXMLChunk () const noexcept { return _iXMLChunk; }
    void setiXMLChunk (ARAiXMLChunk* chunk) noexcept;

private:
    std::string _name;
    const int64_t _sampleCount;
    const double _sampleRate;
    const double _duration;
    const int _channelCount;
    const bool _merits64BitSamples;
    ARAiXMLChunk* _iXMLChunk { nullptr };
    std::vector<MIDINote> _midiNotes;
};

/*******************************************************************************/

// Dummy in-memory audio file based on a generated pulsed sine wave.
class SineAudioFile : public AudioFileBase
{
public:
    SineAudioFile (const std::string& name, double duration, double sampleRate, int channelCount);

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                      void* const buffers[], bool use64BitSamples) noexcept override;

    bool saveToFile (const std::string& path) override;
};

/*******************************************************************************/

// Encapsulation of a WAVE or AIFF audio file.
class AudioDataFile : public AudioFileBase
{
public:
    AudioDataFile (const std::string& path, icstdsp::AudioFile&& audioFile);
    AudioDataFile (const std::string& path, const std::vector<std::vector<float>>& samples, double sampleRate);

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                      void* const buffers[], bool use64BitSamples) noexcept override;

    bool saveToFile (const std::string& path) override;

private:
    icstdsp::AudioFile _audioFile;
};

/*******************************************************************************/

// Encapsulation of a MIDI file.
class MIDIFile : public AudioFileBase
{
public:
    MIDIFile (const std::string& path, smf::MidiFile&& midiFile);

    bool readSamples (int64_t samplePosition, int64_t samplesPerChannel,
                      void* const buffers[], bool use64BitSamples) noexcept override;

    bool saveToFile (const std::string& path) override;

private:
    smf::MidiFile _midiFile;
};
