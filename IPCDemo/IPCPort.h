//------------------------------------------------------------------------------
//! \file       IPCPort.h
//!             messaging used for IPC in SDK IPC demo example
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2021, Celemony Software GmbH, All Rights Reserved.
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

// A simple proof-of-concept wrapper for an IPC communication channel.
// Error handling is limited to assertions, and no endianess, alignment or other encoding issues are handled.
class IPCPort
{
    IPCPort (const IPCPort& other) = delete;
    IPCPort& operator= (const IPCPort& other) = delete;

public:
    IPCPort (IPCPort&& other) noexcept;
    IPCPort& operator= (IPCPort&& other)  noexcept;
    ~IPCPort ();

    typedef IPCMessage (*Callback) (const IPCMessage&);
    static IPCPort createPublishingID (const char* remotePortID, Callback callback);
    static IPCPort createConnectedToID (const char* remotePortID);
    explicit IPCPort (CFMessagePortRef __attribute__((cf_consumed)) port = nullptr);

    void sendWithoutReply (const IPCMessage& message);
    IPCMessage sendAndAwaitReply (const IPCMessage& message);

private:
    CFMessagePortRef _port {};
};
