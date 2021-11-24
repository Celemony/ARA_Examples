//------------------------------------------------------------------------------
//! \file       MainProcess.cpp
//!             implementation of the SDK IPC demo example, main process
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2021, Celemony Software GmbH, All Rights Reserved.
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
// This is a brief proof-of-concept demo that hooks up an ARA capable plug-in
// in a separate process using IPC.
// Thanks to ARAs usage of opaque refs/host refs which are transparent to
// the other side of the API, this can be easily be accomplished by merely
// transmitting the call arguments in an appropriate way.
// This educational example is not suitable for production code without further
// improvements - for the sake of readability of the code, it only covers a
// subsection of the ARA API and ignores proper error handling beyond asserts.
// Implementing proper error handling will likely have impact on the overall design
// (in particular with regards to possible IPC failures).
// It also assumes that both processes use the same pointer size.
// Further, threading issues are ignored - while most ARA calls are restricted
// to the model thread, audio access is potentially called concurrently from
// multiple threadeds and may needs to implement proper locking when accessing
// the IPC transmission channel.
// Playback control also may be requested from several threads, but can be
// enqueued and processed asynchronously through the model update IPC channel.
// A proper production code implementation would probably be designed using
// proxies that encapsulate the IPC so that the other code will only have to
// deal with the original ARA API (plus additional IPC error handling).
//------------------------------------------------------------------------------

// test code includes
#include "IPCMessage.h"
#include "IPCPort.h"
#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

#include <thread>

// ARA framework includes
#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARADispatchBase.h"
#include "ARA_Library/Debug/ARAContentLogger.h"

using namespace ARA;


// in this simple demo application, we need logging to be always enabled, even in release builds.
// this needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif


#define kTestAudioSourceSampleRate 44100    /* Hertz */
#define kTestAudioSourceDuration 5          /* seconds */
#define kTestAudioSourceChannelCount 2
#define kHostAudioSourceHostRef ((ARAAudioSourceHostRef) 1)
#define kAudioAccessControllerHostRef ((ARAAudioAccessControllerHostRef) 10)
#define kAudioReader32BitHostRef ((ARAAudioReaderHostRef) 20)
#define kAudioReader64BitHostRef ((ARAAudioReaderHostRef) 21)


// ARAAudioAccessControllerInterface (required)
ARAAudioReaderHostRef ARA_CALL ARACreateAudioReaderForSource (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARABool use64BitSamples)
{
    ARAAudioReaderHostRef audioReaderHostRef { (use64BitSamples) ? kAudioReader64BitHostRef : kAudioReader32BitHostRef };
    ARA_VALIDATE_API_ARGUMENT (controllerHostRef, controllerHostRef == kAudioAccessControllerHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSourceHostRef, audioSourceHostRef == kHostAudioSourceHostRef);
    ARA_LOG ("createAudioReaderForSource() returns fake ref %p.", audioReaderHostRef);
    return audioReaderHostRef;
}
ARABool ARA_CALL ARAReadAudioSamples (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef,
                                        ARASamplePosition samplePosition, ARASampleCount samplesPerChannel, void* const buffers[])
{
    ARA_VALIDATE_API_ARGUMENT (controllerHostRef, controllerHostRef == kAudioAccessControllerHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioReaderHostRef, (audioReaderHostRef == kAudioReader32BitHostRef) || (audioReaderHostRef == kAudioReader64BitHostRef));
    ARA_VALIDATE_API_ARGUMENT (nullptr, 0 <= samplePosition);
    ARA_VALIDATE_API_ARGUMENT (nullptr, samplePosition + samplesPerChannel <= kTestAudioSourceSampleRate * kTestAudioSourceDuration);
    ARA_VALIDATE_API_ARGUMENT (buffers, buffers != nullptr);
    ARA_VALIDATE_API_ARGUMENT (buffers, buffers[0] != nullptr);
    ARABool use64BitSamples = (audioReaderHostRef == kAudioReader64BitHostRef) ? kARATrue : kARAFalse;
    RenderPulsedSineSignal (samplePosition, static_cast<double> (kTestAudioSourceSampleRate), kTestAudioSourceSampleRate * kTestAudioSourceDuration,
                            kTestAudioSourceChannelCount, samplesPerChannel, buffers, use64BitSamples);
    return kARATrue;
}
void ARA_CALL ARADestroyAudioReader (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef)
{
    ARA_VALIDATE_API_ARGUMENT (controllerHostRef, controllerHostRef == kAudioAccessControllerHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioReaderHostRef, (audioReaderHostRef == kAudioReader32BitHostRef) || (audioReaderHostRef == kAudioReader64BitHostRef));
    ARA_LOG ("destroyAudioReader() called for fake ref %p.", audioReaderHostRef);
}


template<typename FloatT>
IPCMessage _readAudioSamples (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef readerHostRef,
                                 ARASamplePosition samplePosition, ARASampleCount samplesPerChannel)
{
    std::vector<FloatT> bufferData;
    bufferData.resize (static_cast<size_t> (kTestAudioSourceChannelCount * samplesPerChannel));
    void* sampleBuffers[kTestAudioSourceChannelCount];
    for (auto i { 0 }; i < kTestAudioSourceChannelCount; ++i)
        sampleBuffers[i] = bufferData.data () + i * samplesPerChannel;
    ARABool success { ARAReadAudioSamples (controllerHostRef, readerHostRef, samplePosition, samplesPerChannel, sampleBuffers) };
    return IPCMessage { "readAudioSamplesReply", "result", success, "bufferData", bufferData };
}

IPCMessage audioAccessFromPlugInCallBack (const IPCMessage& message)
{
    if (message.isMessageWithID ("createAudioReaderForSource"))
    {
        ARAAudioReaderHostRef readerRef { ARACreateAudioReaderForSource (message.getArgValue<ARAAudioAccessControllerHostRef> ("controllerHostRef"),
                                                                         message.getArgValue<ARAAudioSourceHostRef> ("audioSourceHostRef"),
                                                                         message.getArgValue<ARABool> ("use64BitSamples")) };
        return IPCMessage { "createAudioReaderForSourceReply", "readerRef", readerRef };
    }
    else if (message.isMessageWithID ("readAudioSamples"))
    {
        const auto readerRef { message.getArgValue<ARAAudioReaderHostRef> ("readerRef") };
        if (readerRef == kAudioReader64BitHostRef)
            return _readAudioSamples<double> (message.getArgValue<ARAAudioAccessControllerHostRef> ("controllerHostRef"), readerRef,
                                              message.getArgValue<ARASamplePosition> ("samplePosition"), message.getArgValue<ARASampleCount> ("samplesPerChannel"));
        else
            return _readAudioSamples<float> (message.getArgValue<ARAAudioAccessControllerHostRef> ("controllerHostRef"), readerRef,
                                             message.getArgValue<ARASamplePosition> ("samplePosition"), message.getArgValue<ARASampleCount> ("samplesPerChannel"));
    }
    else if (message.isMessageWithID ("destroyAudioReader"))
    {
        ARADestroyAudioReader (message.getArgValue<ARAAudioAccessControllerHostRef> ("controllerHostRef"),
                               message.getArgValue<ARAAudioReaderHostRef> ("readerRef"));
        return IPCMessage {};
    }

    ARA_INTERNAL_ASSERT (false && "unhandled methodSelector");
    return IPCMessage {};
}


void audioAccessThreadHandler (bool& keepRunning)
{
    ARA_LOG ("audio access thread started.");
    auto audioAccessFromPlugInPort { IPCPort::createPublishingID ("com.arademocompany.IPCDemo.audioAccessFromPlugIn", &audioAccessFromPlugInCallBack) };
    while (keepRunning)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.05, false);
    ARA_LOG ("audio access thread stopped.");
}

ARA_SETUP_DEBUG_MESSAGE_PREFIX("IPC-Main");

int main (int argc, const char * argv[])
{
    ARA_LOG ("launched.");

    // launch plug-in process
    const auto launchResult = system ("./ARAIPCDemoPlugInProcess &");
    ARA_INTERNAL_ASSERT (launchResult == 0);

    // detach thread for audio access handler
    auto keepRunning { true };
    std::thread audioAccessThread { audioAccessThreadHandler, std::ref (keepRunning) };
    ARA_LOG ("launched plug-in process.");

// for some reason, the clang analyzer produces several false positives here related to SizedStruct<>
// and how it initializes its members, and to keepRunning being captured - we're disabling it here.
#if !defined(__clang_analyzer__)

    // connect to main process for managing model
    IPCPort modelPortToPlugIn { IPCPort::createConnectedToID ("com.arademocompany.IPCDemo.modelPortToPlugIn") };

    // set a breakpoint to this line if you want to attach the debugger to the plug-in process
    ARA_LOG ("connected to plug-in process.");

    SizedStruct<ARA_STRUCT_MEMBER (ARADocumentProperties, name)> documentProperties { "Test document" };
    //documentControllerInstance = factory->createDocumentControllerWithDocument (&documentEntry, &documentProperties);
    IPCMessage reply { modelPortToPlugIn.sendAndAwaitReply ({ "createDocumentControllerWithDocument",
                                                                 "hostInstance.audioAccessControllerHostRef", kAudioAccessControllerHostRef,
                                                                 "properties.name", documentProperties.name
                                                               }) };
    auto remoteDocumentRef { reply.getArgValue<ARADocumentControllerRef> ("controllerRef") };

    //documentControllerInterface->beginEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply ({ "beginEditing",
                                          "controllerRef", remoteDocumentRef
                                        });

    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)> audioSourceProperties { "Test audio source", "audioSourceTestPersistentID",
                                                                kTestAudioSourceSampleRate * kTestAudioSourceDuration, static_cast<double> (kTestAudioSourceSampleRate),
                                                                kTestAudioSourceChannelCount, kARAFalse };
    //audioSourceRef = documentControllerInterface->createAudioSource (documentControllerRef, kHostAudioSourceHostRef, &audioSourceProperties);
    reply = modelPortToPlugIn.sendAndAwaitReply ({ "createAudioSource",
                                                   "controllerRef", remoteDocumentRef,
                                                   "hostRef", kHostAudioSourceHostRef,
                                                   "properties.name", audioSourceProperties.name,
                                                   "properties.persistentID", audioSourceProperties.persistentID,
                                                   "properties.sampleCount", audioSourceProperties.sampleCount,
                                                   "properties.sampleRate", audioSourceProperties.sampleRate,
                                                   "properties.channelCount", audioSourceProperties.channelCount,
                                                   "properties.merits64BitSamples", audioSourceProperties.merits64BitSamples
                                                 });
    auto audioSourceRef { reply.getArgValue<ARAAudioSourceRef> ("audioSourceRef") };

    //documentControllerInterface->endEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply ({ "endEditing",
                                          "controllerRef", remoteDocumentRef
                                        });

    //documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARATrue);
    modelPortToPlugIn.sendWithoutReply ({"enableAudioSourceSamplesAccess",
                                          "controllerRef", remoteDocumentRef,
                                          "audioSourceRef", audioSourceRef,
                                          "enable", kARATrue
                                        });

    //documentControllerInterface->requestAudioSourceContentAnalysis (documentControllerRef, audioSourceRef, 1, { kARAContentTypeNotes });
    modelPortToPlugIn.sendWithoutReply ({ "requestAudioSourceContentAnalysis",
                                          "controllerRef", remoteDocumentRef,
                                          "audioSourceRef", audioSourceRef,
                                          "contentTypes", std::vector<ARAContentType> { kARAContentTypeNotes }
                                        });

    //wait for documentControllerInterface->isAudioSourceContentAnalysisIncomplete (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
    while (true)
    {
        // this is a crude test implementation - real code wouldn't implement such a simple infinite loop.
        // instead, it would periodically request notifications and evaluate incoming calls to notifyAudioSourceContentChanged().
        // further, it would evaluate notifyAudioSourceAnalysisProgress() to provide proper progress indication,
        // and offer the user a way to cancel the operation if desired.

        // not modelled via IPC yet, currently sent on the remote side where needed.
        //documentControllerInterface->notifyModelUpdates(documentControllerRef);

        //done = documentControllerInterface->isAudioSourceContentAnalysisIncomplete (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
        reply = modelPortToPlugIn.sendAndAwaitReply ({ "isAudioSourceContentAnalysisIncomplete",
                                                       "controllerRef", remoteDocumentRef,
                                                       "audioSourceRef", audioSourceRef,
                                                       "contentType", kARAContentTypeNotes
                                                     });
        if (reply.getArgValue<ARABool> ("result") == kARAFalse)
            break;

        std::this_thread::sleep_for (std::chrono::milliseconds { 50 });
    }

    //hasEvents = documentControllerInterface->isAudioSourceContentAvailable (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
    reply = modelPortToPlugIn.sendAndAwaitReply ({ "isAudioSourceContentAvailable",
                                                    "controllerRef", remoteDocumentRef,
                                                    "audioSourceRef", audioSourceRef,
                                                    "contentType", kARAContentTypeNotes
                                                  });
    if (reply.getArgValue<ARABool> ("result") != kARAFalse)
    {
        //contentReaderRef = documentControllerInterface->createAudioSourceContentReader (documentControllerRef, audioSourceRef, kARAContentTypeNotes, nullptr);
        reply = modelPortToPlugIn.sendAndAwaitReply ({ "createAudioSourceContentReader",
                                                       "controllerRef", remoteDocumentRef,
                                                       "audioSourceRef", audioSourceRef,
                                                       "contentType", kARAContentTypeNotes
                                                       /* optional contentTimeRange argument not implemented here to keep the example simple */
                                                     });
        auto contentReaderRef = reply.getArgValue<ARAContentReaderRef> ("contentReaderRef");

        //eventCount = documentControllerInterface->getContentReaderEventCount (documentControllerRef, contentReaderRef);
        reply = modelPortToPlugIn.sendAndAwaitReply ({ "getContentReaderEventCount",
                                                       "controllerRef", remoteDocumentRef,
                                                       "contentReaderRef", contentReaderRef
                                                     });
        const auto eventCount { reply.getArgValue<ARAInt32> ("result") };
        ARA_LOG ("%i notes available for audio source %s:", eventCount, audioSourceProperties.name);
        for (ARAInt32 i = 0; i < eventCount; ++i)
        {
            //noteData = (const ARAContentNote *)documentControllerInterface->getContentReaderDataForEvent (documentControllerRef, contentReaderRef, i);
            reply = modelPortToPlugIn.sendAndAwaitReply ({ "getContentReaderDataForEvent",
                                                           "controllerRef", remoteDocumentRef,
                                                           "contentReaderRef", contentReaderRef,
                                                           "eventIndex", i
                                                         });
            auto noteContent { reply.getArgValue<IPCMessage> ("contentData") };
            ContentLogger::logEvent (i, ARAContentNote { noteContent.getArgValue<float>("frequency"),
                                                         noteContent.getArgValue<ARAPitchNumber>("pitchNumber"),
                                                         noteContent.getArgValue<float>("volume"),
                                                         noteContent.getArgValue<ARATimePosition>("startPosition"),
                                                         noteContent.getArgValue<ARATimeDuration>("attackDuration"),
                                                         noteContent.getArgValue<ARATimeDuration>("noteDuration"),
                                                         noteContent.getArgValue<ARATimeDuration>("signalDuration")
                                                       });
        }

        //documentControllerInterface->destroyContentReader (documentControllerRef, contentReaderRef);
        modelPortToPlugIn.sendWithoutReply ({ "destroyContentReader",
                                              "controllerRef", remoteDocumentRef,
                                              "contentReaderRef", contentReaderRef
                                            });
    }

    //documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARAFalse);
    modelPortToPlugIn.sendWithoutReply ({ "enableAudioSourceSamplesAccess",
                                          "controllerRef", remoteDocumentRef,
                                          "audioSourceRef", audioSourceRef,
                                          "enable", kARAFalse
                                        });

    //documentControllerInterface->beginEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply ({ "beginEditing",
                                          "controllerRef", remoteDocumentRef
                                        });

    //documentControllerInterface->destroyAudioSource (documentControllerRef, audioSourceRef);
    modelPortToPlugIn.sendWithoutReply ({ "destroyAudioSource",
                                          "controllerRef", remoteDocumentRef,
                                          "audioSourceRef", audioSourceRef,
                                        });

    //documentControllerInterface->endEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply ({ "endEditing",
                                         "controllerRef", remoteDocumentRef
                                        });

    //documentControllerInterface->destroyDocumentController (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply ({ "destroyDocumentController",
                                          "controllerRef", remoteDocumentRef
                                        });

    // shut everything down
    keepRunning = false;
    audioAccessThread.join ();

#endif  // !defined(__clang_analyzer__)

    ARA_LOG ("completed.");
    return 0;
}
