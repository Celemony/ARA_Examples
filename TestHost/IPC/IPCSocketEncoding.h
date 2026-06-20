//------------------------------------------------------------------------------
//! \file       IPCSocketEncoding.h
//!             Proof-of-concept Unix socket-based implementation of ARAIPCMessageEn-/Decoder
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


#include "ARA_Library/IPC/ARAIPCMessage.h"

#if ARA_ENABLE_IPC

#if !defined (__linux__) && !defined (__WINE__)
    #error "IPCSocketEncoding is a Linux/Wine-only implementation"
#endif

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>


//------------------------------------------------------------------------------
// Wire format
//
// Each encoded message is a flat byte buffer:
//   [uint32_t entry_count]
//   for each entry:
//     [int32_t  key ]
//     [uint8_t  tag ]   0=i32, 1=i64, 2=sz, 3=f32, 4=f64, 5=str, 6=bytes, 7=sub
//     [value bytes  ]
//       i32  : 4 bytes LE
//       i64  : 8 bytes LE
//       sz   : 8 bytes LE (always 64-bit on the wire)
//       f32  : 4 bytes
//       f64  : 8 bytes
//       str  : [uint32_t len][len bytes, no NUL]
//       bytes: [uint32_t len][len bytes]
//       sub  : [uint32_t len][len bytes of nested serialised message]
//
// Both ends of the socketpair run on the same machine (same arch, same
// endianness), so values are written in native byte order and the
// Connection is told receiverEndianessMatches = true.
//------------------------------------------------------------------------------

enum class IPCSocketTag : uint8_t
{
    I32   = 0,
    I64   = 1,
    Size  = 2,
    Float = 3,
    Dbl   = 4,
    Str   = 5,
    Bytes = 6,
    Sub   = 7,
};

struct IPCSocketSubMsg;
using IPCSocketValue = std::variant<
    int32_t,
    int64_t,
    uint64_t,
    float,
    double,
    std::string,
    std::vector<uint8_t>,
    std::shared_ptr<IPCSocketSubMsg>
>;

class IPCSocketMessageEncoder;

struct IPCSocketSubMsg
{
    std::shared_ptr<IPCSocketMessageEncoder> enc;
};


class IPCSocketMessageEncoder : public ARA::IPC::MessageEncoder
{
public:
    IPCSocketMessageEncoder () = default;

    void appendInt32  (ARA::IPC::MessageArgumentKey k, int32_t  v) override;
    void appendInt64  (ARA::IPC::MessageArgumentKey k, int64_t  v) override;
    void appendSize   (ARA::IPC::MessageArgumentKey k, size_t   v) override;
    void appendFloat  (ARA::IPC::MessageArgumentKey k, float    v) override;
    void appendDouble (ARA::IPC::MessageArgumentKey k, double   v) override;
    void appendString (ARA::IPC::MessageArgumentKey k, const char* v) override;
    void appendBytes  (ARA::IPC::MessageArgumentKey k, const uint8_t* data, size_t size, bool copy) override;
    std::unique_ptr<ARA::IPC::MessageEncoder> appendSubMessage (ARA::IPC::MessageArgumentKey k) override;

    struct Entry
    {
        ARA::IPC::MessageArgumentKey key;
        IPCSocketTag                 tag;
        IPCSocketValue               val;
    };
    const std::vector<Entry>& entries () const { return _entries; }

private:
    void _store (ARA::IPC::MessageArgumentKey k, IPCSocketValue v, IPCSocketTag t);
    std::vector<Entry> _entries;
};


class IPCSocketMessageDecoder : public ARA::IPC::MessageDecoder
{
public:
    struct Entry
    {
        ARA::IPC::MessageArgumentKey key;
        IPCSocketTag                 tag;
        int32_t              i32   {};
        int64_t              i64   {};
        uint64_t             sz    {};
        float                f32   {};
        double               f64   {};
        std::string          str;
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> subBytes;
    };

    explicit IPCSocketMessageDecoder (std::vector<Entry> entries)
        : _entries (std::move (entries)) {}

    bool readInt32  (ARA::IPC::MessageArgumentKey k, int32_t*  v) const override;
    bool readInt64  (ARA::IPC::MessageArgumentKey k, int64_t*  v) const override;
    bool readSize   (ARA::IPC::MessageArgumentKey k, size_t*   v) const override;
    bool readFloat  (ARA::IPC::MessageArgumentKey k, float*    v) const override;
    bool readDouble (ARA::IPC::MessageArgumentKey k, double*   v) const override;
    bool readString (ARA::IPC::MessageArgumentKey k, const char** v) const override;
    bool readBytesSize (ARA::IPC::MessageArgumentKey k, size_t* sz) const override;
    void readBytes  (ARA::IPC::MessageArgumentKey k, uint8_t* out) const override;
    std::unique_ptr<ARA::IPC::MessageDecoder> readSubMessage (ARA::IPC::MessageArgumentKey k) const override;
    bool hasDataForKey (ARA::IPC::MessageArgumentKey k) const override;

    static std::unique_ptr<IPCSocketMessageDecoder> createWithMessageData (const uint8_t* data, size_t len);
    std::vector<uint8_t> createEncodedMessage () const;

    static std::unique_ptr<ARA::IPC::MessageEncoder> createEncoder ()
        { return std::make_unique<IPCSocketMessageEncoder> (); }

private:
    const Entry* _find (ARA::IPC::MessageArgumentKey k, IPCSocketTag t) const;
    std::vector<Entry> _entries;
};

std::vector<uint8_t> IPCSocketEncodeMessage (const IPCSocketMessageEncoder& enc);

#endif // ARA_ENABLE_IPC
