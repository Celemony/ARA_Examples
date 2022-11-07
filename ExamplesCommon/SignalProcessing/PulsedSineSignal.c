//------------------------------------------------------------------------------
//! \file       PulsedSineSignal.c
//!             creating a pulsed sine test signal for ARA examples
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

#include "PulsedSineSignal.h"

#include <math.h>
#if !defined(M_PI)
    #define M_PI 3.14159265358979323846264338327950288 /* taken from the macOS variant of math.h - M_PI is a custom extension of C11 */
#endif

void RenderPulsedSineSignal (int64_t samplePosition, double sampleRate, int64_t sampleCount,
                             int32_t channelCount, int64_t samplesPerChannel,
                             void* const buffers[], bool use64BitSamples)
{
    int64_t index = 0;
    while (samplesPerChannel--)
    {
        double value = 0.0;
        if ((0 <= samplePosition) && (samplePosition < sampleCount))
        {
            const double normalizedTime = ((double)samplePosition) * 440.0 / sampleRate;
            value = (fmod (normalizedTime, 440.0) <= 220.0) ? sin (normalizedTime * M_PI * 2.0) : 0.0;
            value *= (fmod (normalizedTime, 880.0) <= 440.0) ? 1.0 : 0.125;
        }

        for (int32_t c = 0; c < channelCount; ++c)
        {
            if (use64BitSamples != false)
                ((double*) buffers[c])[index] = value;
            else
                ((float*) buffers[c])[index] = (float)value;
        }
        ++samplePosition;
        ++index;
    }
}
