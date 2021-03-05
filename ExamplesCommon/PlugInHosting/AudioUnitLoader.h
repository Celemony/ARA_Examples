//------------------------------------------------------------------------------
//! \file       AudioUnitLoader.h
//!             Audio Unit specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2021, Celemony Software GmbH, All Rights Reserved.
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

#include <CoreServices/CoreServices.h>

struct ComponentInstanceRecord;
typedef struct ComponentInstanceRecord * AudioUnit;

struct OpaqueAudioComponent;
typedef struct OpaqueAudioComponent * AudioComponent;


#if defined(__cplusplus)
extern "C"
{
#endif

AudioComponent AudioUnitFindValidARAComponentWithIDs(OSType type, OSType subtype, OSType manufacturer);
const struct ARA_NAMESPACE ARAFactory * AudioUnitGetARAFactory(AudioComponent audioComponent);
AudioUnit AudioUnitOpen(AudioComponent audioComponent);
const struct ARA_NAMESPACE ARAPlugInExtensionInstance * AudioUnitBindToARADocumentController(AudioUnit audioUnit, ARA_NAMESPACE ARADocumentControllerRef controllerRef, ARA_NAMESPACE ARAPlugInInstanceRoleFlags assignedRoles);
void AudioUnitStartRendering(AudioUnit audioUnit, UInt32 blockSize, double sampleRate);
void AudioUnitRenderBuffer(AudioUnit audioUnit, UInt32 blockSize, SInt64 samplePosition, float * buffer);
void AudioUnitStopRendering(AudioUnit audioUnit);
void AudioUnitClose(AudioUnit audioUnit);

#if defined(__cplusplus)
}   // extern "C"
#endif
