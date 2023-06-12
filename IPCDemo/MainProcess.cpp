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
#include "ARAIPCProxyPlugIn.h"

#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"

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
#define kArchivingControllerHostRef ((ARAArchivingControllerHostRef) 11)
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
static const ARAAudioAccessControllerInterface hostAudioAccessControllerInterface = { ARA_IMPLEMENTED_STRUCT_SIZE (ARAAudioAccessControllerInterface, destroyAudioReader),
                                                                                        &ARACreateAudioReaderForSource, &ARAReadAudioSamples, &ARADestroyAudioReader };

// ARAArchivingControllerInterface (required)
static ARASize ARA_CALL ARAGetArchiveSize (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return 0;
}
static ARABool ARA_CALL ARAReadBytesFromArchive (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef,
                                                ARASize position, ARASize length, ARAByte buffer[])
{
    memset (&buffer[position], 0, length);
    return kARAFalse;
}
static ARABool ARA_CALL ARAWriteBytesToArchive (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                                ARASize position, ARASize length, const ARAByte buffer[])
{
    return kARATrue;
}
static void ARA_CALL ARANotifyDocumentArchivingProgress (ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
static void ARA_CALL ARANotifyDocumentUnarchivingProgress (ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
static ARAPersistentID ARA_CALL ARAGetDocumentArchiveID (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return nullptr;
}
static const ARAArchivingControllerInterface hostArchivingInterface = { ARA_IMPLEMENTED_STRUCT_SIZE (ARAArchivingControllerInterface, getDocumentArchiveID),
                                                                        &ARAGetArchiveSize, &ARAReadBytesFromArchive, &ARAWriteBytesToArchive,
                                                                        &ARANotifyDocumentArchivingProgress, &ARANotifyDocumentUnarchivingProgress,
                                                                        &ARAGetDocumentArchiveID };

ARA_SETUP_DEBUG_MESSAGE_PREFIX ("IPC-Main");

int main (int argc, const char * argv[])
{
    ARA_LOG ("launched.");

    // launch plug-in process
    const auto launchResult = system ("./ARAIPCDemoPlugInProcess &");
    ARA_INTERNAL_ASSERT (launchResult == 0);
    ARA_LOG ("launched plug-in process.");

    // connect to main process for managing model
    ProxyPlugIn::Factory proxyFactory { "com.arademocompany.IPCDemo.hostCommands", "com.arademocompany.IPCDemo.plugInCallbacks" };

    // set a breakpoint to this line if you want to attach the debugger to the plug-in process
    ARA_LOG ("connected to plug-in process.");

    ARADocumentControllerHostInstance documentEntry = { ARA_IMPLEMENTED_STRUCT_SIZE (ARADocumentControllerHostInstance, playbackControllerInterface),
                                                        kAudioAccessControllerHostRef, &hostAudioAccessControllerInterface,
                                                        kArchivingControllerHostRef, &hostArchivingInterface,
                                                        nullptr, nullptr, /* no optional content access in this simple example host */
                                                        nullptr, nullptr, /* no optional model updates in this simple example host */
                                                        nullptr, nullptr  /* no optional playback control in this simple example host */ };
    SizedStruct<ARA_STRUCT_MEMBER (ARADocumentProperties, name)> documentProperties { "Test document" };
    const auto documentControllerInstance { proxyFactory.createDocumentControllerWithDocument (&documentEntry, &documentProperties) };
    const auto documentControllerInterface { documentControllerInstance->documentControllerInterface };
    auto documentControllerRef { documentControllerInstance->documentControllerRef };

    // start editing the document
    documentControllerInterface->beginEditing (documentControllerRef);

    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)> audioSourceProperties { "Test audio source", "audioSourceTestPersistentID",
                                                                kTestAudioSourceSampleRate * kTestAudioSourceDuration, static_cast<double> (kTestAudioSourceSampleRate),
                                                                kTestAudioSourceChannelCount, kARAFalse };
    auto audioSourceRef { documentControllerInterface->createAudioSource (documentControllerRef, kHostAudioSourceHostRef, &audioSourceProperties) };

    documentControllerInterface->endEditing (documentControllerRef);

    documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARATrue);

    const ARAContentType contentType { kARAContentTypeNotes };
    documentControllerInterface->requestAudioSourceContentAnalysis (documentControllerRef, audioSourceRef, 1, &contentType);

    while (true)
    {
        // this is a crude test implementation - real code wouldn't implement such a simple infinite loop.
        // instead, it would evaluate incoming calls to notifyAudioSourceContentChanged().
        // further, it would evaluate notifyAudioSourceAnalysisProgress() to provide proper progress indication,
        // and offer the user a way to cancel the operation if desired.

        documentControllerInterface->notifyModelUpdates (documentControllerRef);

        if (documentControllerInterface->isAudioSourceContentAnalysisIncomplete (documentControllerRef, audioSourceRef, kARAContentTypeNotes) == kARAFalse)
            break;

        std::this_thread::sleep_for (std::chrono::milliseconds { 50 });
    }

    if  (documentControllerInterface->isAudioSourceContentAvailable (documentControllerRef, audioSourceRef, kARAContentTypeNotes) != kARAFalse)
    {
        auto contentReaderRef { documentControllerInterface->createAudioSourceContentReader (documentControllerRef, audioSourceRef, kARAContentTypeNotes, nullptr) };

        const auto eventCount { documentControllerInterface->getContentReaderEventCount (documentControllerRef, contentReaderRef) };
        ARA_LOG ("%i notes available for audio source %s:", eventCount, audioSourceProperties.name);
        for (ARAInt32 i { 0 }; i < eventCount; ++i)
            ContentLogger::logEvent (i, *(const ARAContentNote *)documentControllerInterface->getContentReaderDataForEvent (documentControllerRef, contentReaderRef, i));

        documentControllerInterface->destroyContentReader (documentControllerRef, contentReaderRef);
    }

    documentControllerInterface->enableAudioSourceSamplesAccess (documentControllerRef, audioSourceRef, kARAFalse);

    documentControllerInterface->beginEditing (documentControllerRef);

    documentControllerInterface->destroyAudioSource (documentControllerRef, audioSourceRef);

    // shut everything down
    documentControllerInterface->endEditing (documentControllerRef);

    documentControllerInterface->destroyDocumentController (documentControllerRef);

    return 0;
}
