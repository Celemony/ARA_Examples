//------------------------------------------------------------------------------
//! \file       CLAPLoader.h
//!             CLAP specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2022-2024, Celemony Software GmbH, All Rights Reserved.
//!             Developed in cooperation with Timo Kaluza (defiantnerd)
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

struct _CLAPBinary;
struct _CLAPPlugIn;
typedef struct _CLAPBinary * CLAPBinary;
typedef struct _CLAPPlugIn * CLAPPlugIn;

CLAPBinary CLAPLoadBinary(const char * binaryName);
const ARA_NAMESPACE ARAFactory * CLAPGetARAFactory(CLAPBinary clapBinary, const char * optionalPlugInName);
CLAPPlugIn CLAPCreatePlugIn(CLAPBinary clapBinary, const char * optionalPlugInName);
const ARA_NAMESPACE ARAPlugInExtensionInstance * CLAPBindToARADocumentController(CLAPPlugIn clapPlugIn, ARA_NAMESPACE ARADocumentControllerRef controllerRef, ARA_NAMESPACE ARAPlugInInstanceRoleFlags assignedRoles);
void CLAPStartRendering(CLAPPlugIn clapPlugIn, uint32_t maxBlockSize, double sampleRate);
void CLAPRenderBuffer(CLAPPlugIn clapPlugIn, uint32_t blockSize, int64_t samplePosition, float * buffer);
void CLAPStopRendering(CLAPPlugIn clapPlugIn);
void CLAPDestroyPlugIn(CLAPPlugIn clapPlugIn);
void CLAPUnloadBinary(CLAPBinary clapBinary);

#if defined(__cplusplus)
}   // extern "C"
#endif
