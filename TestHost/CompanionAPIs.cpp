//------------------------------------------------------------------------------
//! \file       CompanionAPIs.cpp
//!             used by the test host to load a companion API plug-in binary
//!             and create / destroy plug-in instances with ARA2 roles
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
constexpr auto kIPCCreateEffectMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-1> () };
constexpr auto kIPCStartRenderingMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-2> () };
constexpr auto kIPCRenderSamplesMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-3> () };
constexpr auto kIPCStopRenderingMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-4> () };
constexpr auto kIPCDestroyEffectMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-5> () };
constexpr auto kIPCTerminateMethodID { ARA::IPC::MethodID::createWithCustomMessageID<-6> () };


constexpr auto mainChannelIDSuffix { ".main" };
constexpr auto otherChannelIDSuffix { ".other" };


#if defined (__GNUC__)
    _Pragma ("GCC diagnostic push")
    _Pragma ("GCC diagnostic ignored \"-Wunused-template\"")
#endif

ARA_MAP_IPC_REF (ARA::IPC::Connection, ARA::IPC::ARAIPCConnectionRef)

#if defined (__GNUC__)
    _Pragma ("GCC diagnostic pop")
#endif


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

    void startRendering (int channelCount, int maxBlockSize, double sampleRate) override
    {
        AudioUnitStartRendering (_audioUnit, static_cast<UInt32> (channelCount), static_cast<UInt32> (maxBlockSize), sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float** buffers) override
    {
        AudioUnitRenderBuffer (_audioUnit, static_cast<UInt32> (blockSize), samplePosition, buffers);
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

    void startRendering (int channelCount, int maxBlockSize, double sampleRate) override
    {
        VST3StartRendering (_vst3Effect, channelCount, maxBlockSize, sampleRate);
        _sampleRate = sampleRate;
    }

    void renderSamples (int blockSize, int64_t samplePosition, float** buffers) override
    {
        VST3RenderBuffer (_vst3Effect, blockSize, _sampleRate, samplePosition, buffers);
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

    void startRendering (int channelCount, int maxBlockSize, double sampleRate) override
    {
        CLAPStartRendering (_clapPlugIn, static_cast<uint32_t> (channelCount), static_cast<uint32_t> (maxBlockSize), sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float** buffers) override
    {
        CLAPRenderBuffer (_clapPlugIn, static_cast<uint32_t> (blockSize), samplePosition, buffers);
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
static OSType parseOSType (const std::string& idString)
{
    ARA_INTERNAL_ASSERT (idString.size () == sizeof (OSType));
    return static_cast<uint32_t> (idString[3])        | (static_cast<uint32_t> (idString[2]) << 8) |
          (static_cast<uint32_t> (idString[1]) << 16) | (static_cast<uint32_t> (idString[0]) << 24);
}

static std::string createAUEntryDescription (const std::string& type, const std::string& subType, const std::string& manufacturer)
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
        validateAndSetFactory (AudioUnitGetARAFactory (audioUnitInstance, &_connectionRef));
        AudioUnitCloseInstance (audioUnitInstance);
    }

    ~AUPlugInEntry () override
    {
        AudioUnitCleanupComponent (_audioUnitComponent);
    }

    bool usesIPC () const override
    {
        return _connectionRef != nullptr;
    }

    void initializeARA (ARA::ARAAssertFunction* assertFunctionAddress) override
    {
        if (usesIPC ())
            ARA::IPC::ARAIPCProxyPlugInInitializeARA (_connectionRef, getARAFactory ()->factoryID, getDesiredAPIGeneration (getARAFactory ()));
        else
            PlugInEntry::initializeARA (assertFunctionAddress);
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        if (usesIPC ())
            return ARA::IPC::ARAIPCProxyPlugInCreateDocumentControllerWithDocument (_connectionRef, getARAFactory ()->factoryID, hostInstance, properties);
        else
            return PlugInEntry::createDocumentControllerWithDocument (hostInstance, properties);
    }

    void uninitializeARA () override
    {
        if (usesIPC ())
            ARA::IPC::ARAIPCProxyPlugInUninitializeARA (_connectionRef, getARAFactory ()->factoryID);
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
    ARA::IPC::ARAIPCConnectionRef _connectionRef {};
};

#endif // defined (__APPLE__)

/*******************************************************************************/

static std::string createEntryDescription (const std::string& apiName, const std::string& binaryName, const std::string& optionalPlugInName)
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

class Connection : public ARA::IPC::Connection
{
public:
    Connection (IPCMessageChannel* mainThreadChannel, IPCMessageChannel* otherThreadsChannel)
    : ARA::IPC::Connection { &_runReceiveLoop, this },
      _mainThreadChannel { mainThreadChannel }
    {
        setMainThreadChannel (mainThreadChannel);
        setOtherThreadsChannel (otherThreadsChannel);
    }

    ARA::IPC::MessageEncoder* createEncoder () override
    {
#if USE_ARA_CF_ENCODING
        return new ARA::IPC::CFMessageEncoder {};
#else
        return new IPCXMLMessageEncoder {};
#endif
    }

    // \todo currently not implemented, we rely on running on the same machine for now
    //       C++20 offers std::endian which allows for a simple implementation upon connecting...
    bool receiverEndianessMatches ()  override
    {
        return true;
    }

    bool runReceiveLoop (int32_t milliseconds)
    {
        return _mainThreadChannel->runReceiveLoop (milliseconds);
    }

private:
    static void _runReceiveLoop (void* connection)
    {
        static_cast<Connection*> (connection)->runReceiveLoop (10);
    }

private:
    IPCMessageChannel* const _mainThreadChannel;
};

class ProxyPlugInConnection : public Connection
{
public:
    using Connection::Connection;
    bool sendsHostMessages () const override { return true; }
};

class ProxyHostConnection : public Connection
{
public:
    using Connection::Connection;
    bool sendsHostMessages () const override { return false; }
};

class IPCPlugInInstance : public PlugInInstance, protected ARA::IPC::RemoteCaller
{
public:
    IPCPlugInInstance (ARA::IPC::ARAIPCPlugInInstanceRef remoteRef, ARA::IPC::Connection* connection)
    : RemoteCaller { connection },
      _remoteRef { remoteRef },
      _channelCount { 0 }
    {}

    ~IPCPlugInInstance () override
    {
        remoteCall (kIPCDestroyEffectMethodID, _remoteRef);
        if (getARAPlugInExtensionInstance ())
            ARA::IPC::ARAIPCProxyPlugInCleanupBinding (getARAPlugInExtensionInstance ());
    }

    void bindToDocumentControllerWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        // \todo these are the roles that our companion API Loaders implicitly assume - they should be published properly
        const ARA::ARAPlugInInstanceRoleFlags knownRoles { ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole };
        auto plugInExtension { ARA::IPC::ARAIPCProxyPlugInBindToDocumentController (_remoteRef, documentControllerRef, knownRoles, assignedRoles) };
        validateAndSetPlugInExtensionInstance (plugInExtension, assignedRoles);
    }

    void startRendering (int channelCount, int maxBlockSize, double sampleRate) override
    {
        ARA_INTERNAL_ASSERT (_channelCount == 0);
        _channelCount = channelCount;
        remoteCall (kIPCStartRenderingMethodID, _remoteRef, channelCount, maxBlockSize, sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float** buffers) override
    {
        ARA_INTERNAL_ASSERT (_channelCount != 0);
        const auto channelCount { static_cast<size_t> (_channelCount) };
        const auto bufferSize { sizeof (float) * static_cast<size_t> (blockSize) };

        // recursively limit message size to keep IPC responsive
        if (blockSize > 8192)
        {
            const auto blockSize1 { blockSize / 2 };
            renderSamples (blockSize1, samplePosition, buffers);

            const auto blockSize2 { blockSize - blockSize1 };
            std::vector<float*> buffers2;
            buffers2.reserve (channelCount);
            for (auto i { 0U }; i < channelCount; ++i)
                buffers2.emplace_back (buffers[i] + static_cast<size_t> (blockSize1));

            return renderSamples (blockSize2, samplePosition + blockSize1, buffers2.data ());
        }

        // custom decoding to deal with float data memory ownership
        CustomDecodeFunction customDecode {
            [&channelCount, &bufferSize, &buffers] (const ARA::IPC::MessageDecoder* decoder) -> void
            {
                std::vector<size_t> resultSizes;
                std::vector<ARA::IPC::BytesDecoder> decoders;
                resultSizes.reserve (channelCount);
                decoders.reserve (channelCount);
                for (auto i { 0U }; i < channelCount; ++i)
                {
                    resultSizes.emplace_back (bufferSize);
                    decoders.emplace_back (reinterpret_cast<uint8_t*> (buffers[i]), resultSizes[i]);
                }

                ARA::IPC::ArrayArgument<ARA::IPC::BytesDecoder> channelData { decoders.data (), decoders.size () };
                const auto success { decodeReply (channelData, decoder) };
                ARA_INTERNAL_ASSERT (success);
                if (success)
                    ARA_INTERNAL_ASSERT (channelData.count == channelCount);

                for (auto i { 0U }; i < channelCount; ++i)
                {
                    if (success)
                        ARA_INTERNAL_ASSERT (resultSizes[i] == bufferSize);
                    else
                        std::memset (buffers[i], 0, bufferSize);
                }

            } };

        remoteCall (customDecode, kIPCRenderSamplesMethodID, _remoteRef, blockSize, samplePosition );
    }

    void stopRendering () override
    {
        ARA_INTERNAL_ASSERT (_channelCount != 0);
        remoteCall (kIPCStopRenderingMethodID, _remoteRef);
        _channelCount = 0;
    }

private:
    ARA::IPC::ARAIPCPlugInInstanceRef const _remoteRef;
    int _channelCount;
};

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

class IPCPlugInEntry : public PlugInEntry, private RemoteLauncher
{
private:
    static const ARA::ARAFactory* defaultGetFactory (ARA::IPC::ARAIPCConnectionRef connection)
    {
        const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (connection) };
        ARA_INTERNAL_ASSERT (count > 0);
        return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (connection, 0U);
    }

    IPCPlugInEntry (std::string&& description, const std::string& launchArgs,
                    const std::string& channelID,
                    const std::function<const ARA::ARAFactory* (ARA::IPC::ARAIPCConnectionRef)>& getFactoryFunction)
    : PlugInEntry { std::move (description) },
      RemoteLauncher { launchArgs, channelID },
      _connection { IPCMessageChannel::createConnectedToID (channelID + mainChannelIDSuffix),
                    IPCMessageChannel::createConnectedToID (channelID + otherChannelIDSuffix) },
      _proxyPlugIn { &_connection }
    {
        _connection.setMessageHandler (ARA::IPC::ProxyPlugIn::handleReceivedMessage);
        validateAndSetFactory (getFactoryFunction (toIPCRef (&_connection)));
    }

public:
    // \todo the current ARA IPC implementation does not support sending ARA asserts to the host...
    IPCPlugInEntry (std::string&& description, const std::string& launchArgs,
                    const std::function<const ARA::ARAFactory* (ARA::IPC::ARAIPCConnectionRef)>& getFactoryFunction = defaultGetFactory)
    : IPCPlugInEntry { std::move (description), launchArgs, _createChannelID (), getFactoryFunction }
    {}

    ~IPCPlugInEntry () override
    {
        _proxyPlugIn.remoteCall (kIPCTerminateMethodID);
    }

    bool usesIPC () const override
    {
        return true;
    }

#if !USE_ARA_BACKGROUND_IPC
    void idleThreadForDuration (int32_t milliseconds) override
    {
        _connection.runReceiveLoop (milliseconds);
    }
#endif

    void initializeARA (ARA::ARAAssertFunction* /*assertFunctionAddress*/) override
    {
        ARA::IPC::ARAIPCProxyPlugInInitializeARA (toIPCRef (&_connection), getARAFactory ()->factoryID, getDesiredAPIGeneration (getARAFactory ()));
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        return ARA::IPC::ARAIPCProxyPlugInCreateDocumentControllerWithDocument (toIPCRef (&_connection), getARAFactory ()->factoryID, hostInstance, properties);
    }

    void uninitializeARA () override
    {
        ARA::IPC::ARAIPCProxyPlugInUninitializeARA (toIPCRef (&_connection), getARAFactory ()->factoryID);
    }

    std::unique_ptr<PlugInInstance> createPlugInInstance () override
    {
        ARA::IPC::ARAIPCPlugInInstanceRef remoteInstanceRef {};
        _proxyPlugIn.remoteCall (remoteInstanceRef, kIPCCreateEffectMethodID);
        return std::make_unique<IPCPlugInInstance> (remoteInstanceRef, &_connection);
    }

private:
    ProxyPlugInConnection _connection;
    ARA::IPC::ProxyPlugIn _proxyPlugIn;
};

/*******************************************************************************/

class IPCGenericPlugInEntry : public IPCPlugInEntry
{
protected:
    IPCGenericPlugInEntry (const std::string& commandLineArg, const std::string& apiName, const std::string& binaryName, const std::string& optionalPlugInName)
    : IPCPlugInEntry { createEntryDescription (apiName, binaryName, optionalPlugInName),
                        commandLineArg + " " + binaryName + " " + optionalPlugInName,
                        [&optionalPlugInName] (ARA::IPC::ARAIPCConnectionRef connection) -> const ARA::ARAFactory*
                        {
                            const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (connection) };
                            ARA_INTERNAL_ASSERT (count > 0);

                            if (optionalPlugInName.empty ())
                                return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (connection, 0U);

                            for (auto i { 0U }; i < count; ++i)
                            {
                                auto factory { ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (connection, i) };
                                if (0 == std::strcmp (factory->plugInName, optionalPlugInName.c_str ()))
                                    return factory;
                            }
                            ARA_INTERNAL_ASSERT (false);
                            return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (connection, 0U);
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

class ProxyHost : public ARA::IPC::ProxyHost
{
public:
    ProxyHost (Connection* connection)
    : ARA::IPC::ProxyHost { connection }
    {
        connection->setMessageHandler ([this] (auto&& ...args) { handleReceivedMessage (args...); });
    }

    void handleReceivedMessage (const ARA::IPC::MessageID messageID, const ARA::IPC::MessageDecoder* const decoder,
                                ARA::IPC::MessageEncoder* const replyEncoder)
    {
        if (!ARA::IPC::MethodID::isCustomMessageID (messageID))
        {
            ARA::IPC::ProxyHost::handleReceivedMessage (messageID, decoder, replyEncoder);
        }
        else if (messageID == kIPCCreateEffectMethodID)
        {
            auto plugInInstance { _plugInEntry->createPlugInInstance () };
            const auto plugInInstanceRef { reinterpret_cast<size_t> (plugInInstance.get ()) };
            plugInInstance.release ();  // ownership is transferred to host - keep around until kIPCDestroyEffect
            ARA::IPC::encodeArguments (replyEncoder, plugInInstanceRef);
        }
        else if (messageID == kIPCStartRenderingMethodID)
        {
            size_t plugInInstanceRef;
            int32_t channelCount;
            int32_t maxBlockSize;
            double sampleRate;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef, channelCount, maxBlockSize, sampleRate);

            auto& renderData { _plugInInstanceRenderDataMap[plugInInstanceRef] = { RenderData {} } };
            for (auto i { 0U }; i < static_cast<size_t> (channelCount); ++i)
            {
                renderData.samples.emplace_back (static_cast<size_t> (maxBlockSize));
                renderData.buffers.emplace_back (renderData.samples[i].data ());
                renderData.encoders.emplace_back (nullptr, 0, false);   // encoders will be updated with proper buffer area when rendering
            }

            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->startRendering (channelCount, maxBlockSize, sampleRate);
        }
        else if (messageID == kIPCRenderSamplesMethodID)
        {
            size_t plugInInstanceRef;
            int32_t blockSize;
            int64_t samplePosition;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef, blockSize, samplePosition);

            auto& renderData { _plugInInstanceRenderDataMap[plugInInstanceRef] };
            const auto channelCount { renderData.samples.size () };
            ARA_INTERNAL_ASSERT (static_cast<size_t> (blockSize) <= renderData.samples[0].size ());
            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->renderSamples (blockSize, samplePosition, renderData.buffers.data ());

            // \todo this ignores potential byte order issues...
            for (auto i { 0U }; i < static_cast<size_t> (channelCount); ++i)
                renderData.encoders[i] = ARA::IPC::BytesEncoder { reinterpret_cast<const uint8_t *> (renderData.samples[i].data ()),
                                                                  static_cast<size_t>(blockSize) * sizeof (float), false };
            ARA::IPC::encodeReply (replyEncoder, ARA::IPC::ArrayArgument<const ARA::IPC::BytesEncoder> { renderData.encoders.data (), renderData.encoders.size () });
        }
        else if (messageID == kIPCStopRenderingMethodID)
        {
            size_t plugInInstanceRef;
            ARA::IPC::decodeArguments (decoder, plugInInstanceRef);

            reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->stopRendering ();

#if ARA_ENABLE_INTERNAL_ASSERTS
            const auto erased
#endif
                              { _plugInInstanceRenderDataMap.erase (plugInInstanceRef) };
            ARA_INTERNAL_ASSERT (erased != 0);
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
            ARA_INTERNAL_ASSERT (false && "unhandled message ID");
        }
    }
private:
    struct RenderData
    {
        std::vector<std::vector<float>> samples;
        std::vector<float*> buffers;
        std::vector<ARA::IPC::BytesEncoder> encoders;
    };
    std::map<size_t, RenderData> _plugInInstanceRenderDataMap;
};

namespace RemoteHost
{
int main (std::unique_ptr<PlugInEntry> plugInEntry, const std::string& channelID)
{
    _plugInEntry = std::move (plugInEntry);

    ProxyHostConnection connection { IPCMessageChannel::createPublishingID (channelID + mainChannelIDSuffix),
                                     IPCMessageChannel::createPublishingID (channelID + otherChannelIDSuffix) };
    ProxyHost proxy { &connection };

    ARA::IPC::ARAIPCProxyHostAddFactory (_plugInEntry->getARAFactory ());
    ARA::IPC::ARAIPCProxyHostSetBindingHandler ([] (ARA::IPC::ARAIPCPlugInInstanceRef plugInInstanceRef,
                                                    ARA::ARADocumentControllerRef controllerRef,
                                                    ARA::ARAPlugInInstanceRoleFlags knownRoles, ARA::ARAPlugInInstanceRoleFlags assignedRoles)
                                                        -> const ARA::ARAPlugInExtensionInstance*
                                                {
                                                    // \todo these are the roles that our companion API Loaders implicitly assume - they should be published properly
                                                    ARA_INTERNAL_ASSERT (knownRoles == (ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole) );
                                                    reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->bindToDocumentControllerWithRoles (controllerRef, assignedRoles);
                                                    return reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->getARAPlugInExtensionInstance ();
                                                });

    while (!_shutDown)
        connection.runReceiveLoop (100 /*ms*/);

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
    ARA_INTERNAL_ASSERT (factory->lowestSupportedApiGeneration <= ARA::kARAAPIGeneration_3_0_Draft);
    ARA_INTERNAL_ASSERT (factory->highestSupportedApiGeneration >= ARA::kARAAPIGeneration_2_0_Final);

    _factory = factory;
}

ARA::ARAAPIGeneration PlugInEntry::getDesiredAPIGeneration (const ARA::ARAFactory* const factory)
{
    ARA::ARAAPIGeneration desiredApiGeneration { ARA::kARAAPIGeneration_3_0_Draft };
    if (desiredApiGeneration > factory->highestSupportedApiGeneration)
        desiredApiGeneration = factory->highestSupportedApiGeneration;
    return desiredApiGeneration;
}

void PlugInEntry::initializeARA (ARA::ARAAssertFunction* assertFunctionAddress)
{
    ARA_INTERNAL_ASSERT (_factory);

    // initialize ARA factory with interface configuration
    const ARA::SizedStruct<&ARA::ARAInterfaceConfiguration::assertFunctionAddress> interfaceConfig = { getDesiredAPIGeneration (_factory), assertFunctionAddress };
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
#if defined (__APPLE__)
    if (CFRunLoopGetMain () == CFRunLoopGetCurrent ())
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, true);
    else
#endif
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
            bool needsSpace { false };
            while ((++it != args.end ()) && ((*it)[0] != '-'))
            {
                if (needsSpace)
                    optionalPlugInName += " ";
                optionalPlugInName += *it;
                needsSpace = true;
            }
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
