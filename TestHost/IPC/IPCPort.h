//------------------------------------------------------------------------------
//! \file       IPCPort.h
//!             communication channel used for IPC in SDK IPC demo example
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

#pragma once


#if defined (_WIN32)
    #include <Windows.h>
    #include <string>
    #include <utility>
#elif defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#else
    #error "IPC not yet implemented for this platform"
#endif

#include <functional>


// A simple proof-of-concept wrapper for an IPC communication channel.
// Error handling is limited to assertions.
class IPCPort
{
public:
    // ID type for messages
    using MessageID = int32_t;
#if defined (_WIN32)
    using DataToSend = std::string;
    using ReceivedData = std::pair <const char*, size_t>;
#elif defined (__APPLE__)
    // the ownership of DataToSend is transferred both when passing it into sendMessage(),
    // or when returning it from a ReceiveCallback.
    using DataToSend = CFDataRef;
    using ReceivedData = CFDataRef;
#endif

    // C++ "rule of five" standard methods - copying is not allowed, only move
    IPCPort (const IPCPort& other) = delete;
    IPCPort& operator= (const IPCPort& other) = delete;
    IPCPort (IPCPort&& other) noexcept;
    IPCPort& operator= (IPCPort&& other) noexcept;
    ~IPCPort ();

    // uninitialized port - cannot be used until move-assigned from factory functions
    IPCPort () = default;

    // factory functions for send and receive ports
#if defined (__APPLE__)
    using ReceiveCallback = std::function</*__attribute__((cf_returns_retained))*/ DataToSend (const MessageID messageID, ReceivedData const messageData)>;
#else
    using ReceiveCallback = std::function<DataToSend (const MessageID messageID, ReceivedData const messageData)>;
#endif
    static IPCPort createPublishingID (const char* remotePortID, const ReceiveCallback& callback);
    static IPCPort createConnectedToID (const char* remotePortID);

    // message sending
    // The messageData will be sent to the receiver, blocking the sending thread until the receiver
    // has processed the message and returned a (potentially empty) reply, which will be forwarded
    // to the optional replyHandler (can be nullptr if reply should be ignored)
    using ReplyHandler = std::function<void (ReceivedData)>;
#if defined (__APPLE__)
    void sendMessage (const MessageID messageID, DataToSend const __attribute__((cf_consumed)) messageData, ReplyHandler* replyHandler);
#else
    void sendMessage (const MessageID messageID, DataToSend const messageData, ReplyHandler* replyHandler);
#endif

    // message receiving
    // waits up to the specified amount of milliseconds for an incoming event and processes it
    void runReceiveLoop (int32_t milliseconds);

    // indicate byte order mismatch between sending and receiving machine
    // \todo currently not implemented, we rely on running on the same machine for now
    //       C++20 offers std::endian which allows for a simple implementation upon connecting...
    bool endianessMatches () { return true; }

private:
#if defined (_WIN32)
    explicit IPCPort (const char* remotePortID);
#elif defined (__APPLE__)
    explicit IPCPort (CFMessagePortRef __attribute__((cf_consumed)) port);
#endif

private:
#if defined (_WIN32)
    struct SharedMemory
    {
        static constexpr DWORD maxMessageSize { 4 * 1024 * 1024L - 64};

        size_t messageSize;
        MessageID messageID;
        char messageData[maxMessageSize];
    };

    const ReceiveCallback* _receiveCallback {};
    HANDLE _hWriteMutex {};
    HANDLE _hRequest {};
    HANDLE _hResult {};
    HANDLE _hMap {};
    SharedMemory* _sharedMemory {};

#elif defined (__APPLE__)
    CFMessagePortRef _port {};
#endif
};
