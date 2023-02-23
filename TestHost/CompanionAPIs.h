//------------------------------------------------------------------------------
//! \file       CompanionAPIs.h
//!             used by the test host to load a companion API plug-in binary
//!             and create / destroy plug-in instances with ARA2 roles
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

#pragma once

#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include <string>
#include <vector>

/*******************************************************************************/
// Wrapper class for a companion API plug-in instance
class PlugInInstance
{
protected:
    explicit PlugInInstance (const ARA::ARAPlugInExtensionInstance* instance)
    : _playbackRenderer { instance },
      _editorRenderer { instance },
      _editorView { instance }
    {}

public:
    // Instances will be created through the factory PlugInEntry::createARAPlugInInstanceWithRoles()
    virtual ~PlugInInstance () = default;

    // Companion API specific implementations
    virtual void startRendering (int maxBlockSize, double sampleRate) = 0;
    virtual void renderSamples (int blockSize, int64_t samplePosition, float* buffer) = 0;
    virtual void stopRendering () = 0;

    // Getters for ARA specific plug-in role interfaces
    ARA::Host::PlaybackRenderer* getPlaybackRenderer () { return &_playbackRenderer; }
    ARA::Host::EditorRenderer* getEditorRenderer () { return &_editorRenderer; }
    ARA::Host::EditorView* getEditorView () { return &_editorView; }

private:
    ARA::Host::PlaybackRenderer _playbackRenderer;
    ARA::Host::EditorRenderer _editorRenderer;
    ARA::Host::EditorView _editorView;
};

/*******************************************************************************/
// Wrapper class for the entry into the individual companion API plug-in classes.
class PlugInEntry
{
protected:
    PlugInEntry () = default;

public:
    // Static factory function for parsing companion API plug-in binaries from command line args.
    // If the plug-in supports ARA, the API will be initialized using the provided assert function
    // and uninitialized when the resulting binary is deleted.
    static std::unique_ptr<PlugInEntry> parsePlugInEntry (const std::vector<std::string>& args, ARA::ARAAssertFunction* assertFunctionAddress);

    virtual ~PlugInEntry () = default;

    // String describing the selected plug-in
    const std::string& getDescription () const { return _description; }

    // Return pointer to factory describing the ARA plug-in (nullptr if plug-in does not support ARA)
    const ARA::ARAFactory* getARAFactory () const { return _factory; }

    // Factory function for new ARA plug-in instances with the desired roles
    virtual std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) = 0;

protected:
    // implementation helpers for derived classes
    void initializeARA (const ARA::ARAFactory* factory, ARA::ARAAssertFunction* assertFunctionAddress);
    void uninitializeARA ();
    void validatePlugInExtensionInstance (const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance, ARA::ARAPlugInInstanceRoleFlags assignedRoles);

protected:
    std::string _description {};

private:
    const ARA::ARAFactory* _factory {};
};
