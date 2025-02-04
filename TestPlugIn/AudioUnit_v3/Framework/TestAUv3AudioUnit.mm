//------------------------------------------------------------------------------
//! \file       TestAUv3AudioUnit.m
//!             Audio Unit App Extension implementation,
//!             created via the Xcode 11 project template for Audio Unit App Extensions.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2025, Celemony Software GmbH, All Rights Reserved.
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

#import "TestAUv3AudioUnit.h"
#import "TestAUv3DSPKernel.hpp"
#import "BufferedAudioBus.hpp"

#import "ARATestDocumentController.h"

#import "ARA_Library/IPC/ARAIPCAudioUnit_v3.h"
#import "ARA_Library/IPC/ARAIPCProxyHost.h"


#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE

// \todo this class could be a reusable part of ARA_Library if we could figure out how to avoid the
//       ObjC class name clashes that will arise when multiple plug-ins from different binaries are
//       using/linking this class.
//       The IPC-related additions to TestAUv3AudioUnit could also be moved to a reusable base class
//       from which TestAUv3AudioUnit and others could derive.
//       A workaround might be a set of macros that can be used to provide the code with adjusted
//       class names for client projects, but since some methods may be implemented in the client
//       this is somewhat messy.

API_AVAILABLE(macos(13.0), ios(16.0))
@interface TestAUv3ARAIPCMessageChannel : NSObject<AUMessageChannel>
@end


@implementation TestAUv3ARAIPCMessageChannel {
    ARA::IPC::ARAIPCMessageChannelRef _messageChannelRef;
}

@synthesize callHostBlock = _callHostBlock;

- (instancetype)initAsMainThreadChannel:(BOOL) isMainThreadChannel {
    self = [super init];

    if (self == nil) { return nil; }

    _callHostBlock = nil;
    _messageChannelRef = ARA::IPC::ARAIPCAUProxyHostInitializeMessageChannel(self, isMainThreadChannel);

    return self;
}

- (void)dealloc {
    ARA::IPC::ARAIPCAUProxyHostUninitializeMessageChannel(_messageChannelRef);
}

- (NSDictionary * _Nonnull)callAudioUnit:(NSDictionary *)message {
    return ARA::IPC::ARAIPCAUProxyHostCommandHandler(_messageChannelRef, message);
}

@end

#endif // ARA_AUDIOUNITV3_IPC_IS_AVAILABLE


@interface TestAUv3AudioUnit ()

@property AUAudioUnitBusArray *inputBusArray;
@property AUAudioUnitBusArray *outputBusArray;
@property (nonatomic, readonly) AUAudioUnitBus *outputBus;

@end


@implementation TestAUv3AudioUnit {
    // C++ members need to be ivars; they would be copied on access if they were properties.
    BufferedInputBus _inputBus;
    TestAUv3DSPKernel  _kernel;
    ARA::PlugIn::PlugInExtension * _araPlugInExtension;
}

@synthesize araFactory = _araFactory;
#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
@synthesize araRemoteInstanceRef = _araRemoteInstanceRef;
#endif

// MARK: -  AUAudioUnit Overrides

- (instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription options:(AudioComponentInstantiationOptions)options error:(NSError **)outError {
    self = [super initWithComponentDescription:componentDescription options:options error:outError];
    
    if (self == nil) { return nil; }

    AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:2];

    // Create a DSP kernel to handle the signal processing.
    _kernel.init(format.channelCount, format.sampleRate);
    super.maximumFramesToRender = _kernel.maximumFramesToRender();

    // Create the input and output busses.
    _inputBus.init(format, 8);
    _outputBus = [[AUAudioUnitBus alloc] initWithFormat:format error:nil];
    _outputBus.maximumChannelCount = 8;

    // Create the input and output bus arrays.
    _inputBusArray  = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                                             busType:AUAudioUnitBusTypeInput
                                                              busses: @[_inputBus.bus]];
    _outputBusArray = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                                             busType:AUAudioUnitBusTypeOutput
                                                              busses: @[self.outputBus]];

    // Initialize ARA data
    _araFactory = ARATestDocumentController::getARAFactory();

    _araPlugInExtension = nullptr;

#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE
    static_assert(sizeof(self) == sizeof(NSUInteger), "opaque ref type size mismatch");
    _araRemoteInstanceRef = (NSUInteger)self;
#endif

    return self;
}

- (void)dealloc {
    if (_araPlugInExtension)
    {
        delete _araPlugInExtension;
        _araPlugInExtension = nullptr;
    }
}


// If an audio unit has input, an audio unit's audio input connection points.
// Subclassers must override this property getter and should return the same object every time.
// See sample code.
- (AUAudioUnitBusArray *)inputBusses {
    return _inputBusArray;
}

// An audio unit's audio output connection points.
// Subclassers must override this property getter and should return the same object every time.
// See sample code.
- (AUAudioUnitBusArray *)outputBusses {
    return _outputBusArray;
}

// Allocate resources required to render.
// Subclassers should call the superclass implementation.
- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)outError {
    if (self.outputBus.format.channelCount != _inputBus.bus.format.channelCount) {
        if (outError) {
            *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:kAudioUnitErr_FailedInitialization userInfo:nil];
        }
        // Notify superclass that initialization was not successful
        self.renderResourcesAllocated = NO;

        return NO;
    }

    [super allocateRenderResourcesAndReturnError:outError];

    _inputBus.allocateRenderResources(self.maximumFramesToRender);

    _kernel.init(self.outputBus.format.channelCount, self.outputBus.format.sampleRate);
    _kernel.setMaximumFramesToRender(self.maximumFramesToRender);
    _kernel.setTransportStateBlock(self.transportStateBlock);
    _kernel.enableRendering();

    return YES;
}

// Deallocate resources allocated in allocateRenderResourcesAndReturnError:
// Subclassers should call the superclass implementation.
- (void)deallocateRenderResources {
    _kernel.disableRendering();

    _inputBus.deallocateRenderResources();

    [super deallocateRenderResources];
}

// MARK: -  ARAAudioUnit protocol conformance

- (nonnull const ARA::ARAPlugInExtensionInstance *)bindToDocumentController:(nonnull ARA::ARADocumentControllerRef)documentControllerRef
                        withRoles:(ARA::ARAPlugInInstanceRoleFlags)assignedRoles knownRoles:(ARA::ARAPlugInInstanceRoleFlags)knownRoles {
    _araPlugInExtension = new ARA::PlugIn::PlugInExtension;
    _kernel.setARAPlugInExtension(_araPlugInExtension);
    return _araPlugInExtension->bindToARA(documentControllerRef, knownRoles, assignedRoles);
}

// MARK: -  AUAudioUnit (AUAudioUnitImplementation)

// Subclassers must provide a AUInternalRenderBlock (via a getter) to implement rendering.
- (AUInternalRenderBlock)internalRenderBlock {
    /*
     Capture in locals to avoid ObjC member lookups. If "self" is captured in
     render, we're doing it wrong.
     */
    // Specify captured objects are mutable.
    __block TestAUv3DSPKernel *state = &_kernel;
    __block BufferedInputBus *input = &_inputBus;

    return ^AUAudioUnitStatus(AudioUnitRenderActionFlags *actionFlags,
                              const AudioTimeStamp       *timestamp,
                              AVAudioFrameCount           frameCount,
                              NSInteger                   outputBusNumber,
                              AudioBufferList            *outputData,
                              const AURenderEvent        *realtimeEventListHead,
                              AURenderPullInputBlock      pullInputBlock) {

        AudioUnitRenderActionFlags pullFlags = 0;

        if (frameCount > state->maximumFramesToRender()) {
            return kAudioUnitErr_TooManyFramesToProcess;
        }

        AUAudioUnitStatus err = input->pullInput(&pullFlags, timestamp, frameCount, 0, pullInputBlock);

        if (err != noErr) { return err; }

        AudioBufferList *inAudioBufferList = input->mutableAudioBufferList;

        /*
         Important:
         If the caller passed non-null output pointers (outputData->mBuffers[x].mData), use those.

         If the caller passed null output buffer pointers, process in memory owned by the Audio Unit
         and modify the (outputData->mBuffers[x].mData) pointers to point to this owned memory.
         The Audio Unit is responsible for preserving the validity of this memory until the next call to render,
         or deallocateRenderResources is called.

         If your algorithm cannot process in-place, you will need to preallocate an output buffer
         and use it here.

         See the description of the canProcessInPlace property.
         */

        // If passed null output buffer pointers, process in-place in the input buffer.
        AudioBufferList *outAudioBufferList = outputData;
        if (outAudioBufferList->mBuffers[0].mData == nullptr) {
            for (UInt32 i = 0; i < outAudioBufferList->mNumberBuffers; ++i) {
                outAudioBufferList->mBuffers[i].mData = inAudioBufferList->mBuffers[i].mData;
            }
        }

        state->setBuffers(inAudioBufferList, outAudioBufferList);
        state->processWithEvents(timestamp, frameCount, realtimeEventListHead, nil /* MIDIOutEventBlock */);

        return noErr;
    };
}


#if ARA_AUDIOUNITV3_IPC_IS_AVAILABLE

NSObject<AUMessageChannel> * __strong _mainMessageChannel API_AVAILABLE(macos(13.0), ios(16.0)) = nil;
NSObject<AUMessageChannel> * __strong _otherMessageChannel API_AVAILABLE(macos(13.0), ios(16.0)) = nil;

void createSharedMessageChannelsIfNeeded() API_AVAILABLE(macos(13.0), ios(16.0)) {
    if (_mainMessageChannel)
        return;

    ARA::IPC::ARAIPCProxyHostAddFactory(ARATestDocumentController::getARAFactory());

    _mainMessageChannel = [[TestAUv3ARAIPCMessageChannel alloc] initAsMainThreadChannel:YES];
    _otherMessageChannel = [[TestAUv3ARAIPCMessageChannel alloc] initAsMainThreadChannel:NO];
}

__attribute__((destructor))
void destroySharedMessageChannelsIfNeeded() {
    if (@available(macOS 13.0, iOS 16.0, *))
    {
        if (_mainMessageChannel)
            ARA::IPC::ARAIPCAUProxyHostUninitialize();

        _mainMessageChannel = nil;
        _otherMessageChannel = nil;
    }
}

// \todo the return value should be _Nullable!
- (id<AUMessageChannel> _Nonnull)messageChannelFor:(NSString * _Nonnull)channelName {
    if (@available(macOS 13.0, iOS 16.0, *))
    {
        if ([channelName isEqualToString:ARA_AUDIOUNIT_MAIN_THREAD_MESSAGES_UTI])
        {
            createSharedMessageChannelsIfNeeded();
            return _mainMessageChannel;
        }
        if ([channelName isEqualToString:ARA_AUDIOUNIT_OTHER_THREADS_MESSAGES_UTI])
        {
            createSharedMessageChannelsIfNeeded();
            return _otherMessageChannel;
        }
    }
    return nil;
}

#endif // ARA_AUDIOUNITV3_IPC_IS_AVAILABLE

@end
