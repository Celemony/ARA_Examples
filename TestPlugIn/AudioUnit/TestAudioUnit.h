//------------------------------------------------------------------------------
//! \file       TestAudioUnit.h
//!             Audio Unit effect class for the ARA test plug-in,
//!             created via the Xcode 3 project template for Audio Unit effects.
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

#ifndef __TestAudioUnit_h__
#define __TestAudioUnit_h__

_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wimport-preprocessor-directive-pedantic\"")

#include "AUEffectBase.h"

_Pragma ("GCC diagnostic pop")

#if !CA_USE_AUDIO_PLUGIN_ONLY
    #error "Audio Unit v1 is no longer supported."
#endif

#include "ARA_Library/PlugIn/ARAPlug.h"

class TestAudioUnit : public ausdk::AUEffectBase
{
public:
    TestAudioUnit(AudioUnit component);

    virtual OSStatus            Initialize();
    virtual void                Cleanup();

    virtual UInt32              SupportedNumChannels(const AUChannelInfo** outInfo);
    virtual CFURLRef            CopyIconLocation ();

    virtual OSStatus            GetPropertyInfo(AudioUnitPropertyID        inID,
                                                AudioUnitScope            inScope,
                                                AudioUnitElement        inElement,
                                                UInt32 &                outDataSize,
                                                bool &                  outWritable );
    virtual OSStatus            GetProperty(AudioUnitPropertyID inID,
                                            AudioUnitScope         inScope,
                                            AudioUnitElement     inElement,
                                            void *                outData);

    virtual OSStatus            ProcessBufferLists(
                                            AudioUnitRenderActionFlags &    ioActionFlags,
                                            const AudioBufferList &            inBuffer,
                                            AudioBufferList &                outBuffer,
                                            UInt32                            inFramesToProcess );
    virtual OSStatus            Render(AudioUnitRenderActionFlags &        ioActionFlags,
                                            const AudioTimeStamp &            inTimeStamp,
                                            UInt32                            inNumberFrames);
private:
    ARA::PlugIn::PlugInExtension _araPlugInExtension;
};

#endif
