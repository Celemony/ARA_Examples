//------------------------------------------------------------------------------
//! \file       ARAPlaybackController.cpp
//!             implementation of the host ARAPlaybackControllerInterface
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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#include "ARAPlaybackController.h"

void ARAPlaybackController::requestStartPlayback () noexcept
{
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());
    ARA_WARN ("requestStartPlayback () not implemented.");
}

void ARAPlaybackController::requestStopPlayback () noexcept
{
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());
    ARA_WARN ("requestStopPlayback () not implemented");
}

void ARAPlaybackController::requestSetPlaybackPosition (ARA::ARATimePosition timePosition) noexcept
{
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());
    ARA_WARN ("requestSetPlaybackPosition () not implemented, requested time is %.2f", timePosition);
}

void ARAPlaybackController::requestSetCycleRange (ARA::ARATimePosition startTime, ARA::ARATimeDuration duration) noexcept
{
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());
    ARA_WARN ("requestSetCycleRange () not implemented, requested range is %.2f to %.2f", startTime, startTime + duration);
}

void ARAPlaybackController::requestEnableCycle (bool enable) noexcept
{
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());
    ARA_WARN ("requestEnableCycle () not implemented, requested to turn %s.", enable ? "on" : "off");
}
