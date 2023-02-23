//------------------------------------------------------------------------------
//! \file       IPCMessage.cpp
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

#include "IPCMessage.h"
#include "ARA_Library/Debug/ARADebug.h"

#include <string>
#include <set>

void IPCMessage::_setup (CFDictionaryRef dictionary, bool isWritable)
{
    if (_dictionary)
        CFRelease (_dictionary);

    if (!dictionary)
        _dictionary = nullptr;
    else if (!isWritable)
        _dictionary = (CFDictionaryRef) CFRetain (dictionary);   // we can reference immutable dictionaries instead of copying
    else
        _dictionary = CFDictionaryCreateMutableCopy (kCFAllocatorDefault, 0, dictionary);
    _isWritable = isWritable;
}

IPCMessage::IPCMessage (const IPCMessage& other)
{
    *this = other;
}

IPCMessage::IPCMessage (IPCMessage&& other) noexcept
{
    *this = std::move (other);
}

IPCMessage& IPCMessage::operator= (const IPCMessage& other)
{
    _setup (other._dictionary, other._isWritable);
    return *this;
}

IPCMessage& IPCMessage::operator= (IPCMessage&& other) noexcept
{
    std::swap (_dictionary, other._dictionary);
// other will be cleaned up properly regardless of _isWritable, so we only need copy not swap
//  std::swap (_isWritable, other._isWritable);
    _isWritable = other._isWritable;
    return *this;
}

IPCMessage::~IPCMessage ()
{
    _setup (nullptr, false);
}

CFStringRef IPCMessage::_createEncodedTag (const char* tag)
{
    auto result { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, tag, kCFStringEncodingASCII,  kCFAllocatorNull) };
    ARA_INTERNAL_ASSERT (result);
    return result;
}

const char* IPCMessage::_getKeyForArrayIndex (size_t index)
{
    static std::vector<std::string> cache;
    for (auto i = cache.size (); i <= index; ++i)
        cache.emplace_back (std::to_string (i));
    return cache[index].c_str ();
}

IPCMessage::IPCMessage (CFDataRef data)
{
    auto dictionary { (CFDictionaryRef) CFPropertyListCreateWithData (kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr) };
    ARA_INTERNAL_ASSERT (dictionary && (CFGetTypeID (dictionary) == CFDictionaryGetTypeID ()));
    _setup (dictionary, false);
    CFRelease (dictionary);
}

void IPCMessage::_appendArg (const char* argKey, int32_t argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &argValue));
}

void IPCMessage::_appendArg (const char* argKey, int64_t argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt64Type, &argValue));
}

void IPCMessage::_appendArg (const char* argKey, size_t argValue)
{
    static_assert (sizeof (SInt64) == sizeof (size_t), "currently only implemented for 64 bit");
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt64Type, &argValue));
}

void IPCMessage::_appendArg (const char* argKey, float argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberFloatType, &argValue));
}

void IPCMessage::_appendArg (const char* argKey, double argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberDoubleType, &argValue));
}

void IPCMessage::_appendArg (const char* argKey, const char* argValue)
{
    _appendEncodedArg (argKey, CFStringCreateWithCString (kCFAllocatorDefault, argValue, kCFStringEncodingUTF8));
}

void IPCMessage::_appendArg (const char* argKey, const std::vector<uint8_t>& argValue)
{
    _appendEncodedArg (argKey, CFDataCreate (kCFAllocatorDefault, argValue.data (), (CFIndex) argValue.size ()));
}

void IPCMessage::_appendArg (const char* argKey, const IPCMessage& argValue)
{
    _appendEncodedArg (argKey, (CFDictionaryRef) CFRetain (argValue._dictionary));  // since _dictionary is immutable after creation, we can reference it instead of copy
}

void IPCMessage::_appendEncodedArg (const char* argKey, CFTypeRef argObject)
{
    ARA_INTERNAL_ASSERT (argObject);
    auto keyObject { _createEncodedTag (argKey) };
    if (!_isWritable && _dictionary)
    {
        auto old = _dictionary;
        _dictionary = CFDictionaryCreateMutableCopy (kCFAllocatorDefault, 0, _dictionary);
        CFRelease (old);
    }
    _isWritable = true;
    if (!_dictionary)
        _dictionary = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue ((CFMutableDictionaryRef) _dictionary, keyObject, argObject);
    CFRelease (keyObject);
    CFRelease (argObject);
}

CFDataRef IPCMessage::createEncodedMessage () const
{
    if (!_dictionary)
        return nullptr;
    auto result { CFPropertyListCreateData (kCFAllocatorDefault, _dictionary, kCFPropertyListBinaryFormat_v1_0, 0, nullptr) };
    ARA_INTERNAL_ASSERT (result);
    return result;
}

void IPCMessage::_readArg (const char* argKey, int32_t& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt32Type, &argValue);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, int64_t& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt64Type, &argValue);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, size_t& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    static_assert (sizeof (SInt64) == sizeof (size_t), "currently only implemented for 64 bit");
    auto key { _createEncodedTag (argKey) };
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt64Type, &argValue);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, float& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberFloatType, &argValue);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, double& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberDoubleType, &argValue);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, const char*& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto string { (CFStringRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (string && (CFGetTypeID (string) == CFStringGetTypeID ()));
    argValue = CFStringGetCStringPtr (string, kCFStringEncodingUTF8);
    if (!argValue)      // CFStringGetCStringPtr() may fail e.g. with chord names like "G/D"
    {
        const auto length { CFStringGetLength (string) };
        std::string temp;   // \todo does not work: { static_cast<size_t> (length), char { 0 } };
        temp.assign ( static_cast<size_t> (length) , char { 0 } );
        CFIndex ARA_MAYBE_UNUSED_VAR (count) { CFStringGetBytes (string, CFRangeMake (0, length), kCFStringEncodingUTF8, 0, false, (UInt8*)(&temp[0]), length, nullptr) };
        ARA_INTERNAL_ASSERT (count == length);
        static std::set<std::string> strings;   // \todo static cache of "undecodeable" strings requires single-threaded use - maybe make iVar instead?
        strings.insert (temp);
        argValue = strings.find (temp)->c_str ();
    }
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, std::vector<uint8_t>& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto bytes { (CFDataRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (bytes && (CFGetTypeID (bytes) == CFDataGetTypeID ()));
    const auto length { CFDataGetLength (bytes) };
    argValue.resize (static_cast<size_t> (length));
    CFDataGetBytes (bytes, CFRangeMake (0, length), argValue.data ());
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, IPCMessage& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto dictionary { (CFDictionaryRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (_dictionary && (CFGetTypeID (_dictionary) == CFDictionaryGetTypeID ()));
    argValue._setup (dictionary, false);
    CFRelease (key);
}
