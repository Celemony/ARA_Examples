//------------------------------------------------------------------------------
//! \file       TestAudioUnit.cpp
//!             Audio Unit effect class for the ARA test plug-in,
//!             created via the Xcode 3 project template for Audio Unit effects.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2025, Celemony Software GmbH, All Rights Reserved.
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

#include "TestAudioUnit.h"
#include "ARATestPlaybackRenderer.h"
#include "ARATestDocumentController.h"

#include "ARA_API/ARAAudioUnit.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

AUSDK_COMPONENT_ENTRY(ausdk::AUBaseFactory, TestAudioUnit)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
TestAudioUnit::TestAudioUnit(AudioUnit component)
    : AUEffectBase(component)
{
    CreateElements();
}

OSStatus TestAudioUnit::Initialize()
{
    OSStatus result = AUEffectBase::Initialize();

    if (ARATestPlaybackRenderer* playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer>())
        playbackRenderer->enableRendering(GetSampleRate(), GetNumberOfChannels(), GetMaxFramesPerSlice(), true);

    return result;
}

void TestAudioUnit::Cleanup()
{
    if (ARATestPlaybackRenderer* playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer>())
        playbackRenderer->disableRendering();

    AUEffectBase::Cleanup();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 TestAudioUnit::SupportedNumChannels(const AUChannelInfo** outInfo)
{
    static const AUChannelInfo channelInfo { -1, -1 };

    if (outInfo)
        *outInfo = &channelInfo;

    return 1;
}

CFURLRef TestAudioUnit::CopyIconLocation()
{
    static_assert(sizeof(ARA_PRODUCT_BUNDLE_IDENTIFIER) > 5, "ARA_PRODUCT_BUNDLE_IDENTIFIER must be defined when compiling this file");
    // proper code should check for errors here!
    return CFBundleCopyResourceURL(CFBundleGetBundleWithIdentifier(CFSTR(ARA_PRODUCT_BUNDLE_IDENTIFIER)), CFSTR("ARAExamples.icns"), nullptr, nullptr);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus TestAudioUnit::GetPropertyInfo( AudioUnitPropertyID    inID,
                                            AudioUnitScope        inScope,
                                            AudioUnitElement    inElement,
                                            UInt32 &            outDataSize,
                                            bool &              outWritable)
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch (inID)
        {
            case ARA::kAudioUnitProperty_ARAFactory:
                outWritable = false;
                outDataSize = sizeof(ARA::ARAAudioUnitFactory);
                return noErr;

#if ARA_SUPPORT_VERSION_1
            case ARA::kAudioUnitProperty_ARAPlugInExtensionBinding:
#endif
            case ARA::kAudioUnitProperty_ARAPlugInExtensionBindingWithRoles:
                outWritable = false;
                outDataSize = sizeof(ARA::ARAAudioUnitPlugInExtensionBinding);
                return noErr;
        }
    }

    return AUEffectBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

OSStatus TestAudioUnit::GetProperty(    AudioUnitPropertyID inID,
                                        AudioUnitScope         inScope,
                                        AudioUnitElement     inElement,
                                        void *                outData )
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch (inID)
        {
            case ARA::kAudioUnitProperty_ARAFactory:
            {
                if (((ARA::ARAAudioUnitFactory *) outData)->inOutMagicNumber != ARA::kARAAudioUnitMagic)
                    return kAudioUnitErr_InvalidProperty;   // if the magic value isn't found, the property ID is re-used outside the ARA context with different, unsupported sematics

                ((ARA::ARAAudioUnitFactory *) outData)->outFactory = ARATestDocumentController::getARAFactory();
                return noErr;
            }

#if ARA_SUPPORT_VERSION_1
            case ARA::kAudioUnitProperty_ARAPlugInExtensionBinding:
#endif
            case ARA::kAudioUnitProperty_ARAPlugInExtensionBindingWithRoles:
            {
                if (((ARA::ARAAudioUnitPlugInExtensionBinding *) outData)->inOutMagicNumber != ARA::kARAAudioUnitMagic)
                    return kAudioUnitErr_InvalidProperty;   // if the magic value isn't found, the property ID is re-used outside the ARA context with different, unsupported sematics

                ARA::ARAPlugInInstanceRoleFlags knownRoles;
                ARA::ARAPlugInInstanceRoleFlags assignedRoles;
#if ARA_SUPPORT_VERSION_1
                if (inID == ARA::kAudioUnitProperty_ARAPlugInExtensionBinding)
                {
                    ARA_VALIDATE_API_STATE(ARA::PlugIn::DocumentController::getUsedApiGeneration() < ARA::kARAAPIGeneration_2_0_Draft);
                    knownRoles = ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;
                    assignedRoles = ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole;

                }
                else
#endif
                {
                    knownRoles = ((ARA::ARAAudioUnitPlugInExtensionBinding *) outData)->knownRoles;
                    assignedRoles = ((ARA::ARAAudioUnitPlugInExtensionBinding *) outData)->assignedRoles;
                }

                auto documentControllerRef = ((ARA::ARAAudioUnitPlugInExtensionBinding *) outData)->inDocumentControllerRef;
                const auto instance = _araPlugInExtension.bindToARA(documentControllerRef, knownRoles, assignedRoles);
                ((ARA::ARAAudioUnitPlugInExtensionBinding *) outData)->outPlugInExtension = instance;
                return (instance) ? noErr : kAudioUnitErr_CannotDoInCurrentContext;
            }
        }
    }

    return AUEffectBase::GetProperty(inID, inScope, inElement, outData);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus TestAudioUnit::ProcessBufferLists(    AudioUnitRenderActionFlags &    /*ioActionFlags*/,
                                                const AudioBufferList &            inBuffer,
                                                AudioBufferList &                outBuffer,
                                                UInt32                            inFramesToProcess )
{
    ARA_VALIDATE_API_CONDITION(outBuffer.mNumberBuffers == GetNumberOfChannels());
    ARA_VALIDATE_API_CONDITION(inFramesToProcess <= GetMaxFramesPerSlice());

    // quick and dirty example implementation, any proper Audio Unit will handle many more cases here!

    Boolean outIsPlaying = FALSE;
    Float64 outCurrentSampleInTimeLine = 0.0;
    CallHostTransportState(&outIsPlaying, nullptr, &outCurrentSampleInTimeLine, nullptr, nullptr, nullptr);

    UInt32 channelCount = GetNumberOfChannels();
    Float32 * channels[channelCount];
    for (UInt32 i = 0; i < channelCount; ++i)
        channels[i] = (Float32 *) outBuffer.mBuffers[i].mData;

    if (auto playbackRenderer = _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer>())
    {
        // if we're an ARA playback renderer, calculate ARA playback output
        playbackRenderer->renderPlaybackRegions(channels, ARA::roundSamplePosition(outCurrentSampleInTimeLine), inFramesToProcess, outIsPlaying);
    }
    else
    {
        // if we're no ARA playback renderer, we're just copying the inputs to the outputs, which is
        // appropriate both when being only an ARA editor renderer, or when being used in non-ARA mode.
        for (UInt32 i = 0; i < channelCount; ++i)
        {
            if (channels[i] != inBuffer.mBuffers[i].mData)      // check in-place processing
                std::memcpy(channels[i], inBuffer.mBuffers[i].mData, sizeof(float) * inFramesToProcess);
        }
    }

    // if we are an ARA editor renderer, we now would add out preview signal to the output, but
    // our test implementation does not support editing and thus never generates any preview signal.
//  if (auto editorRenderer = _araPlugInExtension.getEditorRenderert<ARATestEditorRenderer*>()))
//      editorRenderer->addEditorSignal(...);

    return noErr;
}

OSStatus TestAudioUnit::Render(    AudioUnitRenderActionFlags &ioActionFlags,
                                    const AudioTimeStamp &        inTimeStamp,
                                    UInt32                        nFrames)
{
    // ARA playback renderers don't need to have input - the base SDK cannot handle this so we need to special-case here.
    if (!HasInput(0) &&
        _araPlugInExtension.getPlaybackRenderer<ARATestPlaybackRenderer>())
    {
        AudioBufferList inputBufferList;
        inputBufferList.mNumberBuffers = 0;

        ausdk::AUOutputElement *pOutputElement = GetOutput(0);
        if (!pOutputElement)
            return kAudioUnitErr_NoConnection;

        return ProcessBufferLists(ioActionFlags,
                                  inputBufferList,
                                  pOutputElement->GetBufferList(),
                                  nFrames);
    }

    return AUEffectBase::Render(ioActionFlags, inTimeStamp, nFrames);
}
