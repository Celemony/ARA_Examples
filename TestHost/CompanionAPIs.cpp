//------------------------------------------------------------------------------
//! \file       CompanionAPIs.cpp
//!             used by the test host to load a companion API plug-in binary
//!             and create / destroy plug-in instances with ARA2 roles
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2023, Celemony Software GmbH, All Rights Reserved.
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

#ifndef ARA_ENABLE_CLAP
    #define ARA_ENABLE_CLAP 0
#endif
#if ARA_ENABLE_CLAP
    #include "ExamplesCommon/PlugInHosting/CLAPLoader.h"
#endif

#ifndef ARA_ENABLE_VST3
    #define ARA_ENABLE_VST3 0
#endif
#if ARA_ENABLE_VST3
    #include "ExamplesCommon/PlugInHosting/VST3Loader.h"
#endif

#include <codecvt>
#include <locale>
#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <thread>

/*******************************************************************************/

#if ARA_ENABLE_IPC

#if defined (__linux__)
    #error "IPC not yet implemented for Linux"
#endif


#include "ARA_Library/IPC/ARAIPCProxyHost.h"
#include "ARA_Library/IPC/ARAIPCProxyPlugIn.h"
#include "ARA_Library/IPC/ARAIPCEncoding.h"
#include "IPC/IPCMessageChannel.h"
#if USE_ARA_CF_ENCODING
    #include "ARA_Library/IPC/ARAIPCCFEncoding.h"
#else
    #include "IPC/IPCXMLEncoding.h"
#endif

std::string executablePath {};

// minimal set of commands to run a companion API plug-in through IPC
constexpr auto kIPCCreateEffectMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-1> () };
constexpr auto kIPCStartRenderingMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-2> () };
constexpr auto kIPCRenderSamplesMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-3> () };
constexpr auto kIPCStopRenderingMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-4> () };
constexpr auto kIPCDestroyEffectMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-5> () };
constexpr auto kIPCTerminateMethodID { ARA::IPC::MethodID::createWithNonARAMethodID<-6> () };


// helper function to create unique IPC message channel IDs for each run
static const std::string _createChannelID ()
{
    std::string baseID { "org.ara-audio.examples.testhost.ipc." };

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
    CFStringRef asString { CFUUIDCreateString (kCFAllocatorDefault, uuid) };
    const auto result { baseID + CFStringGetCStringPtr (asString, kCFStringEncodingMacRoman) };
    CFRelease (asString);
    CFRelease (uuid);
    return result;
#endif
}

#endif // ARA_ENABLE_IPC

/*******************************************************************************/

void PlugInInstance::validateAndSetPlugInExtensionInstance (const ARA::ARAPlugInExtensionInstance* plugInExtensionInstance, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
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

    _instance = plugInExtensionInstance;
}

/*******************************************************************************/

#if defined (__APPLE__)

class AUPlugInInstance : public PlugInInstance
{
public:
    explicit AUPlugInInstance (AudioUnitInstance audioUnit)
    : _audioUnit { audioUnit }
    {}

    ~AUPlugInInstance () override
    {
        AudioUnitCloseInstance (_audioUnit);
    }

    void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        const auto plugInExtensionInstance { AudioUnitBindToARADocumentController (_audioUnit, documentControllerRef, assignedRoles) };
        validateAndSetPlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
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

#if ARA_ENABLE_VST3

class VST3PlugInInstance : public PlugInInstance
{
public:
    explicit VST3PlugInInstance (VST3Effect vst3Effect)
    : _vst3Effect { vst3Effect }
    {}

    ~VST3PlugInInstance () override
    {
        VST3DestroyEffect (_vst3Effect);
    }

    void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        const auto plugInExtensionInstance { VST3BindToARADocumentController (_vst3Effect, documentControllerRef, assignedRoles) };
        validateAndSetPlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
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

#endif  // ARA_ENABLE_VST3

/*******************************************************************************/

#if ARA_ENABLE_CLAP

class CLAPPlugInInstance : public PlugInInstance
{
public:
    explicit CLAPPlugInInstance (CLAPPlugIn clapPlugIn)
    : _clapPlugIn { clapPlugIn }
    {}

    ~CLAPPlugInInstance () override
    {
        CLAPDestroyPlugIn (_clapPlugIn);
    }

    void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        const auto plugInExtensionInstance { CLAPBindToARADocumentController (_clapPlugIn, documentControllerRef, assignedRoles) };
        validateAndSetPlugInExtensionInstance (plugInExtensionInstance, assignedRoles);
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        CLAPStartRendering (_clapPlugIn, static_cast<uint32_t> (maxBlockSize), sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        CLAPRenderBuffer (_clapPlugIn, static_cast<uint32_t> (blockSize), samplePosition, buffer);
    }

    void stopRendering () override
    {
        CLAPStopRendering (_clapPlugIn);
    }

private:
    CLAPPlugIn const _clapPlugIn;
};

#endif  // ARA_ENABLE_CLAP

/*******************************************************************************/

#if defined (__APPLE__)

// very crude conversion from string to OSType
OSType parseOSType (const std::string& idString)
{
    ARA_INTERNAL_ASSERT (idString.size () == sizeof (OSType));
    return static_cast<uint32_t> (idString[3])        | (static_cast<uint32_t> (idString[2]) << 8) |
          (static_cast<uint32_t> (idString[1]) << 16) | (static_cast<uint32_t> (idString[0]) << 24);
}

std::string createAUEntryDescription (const std::string& type, const std::string& subType, const std::string& manufacturer)
{
    return std::string { "Audio Unit (" } + type + " - " + subType + " - " + manufacturer + ")";
}

class AUPlugInEntry : public PlugInEntry
{
public:
    AUPlugInEntry (const std::string& type, const std::string& subType, const std::string& manufacturer, bool useIPCIfPossible)
    : PlugInEntry { createAUEntryDescription (type, subType, manufacturer) },
      _audioUnitComponent { AudioUnitPrepareComponentWithIDs (parseOSType (type), parseOSType (subType), parseOSType (manufacturer)) }
    {
        AudioUnitInstance audioUnitInstance = AudioUnitOpenInstance (_audioUnitComponent, useIPCIfPossible);
        validateAndSetFactory (AudioUnitGetARAFactory (audioUnitInstance, &_messageChannel));
        AudioUnitCloseInstance (audioUnitInstance);
    }

    ~AUPlugInEntry () override
    {
        AudioUnitCleanupComponent (_audioUnitComponent);
    }

    bool usesIPC () const override
    {
        return _messageChannel != nullptr;
    }

    void initializeARA (ARA::ARAAssertFunction* assertFunctionAddress) override
    {
        if (usesIPC ())
            ARA::IPC::ARAIPCProxyPlugInInitializeARA (_messageChannel, getARAFactory ()->factoryID, getDesiredAPIGeneration (getARAFactory ()));
        else
            PlugInEntry::initializeARA (assertFunctionAddress);
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        if (usesIPC ())
            return ARA::IPC::ARAIPCProxyPlugInCreateDocumentControllerWithDocument (_messageChannel,
                                                                                    ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (_messageChannel, 0)->factoryID,
                                                                                    hostInstance, properties);
        else
            return PlugInEntry::createDocumentControllerWithDocument (hostInstance, properties);
    }

    void uninitializeARA () override
    {
        if (usesIPC ())
            ARA::IPC::ARAIPCProxyPlugInUninitializeARA (_messageChannel, getARAFactory ()->factoryID);
        else
            PlugInEntry::uninitializeARA ();
    }

    std::unique_ptr<PlugInInstance> createPlugInInstance () override
    {
        auto audioUnit { AudioUnitOpenInstance (_audioUnitComponent, usesIPC ()) };
        return std::make_unique<AUPlugInInstance> (audioUnit);
    }

private:
    AudioUnitComponent const _audioUnitComponent;
    ARA::IPC::ARAIPCMessageChannel* _messageChannel {};
};

#endif // defined (__APPLE__)

/*******************************************************************************/

std::string createEntryDescription (const std::string& apiName, const std::string& binaryName, const std::string& optionalPlugInName)
{
    return apiName + " " + ((optionalPlugInName.empty ()) ? "" : optionalPlugInName + " ") + "@ " + binaryName;
}

#if ARA_ENABLE_VST3

class VST3PlugInEntry : public PlugInEntry
{
public:
    VST3PlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName)
    : PlugInEntry { createEntryDescription ("VST3", binaryName, optionalPlugInName) },
      _vst3Binary { VST3LoadBinary (binaryName.c_str ()) },
      _optionalPlugInName { optionalPlugInName }
    {
        validateAndSetFactory (VST3GetARAFactory (_vst3Binary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()));
    }

    ~VST3PlugInEntry () override
    {
        VST3UnloadBinary (_vst3Binary);
    }

    std::unique_ptr<PlugInInstance> createPlugInInstance () override
    {
        auto vst3Instance { VST3CreateEffect (_vst3Binary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()) };
        return std::make_unique<VST3PlugInInstance> (vst3Instance);
    }

private:
    VST3Binary const _vst3Binary;
    const std::string _optionalPlugInName;
};

#endif  // ARA_ENABLE_VST3

/*******************************************************************************/

#if ARA_ENABLE_CLAP

class CLAPPlugInEntry : public PlugInEntry
{
public:
    CLAPPlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName)
    : PlugInEntry { createEntryDescription ("CLAP", binaryName, optionalPlugInName) },
      _clapBinary { CLAPLoadBinary (binaryName.c_str ()) },
      _optionalPlugInName { optionalPlugInName }
    {
        validateAndSetFactory (CLAPGetARAFactory (_clapBinary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()));
    }

    ~CLAPPlugInEntry () override
    {
        CLAPUnloadBinary (_clapBinary);
    }

    std::unique_ptr<PlugInInstance> createPlugInInstance () override
    {
        auto clapInstance { CLAPCreatePlugIn (_clapBinary, (_optionalPlugInName.empty ()) ? nullptr : _optionalPlugInName.c_str ()) };
        return std::make_unique<CLAPPlugInInstance> (clapInstance);
    }

private:
    CLAPBinary const _clapBinary;
    const std::string _optionalPlugInName;
};

#endif  // ARA_ENABLE_CLAP

/*******************************************************************************/

#if ARA_ENABLE_IPC

class IPCPlugInInstance : public PlugInInstance, protected ARA::IPC::RemoteCaller
{
public:
    IPCPlugInInstance (size_t remoteRef, ARA::IPC::ARAIPCMessageChannel* messageChannel)
    : RemoteCaller { messageChannel },
      _remoteRef { remoteRef }
    {}

    ~IPCPlugInInstance () override
    {
        remoteCall (kIPCDestroyEffectMethodID, _remoteRef);
        if (getARAPlugInExtensionInstance ())
            ARA::IPC::ARAIPCProxyPlugInCleanupBinding (getARAPlugInExtensionInstance ());
    }

    void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        // \todo these are the roles that our Companion API Loaders implicitly assume - they should be published properly
        const ARA::ARAPlugInInstanceRoleFlags knownRoles { ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole };
        auto plugInExtension { ARA::IPC::ARAIPCProxyPlugInBindToDocumentController (_remoteRef, getMessageChannel (), documentControllerRef, knownRoles, assignedRoles) };
        validateAndSetPlugInExtensionInstance (plugInExtension, assignedRoles);
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        remoteCall (kIPCStartRenderingMethodID, _remoteRef, maxBlockSize, sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        const auto byteSize { static_cast<size_t> (blockSize) * sizeof (float) };
        auto resultSize { byteSize };
        ARA::IPC::BytesDecoder reply { reinterpret_cast<uint8_t*> (buffer), resultSize };
        remoteCall (reply, kIPCRenderSamplesMethodID, _remoteRef, samplePosition,
                    ARA::IPC::BytesEncoder { reinterpret_cast<const uint8_t*> (buffer), byteSize, false });
        ARA_INTERNAL_ASSERT (resultSize == byteSize);
    }

    void stopRendering () override
    {
        remoteCall (kIPCStopRenderingMethodID, _remoteRef);
    }

private:
    size_t const _remoteRef;
};


/*******************************************************************************/

// helper class to launch remote before initializing related IPC channel members
struct RemoteLauncher
{
    explicit RemoteLauncher (const std::string& launchArgs, const std::string& channelID)
    {
        ARA_LOG ("launching remote plug-in process.");
#if defined (_WIN32)
        const auto commandLine { std::string { "start " + executablePath + " " + launchArgs + " -_ipcRemote " + channelID  } };
#else
        const auto commandLine { std::string { executablePath + " " + launchArgs + " -_ipcRemote " + channelID + " &" } };
#endif
        const auto launchResult { system (commandLine.c_str ()) };
        ARA_INTERNAL_ASSERT (launchResult == 0);
    }
};

/*******************************************************************************/

class IPCPlugInEntry : public PlugInEntry, private RemoteLauncher, protected ARA::IPC::RemoteCaller, public ARA::IPC::ARAIPCProxyPlugInMessageHandler
{
private:
    static const ARA::ARAFactory* defaultGetFactory (ARA::IPC::ARAIPCMessageChannel* messageChannel)
    {
        const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (messageChannel) };
        ARA_INTERNAL_ASSERT (count > 0);
        return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (messageChannel, 0U);
    }

    IPCPlugInEntry (std::string&& description, const std::string& launchArgs,
                    const std::string channelID,
                    const std::function<const ARA::ARAFactory* (ARA::IPC::ARAIPCMessageChannel*)>& getFactoryFunction)
    : PlugInEntry { std::move (description) },
      RemoteLauncher { launchArgs, channelID },
      ARA::IPC::RemoteCaller { IPCMessageChannel::createConnectedToID (channelID, this) }
    {
        validateAndSetFactory (getFactoryFunction (static_cast<IPCMessageChannel*> (getMessageChannel ())));
    }

public:
    // \todo the current ARA IPC implementation does not support sending ARA asserts to the host...
    IPCPlugInEntry (std::string&& description, const std::string& launchArgs,
                    const std::function<const ARA::ARAFactory* (ARA::IPC::ARAIPCMessageChannel*)>& getFactoryFunction = defaultGetFactory)
    : IPCPlugInEntry { std::move (description), launchArgs, _createChannelID (), getFactoryFunction }
    {}

    ~IPCPlugInEntry () override
    {
        remoteCall (kIPCTerminateMethodID);

        delete getMessageChannel ();
    }

    bool usesIPC () const override
    {
        return true;
    }

#if !USE_ARA_BACKGROUND_IPC
    void idleThreadForDuration (int32_t milliseconds) override
    {
        static_cast<IPCMessageChannel*> (getMessageChannel ())->runReceiveLoop (milliseconds);
    }
#endif

    void initializeARA (ARA::ARAAssertFunction* /*assertFunctionAddress*/) override
    {
        ARA::IPC::ARAIPCProxyPlugInInitializeARA (getMessageChannel (), getARAFactory ()->factoryID, getDesiredAPIGeneration (getARAFactory ()));
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        return ARA::IPC::ARAIPCProxyPlugInCreateDocumentControllerWithDocument (getMessageChannel (), getARAFactory ()->factoryID, hostInstance, properties);
    }

    void uninitializeARA () override
    {
        ARA::IPC::ARAIPCProxyPlugInUninitializeARA (getMessageChannel (), getARAFactory ()->factoryID);
    }

    std::unique_ptr<PlugInInstance> createPlugInInstance () override
    {
        size_t remoteInstanceRef {};
        remoteCall (remoteInstanceRef, kIPCCreateEffectMethodID);
        return std::make_unique<IPCPlugInInstance> (remoteInstanceRef, getMessageChannel ());
    }
};

/*******************************************************************************/

class IPCGenericPlugInEntry : public IPCPlugInEntry
{
protected:
    IPCGenericPlugInEntry (const std::string& commandLineArg, const std::string& apiName, const std::string& binaryName, const std::string& optionalPlugInName)
    : IPCPlugInEntry { createEntryDescription (apiName, binaryName, optionalPlugInName),
                        commandLineArg + " " + binaryName + " " + optionalPlugInName,
                        [&optionalPlugInName] (ARA::IPC::ARAIPCMessageChannel* messageChannel) -> const ARA::ARAFactory*
                        {
                            const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (messageChannel) };
                            ARA_INTERNAL_ASSERT (count > 0);

                            if (optionalPlugInName.empty ())
                                return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (messageChannel, 0U);

                            for (auto i { 0U }; i < count; ++i)
                            {
                                auto factory { ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (messageChannel, i) };
                                if (0 == std::strcmp (factory->plugInName, optionalPlugInName.c_str ()))
                                    return factory;
                            }
                            ARA_INTERNAL_ASSERT (false);
                            return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (messageChannel, 0U);
                        } }
    {}
};

class IPCVST3PlugInEntry : public IPCGenericPlugInEntry
{
public:
    IPCVST3PlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName)
    : IPCGenericPlugInEntry ("-vst3", "VST3", binaryName, optionalPlugInName)
    {}
};

#if ARA_ENABLE_CLAP

class IPCCLAPPlugInEntry : public IPCGenericPlugInEntry
{
public:
    IPCCLAPPlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName)
    : IPCGenericPlugInEntry ("-clap", "CLAP", binaryName, optionalPlugInName)
    {}
};

#endif  // ARA_ENABLE_CLAP

/*******************************************************************************/

#if defined (__APPLE__)

class IPCAUPlugInEntry : public IPCPlugInEntry
{
public:
    IPCAUPlugInEntry (const std::string& type, const std::string& subType, const std::string& manufacturer)
    : IPCPlugInEntry { createAUEntryDescription (type, subType, manufacturer),
                        std::string { "-au " } + type + " " + subType + " " + manufacturer }
    {}
};

#endif // defined (__APPLE__)

/*******************************************************************************/

std::unique_ptr<PlugInEntry> _plugInEntry {};
bool _shutDown { false };

class ProxyMessageHandler : public ARA::IPC::ARAIPCProxyHostMessageHandler
{
public:
    void handleReceivedMessage (ARA::IPC::ARAIPCMessageChannel* messageChannel,
                                const ARA::IPC::ARAIPCMessageID messageID,
                                const ARA::IPC::ARAIPCMessageDecoder* decoder,
                                ARA::IPC::ARAIPCMessageEncoder* const replyEncoder) override
    {
        if (messageID == kIPCCreateEffectMethodID)
        {
            auto plugInInstance { _plugInEntry->createPlugInInstance () };
            const auto plugInInstanceRef { reinterpret_cast<size_t> (plugInInstance.get ()) };
            plugInInstance.release ();  // ownership is transferred to host - keep around until kIPCDestroyEffect
            ARA::IPC::encodeArguments (replyEncoder, plugInInstanceRef);
        }
        else if (messageID == kIPCStartRenderingMethodID)
        {
            size_t plugInInstanceRef;
            int32_t maxBlockSize;
            double sampleRate;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef, maxBlockSize, sampleRate);

            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->startRendering (maxBlockSize, sampleRate);
        }
        else if (messageID == kIPCRenderSamplesMethodID)
        {
            size_t plugInInstanceRef;
            int64_t samplePosition;
            // \todo using static (plus not copy bytes) here assumes single-threaded callbacks, but currently this is a valid requirement
            static std::vector<uint8_t> buffer;
            ARA::IPC::BytesDecoder writer { buffer };
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef, samplePosition, writer);
            ARA_INTERNAL_ASSERT (buffer.size () > 0);

            // \todo this ignores potential float data alignment or byte order issues...
            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->renderSamples (static_cast<int> (buffer.size () / sizeof (float)),
                                                                            samplePosition, reinterpret_cast<float*> (buffer.data ()));
            ARA::IPC::encodeReply (replyEncoder, ARA::IPC::BytesEncoder { buffer, false });
        }
        else if (messageID == kIPCStopRenderingMethodID)
        {
            size_t plugInInstanceRef;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef);

            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->stopRendering ();
        }
        else if (messageID == kIPCDestroyEffectMethodID)
        {
            size_t plugInInstanceRef;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef);

            delete reinterpret_cast<PlugInInstance*> (plugInInstanceRef);
        }
        else if (messageID == kIPCTerminateMethodID)
        {
            _shutDown = true;
        }
        else
        {
            ARAIPCProxyHostMessageHandler::handleReceivedMessage (messageChannel, messageID, decoder, replyEncoder);
        }
    }
};

namespace RemoteHost
{
int main (std::unique_ptr<PlugInEntry> plugInEntry, const std::string& channelID)
{
    _plugInEntry = std::move (plugInEntry);

    ProxyMessageHandler handler;
    auto plugInCallbacksChannel { IPCMessageChannel::createPublishingID (channelID, &handler) };

    ARA::IPC::ARAIPCProxyHostAddFactory (_plugInEntry->getARAFactory ());
    ARA::IPC::ARAIPCProxyHostSetBindingHandler ([] (ARA::IPC::ARAIPCMessageChannel* /*messageChannel*/, ARA::IPC::ARAIPCPlugInInstanceRef plugInInstanceRef,
                                                    ARA::ARADocumentControllerRef controllerRef,
                                                    ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
                                                        -> const ARA::ARAPlugInExtensionInstance*
                                                {
                                                    // \todo these are the roles that our Companion API Loaders implicitly assume - they should be published properly
                                                    ARA_INTERNAL_ASSERT (knownRoles == (ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole) );
                                                    reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->bindToDocumentControllerWithRoles (controllerRef, assignedRoles);
                                                    return reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->getARAPlugInExtensionInstance ();
                                                });

    while (!_shutDown)
        plugInCallbacksChannel->runReceiveLoop (100 /*ms*/);

    _plugInEntry.reset ();

    return 0;
}
}

#endif // ARA_ENABLE_IPC

/*******************************************************************************/

void PlugInEntry::validateAndSetFactory (const ARA::ARAFactory* factory)
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

    _factory = factory;
}

ARA::ARAAPIGeneration PlugInEntry::getDesiredAPIGeneration (const ARA::ARAFactory* const factory)
{
    ARA::ARAAPIGeneration desiredApiGeneration { ARA::kARAAPIGeneration_2_0_Final };
    if (desiredApiGeneration > factory->highestSupportedApiGeneration)
        desiredApiGeneration = factory->highestSupportedApiGeneration;
    return desiredApiGeneration;
}

void PlugInEntry::initializeARA (ARA::ARAAssertFunction* assertFunctionAddress)
{
    ARA_INTERNAL_ASSERT (_factory);

    // initialize ARA factory with interface configuration
    const ARA::SizedStruct<ARA_STRUCT_MEMBER (ARAInterfaceConfiguration, assertFunctionAddress)> interfaceConfig = { getDesiredAPIGeneration (_factory), assertFunctionAddress };
    _factory->initializeARAWithConfiguration (&interfaceConfig);
}

const ARA::ARADocumentControllerInstance* PlugInEntry::createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                             const ARA::ARADocumentProperties* properties)
{
    return _factory->createDocumentControllerWithDocument (hostInstance, properties);
}

void PlugInEntry::uninitializeARA ()
{
    ARA_INTERNAL_ASSERT (_factory);
    _factory->uninitializeARA ();
}

void PlugInEntry::idleThreadForDuration (int32_t milliseconds)
{
    std::this_thread::sleep_for (std::chrono::milliseconds { milliseconds });
}

/*******************************************************************************/

std::unique_ptr<PlugInEntry> PlugInEntry::parsePlugInEntry (const std::vector<std::string>& args)
{
#if ARA_ENABLE_IPC
    executablePath = args[0];
#endif

#if ARA_ENABLE_VST3
    if (args.size () >= 3)
    {
        auto it { std::find (args.begin (), args.end (), "-vst3") };
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<VST3PlugInEntry> (binaryFileName, optionalPlugInName);
        }

#if ARA_ENABLE_IPC
        it = std::find (args.begin (), args.end (), "-ipc_vst3");
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<IPCVST3PlugInEntry> (binaryFileName, optionalPlugInName);
        }
#endif
    }
#endif  // ARA_ENABLE_VST3

#if ARA_ENABLE_CLAP
    if (args.size () >= 3)
    {
        auto it { std::find (args.begin (), args.end (), "-clap") };
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<CLAPPlugInEntry> (binaryFileName, optionalPlugInName);
        }

#if ARA_ENABLE_IPC
        it = std::find (args.begin (), args.end (), "-ipc_clap");
        if (it < args.end () - 1)   // we need at least one follow-up argument
        {
            const auto& binaryFileName { *++it };
            std::string optionalPlugInName {};
            if ((++it != args.end ()) && ((*it)[0] != '-'))
                optionalPlugInName = *it;
            return std::make_unique<IPCCLAPPlugInEntry> (binaryFileName, optionalPlugInName);
        }
#endif
    }
#endif  // ARA_ENABLE_CLAP

#if defined (__APPLE__)
    if (args.size () >= 5)
    {
        auto it { std::find (args.begin (), args.end (), "-au") };
        if (it < args.end () - 3)   // we need 3 follow-up arguments
            return std::make_unique<AUPlugInEntry> (*++it, *++it, *++it, false);

#if ARA_ENABLE_IPC
        it = std::find (args.begin (), args.end (), "-ipc_au");
        if (it < args.end () - 3)   // we need 3 follow-up arguments
        {
            const std::string type { *++it };
            const std::string subType { *++it };
            const std::string manufacturer { *++it };
            AudioUnitComponent component { AudioUnitPrepareComponentWithIDs (parseOSType (type), parseOSType (subType), parseOSType (manufacturer)) };
            const bool isAUV2 { AudioUnitIsV2 (component) };
            AudioUnitCleanupComponent (component);

            if (isAUV2)
                return std::make_unique<IPCAUPlugInEntry> (type, subType, manufacturer);
            else
                return std::make_unique<AUPlugInEntry> (type, subType, manufacturer, true);
        }
#endif
    }
#endif  // defined (__APPLE__)

    return nullptr;
}
