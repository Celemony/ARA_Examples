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

void TestHost::addDocument (std::string documentName, PlugInEntry* plugInEntry)
{
    destroyDocument (documentName);

    auto doc { std::make_unique<Document> (documentName) };
    auto controller { std::make_unique<ARADocumentController> (doc.get (), plugInEntry) };

    _documents[documentName] = { std::move (doc), std::move (controller) };
}

void TestHost::destroyDocument (std::string documentName)
{
    // To clean up the ARA document, remove model graph objects
    // starting at the bottom with playback regions and working upward
    auto araDocumentController = getDocumentController (documentName);

    if (araDocumentController)
    {
        auto document = araDocumentController->getDocument ();
        araDocumentController->beginEditing ();

        while (!document->getAudioSources ().empty ())
        {
            auto& audioSource = document->getAudioSources ().back ();

            while (!audioSource->getAudioModifications ().empty ())
            {
                auto& audioModification = audioSource->getAudioModifications ().back ();

                while (!audioModification->getPlaybackRegions ().empty ())
                    removePlaybackRegion (documentName, audioModification->getPlaybackRegions ().back ().get ());

                removeAudioModification (documentName, audioModification.get ());
            }

            removeAudioSource (documentName, audioSource.get ());
        }

        while (!document->getRegionSequences ().empty ())
            removeRegionSequence (documentName, document->getRegionSequences ().back ().get ());

        while (!document->getMusicalContexts ().empty ())
            removeMusicalContext (documentName, document->getMusicalContexts ().back ().get ());

        araDocumentController->endEditing ();
    }

    _documents.erase (documentName);
}

TestHost::~TestHost ()
{
    std::vector<std::string> documentNames;
    for (const auto& docController : _documents)
        documentNames.push_back (docController.first);
    for (const auto& docName : documentNames)
        destroyDocument (docName);
}

MusicalContext* TestHost::addMusicalContext (std::string documentName, std::string name, ARA::ARAColor color)
{
    auto document = getDocument (documentName);
    document->addMusicalContext (std::make_unique <MusicalContext> (document, name, color));
    auto musicalContext = document->getMusicalContexts ().back ().get ();
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->addMusicalContext (musicalContext);
    return musicalContext;
}

void TestHost::removeMusicalContext (std::string documentName, MusicalContext* musicalContext)
{
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->removeMusicalContext (musicalContext);
    getDocument (documentName)->removeMusicalContext (musicalContext);
}

RegionSequence* TestHost::addRegionSequence (std::string documentName, std::string name, MusicalContext* musicalContext, ARA::ARAColor color)
{
    auto document = getDocument (documentName);
    document->addRegionSequence (std::make_unique<RegionSequence> (document, name, musicalContext, color));
    auto regionSequence = document->getRegionSequences ().back ().get ();
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->addRegionSequence (regionSequence);
    return regionSequence;
}

void TestHost::removeRegionSequence (std::string documentName, RegionSequence* regionSequence)
{
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->removeRegionSequence (regionSequence);
    getDocument (documentName)->removeRegionSequence (regionSequence);
}

AudioSource* TestHost::addAudioSource (std::string documentName, AudioFileBase* audioFile, std::string persistentID)
{
    auto document = getDocument (documentName);
    document->addAudioSource (std::make_unique<AudioSource> (document, audioFile, persistentID));
    auto audioSource = document->getAudioSources ().back ().get ();
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->addAudioSource (audioSource);
    return audioSource;
}

void TestHost::removeAudioSource (std::string documentName, AudioSource* audioSource)
{
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->removeAudioSource (audioSource);
    getDocument (documentName)->removeAudioSource (audioSource);
}

AudioModification* TestHost::addAudioModification (std::string documentName, AudioSource* audioSource, std::string name, std::string persistentID)
{
    audioSource->addAudioModification (std::make_unique<AudioModification> (audioSource, name, persistentID));
    auto audioModification = audioSource->getAudioModifications ().back ().get ();
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->addAudioModification (audioModification);
    return audioModification;
}

void TestHost::removeAudioModification (std::string documentName, AudioModification* audioModification)
{
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->removeAudioModification (audioModification);
    audioModification->getAudioSource ()->removeAudioModification (audioModification);
}

AudioModification* TestHost::cloneAudioModification (std::string documentName, AudioModification* audioModification, std::string name, std::string persistentID)
{
    auto audioSource = audioModification->getAudioSource ();
    audioSource->addAudioModification (std::make_unique<AudioModification> (audioSource, name, persistentID));
    auto clonedModification = audioSource->getAudioModifications ().back ().get ();
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->cloneAudioModification (audioModification, clonedModification);
    return clonedModification;
}

PlaybackRegion* TestHost::addPlaybackRegion (std::string documentName, AudioModification* audioModification,
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
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->addPlaybackRegion (playbackRegion);
    return playbackRegion;
}

void TestHost::removePlaybackRegion (std::string documentName, PlaybackRegion* playbackRegion)
{
    if (auto araDocumentController = getDocumentController (documentName))
        araDocumentController->removePlaybackRegion (playbackRegion);
    playbackRegion->getAudioModification ()->removePlaybackRegion (playbackRegion);
}

Document* TestHost::getDocument (std::string documentName)
{
    return _documents.count (documentName) ? _documents.at (documentName).first.get () : nullptr;
}

ARADocumentController* TestHost::getDocumentController (std::string documentName)
{
    return _documents.count (documentName) ? _documents.at (documentName).second.get () : nullptr;
}
