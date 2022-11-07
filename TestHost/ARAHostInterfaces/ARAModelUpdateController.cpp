//------------------------------------------------------------------------------
//! \file       ARAModelUpdateController.cpp
//!             implementation of the host ARAModelUpdateControllerInterface
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "ARAModelUpdateController.h"
#include "ARA_Library/Debug/ARAContentLogger.h"

// The plug-in will call this function to notify us of audio source analysis progress
// In this case we make sure that it's one of our known audio source "key" references
// and, if so, log a message indicating its analysis progress
void ARAModelUpdateController::notifyAudioSourceAnalysisProgress (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAAnalysisProgressState state, float value) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    const auto audioSourceRef = _araDocumentController->getRef (audioSource);
    ARA_VALIDATE_API_ARGUMENT (audioSource, ARA::contains (getDocument ()->getAudioSources (), audioSource));
    ARA_VALIDATE_API_ARGUMENT (nullptr, 0.0f <= value);
    ARA_VALIDATE_API_ARGUMENT (nullptr, value <= 1.0f);

    switch (state)
    {
        case ARA::kARAAnalysisProgressStarted:
        {
            ARA_VALIDATE_API_STATE (_audioSourceAnaysisProgressValues.count (audioSource) == 0);
            ARA_LOG ("audio source %p (ARAAudioSourceRef %p) analysis started with progress %.f%%.", audioSource, audioSourceRef, 100.0 * value);
            _audioSourceAnaysisProgressValues[audioSource] = value;
            break;
        }
        case ARA::kARAAnalysisProgressUpdated:
        {
            ARA_VALIDATE_API_STATE (_audioSourceAnaysisProgressValues.count (audioSource) != 0);
            ARA_VALIDATE_API_STATE (_audioSourceAnaysisProgressValues[audioSource] <= value);
            ARA_LOG ("audio source %p (ARAAudioSourceRef %p) analysis progress is %.f%%.", audioSource, audioSourceRef, 100.0 * value);
            _audioSourceAnaysisProgressValues[audioSource] = value;
            break;
        }
        case ARA::kARAAnalysisProgressCompleted:
        {
            ARA_VALIDATE_API_STATE (_audioSourceAnaysisProgressValues.count (audioSource) != 0);
            ARA_VALIDATE_API_STATE (_audioSourceAnaysisProgressValues[audioSource] <= value);
            ARA_LOG ("audio source %p (ARAAudioSourceRef %p) analysis finished with progress %.f%%.", audioSource, audioSourceRef, 100.0 * value);
            _audioSourceAnaysisProgressValues.erase (audioSource);
            break;
        }
        default:
        {
            ARA_VALIDATE_API_ARGUMENT (nullptr, false && "invalid progress state");
            break;
        }
    }
}

// The plug-in will call this function to let us know that it has some sort of new content for an audio source
// This could happen if, say, the plug-in detects notes within an audio source
void ARAModelUpdateController::notifyAudioSourceContentChanged (ARA::ARAAudioSourceHostRef audioSourceHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSource, ARA::contains (getDocument ()->getAudioSources (), audioSource));
    ARA_VALIDATE_API_ARGUMENT (range, (range == nullptr) || (0.0 <= range->duration));
    ARA_VALIDATE_API_ARGUMENT (nullptr, scopeFlags.affectEverything () || !scopeFlags.affectSamples ());

    if (_minimalContentUpdateLogging)
        ARA_LOG ("content of audio source %p (ARAAudioSource ref %p) was updated from %.3f to %.3f, flags 0x%X", audioSource, _araDocumentController->getRef (audioSource), ARA::ContentLogger::getStartOfRange (range), ARA::ContentLogger::getEndOfRange (range), scopeFlags);
    else
        ARA::ContentLogger::logUpdatedContent (*_araDocumentController->getDocumentController (), _araDocumentController->getRef (audioSource), range, scopeFlags);
}

// Similar to notifyAudioSourceContentChanged but with a change in scope - now it's limited to a change in an audio modification
// (many ARA 2 hosts don't need to concern themselves with this)
void ARAModelUpdateController::notifyAudioModificationContentChanged (ARA::ARAAudioModificationHostRef audioModificationHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    const auto audioModification = fromHostRef (audioModificationHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioModification, ARA::contains (getDocument ()->getAudioSources (), audioModification->getAudioSource ()));
    ARA_VALIDATE_API_ARGUMENT (audioModification, ARA::contains (audioModification->getAudioSource ()->getAudioModifications (), audioModification));
    ARA_VALIDATE_API_ARGUMENT (range, (range == nullptr) || (0.0 <= range->duration));

    if (_minimalContentUpdateLogging)
        ARA_LOG ("content of audio modification %p (ARAAudioModificationRef ref %p) was updated from %.3f to %.3f, flags 0x%X", audioModification, _araDocumentController->getRef (audioModification), ARA::ContentLogger::getStartOfRange (range), ARA::ContentLogger::getEndOfRange (range), scopeFlags);
    else
        ARA::ContentLogger::logUpdatedContent (*_araDocumentController->getDocumentController (), _araDocumentController->getRef (audioModification), range, scopeFlags);
}

// Similar to notifyAudioSourceContentChanged but with a change in scope - now it's limited to a change with a playback region
void ARAModelUpdateController::notifyPlaybackRegionContentChanged (ARA::ARAPlaybackRegionHostRef playbackRegionHostRef, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags) noexcept
{
    const auto playbackRegion = fromHostRef (playbackRegionHostRef);
    ARA_VALIDATE_API_ARGUMENT (playbackRegion, ARA::contains (getDocument ()->getRegionSequences (), playbackRegion->getRegionSequence ()));
    ARA_VALIDATE_API_ARGUMENT (playbackRegion, ARA::contains (playbackRegion->getRegionSequence ()->getPlaybackRegions (), playbackRegion));
    ARA_VALIDATE_API_ARGUMENT (range, (range == nullptr) || (0.0 <= range->duration));

    if (_minimalContentUpdateLogging)
        ARA_LOG ("content of playback region %p (ARAPlaybackRegionRef ref %p) was updated from %.3f to %.3f, flags 0x%X", playbackRegion, _araDocumentController->getRef (playbackRegion), ARA::ContentLogger::getStartOfRange (range), ARA::ContentLogger::getEndOfRange (range), scopeFlags);
    else
        ARA::ContentLogger::logUpdatedContent (*_araDocumentController->getDocumentController (), _araDocumentController->getRef (playbackRegion), range, scopeFlags);
}
