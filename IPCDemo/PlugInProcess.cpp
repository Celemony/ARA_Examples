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
    ARAChannelCount channelCount { 1 };
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

// \todo convert code to properly support the host/plug-in dispatcher toRef() and fromRef()!
ARADocumentControllerRef toRef (ARARemoteDocument* ptr)
{
    return reinterpret_cast<ARADocumentControllerRef> (ptr);
}
ARAAudioSourceRef toRef (ARARemoteAudioSource* ptr)
{
    return reinterpret_cast<ARAAudioSourceRef> (ptr);
}
ARAContentReaderRef toRef (ARARemoteContentReader* ptr)
{
    return reinterpret_cast<ARAContentReaderRef> (ptr);
}
ARARemoteDocument* fromRef (ARADocumentControllerRef ref)
{
    return reinterpret_cast<ARARemoteDocument*> (ref);
}
ARARemoteAudioSource* fromRef (ARAAudioSourceRef ref)
{
    return reinterpret_cast<ARARemoteAudioSource*> (ref);
}
ARARemoteContentReader* fromRef (ARAContentReaderRef ref)
{
    return reinterpret_cast<ARARemoteContentReader*> (ref);
}


// ARAAudioAccessControllerInterface
ARAAudioReaderHostRef ARA_CALL ARACreateAudioReaderForSource (ARAAudioAccessControllerHostRef controllerHostRef,
                                                                ARAAudioSourceHostRef hostAudioSourceRef, ARABool use64BitSamples)
{
    auto remoteAudioSource { reinterpret_cast<ARARemoteAudioSource*> (hostAudioSourceRef) };
    auto remoteAudioReader { new ARARemoteAudioReader };
    remoteAudioReader->audioSource = remoteAudioSource;
    remoteAudioReader->use64BitSamples = use64BitSamples;

    remoteAudioReader->mainHostRef = decodeReply<ARAAudioReaderHostRef> (audioAccessFromPlugInPort.sendAndAwaitReply (
                                                        HOST_METHOD_ID (ARAAudioAccessControllerInterface, createAudioReaderForSource),
                                                        encodeArguments (controllerHostRef, remoteAudioSource->mainHostRef, use64BitSamples)));
    return reinterpret_cast<ARAAudioReaderHostRef> (remoteAudioReader);
}

void _swap (float* ptr)
{
    *((uint32_t*) ptr) = CFSwapInt32 (*((const uint32_t*) ptr));
}
void _swap (double* ptr)
{
    *((uint64_t*) ptr) = CFSwapInt64 (*((const uint64_t*) ptr));
}

template<typename FloatT>
ARABool _readAudioSamples (const IPCMessage& reply, ARASampleCount samplesPerChannel, ARAChannelCount channelCount, void* const buffers[])
{
    const auto decoded { decodeReply<ARAIPCReadSamplesReply> (reply) };
    const bool success { decoded.dataCount > 0 };
    if (success != kARAFalse)
        ARA_INTERNAL_ASSERT (decoded.dataCount == sizeof (FloatT) * static_cast<size_t> (samplesPerChannel * channelCount));
    else
        ARA_INTERNAL_ASSERT (decoded.dataCount == 0);

    const auto endian { CFByteOrderGetCurrent() };
    ARA_INTERNAL_ASSERT (endian != CFByteOrderUnknown);
    bool needSwap { endian != ((decoded.isLittleEndian != kARAFalse) ? CFByteOrderLittleEndian : CFByteOrderBigEndian) };

    auto sourcePtr = decoded.data;
    const auto channelSize { sizeof (FloatT) * static_cast<size_t> (samplesPerChannel) };
    for (auto i { 0 }; i < channelCount; ++i)
    {
        if (success != kARAFalse)
        {
            std::memcpy (buffers[i], sourcePtr, channelSize);
            if (needSwap)
            {
                for (auto f { 0 }; f < samplesPerChannel; ++f)
                    _swap (static_cast<FloatT*> (buffers[i]) + f);
            }
        }
        else
        {
            std::memset (buffers[i], 0, channelSize);
        }
        sourcePtr += channelSize;
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
        void* buffers2[remoteAudioReader->audioSource->channelCount];
        for (auto i { 0 }; i < remoteAudioReader->audioSource->channelCount; ++i)
            buffers2[i] = static_cast<uint8_t*> (buffers[i]) + static_cast<size_t> (samplesPerChannel1) * sampleSize;

        if (result1 != kARAFalse)
        {
            return ARAReadAudioSamples (controllerHostRef, audioReaderHostRef, samplePosition + samplesPerChannel1, samplesPerChannel2, buffers2);
        }
        else
        {
            for (auto i { 0 }; i < remoteAudioReader->audioSource->channelCount; ++i)
                std::memset (buffers2[i], 0, static_cast<size_t> (samplesPerChannel2) * sampleSize);
            return kARAFalse;
        }
    }

    static os_unfair_lock_s lock { OS_UNFAIR_LOCK_INIT };
    os_unfair_lock_lock (&lock);

    const auto reply { audioAccessFromPlugInPort.sendAndAwaitReply (
                                HOST_METHOD_ID (ARAAudioAccessControllerInterface, readAudioSamples),
                                encodeArguments (controllerHostRef, remoteAudioReader->mainHostRef, samplePosition, samplesPerChannel)) };
    const auto result { (remoteAudioReader->use64BitSamples != kARAFalse) ?
                        _readAudioSamples<double> (reply, samplesPerChannel, remoteAudioReader->audioSource->channelCount, buffers):
                        _readAudioSamples<float> (reply, samplesPerChannel, remoteAudioReader->audioSource->channelCount, buffers)};

    os_unfair_lock_unlock (&lock);
    return result;
}

void ARA_CALL ARADestroyAudioReader (ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef audioReaderHostRef)
{
    auto remoteAudioReader { reinterpret_cast<ARARemoteAudioReader *> (audioReaderHostRef) };
    audioAccessFromPlugInPort.sendWithoutReply (HOST_METHOD_ID (ARAAudioAccessControllerInterface, destroyAudioReader),
                                                encodeArguments (controllerHostRef, remoteAudioReader->mainHostRef));
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

IPCMessage modelPortToPlugInCallBack (const int32_t messageID, const IPCMessage& message)
{
    if (messageID == kCreateDocumentControllerMethodID)
    {
        auto remoteDocument { new ARARemoteDocument };
        remoteDocument->hostInstance.audioAccessControllerInterface = &hostAudioAccessControllerInterface;
        remoteDocument->hostInstance.archivingControllerInterface = &hostArchivingControllerInterface;

        ARADocumentProperties properties;
        decodeArguments(message, remoteDocument->hostInstance.audioAccessControllerHostRef, properties);

        const ARADocumentControllerInstance * documentControllerInstance = factory->createDocumentControllerWithDocument (&remoteDocument->hostInstance, &properties);
        ARA_VALIDATE_API_CONDITION (documentControllerInstance != nullptr);
        ARA_VALIDATE_API_INTERFACE (documentControllerInstance->documentControllerInterface, ARADocumentControllerInterface);
        remoteDocument->documentController = *documentControllerInstance;
        return encodeReply (toRef (remoteDocument));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyDocumentController))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);
        auto remoteDocument { fromRef (controllerRef) };
        remoteDocument->documentController.documentControllerInterface->destroyDocumentController (remoteDocument->documentController.documentControllerRef);

        delete remoteDocument;
        CFRunLoopStop (CFRunLoopGetCurrent ()); // will terminate run loop & shut down

        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, beginEditing))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);
        auto remoteDocument { fromRef (controllerRef) };
        remoteDocument->documentController.documentControllerInterface->beginEditing (remoteDocument->documentController.documentControllerRef);
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, endEditing))
    {
        ARADocumentControllerRef controllerRef;
        decodeArguments (message, controllerRef);
        auto remoteDocument { fromRef (controllerRef) };
        remoteDocument->documentController.documentControllerInterface->endEditing (remoteDocument->documentController.documentControllerRef);
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSource))
    {
        auto remoteAudioSource { new ARARemoteAudioSource };

        ARADocumentControllerRef controllerRef;
        ARAAudioSourceProperties properties;
        decodeArguments (message, controllerRef, remoteAudioSource->mainHostRef, properties);
        auto remoteDocument { fromRef (controllerRef) };

        remoteAudioSource->channelCount = properties.channelCount;
        remoteAudioSource->plugInRef = remoteDocument->documentController.documentControllerInterface->createAudioSource (
                                            remoteDocument->documentController.documentControllerRef, reinterpret_cast<ARAAudioSourceHostRef> (remoteAudioSource), &properties);
        return encodeReply (toRef (remoteAudioSource));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, enableAudioSourceSamplesAccess))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARABool enable;
        decodeArguments (message, controllerRef, audioSourceRef, enable);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };
        remoteDocument->documentController.documentControllerInterface->enableAudioSourceSamplesAccess (
                remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, enable);
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyAudioSource))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        decodeArguments (message, controllerRef, audioSourceRef);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };
        remoteDocument->documentController.documentControllerInterface->destroyAudioSource (
                remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef);

        delete remoteAudioSource;
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAvailable))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioSourceRef, contentType);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };

        // since we've not modelled this in our IPC yet, we are sending it here so the plug-in can update before querying the state
        remoteDocument->documentController.documentControllerInterface->notifyModelUpdates (remoteDocument->documentController.documentControllerRef);

        return encodeReply (remoteDocument->documentController.documentControllerInterface->isAudioSourceContentAvailable (
                                        remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, isAudioSourceContentAnalysisIncomplete))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        decodeArguments (message, controllerRef, audioSourceRef, contentType);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };

        // since we've not modelled this in our IPC yet, we are sending it here so the plug-in can update before querying the state
        remoteDocument->documentController.documentControllerInterface->notifyModelUpdates (remoteDocument->documentController.documentControllerRef);

        return encodeReply (remoteDocument->documentController.documentControllerInterface->isAudioSourceContentAnalysisIncomplete (
                                        remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentType));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, requestAudioSourceContentAnalysis))
    {
        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        std::vector<ARAContentType> contentTypes;
        decodeArguments (message, controllerRef, audioSourceRef, contentTypes);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };
        remoteDocument->documentController.documentControllerInterface->requestAudioSourceContentAnalysis (
                remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentTypes.size (), contentTypes.data ());
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, createAudioSourceContentReader))
    {
        auto remoteContentReader { new ARARemoteContentReader };

        ARADocumentControllerRef controllerRef;
        ARAAudioSourceRef audioSourceRef;
        ARAContentType contentType;
        OptionalArgument<ARAContentTimeRange*> timeRange;
        decodeArguments (message, controllerRef, audioSourceRef, contentType, timeRange);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteAudioSource { fromRef (audioSourceRef) };

        remoteContentReader->plugInRef = remoteDocument->documentController.documentControllerInterface->createAudioSourceContentReader (
                        remoteDocument->documentController.documentControllerRef, remoteAudioSource->plugInRef, contentType, (timeRange.second) ? &timeRange.first : nullptr);
        remoteContentReader->contentType = contentType;
        return encodeReply (toRef (remoteContentReader));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderEventCount))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        decodeArguments (message, controllerRef, contentReaderRef);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteContentReader { fromRef (contentReaderRef) };
        return encodeReply (remoteDocument->documentController.documentControllerInterface->getContentReaderEventCount (
                                    remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef));
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, getContentReaderDataForEvent))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        ARAInt32 eventIndex;
        decodeArguments (message, controllerRef, contentReaderRef, eventIndex);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteContentReader { fromRef (contentReaderRef) };

        const void* eventData = remoteDocument->documentController.documentControllerInterface->getContentReaderDataForEvent (
                                    remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef, eventIndex);
        if (remoteContentReader->contentType == kARAContentTypeNotes)
            return encodeReply (*static_cast<const ARAContentNote*> (eventData));

        ARA_INTERNAL_ASSERT (false && "other content types are not implemented yet");
        return {};
    }
    else if (messageID == PLUGIN_METHOD_ID (ARADocumentControllerInterface, destroyContentReader))
    {
        ARADocumentControllerRef controllerRef;
        ARAContentReaderRef contentReaderRef;
        decodeArguments (message, controllerRef, contentReaderRef);
        auto remoteDocument { fromRef (controllerRef) };
        auto remoteContentReader { fromRef (contentReaderRef) };
        remoteDocument->documentController.documentControllerInterface->destroyContentReader (
                remoteDocument->documentController.documentControllerRef, remoteContentReader->plugInRef);
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
