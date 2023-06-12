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

#include "IPCPort.h"
#include "ARA_Library/Debug/ARADebug.h"

#if defined (NDEBUG)
    constexpr auto messageTimeout { 500 /* milliseconds */};
#else
    // increase to 5 min while debugging so that stopping in the debugger does not break program flow
    constexpr auto messageTimeout { 5 * 60 * 1000 };
#endif

//------------------------------------------------------------------------------
#if defined (_WIN32)
//------------------------------------------------------------------------------

IPCPort::IPCPort (IPCPort&& other) noexcept
{
    *this = std::move (other);
}

IPCPort& IPCPort::operator= (IPCPort&& other) noexcept
{
    std::swap (_handler, other._handler);
    std::swap (_hSendMutex, other._hSendMutex);
    std::swap (_hWriteMutex, other._hWriteMutex);
    std::swap (_hRequest, other._hRequest);
    std::swap (_hResult, other._hResult);
    std::swap (_hMap, other._hMap);
    std::swap (_sharedMemory, other._sharedMemory);
    return *this;
}

IPCPort::~IPCPort ()
{
    if (_sharedMemory)
        ::UnmapViewOfFile (_sharedMemory);
    if (_hMap)
        ::CloseHandle (_hMap);
    if (_hResult)
        ::CloseHandle (_hResult);
    if (_hRequest)
        ::CloseHandle (_hRequest);
    if (_hWriteMutex)
        ::CloseHandle (_hWriteMutex);
    if (_hSendMutex)
        ::CloseHandle (_hSendMutex);
}

IPCPort::IPCPort (const char* remotePortID)
{
    _hWriteMutex = ::CreateMutexA (NULL, FALSE, (std::string { "Write" } + remotePortID).c_str ());
    ARA_INTERNAL_ASSERT (_hWriteMutex != nullptr);
    _hRequest = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Request" } + remotePortID).c_str ());
    ARA_INTERNAL_ASSERT (_hRequest != nullptr);
    _hResult = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Result" } + remotePortID).c_str ());
    ARA_INTERNAL_ASSERT (_hResult != nullptr);
}

IPCPort IPCPort::createPublishingID (const char* remotePortID, Callback callback)
{
    IPCPort port { remotePortID };

    port._handler = callback;

    const auto mapKey { std::string { "Map" } + remotePortID };
    port._hMap = ::CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (SharedMemory), mapKey.c_str ());
    ARA_INTERNAL_ASSERT (port._hMap != nullptr);
    port._sharedMemory = (SharedMemory*) ::MapViewOfFile (port._hMap, FILE_MAP_WRITE, 0, 0, sizeof (SharedMemory));
    ARA_INTERNAL_ASSERT (port._sharedMemory != nullptr);

    return port;
}

IPCPort IPCPort::createConnectedToID (const char* remotePortID)
{
    IPCPort port { remotePortID };

    port._hSendMutex = ::CreateMutexA (NULL, FALSE, (std::string { "Send" } + remotePortID).c_str ());
    ARA_INTERNAL_ASSERT (port._hSendMutex != nullptr);

    const auto mapKey { std::string { "Map" } + remotePortID };
    while (!port._hMap)
    {
        Sleep(100);
        port._hMap = ::OpenFileMappingA(FILE_MAP_WRITE, FALSE, mapKey.c_str());
    }
    port._sharedMemory = (SharedMemory*) ::MapViewOfFile (port._hMap, FILE_MAP_WRITE, 0, 0, 0);
    ARA_INTERNAL_ASSERT (port._sharedMemory != nullptr);

    return port;
}

void IPCPort::_sendMessage (bool blocking, const int32_t messageID, const IPCMessage& message, std::string* result)
{
    ARA_INTERNAL_ASSERT (blocking || result == nullptr);

    const auto data { message.createEncodedMessage () };
    ARA_INTERNAL_ASSERT (data.size () < SharedMemory::maxMessageSize);

    const auto waitSendMutex { ::WaitForSingleObject (_hSendMutex, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitSendMutex == WAIT_OBJECT_0);

    const auto waitWriteMutex { ::WaitForSingleObject (_hWriteMutex, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_OBJECT_0);

    _sharedMemory->messageSize = data.size ();
    _sharedMemory->messageID = messageID;
    std::memcpy (_sharedMemory->messageData, data.c_str (), data.size ());

    ::ResetEvent (_hResult);
    ::SetEvent (_hRequest);
    ::ReleaseMutex (_hWriteMutex);

    if (blocking)
    {
        const auto waitResult { ::WaitForSingleObject (_hResult, messageTimeout) };
        ARA_INTERNAL_ASSERT (waitResult == WAIT_OBJECT_0);
        if (result)
        {
            result->clear ();
            result->insert (0, _sharedMemory->messageData, _sharedMemory->messageSize);
        }
    }

    ::ReleaseMutex (_hSendMutex);
}

void IPCPort::sendNonblocking (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendNonblocking %i", messageID);

    _sendMessage (false, messageID, message, nullptr);
}

void IPCPort::sendBlocking (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendBlocking %i", messageID);

    _sendMessage (true, messageID, message, nullptr);
}

IPCMessage IPCPort::sendAndAwaitReply (const int32_t messageID, const IPCMessage& message)
{
//  ARA_LOG ("IPCPort::sendAndAwaitReply %i", messageID);

    std::string incomingData {};
    _sendMessage (true, messageID, message, &incomingData);
    IPCMessage reply { incomingData.c_str (), incomingData.size () };
    return reply;
}

void IPCPort::runReceiveLoop (int32_t milliseconds)
{
    const auto waitRequest { ::WaitForSingleObject (_hRequest, milliseconds) };
    if (waitRequest == WAIT_TIMEOUT)
        return;
    ARA_INTERNAL_ASSERT (waitRequest == WAIT_OBJECT_0);

    const IPCMessage message { _sharedMemory->messageData, _sharedMemory->messageSize };

    const auto reply { (_handler)(_sharedMemory->messageID, message) };

    const auto replyData = reply.createEncodedMessage ();
    ARA_INTERNAL_ASSERT (replyData.size () < SharedMemory::maxMessageSize);

    const auto waitWriteMutex { ::WaitForSingleObject (_hWriteMutex, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_OBJECT_0);

    _sharedMemory->messageSize = replyData.size ();
    std::memcpy (_sharedMemory->messageData, replyData.c_str (), replyData.size ());

    ::ResetEvent (_hRequest);
    ::SetEvent (_hResult);
    ::ReleaseMutex (_hWriteMutex);
}

//------------------------------------------------------------------------------
#elif defined (__APPLE__)
//------------------------------------------------------------------------------

_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"")

IPCPort::IPCPort (CFMessagePortRef port)
: _port { port }
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
    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_port, messageID, outgoingData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
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
    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_port, messageID, outgoingData, 0.001 * messageTimeout, 0.001 * messageTimeout, kCFRunLoopDefaultMode, &incomingData) };
    os_unfair_lock_unlock (&_sendLock);
    if (outgoingData)
        CFRelease (outgoingData);
    ARA_INTERNAL_ASSERT (incomingData && (portSendResult == kCFMessagePortSuccess));
    return incomingData;
}

void IPCPort::runReceiveLoop (int32_t milliseconds)
{
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, false);
}

_Pragma ("GCC diagnostic pop")

//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------
