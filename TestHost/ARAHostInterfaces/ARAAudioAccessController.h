//------------------------------------------------------------------------------
//! \file       ARAAudioAccessController.h
//!             implementation of the host ARAAudioAccessControllerInterface
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

#pragma once

#include "ARADocumentController.h"

#include <mutex>

/*******************************************************************************/
// Simple audio source reader class that will be passed to readAudioSamples
// as the 'ARAAudioReaderHostRef audioReaderHostRef' parameter.
class AudioSourceReader
{
public:
    explicit AudioSourceReader (const AudioSource* audioSource, bool use64BitSamples) noexcept
    : _audioSource { audioSource },
      _use64BitSamples { use64BitSamples }
    {}

    const AudioSource* getAudioSource () const noexcept { return _audioSource; }

    bool readSamples (ARA::ARASamplePosition samplePosition, ARA::ARASampleCount samplesPerChannel, void* const buffers[]) const noexcept;

private:
    const AudioSource* _audioSource;
    const bool _use64BitSamples;

    ARA_PLUGIN_MANAGED_OBJECT (AudioSourceReader)
};
ARA_MAP_HOST_REF (AudioSourceReader, ARA::ARAAudioReaderHostRef)

/*******************************************************************************/
// Implementation of our test host's audio access controller interface
// The plug-in will call these functions when reading audio samples
class ARAAudioAccessController : public ARA::Host::AudioAccessControllerInterface
{
public:
    ARAAudioAccessController (ARADocumentController* araDocumentController) noexcept
    : _araDocumentController { araDocumentController }
    {}

    // Create an audio reader for the given audio source - because we have no real audio reader object, we instead
    // treat the reference this function returns as a "key" that we'll use when reading this audio source
    ARA::ARAAudioReaderHostRef createAudioReaderForSource (ARA::ARAAudioSourceHostRef audioSourceHostRef, bool use64BitSamples) noexcept override;

    // If this function gets passed the "key" reference returned by the function above then we can use it
    // to render audio samples into to the supplied buffers - the audio samples will form a pulsed sine wave at 440 Hz
    bool readAudioSamples (ARA::ARAAudioReaderHostRef audioReaderHostRef, ARA::ARASamplePosition samplePosition, ARA::ARASampleCount samplesPerChannel, void* const buffers[]) noexcept override;

    // We don't need to actually destroy anything here, but it's worth validating that
    // the reference we're meant to destroy is our original "key" reference
    void destroyAudioReader (ARA::ARAAudioReaderHostRef audioReaderHostRef) noexcept override;

    Document* getDocument () const noexcept { return _araDocumentController->getDocument (); }

#if ARA_VALIDATE_API_CALLS
    static void registerRenderThread ();
    static void unregisterRenderThread ();
#endif

private:
    ARADocumentController* _araDocumentController;
    std::vector<std::unique_ptr<AudioSourceReader>> _audioSourceReaders;
#if ARA_VALIDATE_API_CALLS
    std::mutex _audioSourceReadersMutex;
#endif
};
