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
    #include <utility>
#elif defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
    #include <semaphore.h>
#else
    #error "IPC not yet implemented for this platform"
#endif

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


class IPCSendPort;
class IPCReceivePort;


class IPCMessageChannel : public ARA::IPC::ARAIPCMessageChannel
{
public:
    ~IPCMessageChannel () override;

    // factory functions for send and receive channels
    using ReceiveCallback = void (*) (ARAIPCMessageChannel* messageChannel,
                                      const ARA::IPC::ARAIPCMessageID messageID,
                                      const ARA::IPC::ARAIPCMessageDecoder* decoder,
                                      ARA::IPC::ARAIPCMessageEncoder* const replyEncoder);
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

    // callbacks for the _receivePort
    const ARA::IPC::ARAIPCMessageDecoder* createDecoderForMessage (ARA::IPC::ARAIPCMessageID messageID,
#if defined (_WIN32)
                                                                                                        const char* data, const size_t dataSize);
#elif defined (__APPLE__)
                                                                                                        CFDataRef messageData);
#endif
    void routeReceivedMessage (ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageDecoder* decoder);

private:
    IPCMessageChannel (const ReceiveCallback& callback);
    void _lockTransaction (bool isOnCreationThread);
    void _unlockTransaction ();
    void _sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder);
    void _handleReceivedMessage (ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageDecoder* decoder);

private:
#if defined (_WIN32)
    HANDLE
#elif defined (__APPLE__)
    sem_t*
#endif
           _transactionLock {};     // global lock shared between both processes, taken when starting a new transaction

    IPCSendPort* _sendPort {};
    IPCReceivePort* _receivePort {};

    std::thread::id _creationThreadID { std::this_thread::get_id () };
    ReceiveCallback const _receiveCallback {};
    int32_t _callbackLevel { 0 };
    bool _awaitsReply {};
    ReplyHandler _replyHandler {};
    void* _replyHandlerUserData {};
};
