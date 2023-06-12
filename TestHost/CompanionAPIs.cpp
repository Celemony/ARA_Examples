//------------------------------------------------------------------------------
//! \file       CompanionAPIs.cpp
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

#include "CompanionAPIs.h"
#include "ExamplesCommon/Utilities/StdUniquePtrUtilities.h"

#if defined (__APPLE__)
    #include "ExamplesCommon/PlugInHosting/AudioUnitLoader.h"
#endif
#include "ExamplesCommon/PlugInHosting/VST3Loader.h"

#include <codecvt>
#include <locale>
#include <memory>
#include <string>
#include <vector>
#include <cstring>

/*******************************************************************************/

#if ARA_ENABLE_IPC

#include "IPC/ARAIPCProxyHost.h"
#include "IPC/ARAIPCProxyPlugIn.h"

#if defined (__linux__)
    #error "IPC not yet implemented for Linux"
#endif

// minimal set of commands to run a companion API plug-in through IPC
enum
{
    // \todo to ease conflict-free sharing of the IPC channels, should we limit the message IDs used by
    //       the generic ARA IPC to a well-defined range (e.g. non-negative, or at least not 0 and -1)
    //       the channel can then transform the ARA IPC IDs to occupy some range not used by non-ARA messages
    kIPCCreateARAEffect = -1,
    kIPCStartRendering = -2,
    kIPCRenderSamples = -3,
    kIPCStopRendering = -4,
    kIPCDestroyEffect = -5,
    kIPCTerminate = -6
};

// helper function to create unique port IDs for each run
static const std::string _createPortID ()
{
    std::string baseID { "com.arademocompany.TestHost.IPC." };

#if defined (_WIN32)
    std::string uniqueID;
    GUID guid;
    if (SUCCEEDED (::CoCreateGuid (&guid)))
    {
        LPOLESTR wszCLSID { nullptr };
        if (SUCCEEDED (::StringFromCLSID (guid, &wszCLSID)))
        {
            if (lstrlenW (wszCLSID) == 38)
            {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
#if (1900 <= _MSCVER) && (_MSCVER < 1920)
                // Visual Studio 2015 and 2017 have a bug regarding linking, see
                // https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
                auto temp { reinterpret_cast<const wchar_t *> (wszCLSID) };
                uniqueID = convert.to_bytes (temp + 1, temp + 37);
#else
                uniqueID = convert.to_bytes (wszCLSID + 1, wszCLSID + 37);
#endif
            }
            ::CoTaskMemFree (wszCLSID);
        }
    }
    ARA_INTERNAL_ASSERT (uniqueID.size () == 36);
    return baseID + uniqueID;

#elif defined (__APPLE__)
    CFUUIDRef uuid { CFUUIDCreate (kCFAllocatorDefault) };
    CFStringRef asString { CFUUIDCreateString(kCFAllocatorDefault, uuid) };
    const auto result { baseID + CFStringGetCStringPtr (asString, kCFStringEncodingMacRoman) };
    CFRelease (asString);
    CFRelease (uuid);
    return result;
#endif
}

#endif // ARA_ENABLE_IPC

/*******************************************************************************/

#if defined (__APPLE__)

class AUPlugInInstance : public PlugInInstance
{
public:
    AUPlugInInstance (AudioUnitInstance audioUnit, const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance)
    : PlugInInstance { plugInExtensionInstance },
      _audioUnit { audioUnit }
    {}

    ~AUPlugInInstance () override
    {
        AudioUnitCloseInstance (_audioUnit);
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
    AudioUnitInstance const _audioUnit;
};

#endif // defined (__APPLE__)

/*******************************************************************************/

class VST3PlugInInstance : public PlugInInstance
{
public:
    VST3PlugInInstance (VST3Effect vst3Effect, const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance)
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
    VST3Effect const _vst3Effect;
    double _sampleRate { 44100.0 };
};

/*******************************************************************************/

#if defined (__APPLE__)

// very crude conversion from string to OSType
OSType parseOSType (const std::string& idString)
{
    ARA_INTERNAL_ASSERT (idString.size () == sizeof (OSType));
    return static_cast<uint32_t> (idString[3])        | (static_cast<uint32_t> (idString[2]) << 8) |
          (static_cast<uint32_t> (idString[1]) << 16) | (static_cast<uint32_t> (idString[0]) << 24);
}

std::string createAUEntryDescription (const std::string& type, const std::string& subType, const std::string& manufacturer, bool useIPC)
{
    return std::string { "Audio Unit (" } + ((useIPC) ? "via IPC) (" : "") + type + " - " + subType + " - " + manufacturer + ")";
}

class AUPlugInEntry : public PlugInEntry
{
public:
    AUPlugInEntry (const std::string& type, const std::string& subType, const std::string& manufacturer, ARA::ARAAssertFunction* assertFunctionAddress)
    : _audioUnitComponent { AudioUnitPrepareComponentWithIDs (parseOSType (type), parseOSType (subType), parseOSType (manufacturer)) }
    {
        initializeARA (AudioUnitGetARAFactory (_audioUnitComponent), assertFunctionAddress);

        _description = createAUEntryDescription (type, subType, manufacturer, false);
    }

    ~AUPlugInEntry () override
    {
        uninitializeARA ();
        // unloading is not supported for Audio Units
    }

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        auto audioUnit { AudioUnitOpenInstance (_audioUnitComponent) };
        const auto plugInExtensionInstance { AudioUnitBindToARADocumentController (audioUnit, documentControllerRef, assignedRoles) };
        validatePlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
        return std::make_unique<AUPlugInInstance> (audioUnit, plugInExtensionInstance);
    }

private:
    AudioUnitComponent const _audioUnitComponent;
};

#endif // defined (__APPLE__)

/*******************************************************************************/

std::string createVST3EntryDescription (const std::string& binaryName, const std::string& optionalPlugInName, bool useIPC)
{
    return std::string { "VST3 " } + ((optionalPlugInName.empty ()) ? "" : optionalPlugInName + " ") + "@ " + binaryName + ((useIPC) ? " via IPC" : "");
}

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

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        auto vst3Instance { VST3CreateEffect (_vst3Binary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()) };
        const auto plugInExtensionInstance { VST3BindToARADocumentController (vst3Instance, documentControllerRef, assignedRoles) };
        validatePlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
        return std::make_unique<VST3PlugInInstance> (vst3Instance, plugInExtensionInstance);
    }

private:
    VST3Binary const _vst3Binary;
    const std::string _optionalPlugInName;
};

/*******************************************************************************/

#if ARA_ENABLE_IPC

class IPCPlugInInstance : public PlugInInstance
{
public:
    IPCPlugInInstance (size_t remoteRef, IPCPort& port, std::unique_ptr<ARA::ProxyPlugIn::PlugInExtension> plugInExtension)
    : PlugInInstance { plugInExtension->getInstance () },
      _remoteRef { remoteRef },
      _sender { port },
      _plugInExtension { std::move (plugInExtension) }
    {}

    ~IPCPlugInInstance () override
    {
        _sender.remoteCallWithoutReply (kIPCDestroyEffect, _remoteRef, reinterpret_cast<size_t> (_plugInExtension->getInstance ()->plugInExtensionRef));
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        _sender.remoteCallWithoutReply (kIPCStartRendering, _remoteRef, maxBlockSize, sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        const auto byteSize { static_cast<size_t> (blockSize) * sizeof (float) };
        auto resultSize { byteSize };
        ARA::BytesDecoder reply { reinterpret_cast<uint8_t*> (buffer), resultSize };
        _sender.remoteCallWithReply (reply, kIPCRenderSamples, _remoteRef, samplePosition, ARA::BytesEncoder { reinterpret_cast<const uint8_t*> (buffer), byteSize, false });
        ARA_INTERNAL_ASSERT (resultSize == byteSize);
    }

    void stopRendering () override
    {
        _sender.remoteCallWithoutReply (kIPCStopRendering, _remoteRef);
    }

private:
    size_t const _remoteRef;
    ARA::ARAIPCMessageSender _sender;
    std::unique_ptr<ARA::ProxyPlugIn::PlugInExtension> _plugInExtension;
};

/*******************************************************************************/

class IPCPlugInEntry : public PlugInEntry
{
private:
    // helper class to launch remote before initializing related port members
    struct RemoteLauncher
    {
        explicit RemoteLauncher (const std::string& launchArgs, const std::string& hostCommandsPortID, const std::string& plugInCallbacksPortID)
        {
            ARA_LOG ("launching remote plug-in process.");
#if defined (_WIN32)
            const auto commandLine { std::string { "start ARATestHost " + launchArgs +
                                                   " -_ipcRemote " + hostCommandsPortID + " " + plugInCallbacksPortID  } };
#else
            const auto commandLine { std::string { "./ARATestHost " + launchArgs +
                                                   " -_ipcRemote " + hostCommandsPortID + " " + plugInCallbacksPortID + " &" } };
#endif
            const auto launchResult { system (commandLine.c_str ()) };
            ARA_INTERNAL_ASSERT (launchResult == 0);
        }
    };

    static ARA::ProxyPlugIn::Factory& defaultGetFactory(IPCPort& hostCommandsPort)
    {
        const auto count { ARA::ProxyPlugIn::Factory::initializeFactories (hostCommandsPort) };
        ARA_INTERNAL_ASSERT (count > 0);
        return ARA::ProxyPlugIn::Factory::getFactoryAtIndex (0U);
    }

public:
    IPCPlugInEntry (const std::string& launchArgs, ARA::ARAAssertFunction* assertFunctionAddress,
                    const std::function<ARA::ProxyPlugIn::Factory& (IPCPort& hostCommandsPort)>& getFactoryFunction = defaultGetFactory)
    : _hostCommandsPortID { _createPortID () },
      _plugInCallbacksPortID { _createPortID () },
      _remoteLauncher { launchArgs, _hostCommandsPortID, _plugInCallbacksPortID },
      _plugInCallbacksThread { &IPCPlugInEntry::_plugInCallbacksThreadFunction, this },
      _hostCommandsPort { IPCPort::createConnectedToID (_hostCommandsPortID.c_str ()) },
      _proxyFactory { getFactoryFunction (_hostCommandsPort) }
    {
        setUsesIPC ();
        initializeARA (_proxyFactory.getFactory (), assertFunctionAddress);
    }

    ~IPCPlugInEntry () override
    {
        ARA::ARAIPCMessageSender (_hostCommandsPort).remoteCallWithoutReply (kIPCTerminate);

        _terminateCallbacksThread = true;
        _plugInCallbacksThread.join ();
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        return _proxyFactory.createDocumentControllerWithDocument (hostInstance, properties);
    }

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        // \todo these are the roles that our Companion API Loaders implicitly assume - they should be published properly
        const ARA::ARAPlugInInstanceRoleFlags knownRoles { ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole };

        const auto remoteDocumentControllerRef { _proxyFactory.getDocumentControllerRemoteRef (documentControllerRef) };
        IPCMessage result;
        ARA::ARAIPCMessageSender { _hostCommandsPort }.remoteCallWithReply (result, kIPCCreateARAEffect, remoteDocumentControllerRef, assignedRoles);
        size_t remoteInstanceRef;
        result.readSize (0, remoteInstanceRef);
        size_t remoteExtensionRef;
        result.readSize (1, remoteExtensionRef);
        auto plugInExtension { _proxyFactory.createPlugInExtension (remoteExtensionRef, _hostCommandsPort, documentControllerRef, knownRoles, assignedRoles) };
        validatePlugInExtensionInstance (plugInExtension->getInstance (), assignedRoles);
        return std::make_unique<IPCPlugInInstance> (remoteInstanceRef, _hostCommandsPort, std::move (plugInExtension));
    }

private:
    void _plugInCallbacksThreadFunction ()
    {
        // \todo It would be cleaner to create the port in the c'tor from the main thread,
        //       but for some reason reading audio is then much slower compared to creating it here...?
        _plugInCallbacksPort = IPCPort::createPublishingID (_plugInCallbacksPortID.c_str (), &ARA::ProxyPlugIn::Factory::plugInCallbacksDispatcher);

        while (!_terminateCallbacksThread)
            _plugInCallbacksPort.runReceiveLoop (100);
    }

private:
    const std::string _hostCommandsPortID;
    const std::string _plugInCallbacksPortID;

    RemoteLauncher _remoteLauncher;

    std::thread _plugInCallbacksThread;
    bool _terminateCallbacksThread { false };
    IPCPort _plugInCallbacksPort;
    IPCPort _hostCommandsPort;

    ARA::ProxyPlugIn::Factory& _proxyFactory;
};

/*******************************************************************************/

class IPCVST3PlugInEntry : public IPCPlugInEntry
{
public:
    IPCVST3PlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName, ARA::ARAAssertFunction* assertFunctionAddress)
    : IPCPlugInEntry { std::string { "-vst3 " } + binaryName + " " + optionalPlugInName, assertFunctionAddress,
                        [&optionalPlugInName] (IPCPort& hostCommandsPort) -> ARA::ProxyPlugIn::Factory&
                        {
                            const auto count { ARA::ProxyPlugIn::Factory::initializeFactories (hostCommandsPort) };
                            ARA_INTERNAL_ASSERT (count > 0);

                            if (optionalPlugInName.empty ())
                                return ARA::ProxyPlugIn::Factory::getFactoryAtIndex (0U);

                            for (auto i { 0U }; i < count; ++i)
                            {
                                auto& factory { ARA::ProxyPlugIn::Factory::getFactoryAtIndex (i) };
                                if (0 == std::strcmp (factory.getFactory ()->plugInName, optionalPlugInName.c_str ()))
                                    return factory;
                            }
                            ARA_INTERNAL_ASSERT (false);
                            return ARA::ProxyPlugIn::Factory::getFactoryAtIndex (0U);
                        }}
    {
        _description = createVST3EntryDescription (binaryName, optionalPlugInName, true);
    }
};

/*******************************************************************************/

#if defined (__APPLE__)

class IPCAUPlugInEntry : public IPCPlugInEntry
{
public:
    IPCAUPlugInEntry (const std::string& type, const std::string& subType, const std::string& manufacturer, ARA::ARAAssertFunction* assertFunctionAddress)
    : IPCPlugInEntry { std::string { "-au " } + type + " " + subType + " " + manufacturer, assertFunctionAddress }
    {
        _description = createAUEntryDescription (type, subType, manufacturer, true);
    }
};

#endif // defined (__APPLE__)

/*******************************************************************************/

std::unique_ptr<PlugInEntry> RemoteHost::_plugInEntry {};
bool _shutDown { false };

IPCMessage RemoteHost::_hostCommandHandler (const int32_t messageID, const IPCMessage& message)
{
    if (messageID == kIPCCreateARAEffect)
    {
        ARA::ARADocumentControllerRef documentControllerRemoteRef;
        ARA::ARAPlugInInstanceRoleFlags assignedRoles;
        ARA::decodeArguments (message, documentControllerRemoteRef, assignedRoles);

        auto plugInInstance { _plugInEntry->createARAPlugInInstanceWithRoles (ARA::ProxyHost::getDocumentControllerRefForRemoteRef (documentControllerRemoteRef), assignedRoles) };
        auto plugInExtensionRef { ARA::ProxyHost::createPlugInExtension (plugInInstance->getARAPlugInExtensionInstance ()) };

        IPCMessage reply { ARA::encodeArguments (reinterpret_cast<size_t> (plugInInstance.get ()), plugInExtensionRef) };
        plugInInstance.release ();  // ownership is transferred to host - keep around until kIPCDestroyEffect
        return reply;
    }
    else if (messageID == kIPCStartRendering)
    {
        size_t plugInInstanceRef;
        int32_t maxBlockSize;
        double sampleRate;
        ARA::decodeArguments (message, plugInInstanceRef, maxBlockSize, sampleRate);

        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->startRendering (maxBlockSize, sampleRate);
        return {};
    }
    else if (messageID == kIPCRenderSamples)
    {
        size_t plugInInstanceRef;
        int64_t samplePosition;
        // \todo using static (plus not copy bytes) here assumes single-threaded callbacks, but currently this is a valid requirement
        static std::vector<uint8_t> buffer;
        ARA::BytesDecoder writer { buffer };
        ARA::decodeArguments (message, plugInInstanceRef, samplePosition, writer);
        ARA_INTERNAL_ASSERT (buffer.size () > 0);

        // \todo this ignores potential float data alignment or byte order issues...
        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->renderSamples (static_cast<int> (buffer.size () / sizeof(float)),
                                                                        samplePosition, reinterpret_cast<float*> (buffer.data ()));
        return ARA::encodeReply (ARA::BytesEncoder { buffer, false });
    }
    else if (messageID == kIPCStopRendering)
    {
        size_t plugInInstanceRef;
        ARA::decodeArguments (message, plugInInstanceRef);

        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->stopRendering ();
        return {};
    }
    else if (messageID == kIPCDestroyEffect)
    {
        size_t plugInInstanceRef;
        ARA::ARAPlugInExtensionRef plugInExtensionRef;
        ARA::decodeArguments (message, plugInInstanceRef, plugInExtensionRef);

        ARA::ProxyHost::destroyPlugInExtension (plugInExtensionRef);
        delete reinterpret_cast<PlugInInstance*> (plugInInstanceRef);
        return {};
    }
    else if (messageID == kIPCTerminate)
    {
        _shutDown = true;
        return {};
    }
    else
    {
        return ARA::ProxyHost::hostCommandHandler (messageID, message);
    }
}

int RemoteHost::main (std::unique_ptr<PlugInEntry> plugInEntry, const std::string& hostCommandsPortID, const std::string& plugInCallbacksPortID)
{
    _plugInEntry = std::move (plugInEntry);

    auto hostCommandsPort { IPCPort::createPublishingID (hostCommandsPortID.c_str (), &_hostCommandHandler) };
    auto plugInCallbacksPort { IPCPort::createConnectedToID (plugInCallbacksPortID.c_str ()) };

    ARA::ProxyHost::addFactory (_plugInEntry->getARAFactory ());
    ARA::ProxyHost::setPlugInCallbacksPort (&plugInCallbacksPort);

    while (!_shutDown)
        hostCommandsPort.runReceiveLoop (100);

    _plugInEntry.reset ();

    return 0;
}

#endif // ARA_ENABLE_IPC

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

    if (usesIPC ())
        ARA_VALIDATE_API_CONDITION (factory->initializeARAWithConfiguration == nullptr);
    else
        ARA_VALIDATE_API_CONDITION (factory->initializeARAWithConfiguration != nullptr);
    if (usesIPC ())
        ARA_VALIDATE_API_CONDITION (factory->uninitializeARA == nullptr);
    else
        ARA_VALIDATE_API_CONDITION (factory->uninitializeARA != nullptr);

    ARA_VALIDATE_API_CONDITION (std::strlen (factory->plugInName) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->manufacturerName) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->informationURL) > 0);
    ARA_VALIDATE_API_CONDITION (std::strlen (factory->version) > 0);

    if (usesIPC ())
        ARA_VALIDATE_API_CONDITION (factory->createDocumentControllerWithDocument == nullptr);
    else
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
    if (!_usesIPC)
    {
        const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig = { desiredApiGeneration, assertFunctionAddress };
        factory->initializeARAWithConfiguration (&interfaceConfig);
    }

    _factory = factory;
}

void PlugInEntry::uninitializeARA ()
{
    if (_factory && !_usesIPC)
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

const ARA::ARADocumentControllerInstance* PlugInEntry::createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                             const ARA::ARADocumentProperties* properties)
{
    return _factory->createDocumentControllerWithDocument (hostInstance, properties);
}

/*******************************************************************************/

std::unique_ptr<PlugInEntry> PlugInEntry::parsePlugInEntry (const std::vector<std::string>& args, ARA::ARAAssertFunction* assertFunctionAddress)
{
    if (args.size () >= 2)
    {
        auto it { std::find (args.begin (), args.end (), "-vst3") };
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<VST3PlugInEntry> (binaryFileName, optionalPlugInName, assertFunctionAddress);
        }

#if ARA_ENABLE_IPC
        it = std::find (args.begin (), args.end (), "-ipc_vst3");
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<IPCVST3PlugInEntry> (binaryFileName, optionalPlugInName, assertFunctionAddress);
        }
#endif
    }

#if defined (__APPLE__)
    if (args.size () >= 4)
    {
        auto it { std::find (args.begin (), args.end (), "-au") };
        if (it < args.end () - 3)   // we need 3 follow-up arguments
            return std::make_unique<AUPlugInEntry> (*++it, *++it, *++it, assertFunctionAddress);

#if ARA_ENABLE_IPC
        it = std::find (args.begin (), args.end (), "-ipc_au");
        if (it < args.end () - 3)   // we need 3 follow-up arguments
            return std::make_unique<IPCAUPlugInEntry> (*++it, *++it, *++it, assertFunctionAddress);
#endif
    }
#endif

    return nullptr;
}
