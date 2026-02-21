//------------------------------------------------------------------------------
//! \file       TestVST3Entry.cpp
//!             VST3 factory functions for the ARA test plug-in,
//!             originally created using the VST project generator from the Steinberg VST3 SDK
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

#include "TestVST3Processor.h"
#include "ARATestMainFactory.h"
#include "TestPlugInConfig.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN

#if VST_VERSION >= 0x03060D   /* 3.6.13 */
    #include "public.sdk/source/main/pluginfactory.h"
#else
    #include "public.sdk/source/main/pluginfactoryvst3.h"
#endif


//------------------------------------------------------------------------
//  Module init/exit
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// called after library was loaded
bool InitModule ();
bool InitModule ()
{
    return true;
}

//------------------------------------------------------------------------
// called after library is unloaded
bool DeinitModule ();
bool DeinitModule ()
{
    return true;
}

//------------------------------------------------------------------------
//  VST Class IIDs
//------------------------------------------------------------------------

DEF_CLASS_IID (ARA::IMainFactory)
DEF_CLASS_IID (ARA::IPlugInEntryPoint)
DEF_CLASS_IID (ARA::IPlugInEntryPoint2)


//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------

#define FULL_VERSION_STR TEST_VERSION_STRING "." IN_QUOTES(TEST_BUILD_VERSION)

BEGIN_FACTORY_DEF (TEST_MANUFACTURER_NAME,
                   TEST_INFORMATION_URL,
                   TEST_MAILTO_URL)

    //---First Plug-in included in this factory-------
    // its kVstAudioEffectClass component
    DEF_CLASS2 (INLINE_UID_FROM_FUID (TestVST3Processor::getClassFUID ()),
                PClassInfo::kManyInstances,     // cardinality
                kVstAudioEffectClass,           // the IAudioProcessor component category (do not changed this)
                TEST_PLUGIN_NAME,               // here the Plug-in name
                Vst::kDistributable,            // means that component and controller could be distributed on different computers
                "Fx|OnlyARA",                   // Subcategory for this Plug-in (see Steinberg::Vst::PlugType)
                FULL_VERSION_STR,               // Plug-in version
                kVstVersionString,              // the VST 3 SDK version (do not changed this, use always this define)
                TestVST3Processor::createInstance)   // function pointer called when this component should be instantiated

    // its kVstComponentControllerClass component
    DEF_CLASS2 (INLINE_UID_FROM_FUID (TestVST3Processor::getEditControllerClassFUID ()),
                PClassInfo::kManyInstances,     // cardinality
                kVstComponentControllerClass,   // the Controller category (do not changed this)
                TEST_PLUGIN_NAME,               // controller name (could be the same as component name)
                0,                              // not used here
                "",                             // not used here
                FULL_VERSION_STR,               // Plug-in version
                kVstVersionString,              // the VST 3 SDK version (do not changed this, use always this define)
                TestVST3Processor::createEditControllerInstance) // function pointer called when this component should be instantiated

    // its kARAMainFactoryClass component
    DEF_CLASS2 (INLINE_UID_FROM_FUID (ARATestMainFactory::getClassFUID ()),
                PClassInfo::kManyInstances,     // cardinality
                kARAMainFactoryClass,           // the ARA Main Factory category (do not changed this)
                TEST_PLUGIN_NAME,               // here the Plug-in name (MUST be the same as component name if multiple kVstAudioEffectClass components are used!)
                0,                              // not used here
                "",                             // not used here
                FULL_VERSION_STR,               // Plug-in version (to be changed)
                kVstVersionString,              // the VST 3 SDK version (do not changed this, use always this define)
                ARATestMainFactory::createInstance)     // function pointer called when this component should be instantiated

END_FACTORY

ARA_DISABLE_VST3_WARNINGS_END
