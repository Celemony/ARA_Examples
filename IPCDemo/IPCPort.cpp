//------------------------------------------------------------------------------
//! \file       IPCMessage.cpp
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

#include "IPCPort.h"
#include "ARA_Library/Debug/ARADebug.h"

#if defined(NDEBUG)
    constexpr auto messageTimeout { 0.5 };
#else
    // increase to 5 min while debugging so that stopping in the debugger does not break program flow
    constexpr auto messageTimeout { 5 * 60.0 };
#endif

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

CFDataRef IPCPortCallBack (CFMessagePortRef /*port*/, SInt32 msgid, CFDataRef cfData, void* info)
{
//  ARA_LOG ("IPCPortCallBack %i", msgid);

    const IPCMessage message { cfData };
    return ((IPCPort::Callback) info) (msgid, message).createEncodedMessage ();
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

void IPCPort::sendNonblocking (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendNonblocking %i", messageID);

    auto outgoingData { message.createEncodedMessage () };
    os_unfair_lock_lock (&_sendLock);
    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_port, messageID, outgoingData, messageTimeout, 0.0, nullptr, nullptr) };
    os_unfair_lock_unlock (&_sendLock);
    if (outgoingData)
        CFRelease (outgoingData);
    ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);
}

void IPCPort::sendBlocking (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendBlocking %i", messageID);

    auto incomingData { _sendBlocking (messageID, message) };
    CFRelease (incomingData);
}

IPCMessage IPCPort::sendAndAwaitReply (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendAndAwaitReply %i", messageID);

    auto incomingData { _sendBlocking (messageID, message) };
    IPCMessage reply { incomingData };
    CFRelease (incomingData);
    return reply;
}

CFDataRef IPCPort::_sendBlocking (const int32_t messageID, const IPCMessage& message)
{
    CFDataRef incomingData {};
    auto outgoingData { message.createEncodedMessage () };
    os_unfair_lock_lock (&_sendLock);
    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_port, messageID, outgoingData, messageTimeout, messageTimeout, kCFRunLoopDefaultMode, &incomingData) };
    os_unfair_lock_unlock (&_sendLock);
    if (outgoingData)
        CFRelease (outgoingData);
    ARA_INTERNAL_ASSERT (incomingData && (portSendResult == kCFMessagePortSuccess));
    return incomingData;
}
