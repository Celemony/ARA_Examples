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
// The basic data types transmitted are int32_t, int64_t, size_t, float, double, C strings and
// (large) opaque byte arrays. Note that received string or byte pointers are only valid as long as
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
    IPCMessage (const char* data, size_t dataSize);
    std::string createEncodedMessage () const;
#endif

    // default construction, creating empty message
    IPCMessage () = default;

    // append keyed argument to the message
    template <typename ArgT>
    void append (size_t argKey, ArgT argValue)
    {
        _appendArg (argKey, argValue);
    }

    // getters for receiving code: extract arguments, specifying the desired return type as template argument
    // the optional variant returns the default value of the argument type if key wasn't not found
    template <typename ArgT>
    ArgT getArgValue (size_t argKey) const
    {
        ArgT result;
        _readArg (argKey, result);
        return result;
    }
    template <typename ArgT>
    std::pair<ArgT, bool> getOptionalArgValue (size_t argKey) const
    {
        ArgT result;
        bool didFindKey { false };
        _readArg (argKey, result, &didFindKey);
        return { result, didFindKey };
    }

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    // internal primitive - dictionary will be retained or copied, depending on isWritable
    void _setup (CFDictionaryRef dictionary, bool isWritable);

    // wrap key value into CFString (no reference count transferred to caller)
    static CFStringRef _getEncodedKey (size_t argKey);
#else
    static const char* _getEncodedKey (size_t argKey);
#endif

    // helpers for construction for sending code
    void _appendArg (size_t argKey, int32_t argValue);
    void _appendArg (size_t argKey, int64_t argValue);
    void _appendArg (size_t argKey, size_t argValue);
    void _appendArg (size_t argKey, float argValue);
    void _appendArg (size_t argKey, double argValue);
    void _appendArg (size_t argKey, const char* argValue);
    void _appendArg (size_t argKey, const std::vector<uint8_t>& argValue);
    void _appendArg (size_t argKey, const IPCMessage& argValue);

#if IPC_MESSAGE_USE_CFDICTIONARY
    void _appendEncodedArg (size_t argKey, __attribute__((cf_consumed)) CFTypeRef argObject);
#else
    void _makeWritableIfNeeded ();
    pugi::xml_attribute _appendAttribute (size_t argKey);
#endif

    // helpers for getters for receiving code
    // since we cannot overload by return type, we are returning via reference argument
    void _readArg (size_t argKey, int32_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, int64_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, size_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, float& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, double& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, const char*& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, std::vector<uint8_t>& argValue, bool* didFindKey = nullptr) const;
    void _readArg (size_t argKey, IPCMessage& argValue, bool* didFindKey = nullptr) const;

    static bool _checkKeyFound (bool found, bool* didFindKey)
    {
        if (didFindKey)
        {
            *didFindKey = found;
            return found;
        }
        return true;
    }

private:
#if IPC_MESSAGE_USE_CFDICTIONARY
    CFDictionaryRef _dictionary {};     // if modified, actually converted to a CFMutableDictionaryRef
#else
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
#endif
    bool _isWritable {};
};
