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
// This is a brief proof-of-concept demo that hooks up an ARA capable plug-in
// in a separate process using IPC.
// This educational example is not suitable for production code -
// see MainProcess.cpp for a list of issues.
//------------------------------------------------------------------------------

#pragma once

#include "IPCMessage.h"

#include <os/lock.h>

// A simple proof-of-concept wrapper for an IPC communication channel.
// Error handling is limited to assertions.
class IPCPort
{
public:
    // C++ "rule of five" standard methods - copying is not allowed, only move
    IPCPort (const IPCPort& other) = delete;
    IPCPort& operator= (const IPCPort& other) = delete;
    IPCPort (IPCPort&& other) noexcept;
    IPCPort& operator= (IPCPort&& other) noexcept;
    ~IPCPort ();

    // uninitialized port - cannot be used until move-assigned from factory functions
    IPCPort () = default;

    // factory functions for send and receive ports
    using Callback = IPCMessage (*) (const int32_t messageID, const IPCMessage&);
    static IPCPort createPublishingID (const char* remotePortID, Callback callback);
    static IPCPort createConnectedToID (const char* remotePortID);

    // message sending
    // If no reply is desired, blocking is still necessary in many cases to ensure consistent call order,
    // e.g. if the message potentially triggers any synchronous callbacks from the other side.
    void sendNonblocking (const int32_t messageID, const IPCMessage& message);
    void sendBlocking (const int32_t messageID, const IPCMessage& message);
    IPCMessage sendAndAwaitReply (const int32_t messageID, const IPCMessage& message);

private:
    explicit IPCPort (CFMessagePortRef __attribute__((cf_consumed)) port);
    __attribute__((cf_returns_retained)) CFDataRef _sendBlocking (const int32_t messageID, const IPCMessage& message);

private:
    CFMessagePortRef _port {};
    os_unfair_lock_s _sendLock { OS_UNFAIR_LOCK_INIT };
};
