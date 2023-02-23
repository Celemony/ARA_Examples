//------------------------------------------------------------------------------
//! \file       VST3Loader.h
//!             VST3 specific ARA implementation for the SDK's hosting examples
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

#if defined(__cplusplus)
extern "C"
{
#endif

struct _VST3Binary;
struct _VST3Effect;
typedef struct _VST3Binary * VST3Binary;
typedef struct _VST3Effect * VST3Effect;

VST3Binary VST3LoadBinary(const char * binaryName);
const ARA_NAMESPACE ARAFactory * VST3GetARAFactory(VST3Binary vst3Binary, const char * optionalPlugInName);
VST3Effect VST3CreateEffect(VST3Binary vst3Effect, const char * optionalPlugInName);
const ARA_NAMESPACE ARAPlugInExtensionInstance * VST3BindToARADocumentController(VST3Effect vst3Effect, ARA_NAMESPACE ARADocumentControllerRef controllerRef, ARA_NAMESPACE ARAPlugInInstanceRoleFlags assignedRoles);
void VST3StartRendering(VST3Effect vst3Effect, int32_t maxBlockSize, double sampleRate);
void VST3RenderBuffer(VST3Effect vst3Effect, int32_t blockSize, double sampleRate, int64_t samplePosition, float * buffer);
void VST3StopRendering(VST3Effect vst3Effect);
void VST3DestroyEffect(VST3Effect vst3Effect);
void VST3UnloadBinary(VST3Binary vst3Binary);

#if defined(__cplusplus)
}   // extern "C"
#endif
