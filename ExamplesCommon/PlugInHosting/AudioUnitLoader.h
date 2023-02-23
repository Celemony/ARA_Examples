//------------------------------------------------------------------------------
//! \file       AudioUnitLoader.h
//!             Audio Unit specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "ARA_API/ARAInterface.h"
#include "ARA_Library/Debug/ARADebug.h"

#include <MacTypes.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct OpaqueAudioComponent * AudioUnitComponent;
typedef struct _AudioUnitInstance * AudioUnitInstance;

AudioUnitComponent AudioUnitPrepareComponentWithIDs(OSType type, OSType subtype, OSType manufacturer);
const struct ARA_NAMESPACE ARAFactory * AudioUnitGetARAFactory(AudioUnitComponent audioUnitComponent);
AudioUnitInstance AudioUnitOpenInstance(AudioUnitComponent audioUnitComponent);
const struct ARA_NAMESPACE ARAPlugInExtensionInstance * AudioUnitBindToARADocumentController(AudioUnitInstance audioUnit, ARA_NAMESPACE ARADocumentControllerRef controllerRef, ARA_NAMESPACE ARAPlugInInstanceRoleFlags assignedRoles);
void AudioUnitStartRendering(AudioUnitInstance audioUnit, UInt32 maxBlockSize, double sampleRate);
void AudioUnitRenderBuffer(AudioUnitInstance audioUnit, UInt32 blockSize, SInt64 samplePosition, float * buffer);
void AudioUnitStopRendering(AudioUnitInstance audioUnit);
void AudioUnitCloseInstance(AudioUnitInstance audioUnit);

#if defined(__cplusplus)
}   // extern "C"
#endif
