//------------------------------------------------------------------------------
//! \file       TestHost.cpp
//!             class that maintains the model graph and ARA document controller
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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#include "TestHost.h"

Document* TestHost::addDocument (std::string documentName, PlugInEntry* plugInEntry)
{
    auto doc { new Document { documentName } };
    auto controller { new ARADocumentController { doc, plugInEntry} };
    _documents[doc] = controller;
    return doc;
}

void TestHost::destroyDocument (Document* document)
{
    // To clean up the ARA document, remove model graph objects
    // starting at the bottom with playback regions and working upward
    auto araDocumentController = getDocumentController (document);
    araDocumentController->beginEditing ();

    while (!document->getAudioSources ().empty ())
    {
        auto& audioSource = document->getAudioSources ().back ();

        while (!audioSource->getAudioModifications ().empty ())
        {
            auto& audioModification = audioSource->getAudioModifications ().back ();

            while (!audioModification->getPlaybackRegions ().empty ())
                removePlaybackRegion (document, audioModification->getPlaybackRegions ().back ().get ());

            removeAudioModification (document, audioModification.get ());
        }

        removeAudioSource (document, audioSource.get ());
    }

    while (!document->getRegionSequences ().empty ())
        removeRegionSequence (document, document->getRegionSequences ().back ().get ());

    while (!document->getMusicalContexts ().empty ())
        removeMusicalContext (document, document->getMusicalContexts ().back ().get ());

    araDocumentController->endEditing ();
    
    _documents.erase (document);
    delete araDocumentController;
    delete document;
}

TestHost::~TestHost ()
{
    while (!_documents.empty ())
        destroyDocument (_documents.begin ()->first);
}

MusicalContext* TestHost::addMusicalContext (Document* document, std::string name, ARA::ARAColor color)
{
    document->addMusicalContext (std::make_unique <MusicalContext> (document, name, color));
    auto musicalContext = document->getMusicalContexts ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->addMusicalContext (musicalContext);
    return musicalContext;
}

void TestHost::removeMusicalContext (Document* document, MusicalContext* musicalContext)
{
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->removeMusicalContext (musicalContext);
    document->removeMusicalContext (musicalContext);
}

RegionSequence* TestHost::addRegionSequence (Document* document, std::string name, MusicalContext* musicalContext, ARA::ARAColor color)
{
    document->addRegionSequence (std::make_unique<RegionSequence> (document, name, musicalContext, color));
    auto regionSequence = document->getRegionSequences ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->addRegionSequence (regionSequence);
    return regionSequence;
}

void TestHost::removeRegionSequence (Document* document, RegionSequence* regionSequence)
{
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->removeRegionSequence (regionSequence);
    document->removeRegionSequence (regionSequence);
}

AudioSource* TestHost::addAudioSource (Document* document, AudioFileBase* audioFile, std::string persistentID)
{
    document->addAudioSource (std::make_unique<AudioSource> (document, audioFile, persistentID));
    auto audioSource = document->getAudioSources ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->addAudioSource (audioSource);
    return audioSource;
}

void TestHost::removeAudioSource (Document* document, AudioSource* audioSource)
{
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->removeAudioSource (audioSource);
    document->removeAudioSource (audioSource);
}

AudioModification* TestHost::addAudioModification (Document* document, AudioSource* audioSource, std::string name, std::string persistentID)
{
    audioSource->addAudioModification (std::make_unique<AudioModification> (audioSource, name, persistentID));
    auto audioModification = audioSource->getAudioModifications ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->addAudioModification (audioModification);
    return audioModification;
}

void TestHost::removeAudioModification (Document* document, AudioModification* audioModification)
{
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->removeAudioModification (audioModification);
    audioModification->getAudioSource ()->removeAudioModification (audioModification);
}

AudioModification* TestHost::cloneAudioModification (Document* document, AudioModification* audioModification, std::string name, std::string persistentID)
{
    auto audioSource = audioModification->getAudioSource ();
    audioSource->addAudioModification (std::make_unique<AudioModification> (audioSource, name, persistentID));
    auto clonedModification = audioSource->getAudioModifications ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->cloneAudioModification (audioModification, clonedModification);
    return clonedModification;
}

PlaybackRegion* TestHost::addPlaybackRegion (Document* document, AudioModification* audioModification,
                                             ARA::ARAPlaybackTransformationFlags transformationFlags,
                                             double startInModificationTime, double durationInModificationTime,
                                             double startInPlaybackTime, double durationInPlaybackTime,
                                             RegionSequence* regionSequence,
                                             std::string name, ARA::ARAColor color)
{
    audioModification->addPlaybackRegion (std::make_unique<PlaybackRegion> (
                                                audioModification,
                                                transformationFlags,
                                                startInModificationTime, durationInModificationTime,
                                                startInPlaybackTime, durationInPlaybackTime,
                                                regionSequence,
                                                name, color));
    auto playbackRegion = audioModification->getPlaybackRegions ().back ().get ();
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->addPlaybackRegion (playbackRegion);
    return playbackRegion;
}

void TestHost::removePlaybackRegion (Document* document, PlaybackRegion* playbackRegion)
{
    if (auto araDocumentController = getDocumentController (document))
        araDocumentController->removePlaybackRegion (playbackRegion);
    playbackRegion->getAudioModification ()->removePlaybackRegion (playbackRegion);
}

ARADocumentController* TestHost::getDocumentController (Document* document)
{
    return _documents[document];
}
