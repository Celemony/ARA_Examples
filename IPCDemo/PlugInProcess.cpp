//------------------------------------------------------------------------------
//! \file       PlugInProcess.cpp
//!             implementation of the SDK IPC demo example, plug-in process
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
// This is a brief proof-of-concept demo that hooks up an ARA capable plug-in
// in a separate process using IPC.
// This educational example is not suitable for production code -
// see MainProcess.cpp for a list of issues.
//------------------------------------------------------------------------------

// test code includes
#include "ARAIPCProxyHost.h"

// ARA framework includes
#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARADispatchBase.h"

using namespace ARA;


// in this simple demo application, we need logging to be always enabled, even in release builds.
// this needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif


// list of available companion APIs
#define PLUGIN_FORMAT_AU   1
#define PLUGIN_FORMAT_VST3 2

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    #include "ExamplesCommon/PlugInHosting/AudioUnitLoader.h"
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    #include "ExamplesCommon/PlugInHosting/VST3Loader.h"
#else
    #error "PLUGIN_FORMAT not configured properly in the project"
#endif


static const ARAFactory* factory {};

// asserts
#if ARA_VALIDATE_API_CALLS
static ARAAssertFunction assertFunction = &ARAInterfaceAssert;
#else
static ARAAssertFunction assertFunction = nullptr;
#endif
static ARAAssertFunction * assertFunctionReference = &assertFunction;

ARA_SETUP_DEBUG_MESSAGE_PREFIX("IPC-PlugIn");

int main (int argc, const char * argv[])
{
    // load plug-in
    const SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig { kARAAPIGeneration_2_0_Final, assertFunctionReference };

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aufx', 'AraT', 'ADeC');
//  AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aufx', 'Ara3', 'ADeC');
//  AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aumf', 'MPLG', 'CLMY');
    ARA_INTERNAL_ASSERT (audioUnitComponent != nullptr);

    factory = AudioUnitGetARAFactory (audioUnitComponent);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3Binary vst3Binary = VST3LoadBinary ("ARATestPlugIn.vst3");
//  VST3Binary vst3Binary = VST3LoadBinary ("/Library/Audio/Plug-Ins/VST3/Melodyne.vst3");
    ARA_INTERNAL_ASSERT (vst3Binary != nullptr);

    factory = VST3GetARAFactory (vst3Binary, nullptr);
#endif

    if (factory == nullptr)
    {
        ARA_WARN ("this plug-in doesn't support ARA.");
        return -1;                // this plug-in doesn't support ARA.
    }
    ARA_VALIDATE_API_CONDITION (factory->structSize >= kARAFactoryMinSize);

    if (factory->lowestSupportedApiGeneration > kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN ("this plug-in only supports newer generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }
    if (factory->highestSupportedApiGeneration < kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN ("this plug-in only supports older generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }

#if ARA_VALIDATE_API_CALLS
    ARASetExternalAssertReference (assertFunctionReference);
#endif

    ARA_VALIDATE_API_CONDITION (factory->factoryID != nullptr);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->factoryID) > 5);  // at least "xx.y." needed to form a valid url-based unique ID
    ARA_VALIDATE_API_CONDITION (factory->initializeARAWithConfiguration != nullptr);
    ARA_VALIDATE_API_CONDITION (factory->uninitializeARA != nullptr);
    ARA_VALIDATE_API_CONDITION (factory->createDocumentControllerWithDocument != nullptr);

    factory->initializeARAWithConfiguration (&interfaceConfig);

    ARA_LOG ("launched successfully and loaded plug-in %s.", factory->plugInName);

    // let the proxy set itself up and run the runloop
    ProxyHost::runHost (*factory, "com.arademocompany.IPCDemo.hostCommands", "com.arademocompany.IPCDemo.plugInCallbacks");

    factory->uninitializeARA ();

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    // unloading is not supported for Audio Units
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3UnloadBinary (vst3Binary);
#endif

    ARA_LOG ("completed.");
    return 0;
}
