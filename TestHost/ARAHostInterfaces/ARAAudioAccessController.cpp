//------------------------------------------------------------------------------
//! \file       ARAAudioAccessController.cpp
//!             implementation of the host ARAAudioAccessControllerInterface
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2021, Celemony Software GmbH, All Rights Reserved.
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

#include "ARAAudioAccessController.h"

// The audio sample data read by the plug-in is a pulsed sine wave signal
bool AudioSourceReader::readSamples (ARA::ARASamplePosition samplePosition, ARA::ARASampleCount samplesPerChannel, void* const buffers[]) const noexcept
{
    return _audioSource->getAudioFile ()->readSamples (samplePosition, samplesPerChannel, buffers, _use64BitSamples);
}

/*******************************************************************************/

// Create an audio reader for the given audio source - because we have no real audio reader object, we instead
// treat the reference this function returns as a "key" that we'll use when reading this audio source
ARA::ARAAudioReaderHostRef ARAAudioAccessController::createAudioReaderForSource (ARA::ARAAudioSourceHostRef audioSourceHostRef, bool use64BitSamples) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, ARA::contains (getDocument ()->getAudioSources (), audioSource));
#if ARA_VALIDATE_API_CALLS
    std::lock_guard<std::mutex> lg (_audioSourceReadersMutex);
#endif
    _audioSourceReaders.emplace_back (std::make_unique<AudioSourceReader> (audioSource, use64BitSamples));
    return toHostRef (_audioSourceReaders.back ().get ());
}

// If this function gets passed the "key" reference returned by the function above then we can use it
// to render audio samples into to the supplied buffers - the audio samples will form a pulsed sine wave at 440 Hz
bool ARAAudioAccessController::readAudioSamples (ARA::ARAAudioReaderHostRef audioReaderHostRef, ARA::ARASamplePosition samplePosition, ARA::ARASampleCount samplesPerChannel, void* const buffers[]) noexcept
{
    const auto audioSourceReader = fromHostRef (audioReaderHostRef);
#if ARA_VALIDATE_API_CALLS
    {
        std::lock_guard<std::mutex> lg (_audioSourceReadersMutex);
        ARA_VALIDATE_API_ARGUMENT (audioReaderHostRef, ARA::contains (_audioSourceReaders, audioSourceReader));
    }
#endif
    ARA_VALIDATE_API_ARGUMENT (nullptr, samplesPerChannel >= 0);
    ARA_VALIDATE_API_ARGUMENT (buffers, buffers != nullptr);
    for (int i = 0; i < audioSourceReader->getAudioSource ()->getChannelCount (); ++i)
        ARA_VALIDATE_API_ARGUMENT (buffers, buffers[i] != nullptr);
    return audioSourceReader->readSamples (samplePosition, samplesPerChannel, buffers);
}

// We don't need to actually destroy anything here, but it's worth validating that
// the reference we're meant to destroy is our original "key" reference
void ARAAudioAccessController::destroyAudioReader (ARA::ARAAudioReaderHostRef audioReaderHostRef) noexcept
{
    const auto audioSourceReader = fromHostRef (audioReaderHostRef);
#if ARA_VALIDATE_API_CALLS
    std::lock_guard<std::mutex> lg (_audioSourceReadersMutex);
#endif
    ARA_VALIDATE_API_ARGUMENT (audioReaderHostRef, ARA::contains (_audioSourceReaders, audioSourceReader));
    ARA::find_erase (_audioSourceReaders, audioSourceReader);
}
