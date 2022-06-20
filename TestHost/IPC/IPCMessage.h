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
    // C++ "rule of five" standard methods
    IPCMessage (const IPCMessage& other) { *this = other; }
    IPCMessage (IPCMessage&& other) noexcept { *this = std::move (other); }
    IPCMessage& operator= (const IPCMessage& other);
    IPCMessage& operator= (IPCMessage&& other)  noexcept;
#if IPC_MESSAGE_USE_CFDICTIONARY
    ~IPCMessage ();
#else
    ~IPCMessage () = default;
#endif

    // to be used by IPCPort only: encoding from/to port-internal datas format
#if defined (__APPLE__)
    explicit IPCMessage (CFDataRef data);
    CFDataRef createEncodedMessage () const;
#else
    IPCMessage (const char* data, const size_t dataSize);
    std::string createEncodedMessage () const;
#endif

    // default construction, creating empty message
    IPCMessage () = default;

    // append keyed argument to the message
    void appendInt32 (const int32_t argKey, const int32_t argValue);
    void appendInt64 (const int32_t argKey, const int64_t argValue);
    void appendSize (const int32_t argKey, const size_t argValue);          // also for ptr-sized refs
    void appendFloat (const int32_t argKey, const float argValue);
    void appendDouble (const int32_t argKey, const double argValue);
    void appendString (const int32_t argKey, const char* const argValue);   // UTF8
    void appendBytes (const int32_t argKey, const uint8_t* argValue, const size_t argSize, const bool copy = true); // disable copy if data is alive longer than the message
    void appendMessage (const int32_t argKey, const IPCMessage& argValue);

    // read keyed argument to the message
    // if key is not found, returns false and a default value
    bool readInt32 (const int32_t argKey, int32_t& argValue) const;
    bool readInt64 (const int32_t argKey, int64_t& argValue) const;
    bool readSize (const int32_t argKey, size_t& argValue) const;           // also for ptr-sized refs
    bool readFloat (const int32_t argKey, float& argValue) const;
    bool readDouble (const int32_t argKey, double& argValue) const;
    bool readString (const int32_t argKey, const char*& argValue) const;    // UTF8
    bool readBytesSize (const int32_t argKey, size_t& argSize) const;       // query size and allocate memory, then..
    bool readBytes (const int32_t argKey, uint8_t* const argValue) const;   // copy bytes to that memory (if key found)
    bool readMessage (const int32_t argKey, IPCMessage& argValue) const;

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    // wrap key value into CFString (no reference count transferred to caller)
    static CFStringRef _getEncodedKey (const int32_t argKey);

    // internal primitive - dictionary will be retained or copied, depending on isWritable
    void _setup (CFDictionaryRef dictionary, bool isWritable);

    void _appendEncodedArg (const int32_t argKey, __attribute__((cf_consumed)) CFTypeRef argObject);
#else
    static const char* _getEncodedKey (const int32_t argKey);

    void _makeWritableIfNeeded ();

    pugi::xml_attribute _appendAttribute (const int32_t argKey);
#endif

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    CFDictionaryRef _dictionary {};     // if modified, actually converted to a CFMutableDictionaryRef
#else
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
    mutable std::string _bytesCacheData {};
    mutable int32_t _bytesCacheKey { std::numeric_limits<int32_t>::max () };
#endif
    bool _isWritable {};
};
