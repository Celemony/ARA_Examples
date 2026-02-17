//------------------------------------------------------------------------------
//! \file       TestCases.h
//!             various tests simulating user interaction with the TestHost
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2026, Celemony Software GmbH, All Rights Reserved.
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

#include "ExamplesCommon/AudioFiles/AudioFiles.h"
#include "CompanionAPIs.h"

using AudioFileList = std::vector<std::shared_ptr<AudioFileBase>>;

// Helper function to create dummy audio file representations that play back a pulsed sine signal
AudioFileList createDummyAudioFiles (size_t numFiles);

// Demonstrates updating several properties of ARA model graph objects within an edit cycle
// (note: in an actual application, these updates would likely be spread across individual cycles)
void testPropertyUpdates (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Demonstrates how to update content information if changed in the host
// The plug-in will call back into the host's ARAContentAccessController implementation
// to read the updated data - see ARAContentAccessController
void testContentUpdates (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Demonstrates how to read ARAContentTypes from a plug-in -
// see ContentLogger::log () for implementation of the actual content reading
void testContentReading (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Demonstrates how to clone an audio modification to enable two separate edits of the same audio source
void testModificationCloning (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Demonstrates how to store and restore plug-in document archives
void testArchiving (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// For ARA 2 plug-ins, instead of a monolithic archive for the entire document this test uses
// multiple smaller archives, each containing specific parts of the graph.
void testSplitArchives (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Simulates a "drag & drop" operation by archiving one source and its modification in a
// two source/modification document with a StoreObjectsFilter, and restoring them in another document
void testDragAndDrop (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Demonstrates using a plug-in playback renderer instance to process audio for a playback region,
// using the companion API rendering methods
// Can optionally use an ARA plug-in's time stretching capabilities to stretch a playback region -
// try loading Melodyne to see this feature in action
void testPlaybackRendering (PlugInEntry* plugInEntry, bool enableTimeStretchingIfSupported, const AudioFileList& audioFiles);

// Demonstrates how to communicate view selection and region sequence hiding
// (albeit this is of rather limited use in a non-UI application)
void testEditorView (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Requests plug-in analysis, using every processing algorithm published by the plug-in.
void testProcessingAlgorithms (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Loads an `iXML` ARA audio file chunk from a supplied .WAV or .AIFF file
void testAudioFileChunkLoading (PlugInEntry* plugInEntry, const AudioFileList& audioFiles);

// Requests plug-in analysis and saves audio source state into an `iXML` data chunk in each audio file
// (if chunk authoring is supported by the plug-in) -
// overwrites any current iXML chunk in the files (but only in-memory)
void testAudioFileChunkSaving (PlugInEntry* plugInEntry, AudioFileList& audioFiles);
