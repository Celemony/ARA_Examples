//------------------------------------------------------------------------------
//! \file       PlugInProcess.cpp
//!             implementation of the SDK IPC demo example, plug-in process
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
// This educational example is not suitable for production code -
// see MainProcess.cpp for a list of issues.
//------------------------------------------------------------------------------

// test code includes
#include "IPCMessage.h"
#include "IPCPort.h"
#include "ARAIPCEncoding.h"

// ARA framework includes
#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARADispatchBase.h"

#include <os/lock.h>

using namespace ARA;


// in this simple demo application, we need logging to be always enabled, even in release builds.
// this needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif


// list of available companion APIs
#define PLUGIN_FORMAT_AU   1
#define PLUGIN_FORMAT_VST3 2

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    #include "ExamplesCommon/PlugInHosting/AudioUnitLoader.h"
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    #include "ExamplesCommon/PlugInHosting/VST3Loader.h"
#else
    #error "PLUGIN_FORMAT not configured properly in the project"
#endif


static const ARAFactory* factory {};
static IPCPort audioAccessFromPlugInPort {};


struct ARARemoteDocument
{
    SizedStruct<ARA_STRUCT_MEMBER (ARADocumentControllerHostInstance, playbackControllerInterface)> hostInstance {};
    ARADocumentControllerInstance documentController {};
};

struct ARARemoteAudioSource
{
    ARAAudioSourceHostRef mainHostRef {};
    ARAAudioSourceRef plugInRef {};
    std::string name;
    std::string persistentID;
    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)> properties {};
};

struct ARARemoteAudioReader
{
    ARARemoteAudioSource* audioSource {};
    ARAAudioReaderHostRef mainHostRef {};
    ARABool use64BitSamples {};
};

struct ARARemoteContentReader
{
    ARAContentReaderRef plugInRef {};
    ARAContentType contentType;
};


// ARAAudioAccessControllerInterface
ARAAudioReaderHostRef ARA_CALL ARACreateAudioReaderForSource (ARAAudioAccessControllerHostRef controllerHostRef,
                                                                ARAAudioSourceHostRef hostAudioSourceRef, ARABool use64BitSamples)
{
    auto remoteAudioSource { reinterpret_cast<ARARemoteAudioSource*> (hostAudioSourceRef) };
    auto remoteAudioReader { new ARARemoteAudioReader };
    remoteAudioReader->audioSource = remoteAudioSource;
    remoteAudioReader->use64BitSamples = use64BitSamples;

    const auto reply { audioAccessFromPlugInPort.sendAndAwaitReply (encodeMethodCall ("createAudioReaderForSource",
                                                                        "controllerHostRef", controllerHostRef,
                                                                        "audioSourceHostRef", remoteAudioSource->mainHostRef,
                                                                        "use64BitSamples", use64BitSamples)) };
    remoteAudioReader->mainHostRef = reply.getArgValue<ARAAudioReaderHostRef> ("readerRef");
    return reinterpret_cast<ARAAudioReaderHostRef> (remoteAudioReader);
}

template<typename FloatT>
ARABool _readAudioSamples (const IPCMessage& reply, ARASampleCount samplesPerChannel, ARAChannelCount channelCount, void* const buffers[])
{
    const auto success { reply.getArgValue<ARABool> ("result") };
    auto bufferData { reply.getArgValue<std::vector<FloatT>> ("bufferData") };
    ARA_INTERNAL_ASSERT (bufferData.size () == static_cast<size_t> (samplesPerChannel * channelCount));
    const FloatT* sourcePtr = bufferData.data ();
    for (auto i { 0 }; i < channelCount; ++i)
    {
        auto destinationPtr { static_cast<FloatT*> (buffers[i]) };
        if (success != kARAFalse)
            std::copy (sourcePtr, sourcePtr + samplesPerChannel, destinationPtr);
        else
            std::fill (destinationPtr, destinationPtr + samplesPerChannel, FloatT {});
        sourcePtr += samplesPerChannel;
    }
    return success;
}

ARABool ARA_CALL ARAReadAudioSamples (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef,
                                        ARASamplePosition samplePosition, ARASampleCount samplesPerChannel, void* const buffers[])
{
    auto remoteAudioReader { reinterpret_cast<ARARemoteAudioReader *> (audioReaderHostRef) };

    // recursively limit message size to keep IPC responsive
    if (samplesPerChannel > 8192)
    {
        const auto samplesPerChannel1 { samplesPerChannel / 2 };
        const auto result1 { ARAReadAudioSamples (controllerHostRef, audioReaderHostRef, samplePosition, samplesPerChannel1, buffers) };

        const auto sampleSize { (remoteAudioReader->use64BitSamples != kARAFalse) ? sizeof(double) : sizeof(float) };
        const auto samplesPerChannel2 { samplesPerChannel - samplesPerChannel1 };
        void* buffers2[remoteAudioReader->audioSource->properties.channelCount];
        for (auto i { 0 }; i < remoteAudioReader->audioSource->properties.channelCount; ++i)
            buffers2[i] = static_cast<uint8_t*> (buffers[i]) + static_cast<size_t> (samplesPerChannel1) * sampleSize;

        if (result1 != kARAFalse)
        {
            return ARAReadAudioSamples (controllerHostRef, audioReaderHostRef, samplePosition + samplesPerChannel1, samplesPerChannel2, buffers2);
        }
        else
        {
            for (auto i { 0 }; i < remoteAudioReader->audioSource->properties.channelCount; ++i)
                std::memset (buffers2[i], 0, static_cast<size_t> (samplesPerChannel2) * sampleSize);
            return kARAFalse;
        }
    }

    static os_unfair_lock_s lock { OS_UNFAIR_LOCK_INIT };
    os_unfair_lock_lock (&lock);

    const auto reply { audioAccessFromPlugInPort.sendAndAwaitReply (encodeMethodCall ("readAudioSamples",
                                                                                      "controllerHostRef", controllerHostRef,
                                                                                      "readerRef", remoteAudioReader->mainHostRef,
                                                                                      "samplePosition", samplePosition,
                                                                                      "samplesPerChannel", samplesPerChannel)) };
    const auto result { (remoteAudioReader->use64BitSamples != kARAFalse) ?
                        _readAudioSamples<double> (reply, samplesPerChannel, remoteAudioReader->audioSource->properties.channelCount, buffers):
                        _readAudioSamples<float> (reply, samplesPerChannel, remoteAudioReader->audioSource->properties.channelCount, buffers)};

    os_unfair_lock_unlock (&lock);
    return result;
}

void ARA_CALL ARADestroyAudioReader (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef)
{
    auto remoteAudioReader { reinterpret_cast<ARARemoteAudioReader *> (audioReaderHostRef) };
    audioAccessFromPlugInPort.sendWithoutReply (encodeMethodCall ("destroyAudioReader",
                                                                        "controllerHostRef", controllerHostRef,
                                                                        "readerRef", remoteAudioReader->mainHostRef));
    delete remoteAudioReader;
}
static const SizedStruct<ARA_STRUCT_MEMBER (ARAAudioAccessControllerInterface, destroyAudioReader)> hostAudioAccessControllerInterface {
                                                        &ARACreateAudioReaderForSource, &ARAReadAudioSamples, &ARADestroyAudioReader };

// dummy ARAArchivingControllerInterface
ARASize ARA_CALL ARAGetArchiveSize (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return 0;
}
ARABool ARA_CALL ARAReadBytesFromArchive (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef,
                                            ARASize position, ARASize length, ARAByte buffer[])
{
    memset (&buffer[position], 0, length);
    return kARAFalse;
}
ARABool ARA_CALL ARAWriteBytesToArchive (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                            ARASize position, ARASize length, const ARAByte buffer[])
{
    return kARATrue;
}
void ARA_CALL ARANotifyDocumentArchivingProgress (ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
void ARA_CALL ARANotifyDocumentUnarchivingProgress (ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
ARAPersistentID ARA_CALL ARAGetDocumentArchiveID (ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return nullptr;
}
static const SizedStruct<ARA_STRUCT_MEMBER (ARAArchivingControllerInterface, getDocumentArchiveID)> hostArchivingControllerInterface {
                                                                                        &ARAGetArchiveSize, &ARAReadBytesFromArchive, &ARAWriteBytesToArchive,
                                                                                        &ARANotifyDocumentArchivingProgress, &ARANotifyDocumentUnarchivingProgress,
                                                                                        &ARAGetDocumentArchiveID };

IPCMessage modelPortToPlugInCallBack (const IPCMessage& message)
{
    if (isMethodCall (message, "createDocumentControllerWithDocument"))
    {
        auto remoteDocument { new ARARemoteDocument };

        remoteDocument->hostInstance.audioAccessControllerHostRef = message.getArgValue<ARAAudioAccessControllerHostRef> ("hostInstance.audioAccessControllerHostRef");
        remoteDocument->hostInstance.audioAccessControllerInterface = &hostAudioAccessControllerInterface;
        remoteDocument->hostInstance.archivingControllerInterface = &hostArchivingControllerInterface;

        const auto documentName { message.getArgValue<std::string> ("properties.name") };
        const SizedStruct<ARA_STRUCT_MEMBER (ARADocumentProperties, name)> documentProperties { documentName.c_str () };

        const ARADocumentControllerInstance * documentControllerInstance = factory->createDocumentControllerWithDocument (&remoteDocument->hostInstance, &documentProperties);
        ARA_VALIDATE_API_CONDITION (documentControllerInstance != nullptr);
        ARA_VALIDATE_API_INTERFACE (documentControllerInstance->documentControllerInterface, ARADocumentControllerInterface);
        remoteDocument->documentController = *documentControllerInstance;
        return { "controllerRef", remoteDocument };
    }
    else if (isMethodCall (message, "destroyDocumentController"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        remoteDocument->documentController.documentControllerInterface->destroyDocumentController (remoteDocument->documentController.documentControllerRef);

        delete remoteDocument;
        CFRunLoopStop (CFRunLoopGetCurrent ()); // will terminate run loop & shut down

        return {};
    }
    else if (isMethodCall (message, "beginEditing"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        remoteDocument->documentController.documentControllerInterface->beginEditing (remoteDocument->documentController.documentControllerRef);
        return {};
    }
    else if (isMethodCall (message, "endEditing"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        remoteDocument->documentController.documentControllerInterface->endEditing (remoteDocument->documentController.documentControllerRef);
        return {};
    }
    else if (isMethodCall (message, "createAudioSource"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };

        auto remoteAudioSource { new ARARemoteAudioSource };
        remoteAudioSource->mainHostRef = message.getArgValue<ARAAudioSourceHostRef> ("hostRef");

        auto properties { message.getArgValue<IPCMessage> ("properties") };
        remoteAudioSource->name = properties.getArgValue<std::string> ("name");
        remoteAudioSource->properties.name = remoteAudioSource->name.c_str ();
        remoteAudioSource->persistentID = properties.getArgValue<std::string> ("persistentID");
        remoteAudioSource->properties.persistentID = remoteAudioSource->persistentID.c_str ();
        remoteAudioSource->properties.sampleCount = properties.getArgValue<ARASampleCount> ("sampleCount");
        remoteAudioSource->properties.sampleRate = properties.getArgValue<ARASampleRate> ("sampleRate");
        remoteAudioSource->properties.channelCount = properties.getArgValue<ARAChannelCount> ("channelCount");
        remoteAudioSource->properties.merits64BitSamples = properties.getArgValue<ARABool> ("merits64BitSamples");

        remoteAudioSource->plugInRef = remoteDocument->documentController.documentControllerInterface->createAudioSource (
                                        remoteDocument->documentController.documentControllerRef, reinterpret_cast<ARAAudioSourceHostRef> (remoteAudioSource), &remoteAudioSource->properties);
        return { "audioSourceRef", remoteAudioSource };
    }
    else if (isMethodCall (message, "enableAudioSourceSamplesAccess"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        auto enable { message.getArgValue<ARABool> ("enable") };
        remoteDocument->documentController.documentControllerInterface->enableAudioSourceSamplesAccess (remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, enable);
        return {};
    }
    else if (isMethodCall (message, "destroyAudioSource"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        remoteDocument->documentController.documentControllerInterface->destroyAudioSource (remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef);

        delete remoteAudioSource;
        return {};
    }
    else if (isMethodCall (message, "isAudioSourceContentAvailable"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        auto contentType { message.getArgValue<ARAContentType> ("contentType") };

        // since we've not modelled this in our IPC yet, we are sending it here so the plug-in can update before querying the state
        remoteDocument->documentController.documentControllerInterface->notifyModelUpdates (remoteDocument->documentController.documentControllerRef);

        auto available = remoteDocument->documentController.documentControllerInterface->isAudioSourceContentAvailable (remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentType);
        return { "result", available };
    }
    else if (isMethodCall (message, "isAudioSourceContentAnalysisIncomplete"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        auto contentType { message.getArgValue<ARAContentType> ("contentType") };

        // since we've not modelled this in our IPC yet, we are sending it here so the plug-in can update before querying the state
        remoteDocument->documentController.documentControllerInterface->notifyModelUpdates (remoteDocument->documentController.documentControllerRef);

        auto incomplete = remoteDocument->documentController.documentControllerInterface->isAudioSourceContentAnalysisIncomplete (remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentType);
        return { "result", incomplete };
    }
    else if (isMethodCall (message, "requestAudioSourceContentAnalysis"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        auto contentTypes { message.getArgValue<std::vector<ARAContentType>> ("contentTypes") };
        remoteDocument->documentController.documentControllerInterface->requestAudioSourceContentAnalysis (remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentTypes.size (), contentTypes.data ());
        return {};
    }
    else if (isMethodCall (message, "createAudioSourceContentReader"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteAudioSource { message.getArgValue<ARARemoteAudioSource*> ("audioSourceRef") };
        auto contentType { message.getArgValue<ARAContentType> ("contentType") };
        /* optional contentTimeRange argument not implemented here to keep the example simple */

        auto remoteContentReader { new ARARemoteContentReader };
        remoteContentReader->plugInRef = remoteDocument->documentController.documentControllerInterface->createAudioSourceContentReader (remoteDocument->documentController.documentControllerRef,
                                            remoteAudioSource->plugInRef, contentType, nullptr);
        remoteContentReader->contentType = contentType;
        return { "contentReaderRef", remoteContentReader };
    }
    else if (isMethodCall (message, "getContentReaderEventCount"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteContentReader { message.getArgValue<ARARemoteContentReader*> ("contentReaderRef") };
        ARAInt32 eventCount { remoteDocument->documentController.documentControllerInterface->getContentReaderEventCount (remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef) };
        return { "result", eventCount };
    }
    else if (isMethodCall (message, "getContentReaderDataForEvent"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteContentReader { message.getArgValue<ARARemoteContentReader*> ("contentReaderRef") };
        auto eventIndex { message.getArgValue<ARAInt32> ("eventIndex") };

        const void* eventData = remoteDocument->documentController.documentControllerInterface->getContentReaderDataForEvent (remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef, eventIndex);
        if (remoteContentReader->contentType == kARAContentTypeNotes)
        {
            const auto note { static_cast<const ARAContentNote*> (eventData) };
            return { "frequency", note->frequency,
                     "pitchNumber", note->pitchNumber,
                     "volume", note->volume,
                     "startPosition", note->startPosition,
                     "attackDuration", note->attackDuration,
                     "noteDuration", note->noteDuration,
                     "signalDuration", note->signalDuration };
        }

        ARA_INTERNAL_ASSERT (false && "other content types are not implemented yet");
        return {};
    }
    else if (isMethodCall (message, "destroyContentReader"))
    {
        auto remoteDocument { message.getArgValue<ARARemoteDocument*> ("controllerRef") };
        auto remoteContentReader { message.getArgValue<ARARemoteContentReader*> ("contentReaderRef") };
        remoteDocument->documentController.documentControllerInterface->destroyContentReader (remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef);

        delete remoteContentReader;
        return {};
    }
    ARA_INTERNAL_ASSERT (false && "unhandled message ID");
    return {};
}


// asserts
#if ARA_VALIDATE_API_CALLS
static ARAAssertFunction assertFunction = &ARAInterfaceAssert;
#else
static ARAAssertFunction assertFunction = nullptr;
#endif
static ARAAssertFunction * assertFunctionReference = &assertFunction;

ARA_SETUP_DEBUG_MESSAGE_PREFIX("IPC-PlugIn");

int main (int argc, const char * argv[])
{
    // load plug-in
    const SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig { kARAAPIGeneration_2_0_Final, assertFunctionReference };

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aufx', 'AraT', 'ADeC');
//  AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aufx', 'Ara3', 'ADeC');
//  AudioUnitComponent audioUnitComponent = AudioUnitPrepareComponentWithIDs ('aumf', 'MPLG', 'CLMY');
    ARA_INTERNAL_ASSERT (audioUnitComponent != nullptr);

    factory = AudioUnitGetARAFactory (audioUnitComponent);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3Binary vst3Binary = VST3LoadBinary ("ARATestPlugIn.vst3");
//  VST3Binary vst3Binary = VST3LoadBinary ("/Library/Audio/Plug-Ins/VST3/Melodyne.vst3");
    ARA_INTERNAL_ASSERT (vst3Binary != nullptr);

    factory = VST3GetARAFactory (vst3Binary, nullptr);
#endif

    if (factory == nullptr)
    {
        ARA_WARN ("this plug-in doesn't support ARA.");
        return -1;                // this plug-in doesn't support ARA.
    }
    ARA_VALIDATE_API_CONDITION (factory->structSize >= kARAFactoryMinSize);

    if (factory->lowestSupportedApiGeneration > kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN ("this plug-in only supports newer generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }
    if (factory->highestSupportedApiGeneration < kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN ("this plug-in only supports older generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }

#if ARA_VALIDATE_API_CALLS
    ARASetExternalAssertReference (assertFunctionReference);
#endif

    ARA_VALIDATE_API_CONDITION (factory->factoryID != nullptr);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->factoryID) > 5);  // at least "xx.y." needed to form a valid url-based unique ID
    ARA_VALIDATE_API_CONDITION (factory->initializeARAWithConfiguration != nullptr);
    ARA_VALIDATE_API_CONDITION (factory->uninitializeARA != nullptr);
    ARA_VALIDATE_API_CONDITION (factory->createDocumentControllerWithDocument != nullptr);

    factory->initializeARAWithConfiguration (&interfaceConfig);

    ARA_LOG ("launched successfully and loaded plug-in %s.", factory->plugInName);

    // publish model port to main process
    auto modelPortToPlugIn { IPCPort::createPublishingID ("com.arademocompany.IPCDemo.modelPortToPlugIn", &modelPortToPlugInCallBack) };

    // connect to main process for requesting audio access
    audioAccessFromPlugInPort = IPCPort::createConnectedToID ("com.arademocompany.IPCDemo.audioAccessFromPlugIn");

    // trigger run loop
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, DBL_MAX, false);

    // cleanup
    factory->uninitializeARA ();

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    // unloading is not supported for Audio Units
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3UnloadBinary (vst3Binary);
#endif

    ARA_LOG ("completed.");
    return 0;
}
