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
// This is a brief proof-of-concept demo that hooks up an ARA capable plug-in
// in a separate process using IPC.
// This educational example is not suitable for production code -
// see MainProcess.cpp for a list of issues.
//------------------------------------------------------------------------------

#pragma once

#include <vector>

#if defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#else
    #error "only implemented for macOS at this point"
#endif


// Private helper template for IPCMessage, mapping the data types to the message-internal representation.
// The default mapping is mere forwarding, but pointers must be converted to size_t except for C strings.
template<typename ArgType>
struct _ArgWrapper
    { using WrappedType = ArgType; };
template<>
struct _ArgWrapper<const char*>
    { using WrappedType = const char*; };
template<typename StructT>
struct _ArgWrapper<StructT *>
    { using WrappedType = size_t; };


// A simple proof-of-concept wrapper for the IPC messages sent back and forth in the ARA IPC example.
// Error handling is limited to assertions.
// The basic data types transmitted are only int32_t, int64_t, size_t, float, double and C strings
// (pure ASCII or UTF8 - note that received string pointers are only valid as long as the message
// that provided them is alive). Pointers are encoded as size_t - they may not be traversed on the
// receiving side, but they may be passed back to the sender to be used in callbacks.
// Types can be aggregated in homogenous arrays (std::vector), and messages can be nested in a hierarchy.
// The transmission channel handles proper endianness conversion of the numbers if needed.
// Pointer size is currently limited to 64 bit - if either size had smaller pointers, some additional
// infrastructure would be needed to allocate a unique 32 bit representation for each ref provided
// by the 64 bit process to the 32 bit process, and then map between the two.
class IPCMessage
{
public:
    IPCMessage (const IPCMessage& other);
    IPCMessage (IPCMessage&& other) noexcept;
    IPCMessage& operator= (const IPCMessage& other);
    IPCMessage& operator= (IPCMessage&& other)  noexcept;
    ~IPCMessage ();

    // to be used by IPCPort only: encoding from/to port-internal datas format
    explicit IPCMessage (CFDataRef data);
    CFDataRef createEncodedMessage () const;

    // construction with arguments for sending code with 0..n key/value pairs of the message arguments
    IPCMessage () = default;
    template <typename ArgT, typename... MoreArgs>
    IPCMessage (const char* argKey, ArgT argValue, MoreArgs... moreArgs)
        : IPCMessage (moreArgs...)
    {
        _appendArg (argKey, (typename _ArgWrapper<ArgT>::WrappedType) argValue);
    }

    // appending arguments to an existing message
    template <typename ArgT, typename... MoreArgs>
    void append (const char* argKey, ArgT argValue, MoreArgs... moreArgs)
    {
        _appendArg (argKey, (typename _ArgWrapper<ArgT>::WrappedType) argValue);
        append (moreArgs...);
    }
    void append ()
    {
    }

    // getters for receiving code: extract arguments, specifying the desired return type as template argument
    // the optional variant returns the default value of the argument type if key wasn't not found
    template <typename ArgT>
    ArgT getArgValue (const char* argKey) const
    {
        typename _ArgWrapper<ArgT>::WrappedType result;
        _readArg (argKey, result);
        return (ArgT) result;
    }
    template <typename ArgT>
    std::pair<ArgT, bool> getOptionalArgValue (const char* argKey) const
    {
        typename _ArgWrapper<ArgT>::WrappedType result;
        bool didFindKey { false };
        _readArg (argKey, result, &didFindKey);
        return { (ArgT) result, didFindKey };
    }

private:
    // internal primitive - dictionary will be retained or copied, depending on isWritable
    void _setup (CFDictionaryRef dictionary, bool isWritable);

    // wrap C string into CFString - must release result!
    static CFStringRef _createEncodedTag (const char* tag);

    // provide reusable indexing-keys for arrays
    static const char* _getKeyForArrayIndex (size_t index);

    // helpers for construction for sending code
    void _appendArg (const char* argKey, int32_t argValue);
    void _appendArg (const char* argKey, int64_t argValue);
    void _appendArg (const char* argKey, size_t argValue);
    void _appendArg (const char* argKey, float argValue);
    void _appendArg (const char* argKey, double argValue);
    void _appendArg (const char* argKey, const char* argValue);
    void _appendArg (const char* argKey, const std::vector<uint8_t>& argValue);
    template<typename T, typename std::enable_if<!std::is_same<T, uint8_t>::value, bool>::type = true>
    void _appendArg (const char* argKey, const std::vector<T>& argValue)
    {
        IPCMessage msg;
        msg._appendArg ("count", static_cast<int64_t> (argValue.size ()));
        for (auto i { 0u }; i < argValue.size (); ++i)
            msg._appendArg (_getKeyForArrayIndex (i), argValue[i]);
        _appendArg (argKey, msg);
    }
    void _appendArg (const char* argKey, const IPCMessage& argValue);
    void _appendEncodedArg (const char* argKey, CFTypeRef argObject);   // ownership of the arg is transferred

    // helpers for getters for receiving code
    // since we cannot overload by return type, we are returning via reference argument
    void _readArg (const char* argKey, int32_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, int64_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, size_t& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, float& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, double& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, const char*& argValue, bool* didFindKey = nullptr) const;
    void _readArg (const char* argKey, std::vector<uint8_t>& argValue, bool* didFindKey = nullptr) const;
    template<typename T, typename std::enable_if<!std::is_same<T, uint8_t>::value, bool>::type = true>
    void _readArg (const char* argKey, std::vector<T>& argValue, bool* didFindKey = nullptr) const
    {
        argValue.clear ();
        IPCMessage msg;
        _readArg (argKey, msg, didFindKey);
        if (didFindKey && !*didFindKey)
            return;
        size_t count;
        msg._readArg ("count", count);
        argValue.reserve (count);
        for (size_t i { 0 }; i < count; ++i)
        {
            T value;
            msg._readArg (_getKeyForArrayIndex (i), value);
            argValue.push_back (value);
        }
    }
    void _readArg (const char* argKey, IPCMessage& argValue, bool* didFindKey = nullptr) const;
    static bool _checkKeyFound (bool found, bool* didFindKey);

private:
    CFDictionaryRef _dictionary {};   // if constructed for sending, actually a CFMutableDictionaryRef
    bool _isWritable {};
};
