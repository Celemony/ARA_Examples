//------------------------------------------------------------------------------
//! \file       ARAModelUpdateController.h
//!             implementation of the host ARAModelUpdateControllerInterface
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2024, Celemony Software GmbH, All Rights Reserved.
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

#include "ARADocumentController.h"

/*******************************************************************************/
// Implementation of our test host's model update controller interface
// The plug-in will call these functions to notify the host of changes
// in audio or musical content and post analysis progress notifications
class ARAModelUpdateController : public ARA::Host::ModelUpdateControllerInterface
{
public:
    ARAModelUpdateController (ARADocumentController* araDocumentController) noexcept
    : _araDocumentController { araDocumentController }
    {}

    // The plug-in will call this function to notify us of audio source analysis progress
    // In this case we make sure that it's one of our known audio source "key" references
    // and, if so, log a message indicating its analysis progress
    void notifyAudioSourceAnalysisProgress (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAAnalysisProgressState state, float value) noexcept override;

    // The plug-in will call this function to let us know that it has some sort of new content for an audio source
    // This could happen if, say, the plug-in detects notes within an audio source
    void notifyAudioSourceContentChanged (ARA::ARAAudioSourceHostRef audioSourceHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

    // Similar to notifyAudioSourceContentChanged but with a change in scope - now it's limited to a change in an audio modification
    // (note that since ARA 2, in many situations it is preferable to instead read the newly added playback region content)
    void notifyAudioModificationContentChanged (ARA::ARAAudioModificationHostRef audioModificationHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

    // Similar to notifyAudioSourceContentChanged but with a change in scope - now it's limited to a change with a playback region
    void notifyPlaybackRegionContentChanged (ARA::ARAPlaybackRegionHostRef playbackRegionHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept override;

    void setMinimalContentUpdateLogging (bool flag) { _minimalContentUpdateLogging = flag; }

private:
    Document* getDocument () const noexcept { return _araDocumentController->getDocument (); }

    ARADocumentController* _araDocumentController;
    std::map<AudioSource*, float> _audioSourceAnaysisProgressValues;

    bool _minimalContentUpdateLogging { false };
};
