//------------------------------------------------------------------------------
//! \file       MiniHost.c
//!             Implementation of a minimal ARA host example.
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
// This is a minimalistic ARA host app written in C to illustrate the core steps
// required to load and setup an ARA plug-in.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#if defined(__cplusplus)
    #error "This simple test app is also used to verify ARA is actually C-compatible. Switch off C++ in your compiler for this file and try again."
#endif

#if defined(__APPLE__)
    #include <CoreServices/CoreServices.h>
#endif

// C includes
#include <math.h>
#include <stdio.h>
#include <string.h>

// ARA API includes
#include "ARA_API/ARAInterface.h"

// ARA framework includes
#include "ARA_Library/Debug/ARADebug.h"

// in this simple demo application, we need logging to be always enabled, even in release builds.
// this needs to be done by configuring the project files properly - we verify this precondition here.
#if !ARA_ENABLE_DEBUG_OUTPUT
    #error "ARA_ENABLE_DEBUG_OUTPUT not configured properly in the project"
#endif

// test signal generator
#include "ExamplesCommon/SignalProcessing/PulsedSineSignal.h"


// list of available companion APIs
#define PLUGIN_FORMAT_AU   1
#define PLUGIN_FORMAT_VST3 2

// the project files must pick one of the above formats by defining PLUGIN_FORMAT
#if !defined(PLUGIN_FORMAT)
    #error "PLUGIN_FORMAT not configured properly in the project"
#endif

// Companion API includes
#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    #include "ExamplesCommon/PlugInHosting/AudioUnitLoader.h"

    #define ARA_PLUGIN_AUDIOUNIT_IDS 'aufx', 'AraT', 'ADeC'
//  #define ARA_PLUGIN_AUDIOUNIT_IDS 'aumf', 'MPLG', 'CLMY'
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    #include "ExamplesCommon/PlugInHosting/VST3Loader.h"

    #define ARA_PLUGIN_VST3_BINARY "ARATestPlugIn.vst3"
//  #define ARA_PLUGIN_VST3_BINARY "/Library/Audio/Plug-Ins/VST3/Melodyne.vst3"
#else
    #error "PLUGIN_FORMAT not configured properly in the project"
#endif


// some constants configuring our fake audio source
#define kTestAudioSourceSampleRate 44100.0  /* Hertz */
#define kTestAudioSourceDuration 5.0        /* seconds */
#define kTestAudioSourceSampleCount ((ARASampleCount) (kTestAudioSourceSampleRate * kTestAudioSourceDuration + 0.5))

// we are not using actual objects in this test implementation, so here's a few constants that are used where actual host code would use object pointers or array indices
#define kHostAudioSourceHostRef ((ARAAudioSourceHostRef) 1)
#define kHostAudioModificationHostRef ((ARAAudioModificationHostRef) 2)
#define kHostMusicalContextHostRef ((ARAMusicalContextHostRef) 3)
#define kHostRegionSequenceHostRef ((ARARegionSequenceHostRef) 4)
#define kHostPlaybackRegionHostRef ((ARAPlaybackRegionHostRef) 5)
#define kAudioAccessControllerHostRef ((ARAAudioAccessControllerHostRef) 10)
#define kArchivingControllerHostRef ((ARAArchivingControllerHostRef) 11)
#define kContentAccessControllerHostRef ((ARAContentAccessControllerHostRef) 12)
#define kModelUpdateControllerHostRef ((ARAModelUpdateControllerHostRef) 13)
#define kAudioReader32BitHostRef ((ARAAudioReaderHostRef) 20)
#define kAudioReader64BitHostRef ((ARAAudioReaderHostRef) 21)
#define kHostTempoContentReaderHostRef ((ARAContentReaderHostRef) 30)
#define kHostSignaturesContentReaderHostRef ((ARAContentReaderHostRef) 31)


// ARAAudioAccessControllerInterface (required)
static ARAAudioReaderHostRef ARA_CALL ARACreateAudioReaderForSource(ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARABool use64BitSamples)
{
    ARAAudioReaderHostRef readerHostRef = (use64BitSamples) ? kAudioReader64BitHostRef : kAudioReader32BitHostRef;
    ARA_LOG("createAudioReaderForSource() returns fake host ref %p", readerHostRef);
    return readerHostRef;
}
static ARABool ARA_CALL ARAReadAudioSamples(ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef readerHostRef,
                                            ARASamplePosition samplePosition, ARASampleCount samplesPerChannel, void * const buffers[])
{
    RenderPulsedSineSignal (samplePosition, kTestAudioSourceSampleRate, kTestAudioSourceSampleCount,
                            1, samplesPerChannel, buffers, (readerHostRef == kAudioReader64BitHostRef) ? kARATrue : kARAFalse);
    return kARATrue;
}
static void ARA_CALL ARADestroyAudioReader(ARAAudioAccessControllerHostRef controllerHostRef, ARAAudioReaderHostRef readerHostRef)
{
    ARA_LOG("destroyAudioReader() called for fake host ref %p", readerHostRef);
}
static const ARAAudioAccessControllerInterface hostAudioAccessControllerInterface = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioAccessControllerInterface, destroyAudioReader),
                                                                                        &ARACreateAudioReaderForSource, &ARAReadAudioSamples, &ARADestroyAudioReader };


// ARAArchivingControllerInterface
static ARASize ARA_CALL ARAGetArchiveSize(ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return 0;
}
static ARABool ARA_CALL ARAReadBytesFromArchive(ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef,
                                                ARASize position, ARASize length, ARAByte buffer[])
{
    memset (&buffer[position], 0, length);
    return kARAFalse;
}
static ARABool ARA_CALL ARAWriteBytesToArchive(ARAArchivingControllerHostRef controllerHostRef, ARAArchiveWriterHostRef archiveWriterHostRef,
                                               ARASize position, ARASize length, const ARAByte buffer[])
{
    return kARATrue;
}
static void ARA_CALL ARANotifyDocumentArchivingProgress(ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
static void ARA_CALL ARANotifyDocumentUnarchivingProgress(ARAArchivingControllerHostRef controllerHostRef, float value)
{
}
static ARAPersistentID ARA_CALL ARAGetDocumentArchiveID(ARAArchivingControllerHostRef controllerHostRef, ARAArchiveReaderHostRef archiveReaderHostRef)
{
    return NULL;
}
static const ARAArchivingControllerInterface hostArchivingInterface = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAArchivingControllerInterface, getDocumentArchiveID),
                                                                        &ARAGetArchiveSize, &ARAReadBytesFromArchive, &ARAWriteBytesToArchive,
                                                                        &ARANotifyDocumentArchivingProgress, &ARANotifyDocumentUnarchivingProgress,
                                                                        &ARAGetDocumentArchiveID };


// ARAContentAccessControllerInterface
static const ARAContentBarSignature signatureDefinition = { 4, 4, 0.0 };
static const ARAContentTempoEntry tempoSyncPoints[2] = { { 0.0, 0.0 }, { 0.5, 1.0 } };  // 120 bpm
static ARABool ARA_CALL ARAIsMusicalContextContentAvailable(ARAContentAccessControllerHostRef controllerHostRef, ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type)
{
    if (type == kARAContentTypeTempoEntries)
        return kARATrue;

    if (type == kARAContentTypeBarSignatures)
        return kARATrue;

    return kARAFalse;
}
static ARAContentGrade ARA_CALL ARAGetMusicalContextContentGrade(ARAContentAccessControllerHostRef controllerHostRef, ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type)
{
    if (type == kARAContentTypeTempoEntries)
        return kARAContentGradeAdjusted;

    if (type == kARAContentTypeBarSignatures)
        return kARAContentGradeAdjusted;

    return kARAContentGradeInitial;
}
static ARAContentReaderHostRef ARA_CALL ARACreateMusicalContextContentReader(ARAContentAccessControllerHostRef controllerHostRef, ARAMusicalContextHostRef musicalContextHostRef, ARAContentType type, const ARAContentTimeRange * range)
{
    if (type == kARAContentTypeTempoEntries)
    {
        ARA_LOG("createMusicalContextContentReader() called for fake tempo reader host ref %p", kHostTempoContentReaderHostRef);
        return kHostTempoContentReaderHostRef;
    }

    if (type == kARAContentTypeBarSignatures)
    {
        ARA_LOG("createMusicalContextContentReader() called for fake signatures reader host ref %p", kHostSignaturesContentReaderHostRef);
        return kHostSignaturesContentReaderHostRef;
    }

    return NULL;
}
static ARABool ARA_CALL ARAIsAudioSourceContentAvailable(ARAContentAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type)
{
    return kARAFalse;
}
static ARAContentGrade ARA_CALL ARAGetAudioSourceContentGrade(ARAContentAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type)
{
    return kARAContentGradeInitial;
}
static ARAContentReaderHostRef ARA_CALL ARACreateAudioSourceContentReader(ARAContentAccessControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARAContentType type, const ARAContentTimeRange * range)
{
    return NULL;
}
static ARAInt32 ARA_CALL ARAGetContentReaderEventCount(ARAContentAccessControllerHostRef controllerHostRef, ARAContentReaderHostRef readerHostRef)
{
    if (readerHostRef == kHostTempoContentReaderHostRef)
        return sizeof(tempoSyncPoints) / sizeof(tempoSyncPoints[0]);

    if (readerHostRef == kHostSignaturesContentReaderHostRef)
        return 1;

    return 0;
}
static const void * ARA_CALL ARAGetContentReaderDataForEvent(ARAContentAccessControllerHostRef controllerHostRef, ARAContentReaderHostRef readerHostRef, ARAInt32 eventIndex)
{
    if (readerHostRef == kHostTempoContentReaderHostRef)
        return &tempoSyncPoints[eventIndex];

    if (readerHostRef == kHostSignaturesContentReaderHostRef)
        return &signatureDefinition;

    return NULL;
}
static void ARA_CALL ARADestroyContentReader(ARAContentAccessControllerHostRef controllerHostRef, ARAContentReaderHostRef readerHostRef)
{
    ARA_LOG("plug-in destroyed content reader host ref %p", readerHostRef);
}
static const ARAContentAccessControllerInterface hostContentAccessControllerInterface = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAContentAccessControllerInterface, destroyContentReader),
                                                                        &ARAIsMusicalContextContentAvailable, &ARAGetMusicalContextContentGrade, &ARACreateMusicalContextContentReader,
                                                                        &ARAIsAudioSourceContentAvailable, &ARAGetAudioSourceContentGrade, &ARACreateAudioSourceContentReader,
                                                                        &ARAGetContentReaderEventCount, &ARAGetContentReaderDataForEvent, &ARADestroyContentReader };


// ARAModelUpdateControllerInterface
static void ARA_CALL ARANotifyAudioSourceAnalysisProgress(ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, ARAAnalysisProgressState state, float value)
{
    switch (state)
    {
        case kARAAnalysisProgressStarted:
        {
            ARA_LOG("audio source analysis started with progress %.f%%.", 100.0 * value);
            break;
        }
        case kARAAnalysisProgressUpdated:
        {
            ARA_LOG("audio source analysis progress is %.f%%.", 100.0 * value);
            break;
        }
        case kARAAnalysisProgressCompleted:
        {
            ARA_LOG("audio source analysis finished with progress %.f%%.", 100.0 * value);
            break;
        }
    }
}
static void ARA_CALL ARANotifyAudioSourceContentChanged(ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioSourceHostRef audioSourceHostRef, const ARAContentTimeRange * range, ARAContentUpdateFlags contentFlags)
{
    ARA_LOG("audio source content was updated in range %.3f-%.3f, flags %X", (range) ? range->start : 0.0, (range) ? range->start + range->duration : kTestAudioSourceDuration, contentFlags);
}
static void ARA_CALL ARANotifyAudioModificationContentChanged(ARAModelUpdateControllerHostRef controllerHostRef, ARAAudioModificationHostRef audioModificationHostRef, const ARAContentTimeRange * range, ARAContentUpdateFlags contentFlags)
{
    ARA_LOG("audio modification content was updated in range %.3f-%.3f, flags %X", (range) ? range->start : 0.0, (range) ? range->start + range->duration : kTestAudioSourceDuration, contentFlags);
}
static void ARA_CALL ARANotifyPlaybackRegionContentChanged(ARAModelUpdateControllerHostRef controllerHostRef, ARAPlaybackRegionHostRef playbackRegionHostRef, const ARAContentTimeRange * range, ARAContentUpdateFlags contentFlags)
{
    if (range)
        ARA_LOG("playback region content was updated in range %.3f-%.3f, flags %X", range->start, range->start + range->duration, contentFlags);
    else
        ARA_LOG("playback region content was updated from start-head to start+duration+tail, flags %X", contentFlags);
}
static const ARAModelUpdateControllerInterface hostModelUpdateControllerInterface = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAModelUpdateControllerInterface, notifyPlaybackRegionContentChanged),
                                                                                        &ARANotifyAudioSourceAnalysisProgress, &ARANotifyAudioSourceContentChanged,
                                                                                        &ARANotifyAudioModificationContentChanged, &ARANotifyPlaybackRegionContentChanged };

// asserts
#if ARA_VALIDATE_API_CALLS
    static ARAAssertFunction assertFunction = &ARAInterfaceAssert;
#else
    static ARAAssertFunction assertFunction = NULL;
#endif
static ARAAssertFunction * assertFunctionReference = &assertFunction;


// main
int main (int argc, const char * argv[])
{
    ARAInterfaceConfiguration interfaceConfig = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAInterfaceConfiguration, assertFunctionAddress),
                                                  kARAAPIGeneration_2_0_Final, NULL /* asserts will be configured later if needed */ };

    const ARAPlugInExtensionInstance * plugInInstance = NULL;

    const ARAFactory * factory = NULL;

    const ARADocumentControllerInstance * documentControllerInstance = NULL;
    ARADocumentControllerRef documentControllerRef;
    const ARADocumentControllerInterface * documentControllerInterface = NULL;

    ARADocumentControllerHostInstance documentEntry = { ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentControllerHostInstance, playbackControllerInterface),
                                                        kAudioAccessControllerHostRef, &hostAudioAccessControllerInterface,
                                                        kArchivingControllerHostRef, &hostArchivingInterface,
                                                        kContentAccessControllerHostRef, &hostContentAccessControllerInterface,
                                                        kModelUpdateControllerHostRef, &hostModelUpdateControllerInterface,
                                                        NULL, NULL /* no optional playback control in this simple example host */ };

    ARADocumentProperties documentProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARADocumentProperties, name), "Test document" };

    ARAMusicalContextProperties musicalContextProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAMusicalContextProperties, color),
                                                             NULL /* no name available */, 0, NULL /* no color available */ };
    ARAMusicalContextRef musicalContextRef;

    ARARegionSequenceProperties regionSequenceProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARARegionSequenceProperties, color), "Track 1", 0,
                                                             NULL /* this ref for context must be set properly before using the struct! */, NULL /* no color available */ };
    ARARegionSequenceRef regionSequenceRef;

    ARAAudioSourceProperties audioSourceProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioSourceProperties, merits64BitSamples),
                                                        "Test audio source", "audioSourceTestPersistentID",
                                                        kTestAudioSourceSampleCount, kTestAudioSourceSampleRate, 1, kARAFalse };
    ARAAudioSourceRef audioSourceRef;

    ARAAudioModificationProperties audioModificationProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAAudioModificationProperties, persistentID),
                                                                    "Test audio modification", "audioModificationTestPersistentID" };
    ARAAudioModificationRef audioModificationRef;

    ARAPlaybackRegionProperties playbackRegionProperties = { ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlaybackRegionProperties, color), kARAPlaybackTransformationNoChanges,
                                                            0.0, kTestAudioSourceDuration, 0.0, kTestAudioSourceDuration,
                                                            NULL, NULL /* these refs for context and sequence must be set properly before using the struct! */,
                                                            "Test playback region", NULL /* no color available */ };
    ARAPlaybackRegionRef playbackRegionRef;

    // this demo code only covers actual playback rendering, without support for editor rendering or view
    ARAPlugInInstanceRoleFlags roles = kARAPlaybackRendererRole /*| kARAEditorRendererRole | kARAEditorViewRole*/;

    enum { renderBlockSize = 128, renderBlockCount = 10 };
    double renderSampleRate = 44100.0;
    float outputData[renderBlockCount * renderBlockSize];
    int i;


    // load binary and initalize ARA
    ARASetupDebugMessagePrefix("ARAMiniHost");
    ARA_LOG("loading and initializing plug-in binary");

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnit audioUnit;
    AudioComponent audioComponent = AudioUnitFindValidARAComponentWithIDs(ARA_PLUGIN_AUDIOUNIT_IDS);
    ARA_INTERNAL_ASSERT(audioComponent != NULL);
    factory = AudioUnitGetARAFactory(audioComponent);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    struct VST3Effect * vst3Effect;
    struct VST3Binary * vst3Binary = VST3LoadBinary (ARA_PLUGIN_VST3_BINARY);
    ARA_INTERNAL_ASSERT(vst3Binary != NULL);
    factory = VST3GetARAFactory(vst3Binary, NULL);
#endif

    if (factory == NULL)
    {
        ARA_WARN("this plug-in doesn't support ARA.");
        return -1;                // this plug-in doesn't support ARA.
    }
    if (factory->lowestSupportedApiGeneration > kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN("this plug-in only supports newer generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }
    if (factory->highestSupportedApiGeneration < kARAAPIGeneration_2_0_Final)
    {
        ARA_WARN("this plug-in only supports older generations of ARA.");
        return -1;                // this plug-in doesn't support our generation of ARA.
    }

#if ARA_VALIDATE_API_CALLS
    ARASetExternalAssertReference(assertFunctionReference);
#endif

#ifndef NDEBUG
    interfaceConfig.assertFunctionAddress = assertFunctionReference;
#endif
    factory->initializeARAWithConfiguration(&interfaceConfig);


    // create a document
    ARA_LOG("creating a document controller and setting up the document");

    documentControllerInstance = factory->createDocumentControllerWithDocument(&documentEntry, &documentProperties);
    documentControllerRef = documentControllerInstance->documentControllerRef;
    documentControllerInterface = documentControllerInstance->documentControllerInterface;

    // start editing the document
    documentControllerInterface->beginEditing(documentControllerRef);

    // add a musical context to describe our timeline
    musicalContextRef = documentControllerInterface->createMusicalContext(documentControllerRef, kHostMusicalContextHostRef, &musicalContextProperties);

    // add a region sequence to describe our arrangement with a single track
    regionSequenceProperties.musicalContextRef = musicalContextRef;
    regionSequenceRef = documentControllerInterface->createRegionSequence(documentControllerRef, kHostRegionSequenceHostRef, &regionSequenceProperties);

    // add an audio source to it and an audio modification to contain the edits for this source
    audioSourceRef = documentControllerInterface->createAudioSource(documentControllerRef, kHostAudioSourceHostRef, &audioSourceProperties);

    audioModificationRef = documentControllerInterface->createAudioModification(documentControllerRef, audioSourceRef, kHostAudioModificationHostRef, &audioModificationProperties);

    // add a playback region to render modification in our musical context
    //playbackRegionProperties.musicalContextRef = musicalContextRef;            // deprecated in ARA 2, will be set only when supporting ARA 1 backwards compatibility
    playbackRegionProperties.regionSequenceRef = regionSequenceRef;
    if ((factory->supportedPlaybackTransformationFlags & kARAPlaybackTransformationTimestretch) != 0)   // enable time stretching if supported
    {
        playbackRegionProperties.transformationFlags |= kARAPlaybackTransformationTimestretch;
        playbackRegionProperties.durationInPlaybackTime *= 0.5;
    }
    playbackRegionRef = documentControllerInterface->createPlaybackRegion(documentControllerRef, audioModificationRef, kHostPlaybackRegionHostRef, &playbackRegionProperties);

    // done with editing the document, allow plug-in to access the audio
    documentControllerInterface->endEditing(documentControllerRef);
    documentControllerInterface->enableAudioSourceSamplesAccess(documentControllerRef, audioSourceRef, kARATrue);


    // --- from here on, the model is set up and analysis can be used - actual rendering however requiers the following render setup too. ---


    // create companion plug-in and bind it to the ARA document controller
    ARA_LOG("creating plug-in instance and binding it to the ARA document contoller");
#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    audioUnit = AudioUnitOpen(audioComponent);
    plugInInstance = AudioUnitBindToARADocumentController(audioUnit, documentControllerRef, roles);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    vst3Effect = VST3CreateEffect (vst3Binary, NULL);
    plugInInstance = VST3BindToARADocumentController(vst3Effect, documentControllerRef, roles);
#endif

    // prepare rendering
    ARA_LOG("configuring rendering");
    plugInInstance->playbackRendererInterface->addPlaybackRegion(plugInInstance->playbackRendererRef, playbackRegionRef);
#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnitStartRendering(audioUnit, renderBlockSize, renderSampleRate);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3StartRendering(vst3Effect, renderBlockSize, renderSampleRate);
#endif


    // --- the world is set up, every thing is good to go - real code would do something useful with the plug-in now. ---


    // perform rendering
    ARA_LOG("performing rendering.");

    memset(outputData, 0, sizeof(outputData));
    for (i = 0; i < renderBlockCount; ++i)
    {
        int samplePosition = i * renderBlockSize;
#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
        AudioUnitRenderBuffer(audioUnit, renderBlockSize, samplePosition, outputData + samplePosition);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
        VST3RenderBuffer(vst3Effect, renderBlockSize, renderSampleRate, samplePosition, outputData + samplePosition);
#endif
    }

    // shut everything down again
    ARA_LOG("destroying the document again");

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnitStopRendering(audioUnit);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3StopRendering(vst3Effect);
#endif

    plugInInstance->playbackRendererInterface->removePlaybackRegion(plugInInstance->playbackRendererRef, playbackRegionRef);

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    AudioUnitClose(audioUnit);
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3DestroyEffect (vst3Effect);
#endif

    documentControllerInterface->enableAudioSourceSamplesAccess(documentControllerRef, audioSourceRef, kARAFalse);

    documentControllerInterface->beginEditing(documentControllerRef);
    documentControllerInterface->destroyPlaybackRegion(documentControllerRef, playbackRegionRef);
    documentControllerInterface->destroyAudioModification(documentControllerRef, audioModificationRef);
    documentControllerInterface->destroyAudioSource(documentControllerRef, audioSourceRef);
    documentControllerInterface->destroyRegionSequence(documentControllerRef, regionSequenceRef);
    documentControllerInterface->destroyMusicalContext(documentControllerRef, musicalContextRef);
    documentControllerInterface->endEditing(documentControllerRef);
    documentControllerInterface->destroyDocumentController(documentControllerRef);

    factory->uninitializeARA();

#if PLUGIN_FORMAT == PLUGIN_FORMAT_AU
    // unloading is not supported for Audio Units
#elif PLUGIN_FORMAT == PLUGIN_FORMAT_VST3
    VST3UnloadBinary(vst3Binary);
#endif

    ARA_LOG("teardown completed");

    return 0;
}
