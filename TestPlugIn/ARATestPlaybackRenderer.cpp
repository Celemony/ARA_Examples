//------------------------------------------------------------------------------
//! \file       ARATestPlaybackRenderer.cpp
//!             playback renderer implementation for the ARA test plug-in,
//!             customizing the playback renderer base class of the ARA library
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

#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"
#include "ARATestAudioSource.h"

void ARATestPlaybackRenderer::renderPlaybackRegions (float** ppOutput, ARA::ARASamplePosition samplePosition,
                                                     ARA::ARASampleCount samplesToRender, bool isPlayingBack)
{
    // initialize output buffers with silence, in case no viable playback region intersects with the
    // current buffer, or if the model is currently not accessible due to being edited.
    for (auto c { 0 }; c < _channelCount; ++c)
        std::memset (ppOutput[c], 0, sizeof (float) * static_cast<size_t> (samplesToRender));

    // only output samples while host is playing back
    if (!isPlayingBack)
        return;

    // flag that we've started rendering to prevent the document from being edited while in this callback
    // see TestDocumentController for details.
    auto docController { getDocumentController<ARATestDocumentController> () };
    if (docController->rendererWillAccessModelGraph (this))
    {
        const auto sampleEnd { samplePosition + samplesToRender };
        for (const auto& playbackRegion : getPlaybackRegions ())
        {
            const auto audioSource { playbackRegion->getAudioModification ()->getAudioSource<const ARATestAudioSource> () };

            // render silence if access is currently disabled
            // (this is done here only to ease host debugging - actual plug-ins would have at least
            // some samples cached for realtime access and would continue unless there's a cache miss.)
            if (!audioSource->isSampleAccessEnabled ())
                continue;

            // this simplified test code "rendering" only produces audio if sample rate and channel count match
            if ((audioSource->getChannelCount () != _channelCount) || (audioSource->getSampleRate () != _sampleRate))
                continue;

            // evaluate region borders in song time, calculate sample range to copy in song time
            // (if a plug-in uses playback region head/tail time, it will also need to reflect these values here)
            const auto regionStartSample { playbackRegion->getStartInPlaybackSamples (_sampleRate) };
            if (sampleEnd <= regionStartSample)
                continue;

            const auto regionEndSample { playbackRegion->getEndInPlaybackSamples (_sampleRate) };
            if (regionEndSample <= samplePosition)
                continue;

            auto startSongSample { std::max (regionStartSample, samplePosition) };
            auto endSongSample { std::min (regionEndSample, sampleEnd) };

            // calculate offset between song and audio source samples, clip at region borders in audio source samples
            // (if a plug-in supports time stretching, it will also need to reflect the stretch factor here)
            const auto offsetToPlaybackRegion { playbackRegion->getStartInAudioModificationSamples () - regionStartSample };

            const auto startAvailableSourceSamples { std::max (ARA::ARASamplePosition { 0 }, playbackRegion->getStartInAudioModificationSamples ()) };
            const auto endAvailableSourceSamples { std::min (audioSource->getSampleCount (), playbackRegion->getEndInAudioModificationSamples ()) };

            startSongSample = std::max (startSongSample, startAvailableSourceSamples - offsetToPlaybackRegion);
            endSongSample = std::min (endSongSample, endAvailableSourceSamples - offsetToPlaybackRegion);
            if (endSongSample <= startSongSample)
                continue;

            // add samples from audio source
            for (auto posInSong { startSongSample }; posInSong < endSongSample; ++posInSong)
            {
                const auto posInBuffer { posInSong - samplePosition };
                const auto posInSource { posInSong + offsetToPlaybackRegion };
                for (auto c { 0 }; c < audioSource->getChannelCount (); ++c)
                    ppOutput[c][posInBuffer] += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
            }
        }

        // let the document controller know we're done
        docController->rendererDidAccessModelGraph (this);
    }
}

void ARATestPlaybackRenderer::enableRendering (ARA::ARASampleRate sampleRate, ARA::ARAChannelCount channelCount, ARA::ARASampleCount maxSamplesToRender) noexcept
{
    // proper plug-ins would use this call to manage the resources which they need for rendering,
    // but our test plug-in caches everything it needs in-memory anyways, so this method is near-empty
    _sampleRate = sampleRate;
    _channelCount = channelCount;
    _maxSamplesToRender = maxSamplesToRender;
#if ARA_VALIDATE_API_CALLS
    _isRenderingEnabled = true;
#endif
}

void ARATestPlaybackRenderer::disableRendering () noexcept
{
#if ARA_VALIDATE_API_CALLS
    _isRenderingEnabled = false;
#endif
}

#if ARA_VALIDATE_API_CALLS
void ARATestPlaybackRenderer::willAddPlaybackRegion (ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) noexcept
{
    ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
}

void ARATestPlaybackRenderer::willRemovePlaybackRegion (ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) noexcept
{
    ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
}
#endif
