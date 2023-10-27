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
#include <thread>
#include <vector>
#include <cstring>

/*******************************************************************************/

#if ARA_ENABLE_IPC

#include "ARA_Library/IPC/ARAIPCProxyHost.h"
#include "ARA_Library/IPC/ARAIPCProxyPlugIn.h"
#include "ARA_Library/IPC/ARAIPCEncoding.h"
#include "IPC/IPCPort.h"

#if defined (__linux__)
    #error "IPC not yet implemented for Linux"
#endif

// minimal set of commands to run a companion API plug-in through IPC
enum
{
    kIPCCreateARAEffect = -1,
    kIPCStartRendering = -2,
    kIPCRenderSamples = -3,
    kIPCStopRendering = -4,
    kIPCDestroyEffect = -5,
    kIPCTerminate = -6
};
static_assert (kIPCCreateARAEffect < ARA::IPC::kARAIPCMessageIDRangeStart, "conflicting message IDs");

// check message ID is either one of our commands or in the range of the generic IPC implementation
bool isValidMessageID (const ARA::IPC::ARAIPCMessageID messageID)
{
    if (messageID <= kIPCCreateARAEffect)
        return kIPCTerminate <= messageID;

    if  (messageID < ARA::IPC::kARAIPCMessageIDRangeStart)
        return false;
    return messageID < ARA::IPC::kARAIPCMessageIDRangeEnd;
}

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

// converting IPCMessage to MessageEn-/Decoder
ARA::IPC::ARAIPCMessageEncoder makeMessageEncoder (IPCMessage& message)
{
    static const ARA::IPC::ARAIPCMessageEncoderInterface encoderMethods
    {
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef) -> void
            { delete reinterpret_cast<IPCMessage*> (messageEncoderRef); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, int32_t argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendInt32 (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, int64_t argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendInt64 (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, size_t argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendSize (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, float argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendFloat (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, double argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendDouble (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, const char* argValue) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendString (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey, const uint8_t* argValue, size_t argSize, bool copy) -> void
            { reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendBytes (argKey, argValue, argSize, copy); },
        [] (ARA::IPC::ARAIPCMessageEncoderRef messageEncoderRef, ARA::IPC::ARAIPCMessageKey argKey) -> ARA::IPC::ARAIPCMessageEncoderRef
            { return reinterpret_cast<ARA::IPC::ARAIPCMessageEncoderRef> (reinterpret_cast<IPCMessage*> (messageEncoderRef)->appendSubMessage (argKey)); }
    };

    return { reinterpret_cast<ARA::IPC::ARAIPCMessageEncoderRef> (&message), &encoderMethods };
}

ARA::IPC::ARAIPCMessageDecoder makeMessageDecoder (const IPCMessage& message)
{
    static const ARA::IPC::ARAIPCMessageDecoderInterface decoderMethods
    {
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef) -> void
            { delete reinterpret_cast<const IPCMessage*> (messageDecoderRef); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->isEmpty (); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, int32_t* argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readInt32 (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, int64_t* argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readInt64 (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, size_t* argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readSize (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, float* argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readFloat (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, double* argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readDouble (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, const char** argValue) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readString (argKey, *argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, size_t* argSize) -> bool
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readBytesSize (argKey, *argSize); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey, uint8_t* argValue) -> void
            { return reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readBytes (argKey, argValue); },
        [] (ARA::IPC::ARAIPCMessageDecoderRef messageDecoderRef, ARA::IPC::ARAIPCMessageKey argKey) -> ARA::IPC::ARAIPCMessageDecoderRef
            { return reinterpret_cast<ARA::IPC::ARAIPCMessageDecoderRef> (reinterpret_cast<const IPCMessage*> (messageDecoderRef)->readSubMessage (argKey)); }
    };

    return { reinterpret_cast<const ARA::IPC::ARAIPCMessageDecoderRef> (const_cast<IPCMessage *> (&message)), &decoderMethods };
}

// implementation of the abstract ARAIPCMessageSender interface
class IPCSender
{
public:
    IPCSender (IPCPort& port)
    : _port { port },
      _sender { reinterpret_cast<ARA::IPC::ARAIPCMessageSenderRef> (this), getSenderMethods () }
    {}

    ARA::IPC::ARAIPCMessageEncoder createEncoder ()
    {
        return makeMessageEncoder (*new IPCMessage);
    }

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageEncoder& encoder,
                      ARA::IPC::ARAIPCReplyHandler* const replyHandler, void* replyHandlerUserData)
    {
        ARA_INTERNAL_ASSERT (isValidMessageID (messageID));

        auto reply { _port.sendAndAwaitReply(messageID, *reinterpret_cast<const IPCMessage*> (encoder.ref)) };
        if (replyHandler)
        {
            auto replyDecoder { makeMessageDecoder (reply) };
            (*replyHandler) (replyDecoder, replyHandlerUserData);
        }
        else
        {
            ARA_INTERNAL_ASSERT (reply.isEmpty ());
        }
    }

    bool receiverEndianessMatches ()
    {
        return _port.endianessMatches ();
    }

    operator ARA::IPC::ARAIPCMessageSender ()
    {
        return _sender;
    }

private:
    static const ARA::IPC::ARAIPCMessageSenderInterface* getSenderMethods ()
    {
        static const ARA::IPC::ARAIPCMessageSenderInterface senderMethods =
        {
            [] (ARA::IPC::ARAIPCMessageSenderRef messageSenderRef) -> ARA::IPC::ARAIPCMessageEncoder
                { return reinterpret_cast<IPCSender*> (messageSenderRef)->createEncoder (); },
            [] (ARA::IPC::ARAIPCMessageSenderRef messageSenderRef, ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageEncoder* encoder,
                ARA::IPC::ARAIPCReplyHandler* const replyHandler, void* replyHandlerUserData)
                { return reinterpret_cast<IPCSender*> (messageSenderRef)->sendMessage (messageID, *encoder, replyHandler, replyHandlerUserData); },
            [] (ARA::IPC::ARAIPCMessageSenderRef messageSenderRef) -> bool
                { return reinterpret_cast<IPCSender*> (messageSenderRef)->receiverEndianessMatches (); },
        };
        return &senderMethods;
    }

private:
    IPCPort& _port;
    ARA::IPC::ARAIPCMessageSender _sender;
};

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
    IPCPlugInInstance (size_t remoteRef, IPCPort& port, const ARA::ARAPlugInExtensionInstance* instance)
    : PlugInInstance { instance },
      _remoteRef { remoteRef },
      _sender { port }
    {}

    ~IPCPlugInInstance () override
    {
        ARA::IPC::RemoteCaller { _sender }.remoteCallWithoutReply (kIPCDestroyEffect, _remoteRef, reinterpret_cast<size_t> (getARAPlugInExtensionInstance ()->plugInExtensionRef));
        ARA::IPC::ARAIPCProxyPlugInDestroyPlugInExtension (getARAPlugInExtensionInstance ());
    }

    void startRendering (int maxBlockSize, double sampleRate) override
    {
        ARA::IPC::RemoteCaller { _sender }.remoteCallWithoutReply (kIPCStartRendering, _remoteRef, maxBlockSize, sampleRate);
    }

    void renderSamples (int blockSize, int64_t samplePosition, float* buffer) override
    {
        const auto byteSize { static_cast<size_t> (blockSize) * sizeof (float) };
        auto resultSize { byteSize };
        ARA::IPC::BytesDecoder reply { reinterpret_cast<uint8_t*> (buffer), resultSize };
        ARA::IPC::RemoteCaller { _sender }.remoteCallWithReply (reply, kIPCRenderSamples, _remoteRef, samplePosition, ARA::IPC::BytesEncoder { reinterpret_cast<const uint8_t*> (buffer), byteSize, false });
        ARA_INTERNAL_ASSERT (resultSize == byteSize);
    }

    void stopRendering () override
    {
        ARA::IPC::RemoteCaller { _sender }.remoteCallWithoutReply (kIPCStopRendering, _remoteRef);
    }

private:
    size_t const _remoteRef;
    IPCSender _sender;
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

    static const ARA::ARAFactory* defaultGetFactory(IPCSender& hostCommandsSender)
    {
        const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (hostCommandsSender) };
        ARA_INTERNAL_ASSERT (count > 0);
        return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (hostCommandsSender, 0U);
    }

public:
    IPCPlugInEntry (const std::string& launchArgs, ARA::ARAAssertFunction* assertFunctionAddress,
                    const std::function<const ARA::ARAFactory* (IPCSender& hostCommandsSender)>& getFactoryFunction = defaultGetFactory)
    : _hostCommandsPortID { _createPortID () },
      _plugInCallbacksPortID { _createPortID () },
      _remoteLauncher { launchArgs, _hostCommandsPortID, _plugInCallbacksPortID },
      _plugInCallbacksThread { &IPCPlugInEntry::_plugInCallbacksThreadFunction, this },
      _hostCommandsPort { IPCPort::createConnectedToID (_hostCommandsPortID.c_str ()) },
      _hostCommandsSender { _hostCommandsPort },
      _factory { getFactoryFunction (_hostCommandsSender) }
    {
        setUsesIPC ();
        initializeARA (_factory, assertFunctionAddress);
    }

    ~IPCPlugInEntry () override
    {
        ARA::IPC::RemoteCaller { _hostCommandsSender }.remoteCallWithoutReply (kIPCTerminate);

        _terminateCallbacksThread = true;
        _plugInCallbacksThread.join ();
    }

    const ARA::ARADocumentControllerInstance* createDocumentControllerWithDocument (const ARA::ARADocumentControllerHostInstance* hostInstance,
                                                                                    const ARA::ARADocumentProperties* properties) override
    {
        return ARA::IPC::ARAIPCProxyPlugInCreateDocumentControllerWithDocument (_hostCommandsSender, _factory->factoryID, hostInstance, properties);
    }

    std::unique_ptr<PlugInInstance> createARAPlugInInstanceWithRoles (ARA::ARADocumentControllerRef documentControllerRef, ARA::ARAPlugInInstanceRoleFlags assignedRoles) override
    {
        // \todo these are the roles that our Companion API Loaders implicitly assume - they should be published properly
        const ARA::ARAPlugInInstanceRoleFlags knownRoles { ARA::kARAPlaybackRendererRole | ARA::kARAEditorRendererRole | ARA::kARAEditorViewRole };

        const auto remoteDocumentControllerRef { ARA::IPC::ARAIPCProxyPlugInTranslateDocumentControllerRef (documentControllerRef) };
        size_t remoteInstanceRef {};
        size_t remoteExtensionRef {};
        ARA::IPC::RemoteCaller::CustomDecodeFunction customDecode { [&remoteInstanceRef, &remoteExtensionRef] (const ARA::IPC::ARAIPCMessageDecoder& decoder) -> void
            {
                decoder.methods->readSize (decoder.ref, 0, &remoteInstanceRef);
                decoder.methods->readSize (decoder.ref, 1, &remoteExtensionRef);
            } };
        ARA::IPC::RemoteCaller { _hostCommandsSender }.remoteCallWithReply (customDecode, kIPCCreateARAEffect, remoteDocumentControllerRef, assignedRoles);
        auto plugInExtension { ARA::IPC::ARAIPCProxyPlugInCreatePlugInExtension (remoteExtensionRef, _hostCommandsSender, documentControllerRef, knownRoles, assignedRoles) };
        validatePlugInExtensionInstance (plugInExtension, assignedRoles);
        return std::make_unique<IPCPlugInInstance> (remoteInstanceRef, _hostCommandsPort, plugInExtension);
    }

private:
    void _plugInCallbacksThreadFunction ()
    {
        // \todo It would be cleaner to create the port in the c'tor from the main thread,
        //       but for some reason reading audio is then much slower compared to creating it here...?
        _plugInCallbacksPort = IPCPort::createPublishingID (_plugInCallbacksPortID.c_str (),
                                    [] (const ARA::IPC::ARAIPCMessageID messageID, const IPCMessage& message) -> IPCMessage
                                        {
                                            ARA_INTERNAL_ASSERT (isValidMessageID (messageID));
                                            auto messageDecoder { makeMessageDecoder (message) };
                                            IPCMessage reply;
                                            auto replyEncoder { makeMessageEncoder (reply) };
                                            ARA::IPC::ARAIPCProxyPlugInCallbacksDispatcher (messageID, &messageDecoder, &replyEncoder);
                                            return reply;
                                        });

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
    IPCSender _hostCommandsSender;

    const ARA::ARAFactory* _factory;
};

/*******************************************************************************/

class IPCVST3PlugInEntry : public IPCPlugInEntry
{
public:
    IPCVST3PlugInEntry (const std::string& binaryName, const std::string& optionalPlugInName, ARA::ARAAssertFunction* assertFunctionAddress)
    : IPCPlugInEntry { std::string { "-vst3 " } + binaryName + " " + optionalPlugInName, assertFunctionAddress,
                        [&optionalPlugInName] (IPCSender& hostCommandsSender) -> const ARA::ARAFactory*
                        {
                            const auto count { ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (hostCommandsSender) };
                            ARA_INTERNAL_ASSERT (count > 0);

                            if (optionalPlugInName.empty ())
                                return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (hostCommandsSender, 0U);

                            for (auto i { 0U }; i < count; ++i)
                            {
                                auto factory { ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (hostCommandsSender, i) };
                                if (0 == std::strcmp (factory->plugInName, optionalPlugInName.c_str ()))
                                    return factory;
                            }
                            ARA_INTERNAL_ASSERT (false);
                            return ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (hostCommandsSender, 0U);
                        } }
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
    ARA_INTERNAL_ASSERT (isValidMessageID (messageID));
    auto messageDecoder { makeMessageDecoder (message) };
    if (messageID == kIPCCreateARAEffect)
    {
        ARA::ARADocumentControllerRef documentControllerRemoteRef;
        ARA::ARAPlugInInstanceRoleFlags assignedRoles;
        ARA::IPC::decodeArguments (&messageDecoder, documentControllerRemoteRef, assignedRoles);

        auto plugInInstance { _plugInEntry->createARAPlugInInstanceWithRoles (ARA::IPC::ARAIPCProxyHostTranslateDocumentControllerRef (documentControllerRemoteRef), assignedRoles) };
        auto plugInExtensionRef { ARA::IPC::ARAIPCProxyHostCreatePlugInExtension (plugInInstance->getARAPlugInExtensionInstance ()) };

        IPCMessage reply;
        auto replyEncoder { makeMessageEncoder (reply) };
        ARA::IPC::encodeArguments (replyEncoder, reinterpret_cast<size_t> (plugInInstance.get ()), plugInExtensionRef);
        plugInInstance.release ();  // ownership is transferred to host - keep around until kIPCDestroyEffect
        return reply;
    }
    else if (messageID == kIPCStartRendering)
    {
        size_t plugInInstanceRef;
        int32_t maxBlockSize;
        double sampleRate;
        ARA::IPC::decodeArguments (&messageDecoder, plugInInstanceRef, maxBlockSize, sampleRate);

        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->startRendering (maxBlockSize, sampleRate);
        return {};
    }
    else if (messageID == kIPCRenderSamples)
    {
        size_t plugInInstanceRef;
        int64_t samplePosition;
        // \todo using static (plus not copy bytes) here assumes single-threaded callbacks, but currently this is a valid requirement
        static std::vector<uint8_t> buffer;
        ARA::IPC::BytesDecoder writer { buffer };
        ARA::IPC::decodeArguments (&messageDecoder, plugInInstanceRef, samplePosition, writer);
        ARA_INTERNAL_ASSERT (buffer.size () > 0);

        // \todo this ignores potential float data alignment or byte order issues...
        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->renderSamples (static_cast<int> (buffer.size () / sizeof(float)),
                                                                        samplePosition, reinterpret_cast<float*> (buffer.data ()));
        IPCMessage reply;
        auto replyEncoder { makeMessageEncoder (reply) };
        ARA::IPC::encodeReply (&replyEncoder, ARA::IPC::BytesEncoder { buffer, false });
        return reply;
    }
    else if (messageID == kIPCStopRendering)
    {
        size_t plugInInstanceRef;
        ARA::IPC::decodeArguments (&messageDecoder, plugInInstanceRef);

        reinterpret_cast<PlugInInstance*> (plugInInstanceRef)->stopRendering ();
        return {};
    }
    else if (messageID == kIPCDestroyEffect)
    {
        size_t plugInInstanceRef;
        ARA::ARAPlugInExtensionRef plugInExtensionRef;
        ARA::IPC::decodeArguments (&messageDecoder, plugInInstanceRef, plugInExtensionRef);

        ARA::IPC::ARAIPCProxyHostDestroyPlugInExtension (plugInExtensionRef);
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
        IPCMessage reply;
        auto replyEncoder { makeMessageEncoder (reply) };
        ARA::IPC::ARAIPCProxyHostCommandHandler (messageID, &messageDecoder, &replyEncoder);
        return reply;
    }
}

int RemoteHost::main (std::unique_ptr<PlugInEntry> plugInEntry, const std::string& hostCommandsPortID, const std::string& plugInCallbacksPortID)
{
    _plugInEntry = std::move (plugInEntry);

    auto hostCommandsPort { IPCPort::createPublishingID (hostCommandsPortID.c_str (), &_hostCommandHandler) };
    auto plugInCallbacksPort { IPCPort::createConnectedToID (plugInCallbacksPortID.c_str ()) };
    IPCSender plugInCallbacksSender { plugInCallbacksPort };

    ARA::IPC::ARAIPCProxyHostAddFactory (_plugInEntry->getARAFactory ());
    ARA::IPC::ARAIPCProxyHostSetPlugInCallbacksSender (plugInCallbacksSender);

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
