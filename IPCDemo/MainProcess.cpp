//------------------------------------------------------------------------------
//! \file       MainProcess.cpp
//!             implementation of the SDK IPC demo example, main process
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
// multiple threads and may need to implement proper locking when accessing
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
#include "ARAIPCEncoding.h"

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
    std::vector<ARAByte> bufferData;
    bufferData.resize (sizeof (FloatT) * kTestAudioSourceChannelCount * static_cast<size_t> (samplesPerChannel));
    void* sampleBuffers[kTestAudioSourceChannelCount];
    for (auto i { 0u }; i < kTestAudioSourceChannelCount; ++i)
        sampleBuffers[i] = bufferData.data () + sizeof (FloatT) * i * static_cast<size_t> (samplesPerChannel);

    const auto success { ARAReadAudioSamples (controllerHostRef, readerHostRef, samplePosition, samplesPerChannel, sampleBuffers) };

    const auto endian { CFByteOrderGetCurrent() };
    ARA_INTERNAL_ASSERT (endian != CFByteOrderUnknown);
    return encodeReply (ARAIPCReadSamplesReply { (success != kARAFalse) ? bufferData.size () : 0,
                                                       (success != kARAFalse) ? bufferData.data () : nullptr,
                                                       (endian == CFByteOrderLittleEndian) ? kARATrue : kARAFalse });
}

IPCMessage audioAccessFromPlugInCallBack (const int32_t messageID, const IPCMessage& message)
{
    if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, createAudioReaderForSource))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioSourceHostRef audioSourceHostRef;
        ARABool use64BitSamples;
        decodeArguments (message, controllerHostRef, audioSourceHostRef, use64BitSamples);
        ARAAudioReaderHostRef readerRef { ARACreateAudioReaderForSource (controllerHostRef, audioSourceHostRef, use64BitSamples) };
        return encodeReply (readerRef);
    }
    else if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, readAudioSamples))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioReaderHostRef readerRef;
        ARASamplePosition samplePosition;
        ARASampleCount samplesPerChannel;
        decodeArguments (message, controllerHostRef, readerRef, samplePosition, samplesPerChannel);
        if (readerRef == kAudioReader64BitHostRef)
            return _readAudioSamples<double> (controllerHostRef, readerRef, samplePosition, samplesPerChannel);
        else
            return _readAudioSamples<float> (controllerHostRef, readerRef, samplePosition, samplesPerChannel);
    }
    else if (messageID == HOST_METHOD_ID (ARAAudioAccessControllerInterface, destroyAudioReader))
    {
        ARAAudioAccessControllerHostRef controllerHostRef;
        ARAAudioReaderHostRef readerRef;
        decodeArguments (message, controllerHostRef, readerRef);
        ARADestroyAudioReader (controllerHostRef, readerRef);
        return {};
    }

    ARA_INTERNAL_ASSERT (false && "unhandled methodSelector");
    return {};
}


void audioAccessThreadHandler (bool& keepRunning)
{
    ARA_LOG ("audio access thread started.");
    auto audioAccessFromPlugInPort { IPCPort::createPublishingID ("com.arademocompany.IPCDemo.audioAccessFromPlugIn", &audioAccessFromPlugInCallBack) };
    while (keepRunning)
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.05, false);
    ARA_LOG ("audio access thread stopped.");
}

ARA_SETUP_DEBUG_MESSAGE_PREFIX ("IPC-Main");

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
    auto remoteDocumentRef { decodeReply<ARADocumentControllerRef> (
        modelPortToPlugIn.sendAndAwaitReply (kCreateDocumentControllerMethodID,
                                                encodeArguments (kAudioAccessControllerHostRef, (ARADocumentProperties*)&documentProperties))) };

    //documentControllerInterface->beginEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, beginEditing),
                                                encodeArguments (remoteDocumentRef));

    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)> audioSourceProperties { "Test audio source", "audioSourceTestPersistentID",
                                                                kTestAudioSourceSampleRate * kTestAudioSourceDuration, static_cast<double> (kTestAudioSourceSampleRate),
                                                                kTestAudioSourceChannelCount, kARAFalse };
    //audioSourceRef = documentControllerInterface->createAudioSource (documentControllerRef, kHostAudioSourceHostRef, &audioSourceProperties);
    auto audioSourceRef { decodeReply<ARAAudioSourceRef> (
        modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSource),
                                                encodeArguments (remoteDocumentRef, kHostAudioSourceHostRef, (ARAAudioSourceProperties*)&audioSourceProperties))) };

    //documentControllerInterface->endEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, endEditing),
                                                encodeArguments (remoteDocumentRef));

    //documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARATrue);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, enableAudioSourceSamplesAccess),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, kARATrue));

    //documentControllerInterface->requestAudioSourceContentAnalysis (documentControllerRef, audioSourceRef, 1, { kARAContentTypeNotes });
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestAudioSourceContentAnalysis),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, std::vector<ARAContentType> { kARAContentTypeNotes }));

    //wait for documentControllerInterface->isAudioSourceContentAnalysisIncomplete (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
    while (true)
    {
        // this is a crude test implementation - real code wouldn't implement such a simple infinite loop.
        // instead, it would periodically request notifications and evaluate incoming calls to notifyAudioSourceContentChanged().
        // further, it would evaluate notifyAudioSourceAnalysisProgress() to provide proper progress indication,
        // and offer the user a way to cancel the operation if desired.

        // not modelled via IPC yet, currently sent on the remote side where needed.
        //documentControllerInterface->notifyModelUpdates (documentControllerRef);

        //done = documentControllerInterface->isAudioSourceContentAnalysisIncomplete (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
        if (decodeReply<ARABool> (
                modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAnalysisIncomplete),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, kARAContentTypeNotes)))
            == kARAFalse)
            break;

        std::this_thread::sleep_for (std::chrono::milliseconds { 50 });
    }

    //hasEvents = documentControllerInterface->isAudioSourceContentAvailable (documentControllerRef, audioSourceRef, kARAContentTypeNotes);
    if (decodeReply<ARABool> (
            modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAvailable),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, kARAContentTypeNotes)))
        != kARAFalse)
    {
        //contentReaderRef = documentControllerInterface->createAudioSourceContentReader (documentControllerRef, audioSourceRef, kARAContentTypeNotes, nullptr);
        auto contentReaderRef { decodeReply<ARAContentReaderRef> (
            modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSourceContentReader),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, kARAContentTypeNotes, nullptr))) };

        //eventCount = documentControllerInterface->getContentReaderEventCount (documentControllerRef, contentReaderRef);
        const auto eventCount { decodeReply<ARAInt32> (
            modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderEventCount),
                                                encodeArguments (remoteDocumentRef, contentReaderRef))) };
        ARA_LOG ("%i notes available for audio source %s:", eventCount, audioSourceProperties.name);
        for (ARAInt32 i = 0; i < eventCount; ++i)
        {
            //noteData = (const ARAContentNote *)documentControllerInterface->getContentReaderDataForEvent (documentControllerRef, contentReaderRef, i);
            ContentLogger::logEvent (i, decodeReply<ARAContentNote> (
                modelPortToPlugIn.sendAndAwaitReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderDataForEvent),
                                                encodeArguments (remoteDocumentRef, contentReaderRef, i))));
        }

        //documentControllerInterface->destroyContentReader (documentControllerRef, contentReaderRef);
        modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyContentReader),
                                                encodeArguments (remoteDocumentRef, contentReaderRef));
    }

    //documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARAFalse);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, enableAudioSourceSamplesAccess),
                                                encodeArguments (remoteDocumentRef, audioSourceRef, kARAFalse));

    //documentControllerInterface->beginEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, beginEditing),
                                                encodeArguments (remoteDocumentRef));

    //documentControllerInterface->destroyAudioSource (documentControllerRef, audioSourceRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioSource),
                                                encodeArguments (remoteDocumentRef, audioSourceRef));

    //documentControllerInterface->endEditing (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, endEditing),
                                                encodeArguments (remoteDocumentRef));

    //documentControllerInterface->destroyDocumentController (documentControllerRef);
    modelPortToPlugIn.sendWithoutReply (PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyDocumentController),
                                                encodeArguments (remoteDocumentRef));

    // shut everything down
    keepRunning = false;
    audioAccessThread.join ();

#endif  // !defined(__clang_analyzer__)

    ARA_LOG ("completed.");
    return 0;
}
