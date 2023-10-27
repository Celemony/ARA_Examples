//------------------------------------------------------------------------------
//! \file       TestVST3Processor.cpp
//!             VST3 audio effect class for the ARA test plug-in,
//!             originally created using the VST project generator from the Steinberg VST3 SDK
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2023, Celemony Software GmbH, All Rights Reserved.
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
#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"

#include "ARA_Library/PlugIn/ARAPlug.h"
#include "ARA_Library/Debug/ARADebug.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN
    #include "pluginterfaces/vst/ivstprocesscontext.h"
ARA_DISABLE_VST3_WARNINGS_END

using namespace Steinberg;

// helper to improve readability
int32 getAudioBusChannelCount (const IPtr<Vst::Bus>& bus)
{
    return Vst::SpeakerArr::getChannelCount (FCast<Vst::AudioBus> (bus.get ())->getArrangement ());
}

//-----------------------------------------------------------------------------
TestVST3Processor::TestVST3Processor ()
{
    setControllerClass (getEditControllerClassFUID ());

#if VST_VERSION >= 0x030700   /* 3.7.0 */
    processContextRequirements.needTransportState ();
#endif
}

//------------------------------------------------------------------------
TestVST3Processor::~TestVST3Processor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::initialize (FUnknown* context)
{
    // Here the Plug-in will be instantiated

    //---always initialize the parent-------
    tresult result = AudioEffect::initialize (context);
    // if everything Ok, continue
    if (result != kResultOk)
    {
        return result;
    }

    //--- create Audio IO ------
    addAudioInput (STR16 ("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::terminate ()
{
    // Here the Plug-in will be de-instantiated, last possibility to remove some memory!

    //---do not forget to call parent ------
    return AudioEffect::terminate ();
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setBusArrangements (Vst::SpeakerArrangement* inputs, int32 numIns, Vst::SpeakerArrangement* outputs, int32 numOuts)
{
    // we only support one in and output bus and these buses must have the same number of non-zero channels
    if ((numIns == 1) && (numOuts == 1) &&
        (inputs[0] == outputs[0]) &&
        (Vst::SpeakerArr::getChannelCount (outputs[0]) != 0))
    {
        return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
    }

    return kResultFalse;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setActive (TBool state)
{
    //--- called when the Plug-in is enable/disable (On/Off) -----

    if (ARATestPlaybackRenderer* playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ())
    {
        if (state)
            playbackRenderer->enableRendering (processSetup.sampleRate, getAudioBusChannelCount (audioOutputs[0]), processSetup.maxSamplesPerBlock);
        else
            playbackRenderer->disableRendering ();
    }

    return AudioEffect::setActive (state);
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::process (Vst::ProcessData& data)
{
    //--- Here you have to implement your processing

    if (!data.outputs || !data.outputs[0].numChannels)
        return kResultTrue;

    ARA_VALIDATE_API_CONDITION (data.outputs[0].numChannels == getAudioBusChannelCount (audioOutputs[0]));
    ARA_VALIDATE_API_CONDITION (data.numSamples <= processSetup.maxSamplesPerBlock);

    if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer> ())
    {
        // if we're an ARA playback renderer, calculate ARA playback output
        playbackRenderer->renderPlaybackRegions (data.outputs[0].channelBuffers32, data.processContext->projectTimeSamples,
                                                 data.numSamples, (data.processContext->state & Vst::ProcessContext::kPlaying) != 0);
    }
    else
    {
        // if we're no ARA playback renderer, we're just copying the inputs to the outputs, which is
        // appropriate both when being only an ARA editor renderer, or when being used in non-ARA mode.
        for (int32 c = 0; c < data.outputs[0].numChannels; ++c)
            std::memcpy (data.outputs[0].channelBuffers32[c], data.inputs[0].channelBuffers32[c], sizeof (float) * static_cast<size_t> (data.numSamples));
    }

    // if we are an ARA editor renderer, we now would add out preview signal to the output, but
    // our test implementation does not support editing and thus never generates any preview signal.
//  if (auto editorRenderer = _araPlugInExtension.getEditorRenderer<ARATestEditorRenderer*> ())
//      editorRenderer->addEditorSignal (...);

    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    //--- called before any processing ----
    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API TestVST3Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    // by default kSample32 is supported
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;

    // disable the following comment if your processing support kSample64
    /* if (symbolicSampleSize == Vst::kSample64)
        return kResultTrue; */

    return kResultFalse;
}

//-----------------------------------------------------------------------------
const ARA::ARAFactory* PLUGIN_API TestVST3Processor::getFactory ()
{
    return ARATestDocumentController::getARAFactory ();
}

//-----------------------------------------------------------------------------
#if ARA_SUPPORT_VERSION_1
const ARA::ARAPlugInExtensionInstance* PLUGIN_API TestVST3Processor::bindToDocumentController (ARA::ARADocumentControllerRef documentControllerRef)
{
    ARA_VALIDATE_API_STATE (ARA::PlugIn::DocumentController::getUsedApiGeneration () < ARA::kARAAPIGeneration_2_0_Draft);
    constexpr auto allRoles = ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;
    return _araPlugInExtension.bindToDocumentController (documentControllerRef, allRoles, allRoles);
}
#else
const ARA::ARAPlugInExtensionInstance* PLUGIN_API TestVST3Processor::bindToDocumentController (ARA::ARADocumentControllerRef /*documentControllerRef*/)
{
    ARA_VALIDATE_API_STATE (false && "call is deprecated in ARA 2, host must not call this");
    return nullptr;
}
#endif

//-----------------------------------------------------------------------------
const ARA::ARAPlugInExtensionInstance* PLUGIN_API TestVST3Processor::bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef,
                                                                        ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
{
    return _araPlugInExtension.bindToARA (documentControllerRef, knownRoles, assignedRoles);
}
