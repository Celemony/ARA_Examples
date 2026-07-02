//------------------------------------------------------------------------------
//! \file       TestPersistency.cpp
//!             archiver/unarchiver implementation for the ARA test plug-in
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2026, Celemony Software GmbH, All Rights Reserved.
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

#include "TestPersistency.h"

#include <limits>
#include <vector>

// host-network byte ordering include
#if defined (_WIN32)
    #include <winsock2.h>
#elif defined (__APPLE__)
    #include <arpa/inet.h>
#elif defined (__linux__)
    #include <arpa/inet.h>
    #ifndef htonll
        #define htonll(x) ((1==htonl (1)) ? (x) : (static_cast<uint64_t> (htonl (static_cast<uint32_t> ((x) & 0xFFFFFFFF))) << 32) | htonl (static_cast<uint32_t> ((x) >> 32)))
    #endif
    #ifndef ntohll
        #define ntohll(x) ((1==ntohl (1)) ? (x) : (static_cast<uint64_t> (ntohl (static_cast<uint32_t> ((x) & 0xFFFFFFFF))) << 32) | ntohl (static_cast<uint32_t> ((x) >> 32)))
    #endif
#endif

enum { archiveVersion = 1 };

union ConversionHelper8Byte
{
    explicit ConversionHelper8Byte (double value) : _doubleValue (value) {}
    explicit ConversionHelper8Byte (int64_t value) : _intValue (value) {}
    explicit ConversionHelper8Byte (uint64_t bytes) : _bytes (bytes) {}
    operator double () const { return _doubleValue; }
    operator int64_t () const { return _intValue; }
    operator uint64_t () const { return _bytes; }
private:
    double _doubleValue;
    int64_t _intValue;
    uint64_t _bytes;
};

/*******************************************************************************/

TestArchiver::TestArchiver (const ArchivingFunction& writeFunction) noexcept
: _writeFunction { writeFunction }
{
    writeInt64 (archiveVersion);
}

void TestArchiver::writeBool (bool data) noexcept
{
    write8ByteData ((data) ? 1U : 0U);
}

void TestArchiver::writeDouble (double data) noexcept
{
    static_assert (sizeof (double) == sizeof (uint64_t), "only implemented for architectures where double uses 64 bit");
    write8ByteData (ConversionHelper8Byte { data });
}

void TestArchiver::writeInt64 (int64_t data) noexcept
{
    write8ByteData (ConversionHelper8Byte { data });
}

void TestArchiver::writeSize (size_t data) noexcept
{
    static_assert (sizeof (size_t) <= sizeof (uint64_t), "only implemented for architectures where size_t can be mapped to uint64_t without losing precision");
    write8ByteData (static_cast<uint64_t> (data));
}

void TestArchiver::writeString (std::string data) noexcept
{
    const size_t numBytes { data.size () };
    writeSize (numBytes);
    if (didSucceed () && !_writeFunction (_location, numBytes, reinterpret_cast<const uint8_t*> (data.c_str ())))
        _state = TestArchiveState::iOError;
    _location += numBytes;
}

void TestArchiver::write8ByteData (uint64_t data) noexcept
{
    const uint64_t encodedData { htonll (data) };
    if (didSucceed () && !_writeFunction (_location, sizeof (data), reinterpret_cast<const uint8_t*> (&encodedData)))
        _state = TestArchiveState::iOError;
    _location += sizeof (data);
}

/*******************************************************************************/

TestUnarchiver::TestUnarchiver (const UnarchivingFunction& readFunction) noexcept
: _readFunction { readFunction }
{
    const int64_t version { readInt64 () };
    if (didSucceed () && (version != archiveVersion))
        _state = TestArchiveState::unknownFormatError;
}

bool TestUnarchiver::readBool () noexcept
{
    const uint64_t encodedData { read8ByteData () };
    return encodedData != 0;
}

double TestUnarchiver::readDouble () noexcept
{
    return ConversionHelper8Byte { read8ByteData () };
}

int64_t TestUnarchiver::readInt64 () noexcept
{
    return ConversionHelper8Byte { read8ByteData () };
}

size_t TestUnarchiver::readSize () noexcept
{
    uint64_t data { read8ByteData () };

    if constexpr (sizeof (size_t) < sizeof (uint64_t))
    {
        if (data > std::numeric_limits<size_t>::max ())
        {
            _state = TestArchiveState::incompatibleDataError;
            data = 0;
        }
    }

    return static_cast<size_t> (data);
}

std::string TestUnarchiver::readString ()
{
    std::string data;
    const size_t numBytes { readSize () };
    if (didSucceed () && numBytes)
    {
        std::vector<char> stringBuffer (numBytes + 1);
        if (_readFunction (_location, numBytes, reinterpret_cast<uint8_t*> (stringBuffer.data ())))
            data = stringBuffer.data ();
        else
            _state = TestArchiveState::iOError;
        _location += numBytes;
    }
    return data;
}

uint64_t TestUnarchiver::read8ByteData () noexcept
{
    uint64_t encodedData { 0 };
    if (didSucceed () && !_readFunction (_location, sizeof (encodedData), reinterpret_cast<uint8_t*> (&encodedData)))
        _state = TestArchiveState::iOError;
    if (!didSucceed ())
        encodedData = 0;
    _location += sizeof (encodedData);
    return ntohll (encodedData);
}
