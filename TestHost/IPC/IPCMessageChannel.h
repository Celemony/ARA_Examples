//------------------------------------------------------------------------------
//! \file       IPCMessageChannel.h
//!             Proof-of-concept implementation of ARAIPCMessageChannel
//!             for the ARA SDK TestHost (error handling is limited to assertions).
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


#include "ARA_Library/IPC/ARAIPC.h"

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


// select underlying implementation: Apple CFDictionary or a generic pugixml-based
// Note that the pugixml version is much less efficient because it base64-encodes bytes
// (used for large sample data) which adds encoding overhead and requires additional copies.
#ifndef USE_ARA_CF_ENCODING
    #if defined (__APPLE__)
        #define USE_ARA_CF_ENCODING 1
    #else
        #define USE_ARA_CF_ENCODING 0
    #endif
#endif


class IPCMessageChannel : public ARA::IPC::ARAIPCMessageChannel
{
public:
    ~IPCMessageChannel () override;

    // factory functions for send and receive channels
    using ReceiveCallback = std::function<void (ARAIPCMessageChannel* messageChannel,
                                                const ARA::IPC::ARAIPCMessageID messageID,
                                                const ARA::IPC::ARAIPCMessageDecoder* decoder,
                                                ARA::IPC::ARAIPCMessageEncoder* const replyEncoder)>;
    static IPCMessageChannel* createPublishingID (const std::string& channelID, const ReceiveCallback& callback);
    static IPCMessageChannel* createConnectedToID (const std::string& channelID, const ReceiveCallback& callback);

    ARA::IPC::ARAIPCMessageEncoder* createEncoder () override;

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder,
                      ReplyHandler const replyHandler, void* replyHandlerUserData) override;

    // \todo currently not implemented, we rely on running on the same machine for now
    //       C++20 offers std::endian which allows for a simple implementation upon connecting...
    bool receiverEndianessMatches ()  override { return true; }

    // message receiving
    // waits up to the specified amount of milliseconds for an incoming event and processes it
    void runReceiveLoop (int32_t milliseconds);

private:
    IPCMessageChannel () = default;
#if defined (_WIN32)
    explicit IPCMessageChannel (const std::string& channelID);
    void _sendRequest (const ARA::IPC::ARAIPCMessageID messageID, const std::string& messageData);
#elif defined (__APPLE__)
    static CFDataRef _portCallback (CFMessagePortRef port, SInt32 messageID, CFDataRef messageData, void* info);
    static CFMessagePortRef __attribute__ ((cf_returns_retained)) _createMessagePortPublishingID (const std::string& portID, IPCMessageChannel* channel);
    static CFMessagePortRef __attribute__ ((cf_returns_retained)) _createMessagePortConnectedToID (const std::string& portID);
#endif

private:
#if defined (_WIN32)
    struct SharedMemory
    {
        static constexpr DWORD maxMessageSize { 4 * 1024 * 1024L - 64};

        size_t messageSize;
        ARA::IPC::ARAIPCMessageID messageID;
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
    ReplyHandler _replyHandler {};
    void* _replyHandlerUserData {};
};
