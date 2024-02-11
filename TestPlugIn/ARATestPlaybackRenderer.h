//------------------------------------------------------------------------------
//! \file       ARATestPlaybackRenderer.h
//!             playback renderer implementation for the ARA test plug-in,
//!             customizing the playback renderer base class of the ARA library
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

#pragma once

#include "ARA_Library/PlugIn/ARAPlug.h"

/*******************************************************************************/
class ARATestPlaybackRenderer : public ARA::PlugIn::PlaybackRenderer
{
public:
    using PlaybackRenderer::PlaybackRenderer;

    void renderPlaybackRegions (float* const* ppOutput, ARA::ARASamplePosition samplePosition, ARA::ARASampleCount samplesToRender, bool isPlayingBack);

    void enableRendering (ARA::ARASampleRate sampleRate, ARA::ARAChannelCount channelCount, ARA::ARASampleCount maxSamplesToRender) noexcept;
    void disableRendering () noexcept;

protected:
#if ARA_VALIDATE_API_CALLS
    void willAddPlaybackRegion (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
    void willRemovePlaybackRegion (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
#endif

private:
    ARA::ARASampleRate _sampleRate { 44100.0f };
    ARA::ARASampleCount _maxSamplesToRender { 4096 };
    ARA::ARAChannelCount _channelCount { 1 };
#if ARA_VALIDATE_API_CALLS
    bool _isRenderingEnabled { false };
#endif
};
