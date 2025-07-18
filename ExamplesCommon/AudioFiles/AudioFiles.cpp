//------------------------------------------------------------------------------
//! \file       AudioFiles.cpp
//!             classes representing audio files
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

#include "AudioFiles.h"

#include "ARA_API/ARAAudioFileChunks.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"

#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

#include "3rdParty/pugixml/src/pugixml.hpp"
#include "3rdParty/cpp-base64/base64.h"

#include <cstring>
#include <vector>
#include <sstream>


/*******************************************************************************/

class ARAiXMLChunk
{
public:
    ARAiXMLChunk ()
    : ARAiXMLChunk { 0, nullptr }
    {}

    ARAiXMLChunk (size_t dataLength, const uint8_t data[])
    {
        if (dataLength)
        {
            _iXMLChunk.load_buffer (data, dataLength);
            // enable this to log chunk data without parsing
            //_iXMLChunk.save (std::cout);
        }

        auto iXMLNode { _iXMLChunk.child ("BWFXML") };
        if (iXMLNode.empty ())
            iXMLNode = _iXMLChunk.append_child ("BWFXML");

        auto araNode { iXMLNode.child (ARA::kARAXMLName_ARAVendorKeyword) };
        if (araNode.empty ())
            araNode = iXMLNode.append_child (ARA::kARAXMLName_ARAVendorKeyword);

        _audioSourceArchives = araNode.child (ARA::kARAXMLName_AudioSources);
        if (_audioSourceArchives.empty ())
            _audioSourceArchives = araNode.append_child (ARA::kARAXMLName_AudioSources);
    }

    std::string getAudioSourceData (const std::string& documentArchiveID, bool& openAutomatically,
                                    std::string& plugInName, std::string& plugInVersion,
                                    std::string& manufacturer, std::string& informationURL,
                                    std::string& persistentID) const
    {
        pugi::xml_node archive;
        for (const auto& it : _audioSourceArchives.children (ARA::kARAXMLName_AudioSource))
        {
            if (std::strcmp (it.child_value (ARA::kARAXMLName_DocumentArchiveID), documentArchiveID.c_str ()) == 0)
            {
                archive = it;
                break;
            }
        }
        if (archive.empty ())
        {
            openAutomatically = false;
            plugInName = plugInVersion = manufacturer = informationURL = persistentID = {};
            return {};
        }

        openAutomatically = (std::strcmp (archive.child_value (ARA::kARAXMLName_OpenAutomatically), "true") == 0);

        const auto suggestedPlugIn { archive.child (ARA::kARAXMLName_SuggestedPlugIn) };
        plugInName = suggestedPlugIn.child_value (ARA::kARAXMLName_PlugInName);
        plugInVersion = suggestedPlugIn.child_value (ARA::kARAXMLName_LowestSupportedVersion);
        manufacturer = suggestedPlugIn.child_value (ARA::kARAXMLName_ManufacturerName);
        informationURL = suggestedPlugIn.child_value (ARA::kARAXMLName_InformationURL);

        persistentID = archive.child_value (ARA::kARAXMLName_PersistentID);

#if __cplusplus >= 201703L
        return base64_decode (std::string_view { archive.child_value (ARA::kARAXMLName_ArchiveData) }, true);
#else
        return base64_decode (archive.child_value (ARA::kARAXMLName_ArchiveData), true);
#endif
    }

    void setAudioSourceData (const std::string& documentArchiveID, bool openAutomatically,
                             const std::string& plugInName, const std::string& plugInVersion,
                             const std::string& manufacturer, const std::string& informationURL,
                             const std::string& persistentID, const std::string& data)
    {
        pugi::xml_node archive;
        for (const auto& it : _audioSourceArchives.children (ARA::kARAXMLName_AudioSource))
        {
            if (std::strcmp (it.child_value (ARA::kARAXMLName_DocumentArchiveID), documentArchiveID.c_str ()) == 0)
            {
                archive = it;
                break;
            }
        }
        if (archive.empty ())
            archive = _audioSourceArchives.append_child (ARA::kARAXMLName_AudioSource);
        else
            archive.remove_children ();

        archive.append_child (ARA::kARAXMLName_DocumentArchiveID).append_child (pugi::node_pcdata).set_value (documentArchiveID.c_str ());
        archive.append_child (ARA::kARAXMLName_OpenAutomatically).append_child (pugi::node_pcdata).set_value ((openAutomatically) ? "true" : "false");

        auto suggestedPlugIn { archive.append_child (ARA::kARAXMLName_SuggestedPlugIn) };
        suggestedPlugIn.append_child (ARA::kARAXMLName_PlugInName).append_child (pugi::node_pcdata).set_value (plugInName.c_str ());
        suggestedPlugIn.append_child (ARA::kARAXMLName_LowestSupportedVersion).append_child (pugi::node_pcdata).set_value (plugInVersion.c_str ());
        suggestedPlugIn.append_child (ARA::kARAXMLName_ManufacturerName).append_child (pugi::node_pcdata).set_value (manufacturer.c_str ());
        suggestedPlugIn.append_child (ARA::kARAXMLName_InformationURL).append_child (pugi::node_pcdata).set_value (informationURL.c_str ());

        archive.append_child (ARA::kARAXMLName_PersistentID).append_child (pugi::node_pcdata).set_value (persistentID.c_str ());

        std::string encodedArchiveData { base64_encode (data) };
        archive.append_child (ARA::kARAXMLName_ArchiveData).append_child (pugi::node_pcdata).set_value (encodedArchiveData.c_str ());
        // enable this to log edited chunk data
        //_iXMLChunk.save (std::cout);
    }

    std::string getData () const
    {
        std::ostringstream writer;
        _iXMLChunk.save (writer);
        return writer.str ();
    }

private:
    pugi::xml_document _iXMLChunk;
    pugi::xml_node _audioSourceArchives;
};

/*******************************************************************************/

void AudioFileBase::setiXMLChunk (ARAiXMLChunk* chunk) noexcept
{
    delete _iXMLChunk;
    _iXMLChunk = chunk;
}

void AudioFileBase::setiXMLARAAudioSourceData (const std::string& documentArchiveID, bool openAutomatically,
                                               const std::string& plugInName, const std::string& plugInVersion,
                                               const std::string& manufacturer, const std::string& informationURL,
                                               const std::string& persistentID, const std::string& data)
{
    if (!_iXMLChunk)
        _iXMLChunk = new ARAiXMLChunk {};
    _iXMLChunk->setAudioSourceData (documentArchiveID, openAutomatically,
                                    plugInName, plugInVersion, manufacturer, informationURL,
                                    persistentID, data);
}

std::string AudioFileBase::getiXMLARAAudioSourceData (const std::string& documentArchiveID, bool& openAutomatically,
                                                      std::string& plugInName, std::string& plugInVersion,
                                                      std::string& manufacturer, std::string& informationURL,
                                                      std::string& persistentID)
{
    if (!_iXMLChunk)
    {
        openAutomatically = false;
        plugInName = plugInVersion = manufacturer = informationURL = persistentID = {};
        return {};
    }

    return _iXMLChunk->getAudioSourceData (documentArchiveID, openAutomatically,
                                           plugInName, plugInVersion, manufacturer, informationURL, persistentID);
}

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

    // if we have iXML data, we copy that into the icstdsp::AudioFile too
    if (auto iXMLChunk { getiXMLChunk () })
    {
        const auto data { iXMLChunk->getData () };
        audioFile.SetiXMLData (reinterpret_cast<const uint8_t*> (data.c_str ()),
                               static_cast<unsigned int> (data.size ()));
    }

    // now we create an AudioDataFile from that, copy over the iXML and store it
    return AudioDataFile { {}, std::move (audioFile) }.saveToFile (path);
}

/*******************************************************************************/

AudioDataFile::AudioDataFile (const std::string& name, icstdsp::AudioFile&& audioFile)
: AudioFileBase { name },
  _audioFile { std::move (audioFile) }
{
    unsigned int dataLength { 0 };
    auto data = _audioFile.GetiXMLData (&dataLength);
    if ((data != nullptr) && (dataLength > 0))
        setiXMLChunk (new ARAiXMLChunk { dataLength, data});
}

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

bool AudioDataFile::saveToFile (const std::string& path)
{
    if (auto iXMLChunk { getiXMLChunk () })
    {
        const auto data { iXMLChunk->getData () };
        _audioFile.SetiXMLData (reinterpret_cast<const uint8_t*> (data.c_str ()),
                                static_cast<unsigned int> (data.size ()));
    }

    auto validatedPath { path };
    const auto extension { (path.length () < 4) ? "" : path.substr (path.length () - 4) };
    if ((extension != ".wav") && (extension != ".aif"))
        validatedPath += ".wav";

    return (_audioFile.SaveWave (validatedPath.c_str ()) == 0);
}
