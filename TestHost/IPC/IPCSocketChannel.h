//------------------------------------------------------------------------------
//! \file       IPCSocketChannel.h
//!             Proof-of-concept Unix domain socket implementation of MessageChannel
//!             for the ARA SDK TestHost on Linux (error handling is limited to assertions).
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2026, Celemony Software GmbH, All Rights Reserved.
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


#include "ARA_Library/IPC/ARAIPCConnection.h"

#if ARA_ENABLE_IPC

#if !defined (__linux__) && !defined (__WINE__)
    #error "IPCSocketChannel is a Linux/Wine-only implementation"
#endif

#include <atomic>
#include <mutex>
#include <thread>


//------------------------------------------------------------------------------
//! Unix domain socket MessageChannel.
//!
//! Each channel wraps one end of a socketpair() fd.  A background receive
//! thread reads frames and calls routeReceivedMessage(), so:
//!   receivesMessagesOnCurrentThread() -> false
//!   waitForMessageOnCurrentThread()   -> false  (never called)
//!
//! Wire frame:
//!   [int32_t  messageID ]  4 bytes LE
//!   [uint32_t payloadLen]  4 bytes LE
//!   [uint8_t  payload[] ]  payloadLen bytes
//!
//! The Connection's MainThreadMessageDispatcher uses the WaitableSingleMessageQueue
//! semaphore path to forward messages to the creation thread via
//! dispatchToCreationThread().
//------------------------------------------------------------------------------

class IPCSocketChannel : public ARA::IPC::MessageChannel
{
public:
    //! Takes ownership of the fd.  Starts the receive thread immediately.
    explicit IPCSocketChannel (int fd);
    ~IPCSocketChannel () override;

    void sendMessage (ARA::IPC::MessageID messageID,
                      std::unique_ptr<ARA::IPC::MessageEncoder> && encoder) override;

    bool receivesMessagesOnCurrentThread () override { return false; }
    bool waitForMessageOnCurrentThread ()   override { return false; }

private:
    void _receiveLoop ();

    int                   _fd;
    std::mutex            _sendMutex;
    std::thread           _recvThread;
    std::atomic<bool>     _stop { false };
};

#endif // ARA_ENABLE_IPC
