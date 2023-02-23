//------------------------------------------------------------------------------
//! \file       TestAUv3DSPKernel.hpp
//!             Audio Unit App Extension DSP implementation,
//!             created via the Xcode 11 project template for Audio Unit App Extensions.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2022, Celemony Software GmbH, All Rights Reserved.
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

#ifndef TestAUv3DSPKernel_hpp
#define TestAUv3DSPKernel_hpp

#include "ARA_Library/PlugIn/ARAPlug.h"
#include "ARATestPlaybackRenderer.h"

/*
 TestAUv3DSPKernel
 Performs simple copying of the input signal to the output.
 As a non-ObjC class, this is safe to use from render thread.
 */
class TestAUv3DSPKernel {
public:
    
    // MARK: Member Functions

    TestAUv3DSPKernel() {}

    void init(int channelCount, double inSampleRate) {
        chanCount = channelCount;
        sampleRate = float(inSampleRate);
    }

    AUAudioFrameCount maximumFramesToRender() const {
        return maxFramesToRender;
    }

    void setMaximumFramesToRender(const AUAudioFrameCount &maxFrames) {
        maxFramesToRender = maxFrames;
    }

    void setTransportStateBlock(AUHostTransportStateBlock transportState) {
        transportStateBlock = transportState;
    }

    void setARAPlugInExtension(ARA::PlugIn::PlugInExtension* extension) {
        araPlugInExtension = extension;
    }

    void setBuffers(AudioBufferList* inBufferList, AudioBufferList* outBufferList) {
        inBufferListPtr = inBufferList;
        outBufferListPtr = outBufferList;
    }

    /**
     This function handles the event list processing and rendering loop for you.
     Call it inside your internalRenderBlock.
     */
    void processWithEvents(AudioTimeStamp const *timestamp, AUAudioFrameCount frameCount, AURenderEvent const *events, AUMIDIOutputEventBlock midiOut) {
        AUEventSampleTime now = AUEventSampleTime(timestamp->mSampleTime);
        AUAudioFrameCount framesRemaining = frameCount;
        AURenderEvent const *event = events;

        while (framesRemaining > 0) {
            // If there are no more events, we can process the entire remaining segment and exit.
            if (event == nullptr) {
                AUAudioFrameCount const bufferOffset = frameCount - framesRemaining;
                process(framesRemaining, bufferOffset);
                return;
            }

            // **** start late events late.
            auto timeZero = AUEventSampleTime(0);
            auto headEventTime = event->head.eventSampleTime;
            AUAudioFrameCount const framesThisSegment = AUAudioFrameCount(std::max(timeZero, headEventTime - now));

            // Compute everything before the next event.
            if (framesThisSegment > 0) {
                AUAudioFrameCount const bufferOffset = frameCount - framesRemaining;
                process(framesThisSegment, bufferOffset);

                // Advance frames.
                framesRemaining -= framesThisSegment;

                // Advance time.
                now += AUEventSampleTime(framesThisSegment);
            }
        }
    }

private:
    void process(AUAudioFrameCount frameCount, AUAudioFrameCount bufferOffset) {
        float * channels[chanCount];
        for (int i = 0; i < chanCount; ++i)
            channels[i] = (float*)outBufferListPtr->mBuffers[i].mData + bufferOffset;

        AUHostTransportStateFlags transportStateFlags = 0;
        double currentSamplePosition = 0.0;
        if (transportStateBlock)
            transportStateBlock(&transportStateFlags, &currentSamplePosition, nullptr, nullptr);
        bool isPlaying = (transportStateFlags & AUHostTransportStateMoving) != 0;

        if (auto playbackRenderer = (araPlugInExtension) ? araPlugInExtension->getPlaybackRenderer<ARATestPlaybackRenderer>() : nullptr) {
            // if we're an ARA playback renderer, calculate ARA playback output
            playbackRenderer->renderPlaybackRegions(channels, ARA::roundSamplePosition(currentSamplePosition), frameCount, isPlaying);
        }
        else {
            // if we're no ARA playback renderer, we're just copying the inputs to the outputs, which is
            // appropriate both when being only an ARA editor renderer, or when being used in non-ARA mode.
            for (int i = 0; i < chanCount; ++i) {
                const float* in = (const float*)inBufferListPtr->mBuffers[i].mData + bufferOffset;
                if (in != channels[i])      // check in-place processing
                    std::memcpy(channels[i] + bufferOffset, in, sizeof(float) * frameCount);
            }
        }
    }

    // MARK: Member Variables

private:
    int chanCount = 0;
    float sampleRate = 44100.0;
    AUAudioFrameCount maxFramesToRender = 512;
    AudioBufferList* inBufferListPtr = nullptr;
    AudioBufferList* outBufferListPtr = nullptr;
    AUHostTransportStateBlock transportStateBlock = nullptr;
    ARA::PlugIn::PlugInExtension* araPlugInExtension = nullptr;
};

#endif /* TestAUv3DSPKernel_hpp */
