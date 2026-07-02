//------------------------------------------------------------------------------
//! \file       IPCSocketEncoding.cpp
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

#include "IPCSocketEncoding.h"

#if ARA_ENABLE_IPC

#include "ARA_Library/Debug/ARADebug.h"

#include <cstring>
#include <limits>


//------------------------------------------------------------------------------
// Wire serialization helpers
//------------------------------------------------------------------------------

namespace
{

void appendU8  (std::vector<uint8_t>& buf, uint8_t  v) { buf.push_back (v); }
void appendU32 (std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back ((v)       & 0xFFu);
    buf.push_back ((v >>  8) & 0xFFu);
    buf.push_back ((v >> 16) & 0xFFu);
    buf.push_back ((v >> 24) & 0xFFu);
}
void appendI32 (std::vector<uint8_t>& buf, int32_t  v) { appendU32 (buf, static_cast<uint32_t> (v)); }
void appendU64 (std::vector<uint8_t>& buf, uint64_t v) { appendU32 (buf, static_cast<uint32_t> (v)); appendU32 (buf, static_cast<uint32_t> (v >> 32)); }
void appendI64 (std::vector<uint8_t>& buf, int64_t  v) { appendU64 (buf, static_cast<uint64_t> (v)); }
void appendF32 (std::vector<uint8_t>& buf, float    v) { uint32_t t; std::memcpy (&t, &v, 4); appendU32 (buf, t); }
void appendF64 (std::vector<uint8_t>& buf, double   v) { uint64_t t; std::memcpy (&t, &v, 8); appendU64 (buf, t); }
void appendRaw (std::vector<uint8_t>& buf, const uint8_t* d, size_t n) { appendU32 (buf, static_cast<uint32_t> (n)); buf.insert (buf.end (), d, d + n); }

uint8_t  readU8  (const uint8_t*& p) { return *p++; }
uint32_t readU32 (const uint8_t*& p)
{
    uint32_t v = static_cast<uint32_t> (p[0]) | (static_cast<uint32_t> (p[1]) << 8)
               | (static_cast<uint32_t> (p[2]) << 16) | (static_cast<uint32_t> (p[3]) << 24);
    p += 4;
    return v;
}
int32_t  readI32 (const uint8_t*& p) { return static_cast<int32_t> (readU32 (p)); }
uint64_t readU64 (const uint8_t*& p) { uint64_t lo = readU32 (p); uint64_t hi = readU32 (p); return lo | (hi << 32); }
int64_t  readI64 (const uint8_t*& p) { return static_cast<int64_t> (readU64 (p)); }
float    readF32 (const uint8_t*& p) { uint32_t t = readU32 (p); float  v; std::memcpy (&v, &t, 4); return v; }
double   readF64 (const uint8_t*& p) { uint64_t t = readU64 (p); double v; std::memcpy (&v, &t, 8); return v; }

} // namespace


//------------------------------------------------------------------------------
// IPCSocketMessageEncoder
//------------------------------------------------------------------------------

void IPCSocketMessageEncoder::_store (ARA::IPC::MessageArgumentKey k, IPCSocketValue v, IPCSocketTag t)
{
    for (auto& e : _entries)
    {
        if (e.key == k)
        {
            e.val = std::move (v);
            e.tag = t;
            return;
        }
    }
    _entries.push_back ({ k, t, std::move (v) });
}

void IPCSocketMessageEncoder::appendInt32  (ARA::IPC::MessageArgumentKey k, int32_t  v) { _store (k, IPCSocketValue { v },                    IPCSocketTag::I32);   }
void IPCSocketMessageEncoder::appendInt64  (ARA::IPC::MessageArgumentKey k, int64_t  v) { _store (k, IPCSocketValue { v },                    IPCSocketTag::I64);   }
void IPCSocketMessageEncoder::appendSize   (ARA::IPC::MessageArgumentKey k, size_t   v) { _store (k, IPCSocketValue { static_cast<uint64_t> (v) }, IPCSocketTag::Size);  }
void IPCSocketMessageEncoder::appendFloat  (ARA::IPC::MessageArgumentKey k, float    v) { _store (k, IPCSocketValue { v },                    IPCSocketTag::Float); }
void IPCSocketMessageEncoder::appendDouble (ARA::IPC::MessageArgumentKey k, double   v) { _store (k, IPCSocketValue { v },                    IPCSocketTag::Dbl);   }

void IPCSocketMessageEncoder::appendString (ARA::IPC::MessageArgumentKey k, const char* v)
{
    _store (k, IPCSocketValue { std::string { v ? v : "" } }, IPCSocketTag::Str);
}

void IPCSocketMessageEncoder::appendBytes (ARA::IPC::MessageArgumentKey k, const uint8_t* data, size_t size, bool /*copy*/)
{
    _store (k, IPCSocketValue { std::vector<uint8_t> { data, data + size } }, IPCSocketTag::Bytes);
}

std::unique_ptr<ARA::IPC::MessageEncoder> IPCSocketMessageEncoder::appendSubMessage (ARA::IPC::MessageArgumentKey k)
{
    auto subEnc = std::make_shared<IPCSocketMessageEncoder> ();
    auto holder = std::make_shared<IPCSocketSubMsg> ();
    holder->enc = subEnc;
    _store (k, IPCSocketValue { holder }, IPCSocketTag::Sub);

    struct NonOwning : public ARA::IPC::MessageEncoder
    {
        IPCSocketMessageEncoder* _t;
        explicit NonOwning (IPCSocketMessageEncoder* t) : _t { t } {}
        void appendInt32  (ARA::IPC::MessageArgumentKey k, int32_t  v) override { _t->appendInt32  (k, v); }
        void appendInt64  (ARA::IPC::MessageArgumentKey k, int64_t  v) override { _t->appendInt64  (k, v); }
        void appendSize   (ARA::IPC::MessageArgumentKey k, size_t   v) override { _t->appendSize   (k, v); }
        void appendFloat  (ARA::IPC::MessageArgumentKey k, float    v) override { _t->appendFloat  (k, v); }
        void appendDouble (ARA::IPC::MessageArgumentKey k, double   v) override { _t->appendDouble (k, v); }
        void appendString (ARA::IPC::MessageArgumentKey k, const char* v) override { _t->appendString (k, v); }
        void appendBytes  (ARA::IPC::MessageArgumentKey k, const uint8_t* d, size_t n, bool c) override { _t->appendBytes (k, d, n, c); }
        std::unique_ptr<ARA::IPC::MessageEncoder> appendSubMessage (ARA::IPC::MessageArgumentKey k) override { return _t->appendSubMessage (k); }
    };
    return std::make_unique<NonOwning> (subEnc.get ());
}


//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

static void _serializeInto (const IPCSocketMessageEncoder& enc, std::vector<uint8_t>& buf)
{
    appendU32 (buf, static_cast<uint32_t> (enc.entries ().size ()));
    for (const auto& e : enc.entries ())
    {
        appendI32 (buf, e.key);
        appendU8  (buf, static_cast<uint8_t> (e.tag));
        switch (e.tag)
        {
            case IPCSocketTag::I32:   appendI32 (buf, std::get<int32_t>  (e.val)); break;
            case IPCSocketTag::I64:   appendI64 (buf, std::get<int64_t>  (e.val)); break;
            case IPCSocketTag::Size:  appendU64 (buf, std::get<uint64_t> (e.val)); break;
            case IPCSocketTag::Float: appendF32 (buf, std::get<float>    (e.val)); break;
            case IPCSocketTag::Dbl:   appendF64 (buf, std::get<double>   (e.val)); break;
            case IPCSocketTag::Str:
            {
                const auto& s = std::get<std::string> (e.val);
                appendRaw (buf, reinterpret_cast<const uint8_t*> (s.data ()), s.size ());
                break;
            }
            case IPCSocketTag::Bytes:
            {
                const auto& b = std::get<std::vector<uint8_t>> (e.val);
                appendRaw (buf, b.data (), b.size ());
                break;
            }
            case IPCSocketTag::Sub:
            {
                std::vector<uint8_t> sub;
                _serializeInto (*std::get<std::shared_ptr<IPCSocketSubMsg>> (e.val)->enc, sub);
                appendRaw (buf, sub.data (), sub.size ());
                break;
            }
        }
    }
}

std::vector<uint8_t> IPCSocketEncodeMessage (const IPCSocketMessageEncoder& enc)
{
    std::vector<uint8_t> buf;
    _serializeInto (enc, buf);
    return buf;
}


//------------------------------------------------------------------------------
// IPCSocketMessageDecoder
//------------------------------------------------------------------------------

std::unique_ptr<IPCSocketMessageDecoder> IPCSocketMessageDecoder::createWithMessageData (const uint8_t* data, size_t len)
{
    const uint8_t* p   = data;
    const uint8_t* end = data + len;

    auto safeRead = [&] (size_t n) -> bool { return static_cast<size_t> (end - p) >= n; };

    if (!safeRead (4))
        return nullptr;
    uint32_t count = readU32 (p);

    std::vector<Entry> entries;
    entries.reserve (count);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (!safeRead (5))
            return nullptr;

        Entry e;
        e.key = readI32 (p);
        e.tag = static_cast<IPCSocketTag> (readU8 (p));

        switch (e.tag)
        {
            case IPCSocketTag::I32:   if (!safeRead (4)) return nullptr; e.i32 = readI32 (p); break;
            case IPCSocketTag::I64:   if (!safeRead (8)) return nullptr; e.i64 = readI64 (p); break;
            case IPCSocketTag::Size:  if (!safeRead (8)) return nullptr; e.sz  = readU64 (p); break;
            case IPCSocketTag::Float: if (!safeRead (4)) return nullptr; e.f32 = readF32 (p); break;
            case IPCSocketTag::Dbl:   if (!safeRead (8)) return nullptr; e.f64 = readF64 (p); break;
            case IPCSocketTag::Str:
            {
                if (!safeRead (4)) return nullptr;
                uint32_t n = readU32 (p);
                if (!safeRead (n)) return nullptr;
                e.str.assign (reinterpret_cast<const char*> (p), n);
                p += n;
                break;
            }
            case IPCSocketTag::Bytes:
            {
                if (!safeRead (4)) return nullptr;
                uint32_t n = readU32 (p);
                if (!safeRead (n)) return nullptr;
                e.bytes.assign (p, p + n);
                p += n;
                break;
            }
            case IPCSocketTag::Sub:
            {
                if (!safeRead (4)) return nullptr;
                uint32_t n = readU32 (p);
                if (!safeRead (n)) return nullptr;
                e.subBytes.assign (p, p + n);
                p += n;
                break;
            }
        }
        entries.push_back (std::move (e));
    }
    return std::make_unique<IPCSocketMessageDecoder> (std::move (entries));
}

const IPCSocketMessageDecoder::Entry* IPCSocketMessageDecoder::_find (ARA::IPC::MessageArgumentKey k, IPCSocketTag t) const
{
    for (const auto& e : _entries)
        if (e.key == k && e.tag == t)
            return &e;
    return nullptr;
}

bool IPCSocketMessageDecoder::readInt32  (ARA::IPC::MessageArgumentKey k, int32_t*  v) const { const auto* e = _find (k, IPCSocketTag::I32);   if (!e) { *v = 0;       return false; } *v = e->i32; return true; }
bool IPCSocketMessageDecoder::readInt64  (ARA::IPC::MessageArgumentKey k, int64_t*  v) const { const auto* e = _find (k, IPCSocketTag::I64);   if (!e) { *v = 0;       return false; } *v = e->i64; return true; }
bool IPCSocketMessageDecoder::readSize   (ARA::IPC::MessageArgumentKey k, size_t*   v) const { const auto* e = _find (k, IPCSocketTag::Size);  if (!e) { *v = 0;       return false; } *v = static_cast<size_t> (e->sz); return true; }
bool IPCSocketMessageDecoder::readFloat  (ARA::IPC::MessageArgumentKey k, float*    v) const { const auto* e = _find (k, IPCSocketTag::Float); if (!e) { *v = 0.0f;    return false; } *v = e->f32; return true; }
bool IPCSocketMessageDecoder::readDouble (ARA::IPC::MessageArgumentKey k, double*   v) const { const auto* e = _find (k, IPCSocketTag::Dbl);   if (!e) { *v = 0.0;     return false; } *v = e->f64; return true; }

bool IPCSocketMessageDecoder::readString (ARA::IPC::MessageArgumentKey k, const char** v) const
{
    const auto* e = _find (k, IPCSocketTag::Str);
    if (!e) { *v = nullptr; return false; }
    *v = e->str.c_str ();
    return true;
}

bool IPCSocketMessageDecoder::readBytesSize (ARA::IPC::MessageArgumentKey k, size_t* sz) const
{
    const auto* e = _find (k, IPCSocketTag::Bytes);
    if (!e) { *sz = 0; return false; }
    *sz = e->bytes.size ();
    return true;
}

void IPCSocketMessageDecoder::readBytes (ARA::IPC::MessageArgumentKey k, uint8_t* out) const
{
    const auto* e = _find (k, IPCSocketTag::Bytes);
    if (!e) return;
    std::memcpy (out, e->bytes.data (), e->bytes.size ());
}

std::unique_ptr<ARA::IPC::MessageDecoder> IPCSocketMessageDecoder::readSubMessage (ARA::IPC::MessageArgumentKey k) const
{
    const auto* e = _find (k, IPCSocketTag::Sub);
    if (!e) return nullptr;
    return createWithMessageData (e->subBytes.data (), e->subBytes.size ());
}

bool IPCSocketMessageDecoder::hasDataForKey (ARA::IPC::MessageArgumentKey k) const
{
    for (const auto& e : _entries)
        if (e.key == k)
            return true;
    return false;
}

#endif // ARA_ENABLE_IPC
