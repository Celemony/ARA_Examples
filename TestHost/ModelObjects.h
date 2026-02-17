//------------------------------------------------------------------------------
//! \file       ModelObjects.h
//!             classes used to build the host model graph
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
#include "ExamplesCommon/AudioFiles/AudioFiles.h"

#include <string>

class Document;
class MusicalContext;
class RegionSequence;
class AudioSource;
class AudioModification;
class PlaybackRegion;

/*******************************************************************************/

// Shared base class for audio sources and musical contexts, which can both store content information.
class ContentContainer
{
public:
    template<typename ContentType>
    using EntryData = std::unique_ptr<std::vector<ContentType>>;

    void setNotes (std::vector<ARA::ARAContentNote> const& notes) { _notes = makeEntryData (notes); }
    void clearNotes () { _notes.reset (); }
    const EntryData<ARA::ARAContentNote>& getNotes () const noexcept { return _notes; }

    void setTempoEntries (std::vector<ARA::ARAContentTempoEntry> const& tempoEntries) { _tempoEntries = makeEntryData (tempoEntries); }
    void clearTempoEntries () { _tempoEntries.reset (); }
    const EntryData<ARA::ARAContentTempoEntry>& getTempoEntries () const noexcept { return _tempoEntries; }

    void setBarSignatures (std::vector<ARA::ARAContentBarSignature> const& barSignatures) { _barSignatures = makeEntryData (barSignatures); }
    void clearBarSignatures () { _barSignatures.reset (); }
    const EntryData<ARA::ARAContentBarSignature>& getBarSignatures () const noexcept { return _barSignatures; }

    void setTuning (ARA::ARAContentTuning const& tuning) { _tuning = makeEntryData (std::vector<ARA::ARAContentTuning> { tuning }); }
    void clearTuning () { _tuning.reset (); }
    const EntryData<ARA::ARAContentTuning>& getTuning () const noexcept { return _tuning; }

    void setKeySignatures (std::vector<ARA::ARAContentKeySignature> const& keySignatures) { _keySignatures = makeEntryData (keySignatures); }
    void clearKeySignatures () { _keySignatures.reset (); }
    const EntryData<ARA::ARAContentKeySignature>& getKeySignatures () const noexcept { return _keySignatures; }

    void setChords (std::vector<ARA::ARAContentChord> const& chords) { _chords = makeEntryData (chords); }
    void clearChords () { _chords.reset (); }
    const EntryData<ARA::ARAContentChord>& getChords () const noexcept { return _chords; }

private:
    template<typename ContentType>
    EntryData<ContentType> makeEntryData (std::vector<ContentType> const& vec) { return std::make_unique<std::vector<ContentType>> (vec); }

    EntryData<ARA::ARAContentNote> _notes;
    EntryData<ARA::ARAContentTempoEntry> _tempoEntries;
    EntryData<ARA::ARAContentBarSignature> _barSignatures;
    EntryData<ARA::ARAContentTuning> _tuning;
    EntryData<ARA::ARAContentKeySignature> _keySignatures;
    EntryData<ARA::ARAContentChord> _chords;
};

/*******************************************************************************/

class Document
{
public:
    Document (std::string name);

    const std::string& getName () const noexcept { return _name; }
    void setName (std::string name) { _name = name; }

    std::vector<std::unique_ptr<MusicalContext>> const& getMusicalContexts () const noexcept { return _musicalContexts; }
    void addMusicalContext (std::unique_ptr<MusicalContext>&& musicalContext) { _musicalContexts.emplace_back (std::move (musicalContext)); }
    void removeMusicalContext (MusicalContext* musicalContext) { ARA::find_erase (_musicalContexts, musicalContext); }

    std::vector<std::unique_ptr<RegionSequence>> const& getRegionSequences () const noexcept { return _regionSequences; }
    void addRegionSequence (std::unique_ptr<RegionSequence>&& regionSequence) { _regionSequences.emplace_back (std::move (regionSequence)); }
    void removeRegionSequence (RegionSequence* regionSequence) { ARA::find_erase (_regionSequences, regionSequence); }

    std::vector<std::unique_ptr<AudioSource>> const& getAudioSources () const noexcept { return _audioSources; }
    void addAudioSource (std::unique_ptr<AudioSource>&& audioSource) { _audioSources.emplace_back (std::move (audioSource)); }
    void removeAudioSource (AudioSource* audioSource) { ARA::find_erase (_audioSources, audioSource); }

private:
    std::string _name;
    std::vector<std::unique_ptr<AudioSource>> _audioSources;
    std::vector<std::unique_ptr<MusicalContext>> _musicalContexts;
    std::vector<std::unique_ptr<RegionSequence>> _regionSequences;
};

/*******************************************************************************/

class MusicalContext : public ContentContainer
{
public:
    MusicalContext (Document* document, std::string name, ARA::ARAColor color);

    Document* getDocument () const noexcept { return _document; }

    const std::string& getName () const noexcept { return _name; }
    void setName (std::string name) { _name = name; }

    int getOrderIndex () const noexcept;

    const ARA::ARAColor& getColor () const noexcept { return _color; }
    void setColor (ARA::ARAColor color) { _color = color; }

    std::vector<RegionSequence*> const& getRegionSequences () const noexcept { return _regionSequences; }
    // Do not call those directly: instead use the related calls at the RegionSequence class, which will implictly call this.
    void _addRegionSequence (RegionSequence* regionSequence) { _regionSequences.push_back (regionSequence); }
    void _removeRegionSequence (RegionSequence* regionSequence) { ARA::find_erase (_regionSequences, regionSequence); }

private:
    Document* const _document;
    std::string _name;
    ARA::ARAColor _color;
    std::vector<RegionSequence*> _regionSequences;
};

/*******************************************************************************/

class RegionSequence
{
public:
    RegionSequence (Document* document, std::string name, MusicalContext* musicalContext, ARA::ARAColor color);
    ~RegionSequence ();

    Document* getDocument () const noexcept { return _document; }

    const std::string& getName () const noexcept { return _name; }
    void setName (std::string name) { _name = name; }

    int getOrderIndex () const noexcept;

    MusicalContext* getMusicalContext () const noexcept { return _musicalContext; }
    void setMusicalContext (MusicalContext* musicalContext);

    const ARA::ARAColor& getColor () const noexcept { return _color; }
    void setColor (ARA::ARAColor color) { _color = color; }

    std::vector<PlaybackRegion*> const& getPlaybackRegions () const noexcept { return _playbackRegions; }
    // Do not call those directly: instead use the related calls at the PlaybackRegion class, which will implictly call this.
    void _addPlaybackRegion (PlaybackRegion* region) { _playbackRegions.push_back (region); }
    void _removePlaybackRegion (PlaybackRegion* region) { ARA::find_erase (_playbackRegions, region); }

private:
    Document* const _document;
    std::string _name;
    MusicalContext* _musicalContext;
    ARA::ARAColor _color;
    std::vector<PlaybackRegion*> _playbackRegions;
};

/*******************************************************************************/

class AudioSource : public ContentContainer
{
public:
    AudioSource (Document* document, AudioFileBase* audioFile, std::string persistentID);

    Document* getDocument () const noexcept { return _document; }
    AudioFileBase* getAudioFile () const noexcept { return _audioFile; }

    const std::string& getName () const noexcept { return _audioFile->getName (); }
    void setName (std::string name) { _audioFile->setName (name); }

    const std::string& getPersistentID () const noexcept { return _persistentID; }
    void setPersistentID (std::string persistentID) { _persistentID = persistentID; }

    int64_t getSampleCount () const noexcept { return _audioFile->getSampleCount (); }
    double getSampleRate () const noexcept { return _audioFile->getSampleRate (); }
    double getDuration () const noexcept { return ARA::timeAtSamplePosition (getSampleCount (), getSampleRate ()); }
    int getChannelCount () const noexcept { return _audioFile->getChannelCount (); }
    bool merits64BitSamples () const noexcept { return _audioFile->merits64BitSamples (); }

    std::vector<std::unique_ptr<AudioModification>> const& getAudioModifications () const noexcept { return _audioModifications; }
    void addAudioModification (std::unique_ptr<AudioModification>&& modification) { _audioModifications.emplace_back (std::move (modification)); }
    void removeAudioModification (AudioModification* modification) { ARA::find_erase (_audioModifications, modification); }

private:
    Document* const _document;
    AudioFileBase* const _audioFile;
    std::string _persistentID;
    std::vector<std::unique_ptr<AudioModification>> _audioModifications;
};

/*******************************************************************************/

class AudioModification
{
public:
    AudioModification (AudioSource* audioSource, std::string name, std::string persistentID);

    AudioSource* getAudioSource () const noexcept { return _audioSource; }

    const std::string& getName () const noexcept { return _name; }
    void setName (std::string name) { _name = name; }

    const std::string& getPersistentID () const noexcept { return _persistentID; }
    void setPersistentID (std::string persistentID) { _persistentID = persistentID; }

    std::vector<std::unique_ptr<PlaybackRegion>> const& getPlaybackRegions () const noexcept { return _playbackRegions; }
    void addPlaybackRegion (std::unique_ptr<PlaybackRegion>&& region) { _playbackRegions.emplace_back (std::move (region)); }
    void removePlaybackRegion (PlaybackRegion* region) { ARA::find_erase (_playbackRegions, region); }

private:
    AudioSource* const _audioSource;
    std::string _name;
    std::string _persistentID;
    std::vector<std::unique_ptr<PlaybackRegion>> _playbackRegions;
};

/*******************************************************************************/

class PlaybackRegion
{
public:
    PlaybackRegion (AudioModification* audioModification,
                    ARA::ARAPlaybackTransformationFlags transformationFlags,
                    double startInModificationTime,
                    double durationInModificationTime,
                    double startInPlaybackTime,
                    double durationInPlaybackTime,
                    RegionSequence* regionSequence,
                    std::string name,
                    ARA::ARAColor color);
    ~PlaybackRegion ();

    AudioModification* getAudioModification () const noexcept { return _audioModification; }

    ARA::ARAPlaybackTransformationFlags getTransformationFlags () const noexcept { return _transformationFlags; }
    void setTransformationFlags (ARA::ARAPlaybackTransformationFlags transformationFlags) { _transformationFlags = transformationFlags; }

    double getStartInModificationTime () const noexcept { return _startInModificationTime; }
    void setStartInModificationTime (double startInModificationTime) { _startInModificationTime = startInModificationTime; }
    double getDurationInModificationTime () const noexcept { return _durationInModificationTime; }
    void setDurationInModificationTime (double durationInModificationTime) { _durationInModificationTime = durationInModificationTime; }
    double getEndInModificationTime () const noexcept { return _startInModificationTime + _durationInModificationTime; }

    double getStartInPlaybackTime () const noexcept { return _startInPlaybackTime; }
    void setStartInPlaybackTime (double startInPlaybackTime) { _startInPlaybackTime = startInPlaybackTime; }
    double getDurationInPlaybackTime () const noexcept { return _durationInPlaybackTime; }
    void setDurationInPlaybackTime (double durationInPlaybackTime) { _durationInPlaybackTime = durationInPlaybackTime; }
    double getEndInPlaybackTime () const noexcept { return _startInPlaybackTime + _durationInPlaybackTime; }

    RegionSequence* getRegionSequence () const noexcept { return _regionSequence; }
    void setRegionSequence (RegionSequence* regionSequence);

    const std::string& getName () const noexcept { return _name; }
    void setName (std::string name) { _name = name; }

    const ARA::ARAColor& getColor () const noexcept { return _color; }
    void setColor (ARA::ARAColor color) { _color = color; }

private:
    AudioModification* const _audioModification;
    ARA::ARAPlaybackTransformationFlags _transformationFlags;
    double _startInModificationTime;
    double _durationInModificationTime;
    double _startInPlaybackTime;
    double _durationInPlaybackTime;
    RegionSequence* _regionSequence;
    std::string _name;
    ARA::ARAColor _color;
};
