//------------------------------------------------------------------------------
//! \file       main.cpp
//!             main implementation of the SDK testhost example
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2022, Celemony Software GmbH, All Rights Reserved.
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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------
// Command line arguments format for testing VST3 plug-ins:
// ./ARATestHost -vst3 [binaryFilePath] [optionalPlugInName] -test [TestCase(s)] -file [AudioFile(s)]
// The optionalPlugInName argument will typically be omitted, it is only needed when the VST3 binary
// contains multiple plug-ins (e.g. WaveShell).
//
// On macOS, Audio Units can also be tested:
// ./ARATestHost -au [type] [subType] [manufacturer] -test [TestCase(s)] -file [AudioFile(s)]
//
// The macOS version also supports running the plug-in in a separate process, connected via IPC,
// by specifying `-ipc_vst3` or `-ipc_au` instead of `-vst3` or `-au`.
//
// If the optional `-test` argument is not supplied, all test cases will be run.
// See implementation of main() at the end of this file for a list of available test cases.
//
// If the optional `-file` argument is not supplied, a pulsed sine wave will be generated in-memory.
//
// Example:
// # run ContentReading and PlaybackRendering tests with Melodyne for VST3:
// ./ARATestHost -vst3 '/Library/Audio/PlugIns/VST3/Melodyne.vst3' -test ContentReading PlaybackRendering
//
// For reference, here are some relevant VST3 file paths on Windows:
// our own ARA SDK test plug-in: "-vst3 ARATestPlugIn.vst3"
// the JUCE_ARA demo plug-in: "-vst3 "C:/Program Files/Common Files/VST3/ARAPluginDemo.vst3""
// Melodyne: "-vst3 "C:/Program Files/Common Files/VST3/Celemony/Melodyne/Melodyne.vst3""
//
// On macOS, the Xcode schemes "Test VST3 API" and "Test AudioUnit API" contain the respective
// paths or IDs as pre-configured arguments under Run -> Arguments, along with a list of the
// available test for convenient configuration.
// If you prefer the command line:
// our own ARA SDK test plug-in: "-vst3 ARATestPlugIn.vst3" or "-au aufx AraT ADeC" or "-au aufx Ara3 ADeC"
// the JUCE_ARA demo plug-in: "-vst3 ~/Library/Audio/Plug-Ins/VST3/ARAPluginDemo.vst3" or "-au aufx AraD ADeC"
// Melodyne: "-vst3 /Library/Audio/Plug-Ins/VST3/Melodyne.vst3" or "-au aumf MPLG CLMY"
//------------------------------------------------------------------------------

#include "TestCases.h"

#include "ARA_Library/Utilities/ARAStdVectorUtilities.h"
#include "ARA_Library/Debug/ARAContentLogger.h"

#include <cstring>

#if ARA_ENABLE_IPC
    #include "ARA_Library/IPC/ARAIPCProxyHost.h"
#endif


// asserts
ARA::ARAAssertFunction assertFunction { &ARA::ARAInterfaceAssert };
ARA::ARAAssertFunction* assertFunctionReference { &assertFunction };


ARA_SETUP_DEBUG_MESSAGE_PREFIX ("ARATestHost");


AudioFileList parseAudioFiles (const std::vector<std::string>& args)
{
    AudioFileList parsedFiles;
    auto it { args.begin () };
    while (it != args.end ())
    {
        if (*it++ != "-file")
            continue;
        while ((it != args.end ()) &&
               ((*it)[0] != '-'))
        {
            icstdsp::AudioFile audioFile;
            int ARA_MAYBE_UNUSED_VAR (err);
            err = audioFile.Load (it->c_str ());
            ARA_INTERNAL_ASSERT (err == 0);
            parsedFiles.emplace_back (std::make_shared<AudioDataFile> (*it++, std::move (audioFile)));
        }
    }

    if (!parsedFiles.empty ())
        return parsedFiles;

    // create single dummy file if not specified
    return createDummyAudioFiles (1);
}

const std::vector<std::string> parseTestCases (const std::vector<std::string>& args)
{
    std::vector<std::string> parsedTests;
    auto it { args.begin () };
    while (it != args.end ())
    {
        if (*it++ != "-test")
            continue;
        while ((it != args.end ()) &&
               ((*it)[0] != '-'))
        {
            parsedTests.emplace_back (*it++);
        }
    }
    return parsedTests;
}

// see start of this file for detailed description of the command line arguments
int main (int argc, const char* argv[])
{
    const std::vector<std::string> args (argv + 1, argv + argc);

    ARA::ARASetExternalAssertReference (assertFunctionReference);

#if ARA_ENABLE_IPC
    // check if run as remote host
    auto it { std::find (args.begin (), args.end (), "-_ipcRemote") };
    const bool isRemoteHost { (args.size () >= 3) && (it < args.end () - 2) };  // we need 2 follow-up arguments
    std::string hostCommandsPortID;
    std::string plugInCallbacksPortID;
    if (isRemoteHost)
    {
        hostCommandsPortID = *(++it);
        plugInCallbacksPortID = *(++it);
        ARA::ARASetupDebugMessagePrefix ("REMOTE ARATestHost");
    }
#endif

    // parse the plug-in binary from the command line arguments
    auto plugInEntry { PlugInEntry::parsePlugInEntry (args) };
    if (!plugInEntry)
    {
        ARA_LOG ("No plug-in binary specified via -vst3 [binaryFilePath].");
        ARA_LOG ("No plug-in binary specified via -ipc_vst3 [binaryFilePath].");
#if defined (__APPLE__)
        ARA_LOG ("No plug-in binary specified via -au [typeID] [subTypeID] [manufacturerID].");
#if ARA_ENABLE_IPC
        ARA_LOG ("No plug-in binary specified via -ipc_au [typeID] [subTypeID] [manufacturerID].");
#endif
#endif
        return -1;
    }

    const auto factory { plugInEntry->getARAFactory () };
    if (!factory)
    {
        ARA_LOG ("Requested plug-in %s does not support ARA, aborting.", plugInEntry->getDescription ().c_str ());
        return -1;
    }

    // start up ARA
    if (!plugInEntry->usesIPC ())
        plugInEntry->initializeARA (assertFunctionReference);

#if ARA_ENABLE_IPC
    if (isRemoteHost)
    {
        ARA_LOG ("Remotely hosting ARA plug-in '%s' in %s", factory->plugInName, plugInEntry->getDescription ().c_str ());

        const auto result { RemoteHost::main (std::move (plugInEntry), hostCommandsPortID, plugInCallbacksPortID) };

        if (!plugInEntry->usesIPC ())
            plugInEntry->uninitializeARA ();

        return result;
    }
#endif

    // debug-output of the factory data
    // when using IPC, set a breakpoint to this line if you want to attach the debugger to the plug-in process
    ARA_LOG ("Testing ARA plug-in '%s' in %s%s:", factory->plugInName, plugInEntry->getDescription ().c_str (),
                                                  plugInEntry->usesIPC () ? " (using IPC)" : "");

    ARA_LOG ("    version: %s", factory->version);
    ARA_LOG ("    manufacturer: %s", factory->manufacturerName);
    ARA_LOG ("    website: %s", factory->informationURL);

    ARA_LOG ("    documentArchiveID: %s", factory->documentArchiveID);
    for (auto i { 0U }; i < factory->compatibleDocumentArchiveIDsCount; ++i)
        ARA_LOG ("    compatibleDocumentArchiveIDs[%i]: %s", i, factory->compatibleDocumentArchiveIDs[i]);

    if (factory->analyzeableContentTypesCount == 0)
        ARA_LOG ("    plug-in does not support content analysis.");
    for (auto i { 0U }; i < factory->analyzeableContentTypesCount; ++i)
        ARA_LOG ("    analyzeableContentTypes[%i]: %s", i, ARA::ContentLogger::getTypeNameForContentType (factory->analyzeableContentTypes[i]));

    ARA_LOG ("    plug-in does%s support time-stretching%s.", ((factory->supportedPlaybackTransformationFlags & ARA::kARAPlaybackTransformationTimestretch) != 0) ? "" : " not",
                                                              ((factory->supportedPlaybackTransformationFlags & ARA::kARAPlaybackTransformationTimestretchReflectingTempo) != 0) ? "(reflecting tempo)" : "");

    ARA_LOG ("    plug-in does%s support content-based fades.", ((factory->supportedPlaybackTransformationFlags & ARA::kARAPlaybackTransformationContentBasedFades) != 0) ? "" : " not");

    ARA_LOG ("    plug-in does%s support storing audio file chunks.", (factory.implements<ARA_STRUCT_MEMBER (ARAFactory, supportsStoringAudioFileChunks)> () &&
                                                                      (factory->supportsStoringAudioFileChunks != ARA::kARAFalse)) ? "" : " not");

    // parse any optional test cases or audio files
    auto audioFiles { parseAudioFiles (args) };
    const auto testCases { parseTestCases (args) };

    // conditionally execute each test case
    const auto shouldTest { [&] (const std::string& testCase) { return testCases.empty () || ARA::contains (testCases, testCase); } };
    if (shouldTest ("PropertyUpdates"))
        testPropertyUpdates (plugInEntry.get (), audioFiles);
    if (shouldTest ("ContentUpdates"))
        testContentUpdates (plugInEntry.get (), audioFiles);
    if (shouldTest ("ContentReading"))
        testContentReading (plugInEntry.get (), audioFiles);
    if (shouldTest ("ModificationCloning"))
        testModificationCloning (plugInEntry.get (), audioFiles);
    if (shouldTest ("Archiving"))
        testArchiving (plugInEntry.get (), audioFiles);
    if (shouldTest ("DragAndDrop"))
        testDragAndDrop (plugInEntry.get (), audioFiles);
    if (shouldTest ("PlaybackRendering"))
        testPlaybackRendering (plugInEntry.get (), true, audioFiles);
    if (shouldTest ("EditorView"))
        testEditorView (plugInEntry.get (), audioFiles);
    if (shouldTest ("Algorithms"))
        testProcessingAlgorithms (plugInEntry.get (), audioFiles);
    if (shouldTest ("AudioFileChunkSaving"))
        testAudioFileChunkSaving (plugInEntry.get (), audioFiles);
    if (shouldTest ("AudioFileChunkLoading"))
        testAudioFileChunkLoading (plugInEntry.get (), audioFiles);

    // shut down ARA
    if (!plugInEntry->usesIPC ())
        plugInEntry->uninitializeARA ();

    return 0;
}
