//------------------------------------------------------------------------------
//! \file       AudioFiles.cpp
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

#include "AudioFiles.h"

#include "ARA_API/ARAAudioFileChunks.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"

#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

#include "3rdParty/pugixml/src/pugixml.hpp"
#include "3rdParty/cpp-base64/base64.h"

#include <cstring>
#include <vector>
#include <sstream>
#include <filesystem>


/*******************************************************************************/

inline static const std::string filenameHelper (const std::string& path)
{
    std::filesystem::path fp { path };
    if (fp.has_filename ())
        return fp.filename ().string ();
    else
        return path;
}

/*******************************************************************************/

class IXMLChunk
{
public:
    IXMLChunk ()
    : IXMLChunk { 0, nullptr }
    {}

    IXMLChunk (size_t dataLength, const uint8_t data[])
    {
        if (dataLength)
        {
            _iXMLChunk.load_buffer (data, dataLength);
            // enable this to log chunk data without parsing
            //_iXMLChunk.save (std::cout);
        }
    }

    std::string getARAAudioSourceData (const std::string& documentArchiveID, bool& openAutomatically,
                                       std::string& plugInName, std::string& plugInVersion,
                                       std::string& manufacturer, std::string& informationURL,
                                       std::string& persistentID) const
    {
        auto araNode { getIXMLSubNodeIfPresent (ARA::kARAXMLName_ARAVendorKeyword) };

        pugi::xml_node archive;
        auto audioSourceArchives { araNode.child (ARA::kARAXMLName_AudioSources) };
        for (const auto& it : audioSourceArchives.children (ARA::kARAXMLName_AudioSource))
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

        return base64_decode (std::string_view { archive.child_value (ARA::kARAXMLName_ArchiveData) }, true);
    }

    void setARAAudioSourceData (const std::string& documentArchiveID, bool openAutomatically,
                                const std::string& plugInName, const std::string& plugInVersion,
                                const std::string& manufacturer, const std::string& informationURL,
                                const std::string& persistentID, const std::string& data)
    {
        auto araNode { getOrCreateIXMLSubNode (ARA::kARAXMLName_ARAVendorKeyword) };

        auto audioSourceArchives { araNode.child (ARA::kARAXMLName_AudioSources) };
        if (audioSourceArchives.empty ())
            audioSourceArchives = araNode.append_child (ARA::kARAXMLName_AudioSources);

        pugi::xml_node archive;
        for (const auto& it : audioSourceArchives.children (ARA::kARAXMLName_AudioSource))
        {
            if (std::strcmp (it.child_value (ARA::kARAXMLName_DocumentArchiveID), documentArchiveID.c_str ()) == 0)
            {
                archive = it;
                break;
            }
        }
        if (archive.empty ())
            archive = audioSourceArchives.append_child (ARA::kARAXMLName_AudioSource);
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

    struct Marker
    {
        std::string text;
        uint64_t startSample;
        uint32_t duration;
    };

    const std::vector<Marker> getMarkerData () const
    {
        auto syncPointsNode { getIXMLSubNodeIfPresent (kIXMLName_SyncPointListKeyword) };
        if (syncPointsNode.empty ())
            return {};

        std::vector<Marker> result;
        for (const auto& it : syncPointsNode.children (kIXMLName_SyncPointKeyword))
        {
            const auto type { it.child_value (kIXMLName_SyncPointTypeKeyword) };
            if (type && (std::strcmp (type, kIXMLName_SyncPointTypeRelativeValue) != 0))
                continue;

            const auto function { it.child_value (kIXMLName_SyncPointFunctionKeyword) };
            if (function && (std::strcmp (function, kIXMLName_SyncPointFunctionMarkerGenericValue) != 0))
                continue;

            const auto commentChild { it.child (kIXMLName_SyncPointCommentKeyword) };
            const auto lowChild { it.child (kIXMLName_SyncPointLowKeyword) };
            if (commentChild.empty () || lowChild.empty ())
                continue;
            const auto low { stoul (std::string { lowChild.child_value () }) };
            const auto highValue { it.child_value (kIXMLName_SyncPointHighKeyword) };
            const auto high { stoul (std::string { highValue }) };
            const auto durationValue { it.child_value (kIXMLName_SyncPointEventDurationKeyword) };
            const auto duration { stoul (std::string { durationValue }) };
            result.emplace_back (Marker { commentChild.child_value (), (static_cast<uint64_t> (high) << 32) + low, static_cast<uint32_t> (duration) });
        }
        return result;
    }

    void setMarkerData (const std::vector<Marker>& markers)
    {
        auto syncPointsNode { getOrCreateIXMLSubNode (kIXMLName_SyncPointListKeyword) };
        syncPointsNode.remove_children ();
        const auto countValue { std::to_string (markers.size ()) };
        syncPointsNode.append_child (kIXMLName_SyncPointCountKeyword).append_child (pugi::node_pcdata).set_value (countValue.c_str ());
        for (const auto& marker : markers)
        {
            auto entry { syncPointsNode.append_child (kIXMLName_SyncPointKeyword) };
            entry.append_child (kIXMLName_SyncPointTypeKeyword).append_child (pugi::node_pcdata).set_value (kIXMLName_SyncPointTypeRelativeValue);
            entry.append_child (kIXMLName_SyncPointFunctionKeyword).append_child (pugi::node_pcdata).set_value (kIXMLName_SyncPointFunctionMarkerGenericValue);
            entry.append_child (kIXMLName_SyncPointCommentKeyword).append_child (pugi::node_pcdata).set_value (marker.text.c_str ());
            const auto lowValue { std::to_string (marker.startSample & 0xFFFFFFFFul) };
            const auto highValue { std::to_string (marker.startSample >> 32) };
            const auto durationValue { std::to_string (marker.duration) };
            entry.append_child (kIXMLName_SyncPointLowKeyword).append_child (pugi::node_pcdata).set_value (lowValue.c_str ());
            entry.append_child (kIXMLName_SyncPointHighKeyword).append_child (pugi::node_pcdata).set_value (highValue.c_str ());
            entry.append_child (kIXMLName_SyncPointEventDurationKeyword).append_child (pugi::node_pcdata).set_value (durationValue.c_str ());
        }
    }

    std::string getData () const
    {
        std::ostringstream writer;
        _iXMLChunk.save (writer);
        return writer.str ();
    }

private:
    pugi::xml_node getOrCreateIXMLSubNode (const char* key)
    {
        auto iXMLNode { _iXMLChunk.child (kIXMLName_IXMLRootKeyword) };
        if (iXMLNode.empty ())
            iXMLNode = _iXMLChunk.append_child (kIXMLName_IXMLRootKeyword);
        auto subNode { iXMLNode.child (key) };
        if (subNode.empty ())
            subNode = iXMLNode.append_child (key);
        return subNode;
    }

    const pugi::xml_node getIXMLSubNodeIfPresent (const char* key) const
    {
        return _iXMLChunk.child (kIXMLName_IXMLRootKeyword).child (key);
    }

private:
    static constexpr auto kIXMLName_IXMLRootKeyword { "BWFXML" };
    static constexpr auto kIXMLName_SyncPointListKeyword { "SYNC_POINT_LIST" };
    static constexpr auto kIXMLName_SyncPointCountKeyword { "SYNC_POINT_COUNT" };
    static constexpr auto kIXMLName_SyncPointKeyword { "SYNC_POINT" };
    static constexpr auto kIXMLName_SyncPointTypeKeyword { "SYNC_POINT_TYPE" };
    static constexpr auto kIXMLName_SyncPointTypeRelativeValue { "RELATIVE" };
    static constexpr auto kIXMLName_SyncPointFunctionKeyword { "SYNC_POINT_FUNCTION" };
    static constexpr auto kIXMLName_SyncPointFunctionMarkerGenericValue { "MARKER_GENERIC" };
    static constexpr auto kIXMLName_SyncPointCommentKeyword { "SYNC_POINT_COMMENT" };
    static constexpr auto kIXMLName_SyncPointLowKeyword { "SYNC_POINT_LOW" };
    static constexpr auto kIXMLName_SyncPointHighKeyword { "SYNC_POINT_HIGH" };
    static constexpr auto kIXMLName_SyncPointEventDurationKeyword { "SYNC_POINT_EVENT_DURATION" };

    pugi::xml_document _iXMLChunk;
};

/*******************************************************************************/

void AudioFileBase::setiXMLChunk (IXMLChunk* chunk) noexcept
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
        _iXMLChunk = new IXMLChunk {};
    _iXMLChunk->setARAAudioSourceData (documentArchiveID, openAutomatically,
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

    return _iXMLChunk->getARAAudioSourceData (documentArchiveID, openAutomatically,
                                           plugInName, plugInVersion, manufacturer, informationURL, persistentID);
}

void AudioFileBase::parseHyphenInLyricsAfterImport (std::vector<LyricsEntry>& lyricsEntries)
{
    bool nextEntryWillBeContinuation { false };
    for (auto& entry : lyricsEntries)
    {
        entry.continuesPreviousWord |= nextEntryWillBeContinuation;

        // handle continuation at begin
        auto first { entry.lyrics.cbegin () };
        if (!entry.lyrics.empty () && (*first == '-'))
        {
            entry.lyrics.erase (first);
            entry.continuesPreviousWord = true;
        }

        // handle continuation at end
        const auto last { --entry.lyrics.cend () };
        if (!entry.lyrics.empty () && (*last == '-'))
        {
            entry.lyrics.erase (last);
            nextEntryWillBeContinuation = true;
        }
        else
        {
            nextEntryWillBeContinuation = false;
        }
    }
}

/*******************************************************************************/

SineAudioFile::SineAudioFile (const std::string& name, double duration, double sampleRate, int32_t channelCount)
: AudioFileBase { name, ARA::samplePositionAtTime (duration, sampleRate), sampleRate, duration, channelCount, true }
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

AudioDataFile::AudioDataFile (const std::string& path, icstdsp::AudioFile&& audioFile)
: AudioFileBase { filenameHelper (path), audioFile.GetSize (), static_cast<double> (audioFile.GetRate ()),
                  ARA::timeAtSamplePosition (audioFile.GetSize (), audioFile.GetRate ()),
                  static_cast<int> (audioFile.GetChannels ()), false },
  _audioFile { std::move (audioFile) }
{
    unsigned int dataLength { 0 };
    auto data { _audioFile.GetiXMLData (&dataLength) };
    if ((data != nullptr) && (dataLength > 0))
    {
        auto iXMLChunk { new IXMLChunk { dataLength, data } };
        setiXMLChunk (iXMLChunk);

        const auto markers { iXMLChunk->getMarkerData () };
        if (!markers.empty ())
        {
            std::vector<LyricsEntry> lyricsEntries;
            for (auto& marker : markers)
            {
                const LyricsEntry entry { marker.text, false, ARA::timeAtSamplePosition (marker.startSample, getSampleRate ()) };

                auto it { lyricsEntries.begin () };
                while ((it != lyricsEntries.end ()) && (it->position < entry.position))
                {
                    ++it;
                }
                if ((it != lyricsEntries.end ()) && (it->position == entry.position))
                    *it = entry;
                else
                    it = lyricsEntries.insert (it, entry);

                if (marker.duration > 0.0)
                {
                    const LyricsEntry nonEntry { "", false, ARA::timeAtSamplePosition (marker.startSample + marker.duration, getSampleRate ()) };
                    auto it2 { ++it };
                    while ((it2 != lyricsEntries.end ()) && (it2->position < nonEntry.position))
                    {
                        ++it2;
                    }
                    it2 = lyricsEntries.erase (it, it2);
                    if ((it2 != lyricsEntries.end ()) && (it2->position == nonEntry.position))
                        ;   // keep existing entry, it already limits the new entry
                    else
                        lyricsEntries.insert (it2, nonEntry);
                }
            }

            parseHyphenInLyricsAfterImport (lyricsEntries);
            setLyricsEntries (lyricsEntries);
        }
    }

    // \todo icstdsp::AudioFile does not support any marker chunks (e.g."cue " in wave files)
    //       we could add this and parse those as lyrics too if iXML didn't contain any...
}

AudioDataFile::AudioDataFile (const std::string& path, const std::vector<std::vector<float>>& samples, double sampleRate)
: AudioFileBase { filenameHelper (path), static_cast<int64_t> (samples[0].size ()), sampleRate,
                  ARA::timeAtSamplePosition (samples[0].size (), sampleRate), static_cast<int> (samples.size ()), false }
{
    auto nspkpos { 0u };
    switch (getChannelCount ())
    {
        case 1: nspkpos = icstdsp::SPK_FRONT_CENTER; break;
        case 2: nspkpos = icstdsp::SPK_FRONT_LEFT | icstdsp::SPK_FRONT_RIGHT; break;
        // \todo add other default layouts as needed
    }
    _audioFile.Create (static_cast<unsigned int> (getSampleCount ()), static_cast<unsigned int> (getChannelCount ()), 24u,
                       static_cast<unsigned int> (getSampleRate () + 0.5), nspkpos);

    for (auto i { 0u }; i < static_cast<unsigned int> (getChannelCount ()); ++i)
        std::memcpy (_audioFile.GetSafePt (static_cast<unsigned int> (i)), samples[i].data (), samples[i].size () * sizeof (float));
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

/*******************************************************************************/

MIDIFile::MIDIFile (const std::string& path, smf::MidiFile&& midiFile)
: AudioFileBase { filenameHelper (path), 0, 0.0, midiFile.getFileDurationInSeconds (), 0, false },
  _midiFile { std::move (midiFile) }
{
    _midiFile.joinTracks ();
    _midiFile.linkEventPairs ();
    _midiFile.doTimeAnalysis ();

    const int eventCount { _midiFile.getEventCount (0) };
    std::vector<MIDINote> midiNotes;
    std::vector<LyricsEntry> lyricsEntries;
    bool foundLyricsEvents { false };
    bool foundTextEvents { false };
    bool nextEntryWillBeContinuation { false };
    for (int i { 0 }; i < eventCount; ++i)
    {
        const auto& event { _midiFile.getEvent (0, i) };
        if (event.isNoteOn ())
        {
            midiNotes.emplace_back (MIDINote { static_cast<uint8_t> (event.getKeyNumber ()), static_cast<uint8_t> (event.getVelocity ()),
                                               event.seconds, event.getDurationInSeconds () });
        }
        else if (event.isLyricText ())
        {
            if (!foundLyricsEvents)
            {
                lyricsEntries.clear ();
                foundLyricsEvents = true;
            }

            if (event.getMetaContent ().empty ())       // melisma: simply skip event to let previous event continue
                continue;

            LyricsEntry entry { event.getMetaContent (), nextEntryWillBeContinuation, event.seconds };;
            nextEntryWillBeContinuation = true;

            auto it { entry.lyrics.end () };
            while (it-- > entry.lyrics.begin ())
            {
                if ((*it == '\r') || (*it == '\n') ||   // remove trailing line/paragraph end marker
                    (*it == ' '))                       // remove trailing continuation marker
                {
                    entry.lyrics.pop_back ();
                    nextEntryWillBeContinuation = false;
                    continue;
                }
                break;
            }

            if (entry.lyrics.empty ())                        // skip empty events
            {
                nextEntryWillBeContinuation = false;
                continue;
            }

            lyricsEntries.emplace_back (entry);
        }
        else if (event.isText ())
        {
            if (foundLyricsEvents)
                continue;
            if (!foundTextEvents)
            {
                lyricsEntries.clear ();
                foundLyricsEvents = true;
            }
            lyricsEntries.emplace_back (LyricsEntry { event.getMetaContent (), false, event.seconds });
        }
        else if (event.isMarkerText ())
        {
            if (foundLyricsEvents || foundTextEvents)
                continue;
            lyricsEntries.emplace_back (LyricsEntry { event.getMetaContent (), false, event.seconds });
        }
    }

    setMIDINotes (midiNotes);

    if (!foundLyricsEvents)
        parseHyphenInLyricsAfterImport (lyricsEntries);
    setLyricsEntries (lyricsEntries);
}

bool MIDIFile::readSamples (int64_t /*samplePosition*/, int64_t /*samplesPerChannel*/,
                                 void* const /*buffers*/[], bool /*use64BitSamples*/) noexcept
{
    abort ();
}

bool MIDIFile::saveToFile (const std::string& path)
{
    auto validatedPath { path };
    const auto extension { (path.length () < 4) ? "" : path.substr (path.length () - 4) };
    if ((extension != ".mid") && (extension != ".midi") && (extension != ".kar") )
        validatedPath += ".mid";

    return (_midiFile.write (validatedPath));
}
