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

#include "ARA_Library/IPC/ARAIPC.h"

#include <string>
#include <vector>


/*******************************************************************************/
// Wrapper class for a companion API plug-in instance
class PlugInInstance
{
protected:
    PlugInInstance () {}

public:
    // Instances will be created through the factory PlugInEntry::createPlugInInstance()
    virtual ~PlugInInstance () = default;

    // Execute the ARA binding
    virtual void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) = 0;

    // Companion API specific implementations
    virtual void startRendering (int maxBlockSize, double sampleRate) = 0;
    virtual void renderSamples (int blockSize, int64_t samplePosition, float* buffer) = 0;
    virtual void stopRendering () = 0;

    // Getters for ARA specific plug-in role interfaces
    ARA::Host::PlaybackRenderer getPlaybackRenderer () { return ARA::Host::PlaybackRenderer { _instance }; }
    ARA::Host::EditorRenderer getEditorRenderer () { return ARA::Host::EditorRenderer { _instance }; }
    ARA::Host::EditorView getEditorView () { return ARA::Host::EditorView { _instance }; }

#if ARA_ENABLE_IPC
    const ARA::ARAPlugInExtensionInstance* getARAPlugInExtensionInstance () { return _instance; }
#endif

protected:
    void validateAndSetPlugInExtensionInstance (const ARA::ARAPlugInExtensionInstance* instance, ARA::ARAPlugInInstanceRoleFlags assignedRoles);

private:
    const ARA::ARAPlugInExtensionInstance* _instance {};
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
    const ARA::SizedStructPtr<ARA::ARAFactory> getARAFactory () const { return _factory; }

    // Factory function for new ARA document controller instances
    virtual const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                            const ARA::ARADocumentProperties* properties);

    // Factory function for new plug-in instances
    virtual std::unique_ptr<PlugInInstance> createPlugInInstance () = 0;

    // Test if IPC is used
    bool usesIPC () const { return _usesIPC; }

protected:
    // implementation helpers for derived classes
#if ARA_ENABLE_IPC
    void setUsesIPC () { _usesIPC = true; }
#endif
    void initializeARA (const ARA::ARAFactory* factory, ARA::ARAAssertFunction* assertFunctionAddress);
    void uninitializeARA ();

protected:
    std::string _description {};

private:
    const ARA::ARAFactory* _factory { nullptr };
    bool _usesIPC { false };
};

#if ARA_ENABLE_IPC
/*******************************************************************************/
// Wrapper class for the remote process main().
namespace RemoteHost
{
    int main (std::unique_ptr<PlugInEntry> plugInEntry, const std::string& hostCommandsPortID, const std::string& plugInCallbacksPortID);
}
#endif
