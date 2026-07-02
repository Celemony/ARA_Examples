//------------------------------------------------------------------------------
//! \file       IPCSocketChannel.cpp
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

#include "IPCSocketChannel.h"

#if ARA_ENABLE_IPC

#include "IPC/IPCSocketEncoding.h"
#include "ARA_Library/Debug/ARADebug.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>


//------------------------------------------------------------------------------
// Low-level frame I/O
//------------------------------------------------------------------------------

static bool _writeAll (int fd, const void* buf, size_t n)
{
    const auto* p = static_cast<const uint8_t*> (buf);
    while (n > 0)
    {
        ssize_t r = ::write (fd, p, n);
        if (r <= 0)
            return false;
        p += r;
        n -= static_cast<size_t> (r);
    }
    return true;
}

static bool _readAll (int fd, void* buf, size_t n)
{
    auto* p = static_cast<uint8_t*> (buf);
    while (n > 0)
    {
        ssize_t r = ::read (fd, p, n);
        if (r <= 0)
            return false;
        p += r;
        n -= static_cast<size_t> (r);
    }
    return true;
}


//------------------------------------------------------------------------------
// IPCSocketChannel
//------------------------------------------------------------------------------

IPCSocketChannel::IPCSocketChannel (int fd)
: _fd { fd }
{
    _recvThread = std::thread { [this] { _receiveLoop (); } };
}

IPCSocketChannel::~IPCSocketChannel ()
{
    _stop.store (true, std::memory_order_release);
    ::shutdown (_fd, SHUT_RDWR);
    if (_recvThread.joinable ())
        _recvThread.join ();
    ::close (_fd);
}

void IPCSocketChannel::sendMessage (ARA::IPC::MessageID messageID,
                                    std::unique_ptr<ARA::IPC::MessageEncoder> && encoder)
{
    const auto payload = IPCSocketEncodeMessage (static_cast<const IPCSocketMessageEncoder&> (*encoder));

    uint32_t hdr[2];
    hdr[0] = static_cast<uint32_t> (messageID);
    hdr[1] = static_cast<uint32_t> (payload.size ());

    std::lock_guard<std::mutex> lock { _sendMutex };
    ARA_INTERNAL_ASSERT (_writeAll (_fd, hdr, 8));
    if (!payload.empty ())
        ARA_INTERNAL_ASSERT (_writeAll (_fd, payload.data (), payload.size ()));
}

void IPCSocketChannel::_receiveLoop ()
{
    while (!_stop.load (std::memory_order_acquire))
    {
        uint32_t hdr[2];
        if (!_readAll (_fd, hdr, 8))
            break;

        auto messageID      = static_cast<ARA::IPC::MessageID> (static_cast<int32_t> (hdr[0]));
        uint32_t payloadLen = hdr[1];

        std::unique_ptr<ARA::IPC::MessageDecoder> decoder;
        if (payloadLen > 0)
        {
            std::vector<uint8_t> payload (payloadLen);
            if (!_readAll (_fd, payload.data (), payloadLen))
                break;
            decoder = IPCSocketMessageDecoder::createWithMessageData (payload.data (), payload.size ());
        }

        routeReceivedMessage (messageID, std::move (decoder));
    }
}

#endif // ARA_ENABLE_IPC
