//------------------------------------------------------------------------------
//! \file       IPCMessage.cpp
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

#include "IPCPort.h"
#include "ARA_Library/Debug/ARADebug.h"

// increase to several seconds while debugging so that staying in the debugger does not break program flow
static constexpr auto messageTimeout { 0.1 };

IPCPort::IPCPort (CFMessagePortRef port) :
    _port { port }
{}

IPCPort::IPCPort (IPCPort&& other) noexcept
{
    *this = std::move (other);
}

IPCPort& IPCPort::operator= (IPCPort&& other) noexcept
{
    std::swap (_port, other._port);
    return *this;
}

IPCPort::~IPCPort ()
{
    if (_port)
    {
        CFMessagePortInvalidate (_port);
        CFRelease (_port);
    }
}

CFDataRef IPCPortCallBack (CFMessagePortRef /*port*/, SInt32 /*msgid*/, CFDataRef cfData, void* info)
{
    const IPCMessage message { cfData };
    return ((IPCPort::Callback) info) (message).createEncodedMessage ();
}

IPCPort IPCPort::createPublishingID (const char* remotePortID, Callback callback)
{
    auto portID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, remotePortID, kCFStringEncodingASCII,  kCFAllocatorNull) };
    CFMessagePortContext portContext { 0, (void*) callback, nullptr, nullptr, nullptr };
    auto port { CFMessagePortCreateLocal (kCFAllocatorDefault, portID, &IPCPortCallBack, &portContext, nullptr) };
    CFRelease (portID);
    CFRunLoopSourceRef runLoopSource { CFMessagePortCreateRunLoopSource (kCFAllocatorDefault, port, 0) };
    CFRunLoopAddSource (CFRunLoopGetCurrent (), runLoopSource, kCFRunLoopDefaultMode);
    CFRelease (runLoopSource);
    ARA_INTERNAL_ASSERT (port != nullptr);
    return IPCPort { port };
}

IPCPort IPCPort::createConnectedToID (const char* remotePortID)
{
    auto timeout { 5.0 };
    CFMessagePortRef port {};

// for some reason, the clang analyzer claims a potential leak of port here, even though it's either null or consumed...
#if !defined (__clang_analyzer__)
    while (timeout > 0.0)
    {
        auto portID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, remotePortID, kCFStringEncodingASCII,  kCFAllocatorNull) };
        if ((port = CFMessagePortCreateRemote (kCFAllocatorDefault, portID)))
            break;
        CFRelease (portID);

        constexpr auto waitTime { 0.01 };
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, false);
        timeout -= waitTime;
    }
    ARA_INTERNAL_ASSERT (port != nullptr);
#endif

    return IPCPort { port };
}

void IPCPort::sendWithoutReply (const IPCMessage& message)
{
    auto outgoingData { message.createEncodedMessage () };
    const auto portSendResult { CFMessagePortSendRequest (_port, 0, outgoingData, messageTimeout, 0.0, nullptr, nullptr) };
    CFRelease (outgoingData);
    ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);

}

IPCMessage IPCPort::sendAndAwaitReply (const IPCMessage& message)
{
    auto outgoingData { message.createEncodedMessage () };
    auto incomingData { CFDataRef {} };
    const auto portSendResult { CFMessagePortSendRequest (_port, 0, outgoingData, messageTimeout, messageTimeout, kCFRunLoopDefaultMode, &incomingData) };
    CFRelease (outgoingData);
    ARA_INTERNAL_ASSERT (incomingData && (portSendResult == kCFMessagePortSuccess));
    IPCMessage reply { incomingData };
    CFRelease (incomingData);
    return reply;
}
