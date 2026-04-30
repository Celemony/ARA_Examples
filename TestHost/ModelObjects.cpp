//------------------------------------------------------------------------------
//! \file       ModelObjects.cpp
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

#include "ModelObjects.h"
#include "ARA_Library/Debug/ARADebug.h"

/*******************************************************************************/

PlaybackRegion::PlaybackRegion (AudioModification* audioModification, ARA::ARAPlaybackTransformationFlags transformationFlags, double startInModificationTime, double durationInModificationTime, double startInPlaybackTime, double durationInPlaybackTime, RegionSequence * regionSequence, std::string name, ARA::ARAColor color)
: _audioModification { audioModification },
  _transformationFlags { transformationFlags },
  _startInModificationTime { startInModificationTime },
  _durationInModificationTime { durationInModificationTime },
  _startInPlaybackTime { startInPlaybackTime },
  _durationInPlaybackTime { durationInPlaybackTime },
  _regionSequence { regionSequence },
  _name { name },
  _color { color }
{
    _regionSequence->_addPlaybackRegion (this);
}

PlaybackRegion::~PlaybackRegion ()
{
    _regionSequence->_removePlaybackRegion (this);
}

// Note that setRegionsequence handles removing this from the old
// region sequence and adding this to the new region sequence
void PlaybackRegion::setRegionSequence (RegionSequence* regionSequence)
{
    if (regionSequence == _regionSequence)
        return;

    _regionSequence->_removePlaybackRegion (this);
    _regionSequence = regionSequence;
    _regionSequence->_addPlaybackRegion (this);
}

/*******************************************************************************/

AudioModification::AudioModification (AudioSource * audioSource, std::string name, std::string persistentID)
: _audioSource { audioSource },
  _name { name },
  _persistentID { persistentID }
{}

/*******************************************************************************/

AudioSource::AudioSource (Document* document, AudioFileBase* audioFile, std::string persistentID)
: _document { document },
  _audioFile { audioFile },
  _persistentID { persistentID }
{
    // at this point, only up to stereo formats are supported because the test code
    // doesn't handle surround channel arrangements yet.
    ARA_INTERNAL_ASSERT (_audioFile->getChannelCount () <= 2);
}

/*******************************************************************************/

RegionSequence::RegionSequence (Document * document, std::string name, std::string persistentID, MusicalContext * musicalContext, ARA::ARAColor color)
: _document { document },
  _name { name },
  _persistentID { persistentID },
  _musicalContext { musicalContext },
  _color { color }
{
    _musicalContext->_addRegionSequence (this);
}

RegionSequence::~RegionSequence ()
{
    _musicalContext->_removeRegionSequence (this);
}

int RegionSequence::getOrderIndex () const noexcept
{
    return static_cast<int> (ARA::index_of (_document->getRegionSequences (), this));
}

void RegionSequence::setMusicalContext (MusicalContext* musicalContext)
{
    if (musicalContext == _musicalContext)
        return;

    _musicalContext->_removeRegionSequence (this);
    _musicalContext = musicalContext;
    _musicalContext->_addRegionSequence (this);
}

/*******************************************************************************/

MusicalContext::MusicalContext (Document * document, std::string name, ARA::ARAColor color)
: _document { document },
  _name { name },
  _color { color }
{}

int MusicalContext::getOrderIndex () const noexcept
{
    return static_cast<int> (ARA::index_of (_document->getMusicalContexts (), this));
}

/*******************************************************************************/

Document::Document (std::string name)
: _name { name }
{}
