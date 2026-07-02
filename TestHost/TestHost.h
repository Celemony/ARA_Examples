//------------------------------------------------------------------------------
//! \file       TestHost.h
//!             class that maintains the model graph and ARA document controller
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

#include "ModelObjects.h"
#include "ARADocumentController.h"
#include "CompanionAPIs.h"

/*******************************************************************************/
// TestHost
// This class represents our ARA host and manages a single document.
// To share this document with an ARA plug-in the host constructs an
// ARA document controller and uses it from the document control APIs.
class TestHost
{
public:

    TestHost () {}
    ~TestHost ();

    // document control
    Document* addDocument (std::string documentName, PlugInEntry* plugInEntry);
    void destroyDocument (Document* document);

    MusicalContext* addMusicalContext (Document* document, std::string name, ARA::ARAColor color);
    void removeMusicalContext (Document* document, MusicalContext* musicalContext);

    RegionSequence* addRegionSequence (Document* document, std::string name, std::string persistentID, MusicalContext* musicalContext, ARA::ARAColor color);
    void removeRegionSequence (Document* document, RegionSequence* regionSequence);

    AudioSource* addAudioSource (Document* document, AudioFileBase* audioFile, std::string persistentID);
    void removeAudioSource (Document* document, AudioSource* audioSource);

    AudioModification* addAudioModification (Document* document, AudioSource* audioSource, std::string name, std::string persistentID);
    void removeAudioModification (Document* document, AudioModification* audioModification);
    AudioModification* cloneAudioModification (Document* document, AudioModification* audioModification, std::string name, std::string persistentID);

    PlaybackRegion* addPlaybackRegion (Document* document, AudioModification* audioModification,
                                       ARA::ARAPlaybackTransformationFlags transformationFlags,
                                       double startInModificationTime, double durationInModificationTime,
                                       double startInPlaybackTime, double durationInPlaybackTime,
                                       RegionSequence* regionSequence,
                                       std::string name, ARA::ARAColor color);
    void removePlaybackRegion (Document* document, PlaybackRegion* playbackRegion);

    // document and ARA document controller access
    ARADocumentController* getDocumentController (Document* document);

private:
    std::map<Document*, ARADocumentController*> _documents;
};
