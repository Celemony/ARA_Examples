//------------------------------------------------------------------------------
//! \file       IPCMessageChannel.cpp
//!             Proof-of-concept implementation of ARAIPCMessageChannel
//!             for the ARA SDK TestHost (error handling is limited to assertions).
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2023, Celemony Software GmbH, All Rights Reserved.
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

#include "IPCMessageChannel.h"
#include "ARA_Library/Debug/ARADebug.h"

#if USE_ARA_CF_ENCODING
    #include "ARA_Library/IPC/ARAIPCCFEncoding.h"
#else
    #include "IPC/IPCXMLEncoding.h"
#endif

#if defined (__APPLE__)
    #include <sys/posix_shm.h>
    #include <sys/stat.h>
#endif


#if defined (NDEBUG)
    constexpr auto messageTimeout { 500 /* milliseconds */};
#else
    // increase to 5 min while debugging so that stopping in the debugger does not break program flow
    constexpr auto messageTimeout { 5 * 60 * 1000 };
#endif


//------------------------------------------------------------------------------
#if defined (_WIN32)
//------------------------------------------------------------------------------


class IPCMessagePort
{
protected:
    IPCMessagePort (const std::string& channelID)
    : _dataAvailable { ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Available" } + channelID).c_str ()) },
      _dataReceived { ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Received" } + channelID).c_str ()) }
    {
        ARA_INTERNAL_ASSERT (_dataAvailable != nullptr);
        ARA_INTERNAL_ASSERT (_dataReceived != nullptr);
    }

public:
    ~IPCMessagePort ()
    {
        ::UnmapViewOfFile (_sharedMemory);
        ::CloseHandle (_fileMapping);
        ::CloseHandle (_dataReceived);
        ::CloseHandle (_dataAvailable);
    }

protected:
    struct SharedMemory
    {
        static constexpr DWORD maxMessageSize { 4 * 1024 * 1024L - 64};

        size_t messageSize;
        ARA::IPC::ARAIPCMessageID messageID;
        char messageData[maxMessageSize];
    };

    HANDLE _dataAvailable {};           // signal set by the sending side indicating new data has been placed in shared memory
    HANDLE _dataReceived {};            // signal set by the receiving side when evaluating the shared memory
    HANDLE _fileMapping {};
    SharedMemory* _sharedMemory {};

};

class IPCReceivePort : public IPCMessagePort
{
public:
    IPCReceivePort (const std::string& channelID, IPCMessageChannel* channel)
    : IPCMessagePort { channelID },
      _channel { channel }
    {
        const auto mapKey { std::string { "Map" } + channelID };
        _fileMapping = ::CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (SharedMemory), mapKey.c_str ());
        ARA_INTERNAL_ASSERT (_fileMapping != nullptr);
        _sharedMemory = (SharedMemory*) ::MapViewOfFile (_fileMapping, FILE_MAP_WRITE, 0, 0, sizeof (SharedMemory));
        ARA_INTERNAL_ASSERT (_sharedMemory != nullptr);
    }

    bool runReceiveLoop (int32_t milliseconds)
    {
        const auto waitRequest { ::WaitForSingleObject (_dataAvailable, milliseconds) };
        if (waitRequest == WAIT_TIMEOUT)
            return false;
        ARA_INTERNAL_ASSERT (waitRequest == WAIT_OBJECT_0);

        const auto messageID { _sharedMemory->messageID };
        const auto decoder { IPCXMLMessageDecoder::createWithMessageData (_sharedMemory->messageData, _sharedMemory->messageSize) };

        ::SetEvent (_dataReceived);

        _channel->routeReceivedMessage (messageID, decoder);
        return true;
    }

private:
    IPCMessageChannel* const _channel;
};

class IPCSendPort : public IPCMessagePort
{
public:
    IPCSendPort (const std::string& channelID)
    : IPCMessagePort { channelID }
    {
        const auto mapKey { std::string { "Map" } + channelID };
        while (!_fileMapping)
        {
            ::Sleep (100);
            _fileMapping = ::OpenFileMappingA (FILE_MAP_WRITE, FALSE, mapKey.c_str ());
        }
        _sharedMemory = (SharedMemory*) ::MapViewOfFile (_fileMapping, FILE_MAP_WRITE, 0, 0, 0);
        ARA_INTERNAL_ASSERT (_sharedMemory != nullptr);
    }

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, const std::string& messageData)
    {
        ARA_INTERNAL_ASSERT (messageData.size () <= SharedMemory::maxMessageSize);

        _sharedMemory->messageID = messageID;
        _sharedMemory->messageSize = messageData.size ();
        std::memcpy (_sharedMemory->messageData, messageData.c_str (), messageData.size ());

        ::SetEvent (_dataAvailable);
        const auto waitResult { ::WaitForSingleObject (_dataReceived, messageTimeout) };
        ARA_INTERNAL_ASSERT (waitResult == WAIT_OBJECT_0);
    }
};

//------------------------------------------------------------------------------
#elif defined (__APPLE__)
//------------------------------------------------------------------------------


class IPCReceivePort
{
public:
    IPCReceivePort (const std::string& portID, IPCMessageChannel* channel)
    {
        auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

        CFMessagePortContext portContext { 0, channel, nullptr, nullptr, nullptr };
        _port = CFMessagePortCreateLocal (kCFAllocatorDefault, wrappedPortID, _portCallback, &portContext, nullptr);
        ARA_INTERNAL_ASSERT (_port != nullptr);

        CFRelease (wrappedPortID);

        CFRunLoopSourceRef runLoopSource { CFMessagePortCreateRunLoopSource (kCFAllocatorDefault, _port, 0) };
        CFRunLoopAddSource (CFRunLoopGetCurrent (), runLoopSource, kCFRunLoopDefaultMode);
        CFRelease (runLoopSource);
    }

    ~IPCReceivePort ()
    {
        CFMessagePortInvalidate (_port);
        CFRelease (_port);
    }

    bool runReceiveLoop (int32_t milliseconds)
    {
        return (CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, true) != kCFRunLoopRunTimedOut);
    }

private:
    static CFDataRef _portCallback (CFMessagePortRef /*port*/, SInt32 messageID, CFDataRef messageData, void* info)
    {
        auto channel { static_cast<IPCMessageChannel*> (info) };
#if USE_ARA_CF_ENCODING
        const auto decoder { ARA::IPC::ARAIPCCFCreateMessageDecoder (messageData) };
#else
        const auto decoder { IPCXMLMessageDecoder::createWithMessageData (messageData) };
#endif
        channel->routeReceivedMessage (messageID, decoder);
        return nullptr;
    }

private:
    CFMessagePortRef _port {};
};

class IPCSendPort
{
public:
    IPCSendPort (const std::string& portID)
    {
        auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

        auto timeout { 0.001 * messageTimeout };
        while (timeout > 0.0)
        {
            if ((_port = CFMessagePortCreateRemote (kCFAllocatorDefault, wrappedPortID)))
                break;

            constexpr auto waitTime { 0.01 };
            CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, true);
            timeout -= waitTime;
        }
        ARA_INTERNAL_ASSERT (_port != nullptr);

        CFRelease (wrappedPortID);
    }

    ~IPCSendPort ()
    {
        CFMessagePortInvalidate (_port);
        CFRelease (_port);
    }

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, CFDataRef messageData)
    {
        const auto ARA_MAYBE_UNUSED_VAR (result) { CFMessagePortSendRequest (_port, messageID, messageData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
        ARA_INTERNAL_ASSERT (result == kCFMessagePortSuccess);
    }

private:
    CFMessagePortRef _port {};
};


//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------


IPCMessageChannel* IPCMessageChannel::createPublishingID (const std::string& channelID, ARA::IPC::ARAIPCMessageHandler* handler)
{
    auto channel { new IPCMessageChannel { handler } };
    channel->_sendPort = new IPCSendPort { channelID + ".from_server" };
    channel->_receivePort = new IPCReceivePort { channelID + ".to_server", channel };
    return channel;
}

IPCMessageChannel* IPCMessageChannel::createConnectedToID (const std::string& channelID, ARA::IPC::ARAIPCMessageHandler* handler)
{
    auto channel { new IPCMessageChannel { handler } };
    channel->_receivePort = new IPCReceivePort { channelID + ".from_server", channel };
    channel->_sendPort = new IPCSendPort { channelID + ".to_server" };
    return channel;
}

IPCMessageChannel::~IPCMessageChannel ()
{
    delete _sendPort;
    delete _receivePort;
}

void IPCMessageChannel::_sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder)
{
#if USE_ARA_CF_ENCODING
    const auto messageData { ARAIPCCFCreateMessageEncoderData (encoder) };
#else
    const auto messageData { static_cast<const IPCXMLMessageEncoder*> (encoder)->createEncodedMessage () };
#endif

    _sendPort->sendMessage (messageID, messageData);

#if defined (__APPLE__)
    if (messageData)
        CFRelease (messageData);
#endif
}

bool IPCMessageChannel::runReceiveLoop (int32_t milliseconds)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _receiveThread);
    return _receivePort->runReceiveLoop (milliseconds);
}

bool IPCMessageChannel::runsReceiveLoopOnCurrentThread ()
{
    return (std::this_thread::get_id () == _receiveThread);
}

void IPCMessageChannel::loopUntilMessageReceived ()
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _receiveThread);
    while (!runReceiveLoop (messageTimeout))
    {}
}

ARA::IPC::ARAIPCMessageEncoder* IPCMessageChannel::createEncoder ()
{
#if USE_ARA_CF_ENCODING
    return ARA::IPC::ARAIPCCFCreateMessageEncoder ();
#else
    return new IPCXMLMessageEncoder {};
#endif
}
