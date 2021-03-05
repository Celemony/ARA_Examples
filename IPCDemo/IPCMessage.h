//------------------------------------------------------------------------------
//! \file       IPCMessage.h
//!             messaging used for IPC in SDK IPC demo example
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2021, Celemony Software GmbH, All Rights Reserved.
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

#include <string>
#include <vector>

#if defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#else
    #error "only implemented for Mac at this point"
#endif


// Private helper template for IPCMessage, mapping the data types to the message-internal representation.
// The default mapping is mere forwarding, but pointers must be converted to size_t except for C strings.
template<typename ArgType>
struct _ArgWrapper
    { static ArgType wrap (ArgType arg) { return arg; } };
template<>
struct _ArgWrapper<const char*>
    { static const char* wrap (const char* arg) { return arg; } };
template<typename StructT>
struct _ArgWrapper<StructT *>
    { static size_t wrap (const StructT* arg) { return (size_t) arg; } };


// A simple proof-of-concept wrapper for the IPC messages sent back and forth in the ARA IPC example.
// Error handling is limited to assertions.
// The basic data types transmitted are only int32_t, int64_t, size_t, float, double and C or C++
// std strings (pure ASCII or UTF8). Pointers are encoded as size_t, which means they may not be
// traversed on the receiving side, but they may be passed back to the sender to be used in callbacks.
// Number types can be aggregated in homogenous arrays (std::vector), and messages can be nested
// in a hierarchy.
// The transmission channel handles proper endianess conversion of the numbers if needed.
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

    // construction with arguments for sending code
    // the first argument is the message ID, followed by key/value pairs of the message arguments
    explicit IPCMessage (const char* messageID);
    template <typename ArgT, typename... MoreArgs>
    IPCMessage (const char* messageID, const char* argKey, ArgT argValue, MoreArgs... moreArgs)
        : IPCMessage (messageID, moreArgs...)
    {
        _appendArg (argKey, _ArgWrapper<ArgT>::wrap (argValue));
    }

    // default c'tor used for empty "no reply" message
    IPCMessage () = default;

    // getter for receiving code: identify message
    bool isMessageWithID (const char* messageID) const;

    // getters for receiving code: extract argument after checking isMessageWithID()
    // specify desired return type as template argument
    template <typename ArgT>
    ArgT getArgValue (const char* argKey) const
    {
        decltype (_ArgWrapper<ArgT>::wrap (ArgT {})) result;
        _readArg (argKey, result);
        return (ArgT) result;
    }

private:
    // internal primitev c'tor - ownership of the dictionary is transferred to the message.
    explicit IPCMessage (CFDictionaryRef dictionary);

    // wrap C string into CFString - must release result!
    static CFStringRef _createEncodedTag (const char* tag);

    // helpers for construction for sending code
    void _appendArg (const char* argKey, int32_t argValue);
    void _appendArg (const char* argKey, int64_t argValue);
    void _appendArg (const char* argKey, size_t argValue);
    void _appendArg (const char* argKey, float argValue);
    void _appendArg (const char* argKey, double argValue);
    void _appendArg (const char* argKey, const char* argValue);
    template<typename T>
    void _appendArg (const char* argKey, const std::vector<T> argValue)
    {
        _appendBytesArg (argKey, argValue.data (), argValue.size () * sizeof (*argValue.data ()));
    }
    void _appendBytesArg (const char* argKey, const void* valueBytes, size_t valueSize);
    void _appendArg (const char* argKey, const IPCMessage& argValue);
    void _appendEncodedArg (const char* argKey, CFTypeRef argObject);   // ownership of the arg is transferred

    // helpers for getters for receiving code
    // since we cannot overload by return type, we are returning via reference argument
    void _readArg (const char* argKey, int32_t& argValue) const;
    void _readArg (const char* argKey, int64_t& argValue) const;
    void _readArg (const char* argKey, size_t& argValue) const;
    void _readArg (const char* argKey, float& argValue) const;
    void _readArg (const char* argKey, double& argValue) const;
    void _readArg (const char* argKey, std::string& argValue) const;
    template<typename T>
    void _readArg (const char* argKey, std::vector<T>& argValue) const
    {
        argValue.clear();
        argValue.resize (_readArrayArgCount (argKey, sizeof (*argValue.data ())));
        _readArrayArgData (argKey, argValue.data ());
    }
    size_t _readArrayArgCount (const char* argKey, size_t valueSize) const;
    void _readArrayArgData (const char* argKey, void* data) const;
    void _readArg (const char* argKey, IPCMessage& argValue) const;

private:
    CFDictionaryRef _dictionary {};   // if constructed for sending, actually a CFMutableDictionaryRef
};
