//------------------------------------------------------------------------------
//! \file       IPCPort.cpp
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

#include "IPCPort.h"
#include "ARA_Library/Debug/ARADebug.h"

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

IPCPort::IPCPort (const std::string& portID)
{
    _hWriteMutex = ::CreateMutexA (NULL, FALSE, (std::string { "Write" } + portID).c_str ());
    ARA_INTERNAL_ASSERT (_hWriteMutex != nullptr);
    _hRequest = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Request" } + portID).c_str ());
    ARA_INTERNAL_ASSERT (_hRequest != nullptr);
    _hResult = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Result" } + portID).c_str ());
    ARA_INTERNAL_ASSERT (_hResult != nullptr);
}

IPCPort IPCPort::createPublishingID (const std::string& portID, const ReceiveCallback& callback)
{
    IPCPort port { portID };
    port._receiveCallback = std::move (callback);

    const auto mapKey { std::string { "Map" } + portID };
    port._hMap = ::CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (SharedMemory), mapKey.c_str ());
    ARA_INTERNAL_ASSERT (port._hMap != nullptr);
    port._sharedMemory = (SharedMemory*) ::MapViewOfFile (port._hMap, FILE_MAP_WRITE, 0, 0, sizeof (SharedMemory));
    ARA_INTERNAL_ASSERT (port._sharedMemory != nullptr);

    return port;
}

IPCPort IPCPort::createConnectedToID (const std::string& portID, const ReceiveCallback& callback)
{
    IPCPort port { portID };

    const auto mapKey { std::string { "Map" } + portID };
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

//  ARA_LOG ("IPCPort received message with ID %i", _sharedMemory->messageID);
    const auto replyData { _receiveCallback (_sharedMemory->messageID, messageData) };

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

IPCPort::IPCPort (IPCPort&& other) noexcept
{
    *this = std::move (other);
}

IPCPort& IPCPort::operator= (IPCPort&& other) noexcept
{
    std::swap (_creationThreadID, other._creationThreadID);
    std::swap (_writeSemaphore, other._writeSemaphore);
    std::swap (_sendPort, other._sendPort);
    std::swap (_receivePort, other._receivePort);
    std::swap (_receiveCallback, other._receiveCallback);
    std::swap (_callbackHandle, other._callbackHandle);
    if (_callbackHandle)
        *_callbackHandle = this;
    if (other._callbackHandle)
        *other._callbackHandle = &other;
    std::swap (_awaitsReply, other._awaitsReply);
    std::swap (_replyHandler, other._replyHandler);
    return *this;
}

IPCPort::~IPCPort ()
{
    if (_sendPort)
    {
        CFMessagePortInvalidate (_sendPort);
        CFRelease (_sendPort);
    }
    if (_receivePort)
    {
        CFMessagePortInvalidate (_receivePort);
        CFRelease (_receivePort);
    }
    if (_writeSemaphore)
        sem_close (_writeSemaphore);
    if (_callbackHandle)
        delete _callbackHandle;
    ARA_INTERNAL_ASSERT (!_awaitsReply);
    ARA_INTERNAL_ASSERT (_replyHandler == nullptr);
}

CFDataRef IPCPort::_portCallback (CFMessagePortRef /*port*/, SInt32 messageID, CFDataRef messageData, void* info)
{
    IPCPort* port = *((IPCPort**) info);
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == port->_creationThreadID);

    if (messageID != 0)
    {
//      ARA_LOG ("IPCPort received message with ID %i", messageID);
        const auto replyData { port->_receiveCallback (messageID, messageData) };

        const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (port->_sendPort, 0, replyData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
        ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);
        if (replyData)
            CFRelease (replyData);
    }
    else
    {
        ARA_INTERNAL_ASSERT (port->_awaitsReply);
        if (port->_replyHandler)
            (*port->_replyHandler) (messageData);
        port->_awaitsReply = false;
    }

    return nullptr;
}

sem_t* _openSemaphore (const std::string& portID, bool create)
{
    const auto previousUMask { umask (0) };

    std::string semName { "/" };
    semName += portID;
    if (semName.length () > PSHMNAMLEN - 1)
        semName.erase (10, semName.length () - PSHMNAMLEN + 1);

    auto result { sem_open (semName.c_str (), ((create) ? O_CREAT | O_EXCL : 0), S_IRUSR | S_IWUSR, 0) };
    ARA_INTERNAL_ASSERT (result != SEM_FAILED);

    if (!create)
        sem_unlink (semName.c_str ());

    umask (previousUMask);

    return result;
}

CFMessagePortRef __attribute__ ((cf_returns_retained)) IPCPort::_createMessagePortPublishingID (const std::string& portID, IPCPort** callbackHandle)
{
    auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

    CFMessagePortContext portContext { 0, callbackHandle, nullptr, nullptr, nullptr };
    auto result = CFMessagePortCreateLocal (kCFAllocatorDefault, wrappedPortID, &_portCallback, &portContext, nullptr);
    ARA_INTERNAL_ASSERT (result != nullptr);

    CFRelease (wrappedPortID);

    CFRunLoopSourceRef runLoopSource { CFMessagePortCreateRunLoopSource (kCFAllocatorDefault, result, 0) };
    CFRunLoopAddSource (CFRunLoopGetCurrent (), runLoopSource, kCFRunLoopDefaultMode);
    CFRelease (runLoopSource);

    return result;
}

CFMessagePortRef __attribute__ ((cf_returns_retained)) IPCPort::_createMessagePortConnectedToID (const std::string& portID)
{
    CFMessagePortRef result {};

    auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

    auto timeout { 5.0 };
    while (timeout > 0.0)
    {
        if ((result = CFMessagePortCreateRemote (kCFAllocatorDefault, wrappedPortID)))
            break;

        constexpr auto waitTime { 0.01 };
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, true);
        timeout -= waitTime;
    }
    ARA_INTERNAL_ASSERT (result != nullptr);

    CFRelease (wrappedPortID);

    return result;
}

IPCPort IPCPort::createPublishingID (const std::string& portID, const ReceiveCallback& callback)
{
    IPCPort port;
    port._receiveCallback = std::move (callback);
    port._writeSemaphore = _openSemaphore (portID, true);
    port._callbackHandle = new IPCPort* { &port };
    port._sendPort = _createMessagePortConnectedToID (portID + ".from_server");
    port._receivePort = _createMessagePortPublishingID (portID + ".to_server", port._callbackHandle);
    return port;
}

IPCPort IPCPort::createConnectedToID (const std::string& portID, const ReceiveCallback& callback)
{
    IPCPort port;
    port._receiveCallback = std::move (callback);
    port._callbackHandle = new IPCPort* { &port };
    port._receivePort = _createMessagePortPublishingID (portID + ".from_server", port._callbackHandle);
    port._sendPort = _createMessagePortConnectedToID (portID + ".to_server");
    port._writeSemaphore = _openSemaphore (portID, false);
    sem_post (port._writeSemaphore);
    return port;
}

void IPCPort::sendMessage (const MessageID messageID, DataToSend const __attribute__((cf_consumed)) messageData, ReplyHandler* replyHandler)
{
//  ARA_LOG ("IPCPort::sendMessage %i", messageID);

    const bool isOnCreationThread { std::this_thread::get_id () == _creationThreadID };
    while (sem_trywait (_writeSemaphore) != 0)
    {
        if (isOnCreationThread)
            runReceiveLoop (1);
        else
            std::this_thread::yield (); // \todo maybe it would be better to sleep for some longer interval here instead of simply yielding?
    }

    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_sendPort, messageID, messageData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
    ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);

    sem_post (_writeSemaphore);

    if (messageData)
        CFRelease (messageData);

    const auto previousAwaitsReply { _awaitsReply };
    const auto previousReplyHandler { _replyHandler };
    _awaitsReply = true;
    _replyHandler = replyHandler;
    do
    {
        if (isOnCreationThread)
            runReceiveLoop (messageTimeout);
        else
            std::this_thread::yield (); // \todo maybe it would be better to sleep for some longer interval here instead of simply yielding?
    } while (_awaitsReply);
    _awaitsReply = previousAwaitsReply;
    _replyHandler = previousReplyHandler;
}

void IPCPort::runReceiveLoop (int32_t milliseconds)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, true);
}

_Pragma ("GCC diagnostic pop")

//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------
