//------------------------------------------------------------------------------
//! \file       IPCMessage.h
//!             messaging used for IPC in SDK IPC demo example
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

// select underlying implementation: Apple CFDictionary or a generic pugixml-based
// Note that the pugixml version is much less efficient because it base64-encodes bytes
// (used for large sample data) which adds encoding overhead and requires additional copies.
#ifndef IPC_MESSAGE_USE_CFDICTIONARY
    #if defined (__APPLE__)
        #define IPC_MESSAGE_USE_CFDICTIONARY 1
    #else
        #define IPC_MESSAGE_USE_CFDICTIONARY 0
    #endif
#endif

#include <memory>
#include <type_traits>
#include <vector>

#if defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#if !IPC_MESSAGE_USE_CFDICTIONARY
    #include "3rdParty/pugixml/src/pugixml.hpp"
    #include "3rdParty/cpp-base64/base64.h"
#endif


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
class IPCMessage
{
public:
    // key type for message arguments
    using MessageKey = int32_t;

    // C++ "rule of five" standard methods
    IPCMessage (const IPCMessage& other) { *this = other; }
    IPCMessage (IPCMessage&& other) noexcept { *this = std::move (other); }
    IPCMessage& operator= (const IPCMessage& other);
    IPCMessage& operator= (IPCMessage&& other)  noexcept;
#if IPC_MESSAGE_USE_CFDICTIONARY
    ~IPCMessage ();
#endif

    // to be used by IPCPort only: encoding from/to port-internal datas format
#if defined (__APPLE__)
    explicit IPCMessage (CFDataRef data);
    __attribute__((cf_returns_retained)) CFDataRef createEncodedMessage () const;
#else
    IPCMessage (const char* data, const size_t dataSize);
    std::string createEncodedMessage () const;
#endif

    // default construction, creating empty message
    IPCMessage () = default;

    // ARA::IPC::ARAIPCMessageEncoder implementation
    void appendInt32 (const MessageKey argKey, const int32_t argValue);
    void appendInt64 (const MessageKey argKey, const int64_t argValue);
    void appendSize (const MessageKey argKey, const size_t argValue);
    void appendFloat (const MessageKey argKey, const float argValue);
    void appendDouble (const MessageKey argKey, const double argValue);
    void appendString (const MessageKey argKey, const char* const argValue);
    void appendBytes (const MessageKey argKey, const uint8_t* argValue, const size_t argSize, const bool copy);
    IPCMessage* appendSubMessage (const MessageKey argKey);

    // ARA::IPC::ARAIPCMessageDecoder implementation
    bool isEmpty () const;
    bool readInt32 (const MessageKey argKey, int32_t& argValue) const;
    bool readInt64 (const MessageKey argKey, int64_t& argValue) const;
    bool readSize (const MessageKey argKey, size_t& argValue) const;
    bool readFloat (const MessageKey argKey, float& argValue) const;
    bool readDouble (const MessageKey argKey, double& argValue) const;
    bool readString (const MessageKey argKey, const char*& argValue) const;
    bool readBytesSize (const MessageKey argKey, size_t& argSize) const;
    void readBytes (const MessageKey argKey, uint8_t* const argValue) const;
    IPCMessage* readSubMessage (const MessageKey argKey) const;

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    // wrap key value into CFString (no reference count transferred to caller)
    static CFStringRef _getEncodedKey (const MessageKey argKey);

    // internal primitive
    // typically, a copy should be made whenever the input is writable -
    // an exception is appending writable sub-messages
    void _setup (CFDictionaryRef dictionary, bool isWritable, bool makeCopy);

    void _appendEncodedArg (const MessageKey argKey, __attribute__((cf_consumed)) CFTypeRef argObject);
#else
    static const char* _getEncodedKey (const MessageKey argKey);

    void _makeWritableIfNeeded ();

    pugi::xml_attribute _appendAttribute (const MessageKey argKey);
#endif

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    CFDictionaryRef _dictionary {};     // if modified, actually converted to a CFMutableDictionaryRef
#else
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
    mutable std::string _bytesCacheData {};
    mutable MessageKey _bytesCacheKey { std::numeric_limits<MessageKey>::max () };
#endif
    bool _isWritable {};
};
