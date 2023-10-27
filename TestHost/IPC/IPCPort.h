//------------------------------------------------------------------------------
//! \file       IPCPort.h
//!             messaging used for IPC in SDK IPC demo example
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "IPCMessage.h"

#if defined (_WIN32)
    #include <Windows.h>
#elif defined (__APPLE__)
    #include <os/lock.h>
#elif defined (__linux__)
    #error "IPC not yet implemented for Linux"
#endif


// A simple proof-of-concept wrapper for an IPC communication channel.
// Error handling is limited to assertions.
class IPCPort
{
public:
    // ID type for messages
    using MessageID = int32_t;

    // C++ "rule of five" standard methods - copying is not allowed, only move
    IPCPort (const IPCPort& other) = delete;
    IPCPort& operator= (const IPCPort& other) = delete;
    IPCPort (IPCPort&& other) noexcept;
    IPCPort& operator= (IPCPort&& other) noexcept;
    ~IPCPort ();

    // uninitialized port - cannot be used until move-assigned from factory functions
    IPCPort () = default;

    // factory functions for send and receive ports
    using Callback = IPCMessage (*) (const MessageID messageID, const IPCMessage& message);
    static IPCPort createPublishingID (const char* remotePortID, Callback callback);
    static IPCPort createConnectedToID (const char* remotePortID);

    // message sending
    // If no reply is desired, blocking is still necessary in many cases to ensure consistent call order,
    // e.g. if the message potentially triggers any synchronous callbacks from the other side.
    void sendNonblocking (const MessageID messageID, const IPCMessage& message);
    void sendBlocking (const MessageID messageID, const IPCMessage& message);
    IPCMessage sendAndAwaitReply (const MessageID messageID, const IPCMessage& message);

    // message receiving
    // waits up to the specified amount of milliseconds for an incoming event and processes it
    void runReceiveLoop (int32_t milliseconds);

    // indicate byte order mismatch between sending and receiving machine
    // \todo currently not implemented, we rely on running on the same machine for now
    //       C++20 offers std::endian which allows for a simple implementation upon connecting...
    bool endianessMatches () { return true; }

private:
#if defined (_WIN32)
    IPCPort (const char* remotePortID);
    void _sendMessage (bool blocking, const MessageID messageID, const IPCMessage& message, std::string* result);
#elif defined (__APPLE__)
    explicit IPCPort (CFMessagePortRef __attribute__((cf_consumed)) port);
    __attribute__((cf_returns_retained)) CFDataRef _sendBlocking (const MessageID messageID, const IPCMessage& message);
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

    Callback _handler {};
    HANDLE _hSendMutex {};
    HANDLE _hWriteMutex {};
    HANDLE _hRequest {};
    HANDLE _hResult {};
    HANDLE _hMap {};
    SharedMemory* _sharedMemory {};

#elif defined (__APPLE__)
    CFMessagePortRef _port {};
    os_unfair_lock_s _sendLock { OS_UNFAIR_LOCK_INIT };
#endif
};
