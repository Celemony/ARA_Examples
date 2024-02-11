//------------------------------------------------------------------------------
//! \file       ARAPlaybackController.h
//!             implementation of the host ARAPlaybackControllerInterface
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

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include "ARADocumentController.h"

/*******************************************************************************/
// Implementation of our test host's playback controller interface
// Since we aren't a real host this doesn't do anything, but it's the
// plug-in's means of controller the host transport
class ARAPlaybackController : public ARA::Host::PlaybackControllerInterface
{
public:
    ARAPlaybackController (ARADocumentController* araDocumentController) noexcept
    : _araDocumentController { araDocumentController }
    {}

    void requestStartPlayback () noexcept override;
    void requestStopPlayback () noexcept override;
    void requestSetPlaybackPosition (ARA::ARATimePosition timePosition) noexcept override;
    void requestSetCycleRange (ARA::ARATimePosition startTime, ARA::ARATimeDuration duration) noexcept override;
    void requestEnableCycle (bool enable) noexcept override;

private:
    ARADocumentController* const _araDocumentController;
};
