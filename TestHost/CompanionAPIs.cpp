//------------------------------------------------------------------------------
//! \file       CompanionAPIs.cpp
//!             used by the test host to load a companion API plug-in binary
//!             and create / destroy plug-in instances with ARA2 roles
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2021, Celemony Software GmbH, All Rights Reserved.
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

#include "CompanionAPIs.h"
#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#if defined (__APPLE__)
    #include "ExamplesCommon/PlugInHosting/AudioUnitLoader.h"
#endif
#include "ExamplesCommon/PlugInHosting/VST3Loader.h"

#include <memory>
#include <string>
#include <vector>
#include <cstring>

/*******************************************************************************/

#if defined (__APPLE__)
class AUPlugInInstance : public PlugInInstance
{
public:
    AUPlugInInstance (AudioUnit audioUnit, const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance)
    : PlugInInstance { plugInExtensionInstance },
      _audioUnit { audioUnit }
    {}

    ~AUPlugInInstance () override
    {
        AudioUnitClose (_audioUnit);
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        AudioUnitStartRendering (_audioUnit, static_cast<UInt32> (maxBlockSize), sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        AudioUnitRenderBuffer (_audioUnit, static_cast<UInt32> (blockSize), samplePosition, buffer);
    }

    void stopRendering () override
    {
        AudioUnitStopRendering (_audioUnit);
    }

private:
    AudioUnit const _audioUnit;
};
#endif

/*******************************************************************************/

class VST3PlugInInstance : public PlugInInstance
{
public:
    VST3PlugInInstance (VST3Effect* vst3Effect, const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance)
    : PlugInInstance { plugInExtensionInstance },
      _vst3Effect { vst3Effect }
    {}

    ~VST3PlugInInstance () override
    {
        VST3DestroyEffect (_vst3Effect);
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        VST3StartRendering (_vst3Effect, maxBlockSize, sampleRate);
        _sampleRate = sampleRate;
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        VST3RenderBuffer (_vst3Effect, blockSize, _sampleRate, samplePosition, buffer);
    }

    void stopRendering () override
    {
        VST3StopRendering (_vst3Effect);
    }

private:
    VST3Effect* const _vst3Effect;
    double _sampleRate { 44100.0 };
};

/*******************************************************************************/

#if defined (__APPLE__)
class AUPlugInEntry : public PlugInEntry
{
    // very crude conversion from string to OSType
    static OSType parseOSType (const std::string& idString)
    {
        ARA_INTERNAL_ASSERT (idString.size () == sizeof (OSType));
        return static_cast<uint32_t> (idString[3])        | (static_cast<uint32_t> (idString[2]) << 8) |
              (static_cast<uint32_t> (idString[1]) << 16) | (static_cast<uint32_t> (idString[0]) << 24);
    }

public:
    AUPlugInEntry (const std::string& type, const std::string& subType, const std::string& manufacturer, ARA::ARAAssertFunction* assertFunctionAddress)
    : _audioComponent { AudioUnitFindValidARAComponentWithIDs (parseOSType (type), parseOSType (subType), parseOSType (manufacturer)) }
    {
        initializeARA (AudioUnitGetARAFactory (_audioComponent), assertFunctionAddress);

        _description = std::string { "Audio Unit (" } + type + " - " + subType + " - " + manufacturer + ")";
    }

    ~AUPlugInEntry () override
    {
        uninitializeARA ();
        // unloading is not supported for Audio Units
    }

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::Host::DocumentController* documentController, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        AudioUnit audioUnit = AudioUnitOpen (_audioComponent);
        const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance = AudioUnitBindToARADocumentController (audioUnit, documentController->getRef (), assignedRoles);
        validatePlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
        return std::make_unique<AUPlugInInstance> (audioUnit, plugInExtensionInstance);
    }

private:
    AudioComponent _audioComponent;
};
#endif

/*******************************************************************************/

class VST3PlugInEntry : public PlugInEntry
{
public:
    VST3PlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName, ARA::ARAAssertFunction* assertFunctionAddress)
    : _vst3Binary { VST3LoadBinary (binaryName.c_str ()) },
      _optionalPlugInName { optionalPlugInName }
    {
        initializeARA (VST3GetARAFactory (_vst3Binary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()), assertFunctionAddress);

        _description = "VST3 ";
        if (!optionalPlugInName.empty ())
            _description += optionalPlugInName + " ";
        _description += "@ " + binaryName;
    }

    ~VST3PlugInEntry () override
    {
        uninitializeARA ();
        VST3UnloadBinary (_vst3Binary);
    }

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::Host::DocumentController* documentController, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        auto vst3Instance = VST3CreateEffect (_vst3Binary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ());
        const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance = VST3BindToARADocumentController (vst3Instance, documentController->getRef (), assignedRoles);
        validatePlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
        return std::make_unique<VST3PlugInInstance> (vst3Instance, plugInExtensionInstance);
    }

private:
    VST3Binary* _vst3Binary;
    const std::string _optionalPlugInName;
};

/*******************************************************************************/

void PlugInEntry::initializeARA (const ARA::ARAFactory* factory, ARA::ARAAssertFunction* assertFunctionAddress)
{
    ARA_INTERNAL_ASSERT (_factory == nullptr);
    if (!factory)   // plug-in does not support ARA
        return;

    // validate factory conditions
    ARA_VALIDATE_API_CONDITION (factory->structSize >= ARA::kARAFactoryMinSize);

#if ARA_CPU_ARM
    ARA_VALIDATE_API_CONDITION (factory->lowestSupportedApiGeneration >= ARA::kARAAPIGeneration_2_0_Final);
#else
    ARA_VALIDATE_API_CONDITION (factory->lowestSupportedApiGeneration >= ARA::kARAAPIGeneration_1_0_Draft);
#endif
    ARA_VALIDATE_API_CONDITION (factory->highestSupportedApiGeneration >= factory->lowestSupportedApiGeneration);

    ARA_VALIDATE_API_CONDITION (std::strlen (factory->factoryID) > 5);          // at least "xx.y." needed to form a valid url-based unique ID

    ARA_VALIDATE_API_CONDITION (factory->initializeARAWithConfiguration != nullptr);
    ARA_VALIDATE_API_CONDITION (factory->uninitializeARA != nullptr);

    ARA_VALIDATE_API_CONDITION (std::strlen (factory->plugInName) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->manufacturerName) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->informationURL) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->version) > 0);

    ARA_VALIDATE_API_CONDITION (factory->createDocumentControllerWithDocument != nullptr);

    ARA_VALIDATE_API_CONDITION (std::strlen (factory->documentArchiveID) > 5);  // at least "xx.y." needed to form a valid url-based unique ID
    if (factory->compatibleDocumentArchiveIDsCount == 0)
        ARA_VALIDATE_API_CONDITION (factory->compatibleDocumentArchiveIDs == nullptr);
    else
        ARA_VALIDATE_API_CONDITION (factory->compatibleDocumentArchiveIDs != nullptr);
    for (auto i { 0U }; i < factory->compatibleDocumentArchiveIDsCount; ++i)
        ARA_VALIDATE_API_CONDITION (std::strlen (factory->compatibleDocumentArchiveIDs[i]) > 5);

    if (factory->analyzeableContentTypesCount == 0)
        ARA_VALIDATE_API_CONDITION (factory->analyzeableContentTypes == nullptr);
    else
        ARA_VALIDATE_API_CONDITION (factory->analyzeableContentTypes != nullptr);

    // if content based fades are supported, they shall be supported on both ends
    if ((factory->supportedPlaybackTransformationFlags & ARA::kARAPlaybackTransformationContentBasedFades) != 0)
        ARA_INTERNAL_ASSERT ((factory->supportedPlaybackTransformationFlags & ARA::kARAPlaybackTransformationContentBasedFades) == ARA::kARAPlaybackTransformationContentBasedFades);

    // ensure that this plug-in is supported by our test host
    ARA_INTERNAL_ASSERT (factory->lowestSupportedApiGeneration <= ARA::kARAAPIGeneration_2_0_Final);
#if ARA_SUPPORT_VERSION_1
    ARA_INTERNAL_ASSERT (factory->highestSupportedApiGeneration >= ARA::kARAAPIGeneration_1_0_Final);
#elif ARA_CPU_ARM
    ARA_INTERNAL_ASSERT (factory->highestSupportedApiGeneration >= ARA::kARAAPIGeneration_2_0_Final);
#else
    ARA_INTERNAL_ASSERT (factory->highestSupportedApiGeneration >= ARA::kARAAPIGeneration_2_0_Draft);
#endif

    ARA::ARAAPIGeneration desiredApiGeneration { ARA::kARAAPIGeneration_2_0_Final };
    if (desiredApiGeneration > factory->highestSupportedApiGeneration)
        desiredApiGeneration = factory->highestSupportedApiGeneration;

    // initialize ARA factory with interface configuration
    const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig = { desiredApiGeneration, assertFunctionAddress };
    factory->initializeARAWithConfiguration (&interfaceConfig);

    _factory = factory;
}

void PlugInEntry::uninitializeARA ()
{
    if (_factory)
        _factory->uninitializeARA ();
}

void PlugInEntry::validatePlugInExtensionInstance (const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
{
    ARA_VALIDATE_API_STATE (plugInExtensionInstance != nullptr);

#if ARA_SUPPORT_VERSION_1
    if (_factory->highestSupportedApiGeneration < kARAAPIGeneration_2_0_Draft)
        return;
#endif

    if ((assignedRoles & ARA::kARAPlaybackRendererRole) != 0)
        ARA_VALIDATE_API_INTERFACE (plugInExtensionInstance->playbackRendererInterface, ARAPlaybackRendererInterface);
    else
        ARA_VALIDATE_API_STATE (plugInExtensionInstance->playbackRendererInterface == nullptr);

    if ((assignedRoles & ARA::kARAEditorRendererRole) != 0)
        ARA_VALIDATE_API_INTERFACE (plugInExtensionInstance->editorRendererInterface, ARAEditorRendererInterface);
    else
        ARA_VALIDATE_API_STATE (plugInExtensionInstance->editorRendererInterface == nullptr);

    if ((assignedRoles & ARA::kARAEditorViewRole) != 0)
        ARA_VALIDATE_API_INTERFACE (plugInExtensionInstance->editorViewInterface, ARAEditorViewInterface);
    else
        ARA_VALIDATE_API_STATE (plugInExtensionInstance->editorViewInterface == nullptr);
}

/*******************************************************************************/

std::unique_ptr<PlugInEntry> PlugInEntry::parsePlugInEntry (const std::vector<std::string>& args, ARA::ARAAssertFunction* assertFunctionAddress)
{
    auto it = std::find (args.begin (), args.end (), "-vst3");
    if (it < args.end () - 1)   // we need at least one follow-up argument
    {
        const auto& binaryFileName { *++it };
        std::string optionalPlugInName {};
        if ((++it != args.end ()) && ((*it)[0] != '-'))
            optionalPlugInName = *it;
        return std::make_unique<VST3PlugInEntry> (binaryFileName, optionalPlugInName, assertFunctionAddress);
    }

#if defined (__APPLE__)
    it = std::find (args.begin (), args.end (), "-au");
    if (it < args.end () - 3)   // we need 3 follow-up arguments
        return std::make_unique<AUPlugInEntry> (*++it, *++it, *++it, assertFunctionAddress);
#endif

    return nullptr;
}
