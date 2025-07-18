//------------------------------------------------------------------------------
//! \file       AudioUnitLoader.m
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

#include "AudioUnitLoader.h"

#if !defined(__APPLE__)
    #error "this file can only be complied for Apple platforms"
#endif

#include <AudioUnit/AudioUnit.h>

#import <AVFoundation/AVFoundation.h>

#include "ARA_API/ARAAudioUnit.h"
#include "ARA_API/ARAAudioUnit_v3.h"
#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/IPC/ARAIPCAudioUnit_v3.h"
#include "ARA_Library/IPC/ARAIPCProxyPlugIn.h"


#if defined (__GNUC__)
    _Pragma ("GCC diagnostic push")
    _Pragma ("GCC diagnostic ignored \"-Wunguarded-availability\"")
#endif

struct _AudioUnitComponent
{
    AudioComponent component;
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    ARAIPCConnectionRef araProxy;
#endif
};

#if defined (__GNUC__)
    _Pragma ("GCC diagnostic pop")
#endif


struct _AudioUnitInstance
{
    AudioUnitComponent audioUnitComponent;
    BOOL isAUv2;
    union
    {
        AudioUnit v2AudioUnit;
        AUAudioUnit * __strong v3AudioUnit;
    };
    SInt64 samplePosition;
    AURenderBlock v3RenderBlock;                    // only for AUv3: cache of render block outside ObjC runtime
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    BOOL isOutOfProcess;                            // loaded in- or out-of-process
    const ARAPlugInExtensionInstance * ipcInstance; // != NULL when bound to ARA via IPC
#endif
};


AudioUnitComponent AudioUnitPrepareComponentWithIDs(OSType type, OSType subtype, OSType manufacturer)
{
    AudioUnitComponent result = malloc(sizeof(struct _AudioUnitComponent));
    result->component = NULL;
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    result->araProxy = NULL;
#endif

    AudioComponentDescription compDesc = { type, subtype, manufacturer, 0, 0 };
    if (@available(macOS 10.10, *))
    {
        @autoreleasepool
        {
// debug code: list all Audio Units that support ARA v2.0
//          [[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsPassingTest:^BOOL (AVAudioUnitComponent *comp, BOOL *stop)
//          {
//              if ([[comp allTagNames] indexOfObject:@kARAAudioComponentTag] != NSNotFound)
//                  NSLog(@"%@ %@ v%@ (%@, %@sandbox-safe) %@", [comp manufacturerName], [comp name], [comp versionString], [comp typeName], [comp isSandboxSafe] ? @"" : @"not ", [comp iconURL]);
//              *stop = NO;
//              return NO;
//          }];

            AVAudioUnitComponent * avComponent = [[[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsMatchingDescription:compDesc] firstObject];
            if (avComponent)
            {
                // ensure we pass auval
                ARA_VALIDATE_API_CONDITION([avComponent passesAUVal]);

                result->component = [avComponent audioComponent];

                // validate we provide a proper icon
                if (@available(macOS 11.0, *))
                {
                    NSImage * icon = AudioComponentCopyIcon(result->component);
                    ARA_INTERNAL_ASSERT(icon != nil);
                    [(NSObject *)icon release];     // cast is necessary because NSImage is only forward-declared here
                }

                if (([avComponent audioComponentDescription].componentFlags & kAudioComponentFlag_IsV3AudioUnit) != 0)
                {
                    // when migrating to Audio Unit v3, plug-ins should also adopt App Sandbox safety
                    ARA_VALIDATE_API_CONDITION([avComponent isSandboxSafe]);
                }
                else
                {
#if ARA_CPU_ARM
                    // on ARM machines, adopting Audio Unit v3 is required for proper ARA support due to out-of-process operation
                    ARA_WARN("This Audio Unit has not yet been updated to version 3 of the Audio Unit API, and therefore cannot use ARA when loaded out-of-process.");
#endif
                }
            }
        }
    }
    else
    {
        result->component = AudioComponentFindNext(NULL, &compDesc);
    }
    ARA_INTERNAL_ASSERT(result->component);
    return result;
}

bool AudioUnitIsV2(AudioUnitComponent audioUnitComponent)
{
    AudioComponentDescription desc;
    AudioComponentGetDescription(audioUnitComponent->component, &desc);
    return ((desc.componentFlags & kAudioComponentFlag_IsV3AudioUnit) == 0);
}

AudioUnitInstance AudioUnitOpenInstance(AudioUnitComponent audioUnitComponent, bool useIPC)
{
    __block AudioUnitInstance result = malloc(sizeof(struct _AudioUnitInstance));
    result->audioUnitComponent = audioUnitComponent;
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    result->isOutOfProcess = useIPC;
    result->ipcInstance = NULL;
#endif

    AudioComponentDescription desc;
    AudioComponentGetDescription(audioUnitComponent->component, &desc);
    result->isAUv2 = AudioUnitIsV2(audioUnitComponent);

    if (result->isAUv2)
    {
        ARA_INTERNAL_ASSERT(!useIPC && "using is IPC not supported for AUv2");
        ARA_INTERNAL_ASSERT((desc.componentFlags & kAudioComponentFlag_RequiresAsyncInstantiation) == 0);
//      \todo for some reason, the OS never sets this flag for v2 AUs, even though they naturally all support it.
//      ARA_INTERNAL_ASSERT((desc.componentFlags & kAudioComponentFlag_CanLoadInProcess) != 0);
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioComponentInstanceNew(audioUnitComponent->component, &result->v2AudioUnit);
        ARA_INTERNAL_ASSERT(status == noErr);
        ARA_INTERNAL_ASSERT(result->v2AudioUnit != NULL);
    }
    else
    {
        // to allow for best performance esp. when reading audio samples through the ARA audio readers,
        // ARA-capable Audio Units should be packaged to support in-process loading
        ARA_INTERNAL_ASSERT((desc.componentFlags & kAudioComponentFlag_CanLoadInProcess) != 0);
        result->v3AudioUnit = nil;

#if !ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
        ARA_INTERNAL_ASSERT(!useIPC && "using IPC requires ARA_AUDIOUNITV3_IPC_IS_AVAILABLE");
#endif

        @autoreleasepool
        {
            // simply blocking the thread is not allowed here, so we need to add proper NSRunLoop around the instantiation process
            NSRunLoop * runloop = [NSRunLoop currentRunLoop];
            [runloop performBlock:^void (void)
            {
                const AudioComponentInstantiationOptions options =
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
                                                                   (useIPC) ? kAudioComponentInstantiation_LoadOutOfProcess :
#endif
                                                                              kAudioComponentInstantiation_LoadInProcess;

                [AUAudioUnit instantiateWithComponentDescription:desc options:options
                             completionHandler:^void (AUAudioUnit * _Nullable auAudioUnit, NSError * _Nullable ARA_MAYBE_UNUSED_ARG(error))
                {
                    ARA_INTERNAL_ASSERT(auAudioUnit != nil);
                    ARA_INTERNAL_ASSERT(error == nil);
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
                    if (@available(macOS 10.15, *))
                        ARA_INTERNAL_ASSERT([auAudioUnit isLoadedInProcess] == !useIPC);
#endif
                    result->v3AudioUnit = [auAudioUnit retain];
                }];
            }];
            // loading out-of-process can take a considerable amount of time, so loop for up to a second if needed
            for (int i = 0; (i < 100) && (result->v3AudioUnit == nil); ++i)
                [runloop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        }
        ARA_INTERNAL_ASSERT(result->v3AudioUnit != nil);
    }
    return result;
}

const ARAFactory * AudioUnitGetARAFactory(AudioUnitInstance audioUnitInstance, ARAIPCConnectionRef * connectionRef)
{
    const ARAFactory * result = NULL;    // initially assume this plug-in doesn't support ARA
    *connectionRef = NULL;

    // check whether the AU supports ARA by trying to get the factory
    if (audioUnitInstance->isAUv2)
    {
        UInt32 propertySize = sizeof(ARAAudioUnitFactory);
        ARAAudioUnitFactory audioUnitFactory = { kARAAudioUnitMagic, NULL };
        Boolean isWriteable = FALSE;

        OSStatus status = AudioUnitGetPropertyInfo(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_ARAFactory, kAudioUnitScope_Global, 0, &propertySize, &isWriteable);
        if ((status == noErr) &&
            (propertySize == sizeof(ARAAudioUnitFactory)) &&
            !isWriteable)
        {
            status = AudioUnitGetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_ARAFactory, kAudioUnitScope_Global, 0, &audioUnitFactory, &propertySize);
            if ((status == noErr) &&
                (propertySize == sizeof(ARAAudioUnitFactory)) &&
                (audioUnitFactory.inOutMagicNumber == kARAAudioUnitMagic))
            {
                ARA_VALIDATE_API_CONDITION(audioUnitFactory.outFactory != NULL);
                result = audioUnitFactory.outFactory;
            }
        }
    }
    else
    {
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
        if (audioUnitInstance->isOutOfProcess)
        {
            if (@available(macOS 13.0, *))
            {
                if (!audioUnitInstance->audioUnitComponent->araProxy)
                    audioUnitInstance->audioUnitComponent->araProxy = ARAIPCAUProxyPlugInInitialize(audioUnitInstance->v3AudioUnit);
                if (audioUnitInstance->audioUnitComponent->araProxy)
                {
                    *connectionRef = audioUnitInstance->audioUnitComponent->araProxy;
     
                    ARA_VALIDATE_API_CONDITION(ARAIPCProxyPlugInGetFactoriesCount(*connectionRef) == 1);
                    result = ARAIPCProxyPlugInGetFactoryAtIndex (*connectionRef, 0);
                    ARA_VALIDATE_API_CONDITION(result != NULL);
                }
            }
        }
        else
#endif
        {
            if ([audioUnitInstance->v3AudioUnit conformsToProtocol:@protocol(ARAAudioUnit)])
            {
                result = [(AUAudioUnit<ARAAudioUnit> *)audioUnitInstance->v3AudioUnit araFactory];
                ARA_VALIDATE_API_CONDITION(result != NULL);
            }
        }
    }

    // validate the AU is properly tagged as ARA (if it supports ARA)
#if ARA_VALIDATE_API_CALLS
    if (result)
    {
        if (@available(macOS 10.10, *))
        {
            @autoreleasepool
            {
                AudioComponentDescription desc;
                OSStatus status = AudioComponentGetDescription(audioUnitInstance->audioUnitComponent->component, &desc);
                ARA_INTERNAL_ASSERT(status == noErr);
                AVAudioUnitComponent * avComponent = [[[AVAudioUnitComponentManager sharedAudioUnitComponentManager] componentsMatchingDescription:desc] firstObject];
                ARA_INTERNAL_ASSERT(avComponent);
                ARA_VALIDATE_API_CONDITION([[avComponent allTagNames] indexOfObject:@kARAAudioComponentTag] != NSNotFound);
            }
        }
    }
#endif

    return result;
}

const ARAPlugInExtensionInstance * AudioUnitBindToARADocumentController(AudioUnitInstance audioUnitInstance, ARADocumentControllerRef controllerRef, ARAPlugInInstanceRoleFlags assignedRoles)
{
    const ARAPlugInInstanceRoleFlags knownRoles = kARAPlaybackRendererRole | kARAEditorRendererRole | kARAEditorViewRole;
    ARA_INTERNAL_ASSERT((assignedRoles | knownRoles) == knownRoles);
    if (audioUnitInstance->isAUv2)
    {
        UInt32 propertySize = sizeof(ARAAudioUnitPlugInExtensionBinding);
        UInt32 ARA_MAYBE_UNUSED_VAR(expectedPropertySize) = propertySize;
        ARAAudioUnitPlugInExtensionBinding audioUnitBinding = { kARAAudioUnitMagic, controllerRef, NULL, knownRoles, assignedRoles };

        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitGetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_ARAPlugInExtensionBindingWithRoles, kAudioUnitScope_Global, 0, &audioUnitBinding, &propertySize);
#if defined(ARA_SUPPORT_VERSION_1) && (ARA_SUPPORT_VERSION_1)
        if (status != noErr)
        {
            propertySize = offsetof(ARAAudioUnitPlugInExtensionBinding, knownRoles);
            expectedPropertySize = propertySize;
            status = AudioUnitGetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_ARAPlugInExtensionBinding, kAudioUnitScope_Global, 0, &audioUnitBinding, &propertySize);
        }
#endif

        ARA_VALIDATE_API_CONDITION(status == noErr);
        ARA_VALIDATE_API_CONDITION(propertySize == expectedPropertySize);
        ARA_VALIDATE_API_CONDITION(audioUnitBinding.inOutMagicNumber == kARAAudioUnitMagic);
        ARA_VALIDATE_API_CONDITION(audioUnitBinding.inDocumentControllerRef == controllerRef);
        ARA_VALIDATE_API_CONDITION(audioUnitBinding.outPlugInExtension != NULL);

        return audioUnitBinding.outPlugInExtension;
    }
    else
    {
        const ARAPlugInExtensionInstance * instance = NULL;
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
        if (audioUnitInstance->isOutOfProcess)
        {
            if (@available(macOS 13.0, *))
            {
                instance = ARAIPCAUProxyPlugInBindToDocumentController(audioUnitInstance->v3AudioUnit, controllerRef, knownRoles, assignedRoles);
                audioUnitInstance->ipcInstance = instance;
            }
        }
        else
#endif
        {
            ARA_INTERNAL_ASSERT([audioUnitInstance->v3AudioUnit conformsToProtocol:@protocol(ARAAudioUnit)]);
            instance = [(AUAudioUnit<ARAAudioUnit> *)audioUnitInstance->v3AudioUnit bindToDocumentController:controllerRef withRoles:assignedRoles knownRoles:knownRoles];
        }
        ARA_VALIDATE_API_CONDITION(instance != NULL);
        return instance;
    }
}


// in order for Melodyne to render the ARA data, it must be set to playback mode (in stop, its built-in pre-listening logic is active)
// thus we implement some crude, minimal transport information here.

OSStatus GetTransportState2(void * inHostUserData, Boolean * outIsPlaying, Boolean * outIsRecording,
                            Boolean * outTransportStateChanged, Float64 * outCurrentSampleInTimeLine,
                            Boolean * outIsCycling, Float64 * outCycleStartBeat, Float64 * outCycleEndBeat)
{
    AudioUnitInstance instance = inHostUserData;

    if (outIsPlaying)
        *outIsPlaying = true;
    if (outIsRecording)
        *outIsRecording = false;
    if (outTransportStateChanged)
        *outTransportStateChanged = (instance->samplePosition == 0) ? true : false;
    if (outCurrentSampleInTimeLine)
        *outCurrentSampleInTimeLine = (double)instance->samplePosition;

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
                        UInt32 ARA_MAYBE_UNUSED_ARG(inNumberFrames), AudioBufferList * _Nullable ioData)
{
    for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
        memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);

    return noErr;
}

void ConfigureBusses(AudioUnit audioUnit, AudioUnitScope inScope, double sampleRate)
{
    UInt32 busCount = 1;
    Boolean outWriteable = NO;
    UInt32 propertySize = 0;
    OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitGetPropertyInfo(audioUnit, kAudioUnitProperty_BusCount, inScope, 0, &propertySize, &outWriteable);
    ARA_INTERNAL_ASSERT(status == noErr);
    ARA_INTERNAL_ASSERT(propertySize == sizeof(busCount));
    status = AudioUnitGetProperty(audioUnit, kAudioUnitProperty_BusCount, inScope, 0, &busCount, &propertySize);
    ARA_INTERNAL_ASSERT(status == noErr);
    ARA_INTERNAL_ASSERT(propertySize == sizeof(busCount));

    if (outWriteable)
    {
        if (busCount != 1)
        {
            status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_BusCount, inScope, 0, &busCount, sizeof(busCount));
            ARA_INTERNAL_ASSERT(status == noErr);
        }
    }
    else
    {
        ARA_INTERNAL_ASSERT(busCount >= 1);
    }

    AudioStreamBasicDescription streamDesc = { sampleRate, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,
                                               sizeof(float), 1, sizeof(float), 1, sizeof(float) * 8, 0 };
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, inScope, 0, &streamDesc, sizeof(streamDesc));
    ARA_INTERNAL_ASSERT(status == noErr);

    UInt32 shouldAllocate = YES;                // note that proper hosts are likely providing their own buffer and set this to NO to minimize allocations!
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_ShouldAllocateBuffer, inScope, 0, &shouldAllocate, sizeof(shouldAllocate));
}

void ConfigureBussesArray(AUAudioUnitBusArray * bussesArray, double sampleRate)
{
    if (bussesArray.countChangeable)
    {
        if (bussesArray.count != 1)
        {
            BOOL ARA_MAYBE_UNUSED_VAR(success) = [bussesArray setBusCount:1 error:nil];
            ARA_INTERNAL_ASSERT(success);
        }
    }
    else
    {
        ARA_INTERNAL_ASSERT(bussesArray.count >= 1);
    }

    AudioStreamBasicDescription streamDesc = { sampleRate, kAudioFormatLinearPCM, kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,
                                               sizeof(float), 1, sizeof(float), 1, sizeof(float) * 8, 0 };

    AVAudioFormat * format = [[AVAudioFormat alloc] initWithStreamDescription:&streamDesc];
    BOOL ARA_MAYBE_UNUSED_VAR(success) = [bussesArray[0] setFormat:format error:nil];
    ARA_INTERNAL_ASSERT(success);
    [format release];

    bussesArray[0].shouldAllocateBuffer = YES;  // note that proper hosts are likely providing their own buffer and set this to NO to minimize allocations!
}

void AudioUnitStartRendering(AudioUnitInstance audioUnitInstance, UInt32 maxBlockSize, double sampleRate)
{
    if (audioUnitInstance->isAUv2)
    {
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitSetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxBlockSize, sizeof(maxBlockSize));
        ARA_INTERNAL_ASSERT(status == noErr);

        status = AudioUnitSetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Global, 0, &sampleRate, sizeof(sampleRate));
        ARA_INTERNAL_ASSERT(status == noErr);

        ConfigureBusses(audioUnitInstance->v2AudioUnit, kAudioUnitScope_Input, sampleRate);
        ConfigureBusses(audioUnitInstance->v2AudioUnit, kAudioUnitScope_Output, sampleRate);

        AURenderCallbackStruct callback = { RenderCallback, NULL };
        status = AudioUnitSetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Output, 0, &callback, sizeof(callback));
        ARA_INTERNAL_ASSERT(status == noErr);

        HostCallbackInfo callbacks = { audioUnitInstance, NULL, NULL, &GetTransportState1, &GetTransportState2 };
        status = AudioUnitSetProperty(audioUnitInstance->v2AudioUnit, kAudioUnitProperty_HostCallbacks, kAudioUnitScope_Global, 0, &callbacks, sizeof(callbacks));
        ARA_INTERNAL_ASSERT(status == noErr);

        status = AudioUnitInitialize(audioUnitInstance->v2AudioUnit);
        ARA_INTERNAL_ASSERT(status == noErr);
    }
    else
    {
        audioUnitInstance->v3AudioUnit.maximumFramesToRender = maxBlockSize;

        ConfigureBussesArray(audioUnitInstance->v3AudioUnit.inputBusses, sampleRate);
        ConfigureBussesArray(audioUnitInstance->v3AudioUnit.outputBusses, sampleRate);

        audioUnitInstance->v3AudioUnit.transportStateBlock =
            ^BOOL (AUHostTransportStateFlags * _Nullable transportStateFlags, double * _Nullable currentSamplePosition,
                   double * _Nullable cycleStartBeatPosition, double * _Nullable cycleEndBeatPosition)
            {
                Boolean outIsPlaying = NO, outIsRecording = NO, outTransportStateChanged = NO, outIsCycling = NO;
                Boolean checkTransport = (transportStateFlags != NULL);
                OSStatus result =  GetTransportState2(audioUnitInstance, (checkTransport) ? &outIsPlaying : NULL, (checkTransport) ? &outIsRecording : NULL,
                                                      (checkTransport) ? &outTransportStateChanged : NULL, currentSamplePosition,
                                                      (checkTransport) ? &outIsCycling : NULL, cycleStartBeatPosition, cycleEndBeatPosition);
                if (checkTransport)
                    *transportStateFlags = ((outTransportStateChanged) ? AUHostTransportStateChanged : 0) +
                                           ((outIsPlaying) ? AUHostTransportStateMoving : 0) +
                                           ((outIsRecording) ? AUHostTransportStateRecording : 0) +
                                           ((outIsCycling) ? AUHostTransportStateCycling : 0);
                return (result == noErr);
            };

        ARA_MAYBE_UNUSED_VAR(BOOL) success = [audioUnitInstance->v3AudioUnit allocateRenderResourcesAndReturnError:nil];
        ARA_INTERNAL_ASSERT(success == YES);

        audioUnitInstance->v3RenderBlock = [[audioUnitInstance->v3AudioUnit renderBlock] retain];
    }
}

void AudioUnitRenderBuffer(AudioUnitInstance audioUnitInstance, UInt32 blockSize, SInt64 samplePosition, float * buffer)
{
    audioUnitInstance->samplePosition = samplePosition;

    AudioUnitRenderActionFlags flags = 0;

    AudioTimeStamp timeStamp;
    memset(&timeStamp, 0, sizeof(timeStamp));
    timeStamp.mSampleTime = (double)samplePosition;
    timeStamp.mFlags = kAudioTimeStampSampleTimeValid;

    AudioBufferList audioBufferList = { 1, { { 1, (UInt32)(sizeof(float) * blockSize), buffer } } };

    if (audioUnitInstance->isAUv2)
    {
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitRender(audioUnitInstance->v2AudioUnit, &flags, &timeStamp, 0, blockSize, &audioBufferList);
        ARA_INTERNAL_ASSERT(status == noErr);
    }
    else
    {
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = audioUnitInstance->v3RenderBlock(&flags, &timeStamp, blockSize, 0, &audioBufferList,
            ^AUAudioUnitStatus (AudioUnitRenderActionFlags *actionFlags, const AudioTimeStamp *timestamp,
                                AUAudioFrameCount frameCount, NSInteger inputBusNumber, AudioBufferList *inputData)
            {
                return RenderCallback(NULL, actionFlags, timestamp, (UInt32)inputBusNumber, frameCount, inputData);
            });
        ARA_INTERNAL_ASSERT(status == noErr);
    }
}

void AudioUnitStopRendering(AudioUnitInstance audioUnitInstance)
{
    if (audioUnitInstance->isAUv2)
    {
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioUnitUninitialize(audioUnitInstance->v2AudioUnit);
        ARA_INTERNAL_ASSERT(status == noErr);
    }
    else
    {
        [audioUnitInstance->v3AudioUnit deallocateRenderResources];
        [audioUnitInstance->v3RenderBlock release];
    }
}

void AudioUnitCloseInstance(AudioUnitInstance audioUnitInstance)
{
    if (audioUnitInstance->isAUv2)
    {
        OSStatus ARA_MAYBE_UNUSED_VAR(status) = AudioComponentInstanceDispose(audioUnitInstance->v2AudioUnit);
        ARA_INTERNAL_ASSERT(status == noErr);
    }
    else
    {
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
        if (@available(macOS 13.0, *))
        {
            if (audioUnitInstance->ipcInstance)
                ARAIPCProxyPlugInCleanupBinding(audioUnitInstance->ipcInstance);
        }
#endif
        [audioUnitInstance->v3AudioUnit release];
    }
    free(audioUnitInstance);
}

void AudioUnitCleanupComponent(AudioUnitComponent audioUnitComponent)
{
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    if (@available(macOS 13.0, *))
    {
        if (audioUnitComponent->araProxy)
            ARAIPCAUProxyPlugInUninitialize(audioUnitComponent->araProxy);
    }
#endif

    free(audioUnitComponent);
    // Explicit unloading is not supported by the Audio Unit API.
}
