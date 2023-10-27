//------------------------------------------------------------------------------
//! \file       IPCPort.cpp
//!             communication channel used for IPC in SDK IPC demo example
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
    std::swap (_receiveCallback, other._receiveCallback);
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

IPCPort IPCPort::createPublishingID (const char* remotePortID, const ReceiveCallback& callback)
{
    IPCPort port { remotePortID };
    port._receiveCallback = &callback;

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

void IPCPort::sendMessage (const MessageID messageID, DataToSend const messageData, ReplyHandler* replyHandler)
{
//  ARA_LOG ("IPCPort::sendMessage %i", messageID);

    ARA_INTERNAL_ASSERT (messageData.size () < SharedMemory::maxMessageSize);

    const auto waitWriteMutex { ::WaitForSingleObject (_hWriteMutex, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_OBJECT_0);

    _sharedMemory->messageSize = messageData.size ();
    _sharedMemory->messageID = messageID;
    std::memcpy (_sharedMemory->messageData, messageData.c_str (), messageData.size ());

    ::ResetEvent (_hResult);
    ::SetEvent (_hRequest);
    ::ReleaseMutex (_hWriteMutex);

    const auto waitResult { ::WaitForSingleObject (_hResult, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitResult == WAIT_OBJECT_0);
    if (replyHandler)
        (*replyHandler) ({ _sharedMemory->messageData, _sharedMemory->messageSize });
}

void IPCPort::runReceiveLoop (int32_t milliseconds)
{
    const auto waitRequest { ::WaitForSingleObject (_hRequest, milliseconds) };
    if (waitRequest == WAIT_TIMEOUT)
        return;
    ARA_INTERNAL_ASSERT (waitRequest == WAIT_OBJECT_0);

    const ReceivedData messageData { _sharedMemory->messageData, _sharedMemory->messageSize };

    const auto replyData { (*_receiveCallback)(_sharedMemory->messageID, messageData) };

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

    return (*(const IPCPort::ReceiveCallback*) info) (msgid, cfData);
}

IPCPort IPCPort::createPublishingID (const char* remotePortID, const ReceiveCallback& callback)
{
    auto portID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, remotePortID, kCFStringEncodingASCII, kCFAllocatorNull) };
    CFMessagePortContext portContext { 0, (void*) &callback, nullptr, nullptr, nullptr };
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

    while (timeout > 0.0)
    {
        auto portID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, remotePortID, kCFStringEncodingASCII, kCFAllocatorNull) };
        port = CFMessagePortCreateRemote (kCFAllocatorDefault, portID);
        CFRelease (portID);
        if (port)
            break;

        constexpr auto waitTime { 0.01 };
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, false);
        timeout -= waitTime;
    }
    ARA_INTERNAL_ASSERT (port != nullptr);

    return IPCPort { port };
}

void IPCPort::sendMessage (const MessageID messageID, DataToSend const __attribute__((cf_consumed)) messageData, ReplyHandler* replyHandler)
{
//  ARA_LOG ("IPCPort::sendMessage %i", messageID);

    ReceivedData incomingData {};
    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_port, messageID, messageData, 0.001 * messageTimeout, 0.001 * messageTimeout, kCFRunLoopDefaultMode, &incomingData) };
    ARA_INTERNAL_ASSERT (incomingData && (portSendResult == kCFMessagePortSuccess));
    if (messageData)
        CFRelease (messageData);
    if (replyHandler)
        (*replyHandler) (incomingData);
    CFRelease (incomingData);
}

void IPCPort::runReceiveLoop (int32_t milliseconds)
{
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, false);
}

_Pragma ("GCC diagnostic pop")

//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------
