//------------------------------------------------------------------------------
//! \file       TestVST3Processor.h
//!             VST3 audio effect class for the ARA test plug-in,
//!             originally created using the VST project generator from the Steinberg VST3 SDK
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

#include "ARA_API/ARAVST3.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include "ARA_Library/PlugIn/ARAPlug.h"

//------------------------------------------------------------------------
//  TestVST3Processor
//------------------------------------------------------------------------
class TestVST3Processor : public Steinberg::Vst::AudioEffect, public ARA::IPlugInEntryPoint, public ARA::IPlugInEntryPoint2
{
public:
    TestVST3Processor ();
    ~TestVST3Processor () SMTG_OVERRIDE;

    // Class IDs
    static const Steinberg::FUID getClassFUID ()
    {
        return Steinberg::FUID (0xA8497651, 0x8C994E9C, 0xA5030D11, 0x3617C309);
    }

    static const Steinberg::FUID getEditControllerClassFUID ()
    {
        return Steinberg::FUID (0x005F4AB1, 0x67CE4E31, 0x9ED45F1D, 0x3F5DC889);
    }

    // Create functions
    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*> (new TestVST3Processor);
    }

    static Steinberg::FUnknown* createEditControllerInstance (void* /*context*/)
    {
        return static_cast<Steinberg::Vst::IEditController*> (new Steinberg::Vst::EditController);
    }

    //------------------------------------------------------------------------
    // AudioEffect overrides:
    //------------------------------------------------------------------------
    /** Called at first after constructor */
    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;

    /** Called at the end before destructor */
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

    /** Switch the Plug-in on/off */
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

    /** Will be called before any process call */
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;

    /** Try to set (host => plug-in) a wanted arrangement for inputs and outputs. */
    Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                      Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;

    /** Asks if a given sample size is supported see SymbolicSampleSizes. */
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

    /** Here we go...the process call */
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;


    //------------------------------------------------------------------------
    // ARA::IPlugInEntryPoint2 overrides:
    //------------------------------------------------------------------------

    /** Get associated ARA factory */
    const ARA::ARAFactory* PLUGIN_API getFactory () SMTG_OVERRIDE;

    /** Bind to ARA document controller instance */
    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentController (ARA::ARADocumentControllerRef documentControllerRef) SMTG_OVERRIDE;
    const ARA::ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef,
                                                    ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles) SMTG_OVERRIDE;

    //------------------------------------------------------------------------
    // Interface
    //------------------------------------------------------------------------
    OBJ_METHODS (TestVST3Processor, AudioEffect)
        DEFINE_INTERFACES
        DEF_INTERFACE (IPlugInEntryPoint)
        DEF_INTERFACE (IPlugInEntryPoint2)
        END_DEFINE_INTERFACES (AudioEffect)
        REFCOUNT_METHODS (AudioEffect)

//------------------------------------------------------------------------
protected:
    ARA::PlugIn::PlugInExtension _araPlugInExtension;
};

ARA_DISABLE_VST3_WARNINGS_END
