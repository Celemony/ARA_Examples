//------------------------------------------------------------------------------
//! \file       ARATestPlaybackRenderer.cpp
//!             playback renderer implementation for the ARA test plug-in,
//!             customizing the playback renderer base class of the ARA library
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

#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"
#include "ARATestAudioSource.h"

#include <cmath>

void ARATestPlaybackRenderer::renderPlaybackRegions (float* const* ppOutput, ARA::ARASamplePosition samplePosition,
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
            const auto audioModification { playbackRegion->getAudioModification () };
            ARA_VALIDATE_API_STATE (!audioModification->isDeactivatedForUndoHistory ());
            const auto audioSource { audioModification->getAudioSource<const ARATestAudioSource> () };
            ARA_VALIDATE_API_STATE (!audioSource->isDeactivatedForUndoHistory ());

            // content-only audio sources (e.g. MIDI) have no samples
            const bool synthesizeOutput { audioSource->isContentOnly () };

            // render silence if access is currently disabled
            // (this is done here only to ease host debugging - actual plug-ins would have at least
            // some samples cached for realtime access and would continue unless there's a cache miss.)
            if (!synthesizeOutput && !audioSource->isSampleAccessEnabled ())
                continue;

            // this simplified test code "rendering" only produces audio if the sample rate matches
            if (!synthesizeOutput && (audioSource->getSampleRate () != _sampleRate))
                continue;

            // evaluate region borders in song time, calculate sample range to process in song time (incl. head/tail)
            const auto regionStartSample { playbackRegion->getStartInPlaybackSamples (_sampleRate) };
            auto regionEndSample { playbackRegion->getEndInPlaybackSamples (_sampleRate) };
            // content-only sources emit a release tail past the region end - extend the render range to cover it
            // \todo see ARATestDocumentController::doGetPlaybackRegionHeadAndTailTime() - this should be dependent on actual note content
            if (synthesizeOutput)
                regionEndSample += _noteReleaseSamples;

            auto startSongSample { std::max (regionStartSample, samplePosition) };
            auto endSongSample { std::min (regionEndSample, sampleEnd) };
            if (endSongSample <= startSongSample)   // region incl. head/tail does not intersect with render range
                continue;

            // calculate offset between song and audio source samples, clip at region borders in audio source samples
            // (if a plug-in supports time stretching, it will also need to reflect the stretch factor here)
            const auto offsetToPlaybackRegion { playbackRegion->getStartInAudioModificationSamples () - regionStartSample };

            if (synthesizeOutput)
            {
                // synthesize a simple sine for each audio source note read from the host via kARAContentTypeNotes
                const auto noteContent { audioSource->getNoteContent () };
                if (!noteContent)
                    continue;

                for (const auto& note : *noteContent)
                {
                    const auto noteStartInSource { ARA::samplePositionAtTime (note._startTime, _sampleRate) };
                    const auto noteEndInSource { ARA::samplePositionAtTime (note._startTime + note._duration, _sampleRate) };

                    const auto noteStartInSong { std::max (startSongSample, noteStartInSource - offsetToPlaybackRegion) };
                    const auto noteEndInSong { std::min (endSongSample, noteEndInSource - offsetToPlaybackRegion + _noteReleaseSamples) };

                    for (auto posInSong { noteStartInSong }; posInSong < noteEndInSong; ++posInSong)
                    {
                        const auto posInBuffer { posInSong - samplePosition };
                        const auto posInSource { posInSong + offsetToPlaybackRegion };
                        const auto samplesIntoNote { posInSource - noteStartInSource };
                        const auto samplesToNoteEnd { noteEndInSource - posInSource };
                        auto envelope { 1.0f };
                        if (samplesToNoteEnd < 0)
                            envelope += static_cast<float> (samplesToNoteEnd) / static_cast<float> (_noteReleaseSamples);

                        const auto normalizedTime { static_cast<double> (samplesIntoNote) * note._frequency / _sampleRate };
                        constexpr double pi { 3.14159265358979323846264338327950288 };
                        const auto value { note._volume * envelope * static_cast<float> (std::sin (2.0 * pi * normalizedTime)) };
                        for (auto c { 0 }; c < _channelCount; ++c)
                            ppOutput[c][posInBuffer] += value;
                    }
                }
            }
            else
            {
                // add samples from audio source
                const auto startAvailableSourceSamples { std::max (ARA::ARASamplePosition { 0 }, playbackRegion->getStartInAudioModificationSamples ()) };
                const auto endAvailableSourceSamples { std::min (audioSource->getSampleCount (), playbackRegion->getEndInAudioModificationSamples ()) };

                startSongSample = std::max (startSongSample, startAvailableSourceSamples - offsetToPlaybackRegion);
                endSongSample = std::min (endSongSample, endAvailableSourceSamples - offsetToPlaybackRegion);

                const auto sourceChannelCount { audioSource->getChannelCount () };
                for (auto posInSong { startSongSample }; posInSong < endSongSample; ++posInSong)
                {
                    const auto posInBuffer { posInSong - samplePosition };
                    const auto posInSource { posInSong + offsetToPlaybackRegion };
                    if (sourceChannelCount == _channelCount)
                    {
                        for (auto c { 0 }; c < sourceChannelCount; ++c)
                            ppOutput[c][posInBuffer] += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                    }
                    else
                    {
                        // crude channel format conversion:
                        // mix down to mono, then distribute the mono signal evenly to all channels.
                        // note that when down-mixing to mono, the result is scaled by channel count,
                        // whereas upon up-mixing it is just copied to all channels.
                        // \todo ambisonic formats should just stick with the mono sum on channel 0,
                        //       but in this simple test code we currently do not distinguish ambisonics
                        float monoSum { 0.0f };
                        for (auto c { 0 }; c < sourceChannelCount; ++c)
                            monoSum += audioSource->getRenderSampleCacheForChannel (c)[posInSource];
                        if (sourceChannelCount > 1)
                            monoSum /= static_cast<float> (sourceChannelCount);
                        for (auto c { 0 }; c < _channelCount; ++c)
                            ppOutput[c][posInBuffer] = monoSum;
                    }
                }
            }
        }

        // let the document controller know we're done
        docController->rendererDidAccessModelGraph (this);
    }
}

void ARATestPlaybackRenderer::enableRendering (ARA::ARASampleRate sampleRate, ARA::ARAChannelCount channelCount, ARA::ARASampleCount maxSamplesToRender, bool apiSupportsToggleRendering) noexcept
{
    // proper plug-ins would use this call to manage the resources which they need for rendering,
    // but our test plug-in caches everything it needs in-memory anyways, so this method is near-empty
    _sampleRate = sampleRate;
    _channelCount = channelCount;
    _maxSamplesToRender = maxSamplesToRender;
    _noteReleaseSamples = ARA::samplePositionAtTime (noteReleaseTime, _sampleRate);
#if ARA_VALIDATE_API_CALLS
    _isRenderingEnabled = true;
    _apiSupportsToggleRendering = apiSupportsToggleRendering;
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
    if (_apiSupportsToggleRendering)
        ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
//  else
//      proper plug-ins would check _isRenderingEnabled here and toggle it off on demand, toggling it back on in didAddPlaybackRegion()
//      this works because hosts using such APIs implicitly guarantee that they do not concurrently render the plug-in while making this call
}

void ARATestPlaybackRenderer::willRemovePlaybackRegion (ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) noexcept
{
    if (_apiSupportsToggleRendering)
        ARA_VALIDATE_API_STATE (!_isRenderingEnabled);
//  else
//      see willAddPlaybackRegion(), same pattern applies here
}
#endif
