//------------------------------------------------------------------------------
//! \file       IPCXMLMessage.h
//!             XML-based messaging used for IPC in SDK IPC demo example
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

#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#if defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#include "3rdParty/pugixml/src/pugixml.hpp"


// A simple proof-of-concept wrapper for the IPC messages sent back and forth in the ARA IPC example.
// Error handling is limited to assertions.
// The basic data types transmitted are int32_t, int64_t, size_t, float, double, UTF8 encoded C strings
// and (large) opaque byte arrays. Note that received string or byte pointers are only valid as long as
// the message that provided them is alive.
// Messages can be nested in a hierarchy.
// The transmission channel handles proper endianness conversion of the numbers if needed.
// The transmission currently assumes the same size_t on both ends of the transmission - if either
// side had smaller pointers, some additional infrastructure would be needed to allocate a unique
// 32 bit representation for each size_t provided by the 64 bit process to the 32 bit process,
// and then map between the two.
class IPCXMLMessage
{
public:
    // key type for message arguments
    using MessageKey = int32_t;

    // C++ "rule of five" standard methods
    IPCXMLMessage (const IPCXMLMessage& other) { *this = other; }
    IPCXMLMessage (IPCXMLMessage&& other) noexcept { *this = std::move (other); }
    IPCXMLMessage& operator= (const IPCXMLMessage& other);
    IPCXMLMessage& operator= (IPCXMLMessage&& other)  noexcept;

    // to be used by IPCPort only: encoding from/to port-internal datas format
#if defined (__APPLE__)
    explicit IPCXMLMessage (CFDataRef data);
    __attribute__((cf_returns_retained)) CFDataRef createEncodedMessage () const;
#else
    IPCXMLMessage (const char* data, const size_t dataSize);
    std::string createEncodedMessage () const;
#endif

    // default construction, creating empty message
    IPCXMLMessage () = default;

    void appendInt32 (const MessageKey argKey, const int32_t argValue);
    void appendInt64 (const MessageKey argKey, const int64_t argValue);
    void appendSize (const MessageKey argKey, const size_t argValue);
    void appendFloat (const MessageKey argKey, const float argValue);
    void appendDouble (const MessageKey argKey, const double argValue);
    void appendString (const MessageKey argKey, const char* const argValue);
    void appendBytes (const MessageKey argKey, const uint8_t* argValue, const size_t argSize, const bool copy);
    IPCXMLMessage* appendSubMessage (const MessageKey argKey);

    bool isEmpty () const;
    bool readInt32 (const MessageKey argKey, int32_t& argValue) const;
    bool readInt64 (const MessageKey argKey, int64_t& argValue) const;
    bool readSize (const MessageKey argKey, size_t& argValue) const;
    bool readFloat (const MessageKey argKey, float& argValue) const;
    bool readDouble (const MessageKey argKey, double& argValue) const;
    bool readString (const MessageKey argKey, const char*& argValue) const;
    bool readBytesSize (const MessageKey argKey, size_t& argSize) const;
    void readBytes (const MessageKey argKey, uint8_t* const argValue) const;
    IPCXMLMessage* readSubMessage (const MessageKey argKey) const;

private:
    void _makeWritableIfNeeded ();
    static const char* _getEncodedKey (const MessageKey argKey);
    pugi::xml_attribute _appendAttribute (const MessageKey argKey);

private:
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
    bool _isWritable {};

    mutable std::string _bytesCacheData {};
    mutable MessageKey _bytesCacheKey { std::numeric_limits<MessageKey>::max () };
};
