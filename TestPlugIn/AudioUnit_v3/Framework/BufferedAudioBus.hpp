//------------------------------------------------------------------------------
//! \file       BufferedAudioBus.hpp
//!             Audio Unit App Extension helper class,
//!             created via the Xcode 11 project template for Audio Unit App Extensions.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2023, Celemony Software GmbH, All Rights Reserved.
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

#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <AVFoundation/AVFoundation.h>

//MARK:- BufferedAudioBus Utility Class
// Utility classes to manage audio formats and buffers for an audio unit implementation's input and output audio busses.

// Reusable non-ObjC class, accessible from render thread.
struct BufferedAudioBus {
    AUAudioUnitBus* bus = nullptr;
    AUAudioFrameCount maxFrames = 0;
    
    AVAudioPCMBuffer* pcmBuffer = nullptr;
    
    AudioBufferList const* originalAudioBufferList = nullptr;
    AudioBufferList* mutableAudioBufferList = nullptr;

    void init(AVAudioFormat* defaultFormat, AVAudioChannelCount maxChannels) {
        maxFrames = 0;
        pcmBuffer = nullptr;
        originalAudioBufferList = nullptr;
        mutableAudioBufferList = nullptr;

        bus = [[AUAudioUnitBus alloc] initWithFormat:defaultFormat error:nil];

        bus.maximumChannelCount = maxChannels;
    }

    void allocateRenderResources(AUAudioFrameCount inMaxFrames) {
        maxFrames = inMaxFrames;

        pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:bus.format frameCapacity: maxFrames];

        originalAudioBufferList = pcmBuffer.audioBufferList;
        mutableAudioBufferList = pcmBuffer.mutableAudioBufferList;
    }
    
    void deallocateRenderResources() {
        pcmBuffer = nullptr;
        originalAudioBufferList = nullptr;
        mutableAudioBufferList = nullptr;
    }
};

// MARK:-  BufferedOutputBus: BufferedAudioBus
// MARK: prepareOutputBufferList()
/*
 BufferedOutputBus
 
 This class provides a prepareOutputBufferList method to copy the internal buffer pointers
 to the output buffer list in case the client passed in null buffer pointers.
 */
struct BufferedOutputBus: BufferedAudioBus {
    void prepareOutputBufferList(AudioBufferList* outBufferList, AVAudioFrameCount frameCount, bool zeroFill) {
        UInt32 byteSize = frameCount * sizeof(float);
        for (UInt32 i = 0; i < outBufferList->mNumberBuffers; ++i) {
            outBufferList->mBuffers[i].mNumberChannels = originalAudioBufferList->mBuffers[i].mNumberChannels;
            outBufferList->mBuffers[i].mDataByteSize = byteSize;
            if (outBufferList->mBuffers[i].mData == nullptr) {
                outBufferList->mBuffers[i].mData = originalAudioBufferList->mBuffers[i].mData;
            }
            if (zeroFill) {
                memset(outBufferList->mBuffers[i].mData, 0, byteSize);
            }
        }
    }
};

// MARK: -  BufferedInputBus: BufferedAudioBus
// MARK: pullInput()
// MARK: prepareInputBufferList()
/*
 BufferedInputBus
 
 This class manages a buffer into which an audio unit with input busses can
 pull its input data.
 */
struct BufferedInputBus : BufferedAudioBus {
    /*
     Gets input data for this input by preparing the input buffer list and pulling
     the pullInputBlock.
     */
    AUAudioUnitStatus pullInput(AudioUnitRenderActionFlags *actionFlags,
                                AudioTimeStamp const* timestamp,
                                AVAudioFrameCount frameCount,
                                NSInteger inputBusNumber,
                                AURenderPullInputBlock pullInputBlock) {
        if (pullInputBlock == nullptr) {
            return kAudioUnitErr_NoConnection;
        }
        
        /*
         Important:
         The Audio Unit must supply valid buffers in (inputData->mBuffers[x].mData) and mDataByteSize.
         mDataByteSize must be consistent with frameCount.

         The AURenderPullInputBlock may provide input in those specified buffers, or it may replace
         the mData pointers with pointers to memory which it owns and guarantees will remain valid
         until the next render cycle.

         See prepareInputBufferList()
         */

        prepareInputBufferList(frameCount);

        return pullInputBlock(actionFlags, timestamp, frameCount, inputBusNumber, mutableAudioBufferList);
    }
    
    /*
     prepareInputBufferList populates the mutableAudioBufferList with the data
     pointers from the originalAudioBufferList.
     
     The upstream audio unit may overwrite these with its own pointers, so each
     render cycle this function needs to be called to reset them.
     */
    void prepareInputBufferList(UInt32 frameCount) {
        UInt32 byteSize = std::min(frameCount, maxFrames) * sizeof(float);
        mutableAudioBufferList->mNumberBuffers = originalAudioBufferList->mNumberBuffers;

        for (UInt32 i = 0; i < originalAudioBufferList->mNumberBuffers; ++i) {
            mutableAudioBufferList->mBuffers[i].mNumberChannels = originalAudioBufferList->mBuffers[i].mNumberChannels;
            mutableAudioBufferList->mBuffers[i].mData = originalAudioBufferList->mBuffers[i].mData;
            mutableAudioBufferList->mBuffers[i].mDataByteSize = byteSize;
        }
    }
};
