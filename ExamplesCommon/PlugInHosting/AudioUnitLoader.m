//------------------------------------------------------------------------------
//! \file       AudioUnitLoader.m
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

#include "AudioUnitLoader.h"

#if !defined(__APPLE__)
    #error "this file can only be complied for Apple platforms"
#endif

#include <AudioUnit/AudioUnit.h>

#import <AVFoundation/AVFoundation.h>

#include "ARA_API/ARAAudioUnit.h"
#include "ARA_Library/Debug/ARADebug.h"

AudioComponent AudioUnitFindValidARAComponentWithIDs(OSType type, OSType subtype, OSType manufacturer)
{
    AudioComponentDescription compDesc = { type, subtype, manufacturer, 0, 0 };
    AudioComponent component = NULL;
    if (@available(macOS 10.10, *))
    {
        @autoreleasepool
        {
// debug code: list all Audio Units that support ARA v2.0
//          [[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsPassingTest:^BOOL(AVAudioUnitComponent *comp, BOOL *stop) {
//              if ([[comp allTagNames] indexOfObject:@kARAAudioComponentTag] != NSNotFound)
//                  NSLog(@"%@ %@ v%@ (%@, %@sandbox-safe) %@", [comp manufacturerName], [comp name], [comp versionString], [comp typeName], [comp isSandboxSafe] ? @"": @"not ", [comp iconURL]);
//              *stop = NO;
//              return NO;
//          }];

            AVAudioUnitComponent * avComponent = [[[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsMatchingDescription:compDesc] firstObject];
            if (avComponent)
            {
                ARA_VALIDATE_API_CONDITION([avComponent passesAUVal]);
                component = [avComponent audioComponent];

#if ARA_CPU_ARM
                // when migrating to ARM machines, we suggest to also add App Sandbox safety
//              ARA_VALIDATE_API_CONDITION([avComponent isSandboxSafe]);
                if (![avComponent isSandboxSafe])
                    ARA_WARN("This Audio Unit is not sandbox-safe, and thus might not work in future macOS releases.");
#endif
            }
        }
    }
    else
    {
        component = AudioComponentFindNext(NULL, &compDesc);
    }
    ARA_INTERNAL_ASSERT(component);
    return component;
}

AudioUnit AudioUnitOpen(AudioComponent audioComponent)
{
    AudioUnit audioUnit = NULL;
    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioComponentInstanceNew(audioComponent, &audioUnit);
    ARA_INTERNAL_ASSERT(status == noErr);
    ARA_INTERNAL_ASSERT(audioUnit != NULL);
    return audioUnit;
}

const struct ARAFactory * AudioUnitGetARAFactory(AudioComponent audioComponent)
{
    UInt32 propertySize = sizeof(ARAAudioUnitFactory);
    ARAAudioUnitFactory audioUnitFactory = { kARAAudioUnitMagic, NULL };
    Boolean isWriteable = FALSE;

    // check if it supports ARA by trying to get the factory
    AudioUnit audioUnit = AudioUnitOpen(audioComponent);
    OSStatus status = AudioUnitGetPropertyInfo(audioUnit, kAudioUnitProperty_ARAFactory, kAudioUnitScope_Global, 0, &propertySize, &isWriteable);
    if ((status != noErr) ||
        (propertySize != sizeof(ARAAudioUnitFactory)) ||
        isWriteable)
    {
        AudioUnitClose(audioUnit);
        return NULL;            // this plug-in doesn't support ARA.
    }

    status = AudioUnitGetProperty(audioUnit, kAudioUnitProperty_ARAFactory, kAudioUnitScope_Global, 0, &audioUnitFactory, &propertySize);
    if ((status != noErr) ||
        (propertySize != sizeof(ARAAudioUnitFactory)) ||
        (audioUnitFactory.inOutMagicNumber != kARAAudioUnitMagic))
    {
        AudioUnitClose(audioUnit);
        return NULL;            // this plug-in doesn't support ARA.
    }
    ARA_VALIDATE_API_CONDITION(audioUnitFactory.outFactory != NULL);

    if (@available(macOS 10.10, *))
    {
        @autoreleasepool
        {
            AudioComponentDescription compDesc;
            status = AudioComponentGetDescription(audioComponent, &compDesc);
            ARA_INTERNAL_ASSERT(status == noErr);
            AVAudioUnitComponent * avComponent = [[[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsMatchingDescription:compDesc] firstObject];
            ARA_INTERNAL_ASSERT(avComponent);
            ARA_VALIDATE_API_CONDITION([[avComponent allTagNames] indexOfObject:@kARAAudioComponentTag] != NSNotFound);
        }
    }

    AudioUnitClose(audioUnit);

    return audioUnitFactory.outFactory;
}

const struct ARAPlugInExtensionInstance * AudioUnitBindToARADocumentController(AudioUnit audioUnit, ARADocumentControllerRef controllerRef, ARAPlugInInstanceRoleFlags assignedRoles)
{
    UInt32 propertySize = sizeof(ARAAudioUnitPlugInExtensionBinding);
    UInt32 ARA_MAYBE_UNUSED_VAR(expectedPropertySize) = propertySize;
    ARAPlugInInstanceRoleFlags knownRoles = kARAPlaybackRendererRole | kARAEditorRendererRole | kARAEditorViewRole;
    ARAAudioUnitPlugInExtensionBinding audioUnitBinding = { kARAAudioUnitMagic, controllerRef, NULL, knownRoles, assignedRoles };

    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitGetProperty(audioUnit, kAudioUnitProperty_ARAPlugInExtensionBindingWithRoles, kAudioUnitScope_Global, 0, &audioUnitBinding, &propertySize);
#if defined(ARA_SUPPORT_VERSION_1) && (ARA_SUPPORT_VERSION_1)
    if (status != noErr)
    {
        propertySize = offsetof(ARAAudioUnitPlugInExtensionBinding, knownRoles);
        expectedPropertySize = propertySize;
        status = AudioUnitGetProperty(audioUnit, kAudioUnitProperty_ARAPlugInExtensionBinding, kAudioUnitScope_Global, 0, &audioUnitBinding, &propertySize);
    }
#endif

    ARA_VALIDATE_API_CONDITION(status == noErr);
    ARA_VALIDATE_API_CONDITION(propertySize == expectedPropertySize);
    ARA_VALIDATE_API_CONDITION(audioUnitBinding.inOutMagicNumber == kARAAudioUnitMagic);
    ARA_VALIDATE_API_CONDITION(audioUnitBinding.inDocumentControllerRef == controllerRef);
    ARA_VALIDATE_API_CONDITION(audioUnitBinding.outPlugInExtension != NULL);

    return audioUnitBinding.outPlugInExtension;
}


// in order for Melodyne to render the ARA data, it must be set to playback mode (in stop, its built-in pre-listening logic is active)
// thus we implement some crude, minimal transport information here.

static Float64 globalSampleIndex = 0.0;

OSStatus GetTransportState2(void * ARA_MAYBE_UNUSED_ARG(inHostUserData), Boolean * outIsPlaying, Boolean * outIsRecording,
                            Boolean * outTransportStateChanged, Float64 * outCurrentSampleInTimeLine,
                            Boolean * outIsCycling, Float64 * outCycleStartBeat, Float64 * outCycleEndBeat)
{
    if (outIsPlaying)
        *outIsPlaying = true;
    if (outIsRecording)
        *outIsRecording = false;
    if (outTransportStateChanged)
        *outTransportStateChanged = (globalSampleIndex == 0) ? true : false;
    if (outCurrentSampleInTimeLine)
        *outCurrentSampleInTimeLine = globalSampleIndex;

    if (outIsCycling)
        *outIsCycling = false;
    if (outCycleStartBeat)
        *outCycleStartBeat = 0;
    if (outCycleEndBeat)
        *outCycleEndBeat = 0;

    return noErr;
}

OSStatus GetTransportState1(void * inHostUserData, Boolean * outIsPlaying,
                            Boolean * outTransportStateChanged, Float64 * outCurrentSampleInTimeLine,
                            Boolean * outIsCycling, Float64 * outCycleStartBeat, Float64 * outCycleEndBeat)
{
    return GetTransportState2(inHostUserData, outIsPlaying, NULL, outTransportStateChanged, outCurrentSampleInTimeLine,
                            outIsCycling, outCycleStartBeat, outCycleEndBeat);
}


OSStatus RenderCallback(void * ARA_MAYBE_UNUSED_ARG(inRefCon), AudioUnitRenderActionFlags * ARA_MAYBE_UNUSED_ARG(ioActionFlags),
                        const AudioTimeStamp * ARA_MAYBE_UNUSED_ARG(inTimeStamp), UInt32 ARA_MAYBE_UNUSED_ARG(inBusNumber),
                        UInt32 ARA_MAYBE_UNUSED_ARG(inNumberFrames), AudioBufferList * __nullable ioData)
{
    for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
        memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);

    return noErr;
}

void AudioUnitStartRendering(AudioUnit audioUnit, UInt32 blockSize, double sampleRate)
{
    UInt32 bufferSize = blockSize;
    OSStatus ARA_MAYBE_UNUSED_VAR(status) = noErr;
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &bufferSize, sizeof(bufferSize));
    ARA_INTERNAL_ASSERT(status == noErr);

    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, sizeof(sampleRate));
    ARA_INTERNAL_ASSERT(status == noErr);

    AudioStreamBasicDescription streamDesc = { sampleRate, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,
                                                sizeof(float), 1, sizeof(float), 1, sizeof(float) * 8, 0 };
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamDesc, sizeof(streamDesc));
    ARA_INTERNAL_ASSERT(status == noErr);
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &streamDesc, sizeof(streamDesc));
    ARA_INTERNAL_ASSERT(status == noErr);

    AURenderCallbackStruct callback = { RenderCallback, NULL };
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Output, 0, &callback, sizeof(callback));
    ARA_INTERNAL_ASSERT(status == noErr);

    HostCallbackInfo callbacks = { NULL, NULL, NULL, &GetTransportState1, &GetTransportState2 };
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_HostCallbacks, kAudioUnitScope_Global, 0, &callbacks, sizeof(callbacks));
    ARA_INTERNAL_ASSERT(status == noErr);

    status = AudioUnitInitialize(audioUnit);
    ARA_INTERNAL_ASSERT(status == noErr);
}

void AudioUnitRenderBuffer(AudioUnit audioUnit, UInt32 blockSize, SInt64 samplePosition, float * buffer)
{
    AudioUnitRenderActionFlags flags = 0;

    AudioTimeStamp timeStamp;
    memset(&timeStamp, 0, sizeof(timeStamp));
    timeStamp.mSampleTime = (double)samplePosition;
    timeStamp.mFlags = kAudioTimeStampSampleTimeValid;

    UInt32 bufferSize = blockSize;

    AudioBufferList audioBufferList = { 1, { { 1, (UInt32)(sizeof(float) * bufferSize), buffer } } };

    globalSampleIndex = (double)samplePosition;

    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitRender(audioUnit, &flags, &timeStamp, 0, bufferSize, &audioBufferList);
    ARA_INTERNAL_ASSERT(status == noErr);
}

void AudioUnitStopRendering(AudioUnit audioUnit)
{
    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitUninitialize(audioUnit);
    ARA_INTERNAL_ASSERT(status == noErr);
}

void AudioUnitClose(AudioUnit audioUnit)
{
    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioComponentInstanceDispose(audioUnit);
    ARA_INTERNAL_ASSERT(status == noErr);
}
