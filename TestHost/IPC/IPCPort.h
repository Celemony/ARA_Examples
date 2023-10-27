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
    #include <mutex>
    #include <utility>
#elif defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
    #include <semaphore.h>
#else
    #error "IPC not yet implemented for this platform"
#endif

#include <functional>
#include <string>
#include <thread>


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

    // uninitialized port - cannot be used until move-assigned from factory functions
    IPCPort () = default;

    ~IPCPort ();

    // factory functions for send and receive ports
#if defined (__APPLE__)
    using ReceiveCallback = std::function</*__attribute__((cf_returns_retained))*/ DataToSend (const MessageID messageID, ReceivedData const messageData)>;
#else
    using ReceiveCallback = std::function<DataToSend (const MessageID messageID, ReceivedData const messageData)>;
#endif
    static IPCPort* createPublishingID (const std::string& portID, const ReceiveCallback& callback);
    static IPCPort* createConnectedToID (const std::string& portID, const ReceiveCallback& callback);

    // message sending
    // The messageData will be sent to the receiver, blocking the sending thread until the receiver
    // has processed the message and returned a (potentially empty) reply, which will be forwarded
    // to the optional replyHandler (can be nullptr if reply should be ignored)
    using ReplyHandler = std::function<void (ReceivedData)>;
#if defined (__APPLE__)
    void sendMessage (const MessageID messageID, const DataToSend __attribute__((cf_consumed)) messageData, ReplyHandler* replyHandler);
#else
    void sendMessage (const MessageID messageID, const DataToSend& messageData, ReplyHandler* replyHandler);
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
    explicit IPCPort (const std::string& portID);
    void _sendRequest (const MessageID messageID, const DataToSend& messageData);
#elif defined (__APPLE__)
    static CFDataRef _portCallback (CFMessagePortRef port, SInt32 messageID, CFDataRef messageData, void* info);
    static CFMessagePortRef __attribute__ ((cf_returns_retained)) _createMessagePortPublishingID (const std::string& portID, IPCPort* port);
    static CFMessagePortRef __attribute__ ((cf_returns_retained)) _createMessagePortConnectedToID (const std::string& portID);
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

    HANDLE _writeLock {};               // global lock shared between both processes, taken when starting a new transaction
    HANDLE _dataAvailable {};           // signal set by the sending side indicating new data has been placed in shared memory
    HANDLE _dataReceived {};            // signal set by the receiving side when evaluating the shared memory
    HANDLE _fileMapping {};
    SharedMemory* _sharedMemory {};
    std::recursive_mutex _readLock {};  // process-level lock blocking access to reading data

#elif defined (__APPLE__)
    sem_t* _writeSemaphore {};
    CFMessagePortRef _sendPort {};
    CFMessagePortRef _receivePort {};
#endif

    std::thread::id _creationThreadID { std::this_thread::get_id () };
    ReceiveCallback _receiveCallback {};
    int32_t _callbackLevel { 0 };
    bool _awaitsReply {};
    ReplyHandler* _replyHandler {};
};
