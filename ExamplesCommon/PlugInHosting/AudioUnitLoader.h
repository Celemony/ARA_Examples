//------------------------------------------------------------------------------
//! \file       AudioUnitLoader.h
//!             Audio Unit specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2024, Celemony Software GmbH, All Rights Reserved.
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
#include "ARA_Library/IPC/ARAIPC.h"

#include <MacTypes.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct _AudioUnitComponent * AudioUnitComponent;
typedef struct _AudioUnitInstance * AudioUnitInstance;

AudioUnitComponent AudioUnitPrepareComponentWithIDs(OSType type, OSType subtype, OSType manufacturer);
bool AudioUnitIsV2(AudioUnitComponent audioUnitComponent);
AudioUnitInstance AudioUnitOpenInstance(AudioUnitComponent audioUnitComponent, bool useIPC);
// On return, *connection will be NULL if Audio Unit does not use IPC, otherwise it will point to
// a valid message channel for all factory-related calls until AudioUnitCleanupComponent() is called.
const ARA_NAMESPACE ARAFactory * AudioUnitGetARAFactory(AudioUnitInstance audioUnit, ARA_IPC_NAMESPACE ARAIPCConnectionRef * connectionRef);
const ARA_NAMESPACE ARAPlugInExtensionInstance * AudioUnitBindToARADocumentController(AudioUnitInstance audioUnit, ARA_NAMESPACE ARADocumentControllerRef controllerRef, ARA_NAMESPACE ARAPlugInInstanceRoleFlags assignedRoles);
void AudioUnitStartRendering(AudioUnitInstance audioUnit, UInt32 maxBlockSize, double sampleRate);
void AudioUnitRenderBuffer(AudioUnitInstance audioUnit, UInt32 blockSize, SInt64 samplePosition, float * buffer);
void AudioUnitStopRendering(AudioUnitInstance audioUnit);
void AudioUnitCloseInstance(AudioUnitInstance audioUnit);
void AudioUnitCleanupComponent(AudioUnitComponent audioUnitComponent);

#if defined(__cplusplus)
}   // extern "C"
#endif
