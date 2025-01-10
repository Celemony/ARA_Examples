//------------------------------------------------------------------------------
//! \file       VST3Loader.cpp
//!             VST3 specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2025, Celemony Software GmbH, All Rights Reserved.
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

#include "VST3Loader.h"

#if defined (NDEBUG)
    #define RELEASE 1
#else
    #define DEVELOPMENT 1
#endif

#include "ARA_API/ARAVST3.h"
#include "ARA_Library/Debug/ARADebug.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN
#include "base/source/fstring.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
ARA_DISABLE_VST3_WARNINGS_END

DEF_CLASS_IID (Steinberg::Vst::IComponent)
DEF_CLASS_IID (Steinberg::Vst::IAudioProcessor)
DEF_CLASS_IID (ARA::IMainFactory)
DEF_CLASS_IID (ARA::IPlugInEntryPoint)
DEF_CLASS_IID (ARA::IPlugInEntryPoint2)

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if defined (_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <conio.h>

    extern "C" typedef bool (PLUGIN_API *InitModuleProc) ();
    extern "C" typedef bool (PLUGIN_API *ExitModuleProc) ();

#elif defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>

    extern "C" typedef bool (*bundleEntryPtr) (CFBundleRef);
    extern "C" typedef bool (*bundleExitPtr) (void);

#elif defined (__linux__)
    #include <dlfcn.h>

    extern "C" typedef bool (PLUGIN_API *ModuleEntryFunc) (void*);
    extern "C" typedef bool (PLUGIN_API *ModuleExitFunc) ();
#endif


#if defined (_MSC_VER)
    __pragma (warning(disable : 4191)) /*unsafe conversion*/
#elif defined (__GNUC__)
    _Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"")
#endif


using namespace Steinberg;
using namespace Vst;


struct _VST3Binary
{
#if defined (_WIN32)
    HMODULE libHandle;
#elif defined (__APPLE__)
    CFBundleRef libHandle;
#elif defined (__linux__)
    void* libHandle;
#endif
    IPtr<IPluginFactory> pluginFactory;
    std::vector<IPtr<ARA::IMainFactory>> araMainFactories;

#if ARA_VALIDATE_API_CALLS
    bool hasMultiplePlugIns;
#endif
};

struct _VST3Effect
{
    IPtr<IComponent> component;

#if ARA_VALIDATE_API_CALLS
    VST3Binary binary;
    std::string className;
#endif
};


VST3Binary VST3LoadBinary (const char* binaryName)
{
#if defined (__APPLE__) || defined (__linux__)
    // if the binary name contains no '/', dlopen () searches the system library paths for the file,
    // and ignores the current directory - to prevent this, we prefix with "./" if needed
    std::string temp { "./" };
    if (!std::strchr (binaryName, '/'))
    {
        temp += binaryName;
        binaryName = temp.c_str ();
    }
#endif

    VST3Binary vst3Binary { new _VST3Binary };
#if defined (_WIN32)
    vst3Binary->libHandle = ::LoadLibraryA (binaryName);
    ARA_INTERNAL_ASSERT (vst3Binary->libHandle);

    const auto initBinaryFunc { (InitModuleProc) ::GetProcAddress (vst3Binary->libHandle, "InitDll") };
    const auto factoryFunc { (GetFactoryProc) ::GetProcAddress (vst3Binary->libHandle, "GetPluginFactory") };

#elif defined (__APPLE__)
    CFURLRef url { CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8*) binaryName, (CFIndex) std::strlen (binaryName), true) };
    ARA_INTERNAL_ASSERT (url);
    vst3Binary->libHandle = CFBundleCreate (kCFAllocatorDefault, url);
    CFRelease (url);
    ARA_INTERNAL_ASSERT (vst3Binary->libHandle);

    Boolean ARA_MAYBE_UNUSED_VAR (didLoad);
    didLoad = CFBundleLoadExecutable (vst3Binary->libHandle);
    ARA_INTERNAL_ASSERT (didLoad);

    const auto initBinaryFunc { (bundleEntryPtr) CFBundleGetFunctionPointerForName (vst3Binary->libHandle, CFSTR ("bundleEntry")) };
    const auto factoryFunc { (GetFactoryProc) CFBundleGetFunctionPointerForName (vst3Binary->libHandle, CFSTR ("GetPluginFactory")) };

#elif defined (__linux__)
    vst3Binary->libHandle = dlopen (binaryName, RTLD_LAZY);
    ARA_INTERNAL_ASSERT (vst3Binary->libHandle);

    const auto initBinaryFunc { (ModuleEntryFunc) dlsym (vst3Binary->libHandle, "ModuleEntry") };
    const auto factoryFunc { (GetFactoryProc) dlsym (vst3Binary->libHandle, "GetPluginFactory") };
#endif

    ARA_INTERNAL_ASSERT (initBinaryFunc);
    bool ARA_MAYBE_UNUSED_VAR (entrySucceeded);
#if defined (_WIN32)
    entrySucceeded = initBinaryFunc ();
#else
    entrySucceeded = initBinaryFunc (vst3Binary->libHandle);
#endif
    ARA_INTERNAL_ASSERT (entrySucceeded);

    ARA_INTERNAL_ASSERT (factoryFunc);
    vst3Binary->pluginFactory = factoryFunc ();
    ARA_INTERNAL_ASSERT (vst3Binary->pluginFactory);

#if ARA_VALIDATE_API_CALLS
    std::vector<PClassInfo> mainFactoryClasses;
    std::vector<PClassInfo> audioProcessorClasses;
#endif
    for (int32 i = 0; i < vst3Binary->pluginFactory->countClasses (); ++i)
    {
        PClassInfo classInfo;
        tresult ARA_MAYBE_UNUSED_VAR (result);
        result = vst3Binary->pluginFactory->getClassInfo (i, &classInfo);
        ARA_INTERNAL_ASSERT (result == Steinberg::kResultOk);

        // find and instantiate all ARA::IMainFactory classes and ensure they have unique names and IDs
        if (std::strcmp (kARAMainFactoryClass, classInfo.category) == 0)
        {
#if ARA_VALIDATE_API_CALLS
            for (const auto& mainFactoryClass : mainFactoryClasses)
            {
                ARA_VALIDATE_API_CONDITION (std::strcmp (mainFactoryClass.name, classInfo.name) != 0);
                ARA_VALIDATE_API_CONDITION (!FUnknownPrivate::iidEqual (mainFactoryClass.cid, classInfo.cid));
            }

            mainFactoryClasses.emplace_back (classInfo);
#endif

            IPtr<ARA::IMainFactory> araMainFactory;
            result = vst3Binary->pluginFactory->createInstance (classInfo.cid, ARA::IMainFactory::iid, (void**) &araMainFactory);
            ARA_INTERNAL_ASSERT (result == kResultOk);
            ARA_INTERNAL_ASSERT (araMainFactory);

#if ARA_VALIDATE_API_CALLS
            // ensure all ARAFactories are unique (address and factoryID)
            const auto araFactory { araMainFactory->getFactory () };
            ARA_VALIDATE_API_CONDITION (araFactory);
            for (auto& otherMainFactory : vst3Binary->araMainFactories)
            {
                const auto otherARAFactory { otherMainFactory->getFactory () };
                ARA_VALIDATE_API_CONDITION (araFactory != otherARAFactory);
                ARA_VALIDATE_API_CONDITION (std::strcmp (araFactory->factoryID, otherARAFactory->factoryID) != 0);
                ARA_VALIDATE_API_CONDITION (std::strcmp (araFactory->plugInName, otherARAFactory->plugInName) != 0);
            }
#endif

            vst3Binary->araMainFactories.emplace_back (std::move (araMainFactory));
        }
#if ARA_VALIDATE_API_CALLS
        // find all IAudioProcessor classes and ensure they have unique names and IDs
        else if (std::strcmp (kVstAudioEffectClass, classInfo.category) == 0)
        {
            for (const auto& audioProcessorClass : audioProcessorClasses)
            {
                ARA_VALIDATE_API_CONDITION (std::strcmp (audioProcessorClass.name, classInfo.name) != 0);
                ARA_VALIDATE_API_CONDITION (!FUnknownPrivate::iidEqual (audioProcessorClass.cid, classInfo.cid));
            }

            audioProcessorClasses.emplace_back (classInfo);
        }
#endif
    }

    // verify we've found at least as many audio processor classes as we've found ARA main factories
    ARA_VALIDATE_API_CONDITION (audioProcessorClasses.size () >= mainFactoryClasses.size ());

#if ARA_VALIDATE_API_CALLS
    // if multiple processors in same binary, verify there's a matchingly named IAudioProcessor class for each ARA::IMainFactory class
    vst3Binary->hasMultiplePlugIns = audioProcessorClasses.size () > 1;
    if (vst3Binary->hasMultiplePlugIns)
    {
        for (const auto& mainFactoryClass : mainFactoryClasses)
        {
            bool foundMatchingClass { false };
            for (const auto& audioProcessorClass : audioProcessorClasses)
            {
                if (std::strcmp (audioProcessorClass.name, mainFactoryClass.name) == 0)
                {
                    ARA_VALIDATE_API_CONDITION ((!foundMatchingClass) && "found multiple IAudioProcessor classes with same name");
                    foundMatchingClass = true;

                    // here we could create an instances of the audioProcessorClass to validate it
                    // returns the same ARAFactory, but this will be validated later in
                    // VST3BindToARADocumentController () to avoid creating a component instance here.
                }
            }
            ARA_VALIDATE_API_CONDITION (foundMatchingClass && "found no IAudioProcessor class for given ARA::IMainFactory class by name");
        }
    }
#endif

    return vst3Binary;
}

const ARA::ARAFactory* VST3GetARAFactory (VST3Binary vst3Binary, const char* optionalPlugInName)
{
    if (vst3Binary->araMainFactories.empty ())
        return nullptr;

    if (!optionalPlugInName)
        return vst3Binary->araMainFactories.front()->getFactory ();

    for (auto& araMainFactory : vst3Binary->araMainFactories)
    {
        const auto araFactory { araMainFactory->getFactory () };
        if (std::strcmp (optionalPlugInName, araFactory->plugInName) == 0)
            return araFactory;
    }
    return nullptr;
}

VST3Effect VST3CreateEffect (VST3Binary vst3Binary, const char* optionalPlugInName)
{
    for (int32 i = 0; i < vst3Binary->pluginFactory->countClasses (); ++i)
    {
        PClassInfo classInfo;
        tresult ARA_MAYBE_UNUSED_VAR (result);
        result = vst3Binary->pluginFactory->getClassInfo (i, &classInfo);
        ARA_INTERNAL_ASSERT (result == Steinberg::kResultOk);

        if (std::strcmp (kVstAudioEffectClass, classInfo.category) == 0)
        {
            if (optionalPlugInName && (std::strcmp (optionalPlugInName, classInfo.name) != 0))
                continue;

            // create only the component part for the purpose of this test code (skipping controller part)
            IPtr<IComponent> component;
            result = vst3Binary->pluginFactory->createInstance (classInfo.cid, IComponent::iid, (void**) &component);
            ARA_INTERNAL_ASSERT (result == kResultOk);
            ARA_INTERNAL_ASSERT (component);
            result = component->initialize (nullptr);
            ARA_INTERNAL_ASSERT (result == kResultOk);
#if ARA_VALIDATE_API_CALLS
            return new _VST3Effect { component, vst3Binary, classInfo.name };
#else
            return new _VST3Effect { component };
#endif
        }
    }

    ARA_INTERNAL_ASSERT (false);
    return nullptr;
}

const ARA::ARAPlugInExtensionInstance* VST3BindToARADocumentController (VST3Effect vst3Effect, ARA::ARADocumentControllerRef controllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
{
    FUnknownPtr<ARA::IPlugInEntryPoint> entry { vst3Effect->component };
    ARA_INTERNAL_ASSERT (entry);

    // both ARA::IMainFactory and the associated ARA::IPlugInEntryPoint must return the same underlying ARAFactory
#if ARA_VALIDATE_API_CALLS
    const auto araFactory { VST3GetARAFactory (vst3Effect->binary, (vst3Effect->binary->hasMultiplePlugIns) ? vst3Effect->className.c_str () : nullptr) };
    ARA_VALIDATE_API_CONDITION (araFactory && "could not find matching ARA::IMainFactory for given IAudioProcessor");
    if (entry->getFactory () != araFactory)
        ARA_VALIDATE_API_CONDITION (std::strcmp (entry->getFactory ()->factoryID, araFactory->factoryID) == 0);

    // class name must match plug-in name in factory
    ARA_VALIDATE_API_CONDITION (std::strcmp (entry->getFactory ()->plugInName, vst3Effect->className.c_str ()) == 0);
#endif

    constexpr ARA::ARAPlugInInstanceRoleFlags knownRoles { ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole };
    ARA_INTERNAL_ASSERT ((assignedRoles | knownRoles) == knownRoles);
    if (FUnknownPtr<ARA::IPlugInEntryPoint2> entry2 { vst3Effect->component })
    {
        if (auto result = entry2->bindToDocumentControllerWithRoles (controllerRef, knownRoles, assignedRoles))
            return result;
    }

#if defined (ARA_SUPPORT_VERSION_1) && (ARA_SUPPORT_VERSION_1)
    ARA_INTERNAL_ASSERT (assignedRoles == knownRoles);
    return entry->bindToDocumentController (controllerRef);
#else
    ARA_INTERNAL_ASSERT (false);
    return nullptr;
#endif
}

void VST3StartRendering (VST3Effect vst3Effect, int32_t maxBlockSize, double sampleRate)
{
    FUnknownPtr<IAudioProcessor> processor { vst3Effect->component };
    ARA_INTERNAL_ASSERT (processor);

    ProcessSetup setup = { kRealtime, kSample32, maxBlockSize, sampleRate };
    tresult ARA_MAYBE_UNUSED_VAR (result);
    result = processor->setupProcessing (setup);
    ARA_INTERNAL_ASSERT (result == kResultOk);

    SpeakerArrangement inputs = SpeakerArr::kMono;
    SpeakerArrangement outputs = SpeakerArr::kMono;
    result = processor->setBusArrangements (&inputs, 1, &outputs, 1);
    ARA_INTERNAL_ASSERT (result == kResultOk);
    result = vst3Effect->component->activateBus (kAudio, kInput, 0, true);
    ARA_INTERNAL_ASSERT (result == kResultOk);
    result = vst3Effect->component->activateBus (kAudio, kOutput, 0, true);
    ARA_INTERNAL_ASSERT (result == kResultOk);

    result = vst3Effect->component->setActive (true);
    ARA_INTERNAL_ASSERT (result == kResultOk);
}

void VST3RenderBuffer (VST3Effect vst3Effect, int32_t blockSize, double sampleRate, int64_t samplePosition, float* buffer)
{
    FUnknownPtr<IAudioProcessor> processor { vst3Effect->component };
    ARA_INTERNAL_ASSERT (processor);

    float* channels[1] = { buffer };
    memset (buffer, 0, static_cast<size_t> (blockSize) * sizeof (float));
    AudioBusBuffers inputs;
    inputs.numChannels = 1;
    inputs.silenceFlags = 0xFFFFFFFFFFFFFFFFU;
    inputs.channelBuffers32 = channels;

    AudioBusBuffers outputs;
    outputs.numChannels = 1;
    outputs.silenceFlags = 0;
    outputs.channelBuffers32 = channels;

    // in order for an ARA playback renderer to produce output, it must be set to playback mode (in stop, only editor renderers are active)
    // thus we implement some crude, minimal transport information here.
    ProcessContext context {};
    context.state = ProcessContext::kPlaying;
    context.sampleRate = sampleRate;
    context.projectTimeSamples = samplePosition;

    ProcessData data;
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample32;
    data.numSamples = blockSize;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = &inputs;
    data.outputs = &outputs;
    data.processContext = &context;

    tresult ARA_MAYBE_UNUSED_VAR (result);
    result = processor->process (data);
    ARA_INTERNAL_ASSERT (result == kResultOk);
}

void VST3StopRendering (VST3Effect vst3Effect)
{
    tresult ARA_MAYBE_UNUSED_VAR (result);
    result = vst3Effect->component->setActive (false);
    ARA_INTERNAL_ASSERT (result == kResultOk);

    result = vst3Effect->component->activateBus (kAudio, kInput, 0, false);
    ARA_INTERNAL_ASSERT (result == kResultOk);
    result = vst3Effect->component->activateBus (kAudio, kOutput, 0, false);
    ARA_INTERNAL_ASSERT (result == kResultOk);
}

void VST3DestroyEffect (VST3Effect vst3Effect)
{
    tresult ARA_MAYBE_UNUSED_VAR (result);
    result = vst3Effect->component->terminate ();
    ARA_INTERNAL_ASSERT (result == kResultOk);
    vst3Effect->component = nullptr;
    delete vst3Effect;
}

void VST3UnloadBinary (VST3Binary vst3Binary)
{
    vst3Binary->araMainFactories.clear ();
    vst3Binary->pluginFactory = nullptr;

#if defined (_WIN32)
    ((ExitModuleProc) ::GetProcAddress (vst3Binary->libHandle, "ExitDll")) ();
    FreeLibrary (vst3Binary->libHandle);

#elif defined (__APPLE__)
    ((bundleExitPtr) CFBundleGetFunctionPointerForName (vst3Binary->libHandle, CFSTR ("bundleExit"))) ();
    CFRelease (vst3Binary->libHandle);

#elif defined (__linux__)
    ((ModuleExitFunc) dlsym (vst3Binary->libHandle, "ModuleExit")) ();
    dlclose (vst3Binary->libHandle);
#endif

    delete vst3Binary;
}
