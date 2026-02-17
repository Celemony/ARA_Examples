//------------------------------------------------------------------------------
//! \file       PulsedSineSignal.h
//!             creating a pulsed sine test signal for ARA examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2026, Celemony Software GmbH, All Rights Reserved.
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

#include <stdint.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C"
{
#endif

// Creates a pulsed sine: half a second sine with 440 Hz, half a second silence.
// Amplitude varies each other second between full scale and 1/8 scale.
// Samples before 0 or at or after sampleCount are set to zero.
void RenderPulsedSineSignal (int64_t samplePosition, double sampleRate, int64_t sampleCount,
                             int32_t channelCount, int64_t samplesPerChannel,
                             void* const buffers[], bool use64BitSamples);

#if defined(__cplusplus)
}   // extern "C"
#endif
