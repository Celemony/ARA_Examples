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

#include "IPCMessage.h"
#include "ARA_Library/Debug/ARADebug.h"

#include <limits>
#include <string>
#include <set>
#include <map>

_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"")

class _CFReleaser
{
public:
    explicit _CFReleaser (CFStringRef ref) : _ref { ref } {}
    _CFReleaser (const _CFReleaser& other) { _ref = (CFStringRef) CFRetain (other._ref); }
    _CFReleaser (_CFReleaser&& other) { _ref = other._ref; other._ref = CFStringRef {}; }
    ~_CFReleaser () { CFRelease (_ref); }
    operator CFStringRef () { return _ref; }
private:
    CFStringRef _ref;
};

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

CFStringRef IPCMessage::_getEncodedKey (size_t argKey)
{
    // \todo All plist formats available for CFPropertyListCreateData () in createEncodedMessage () need CFString keys.
    //       Once we switch to the more modern (NS)XPC API we shall be able to use CFNumber keys directly...
    static std::map<size_t, _CFReleaser> cache;
    auto existingEntry { cache.find (argKey) };
    if (existingEntry != cache.end ())
        return existingEntry->second;
    return  cache.emplace (argKey, CFStringCreateWithCString (kCFAllocatorDefault, std::to_string (argKey).c_str (), kCFStringEncodingUTF8)).first->second;
}

IPCMessage::IPCMessage (CFDataRef data)
{
    if (auto dictionary { (CFDictionaryRef) CFPropertyListCreateWithData (kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr) })
    {
        ARA_INTERNAL_ASSERT (dictionary && (CFGetTypeID (dictionary) == CFDictionaryGetTypeID ()));
        _setup (dictionary, false);
        CFRelease (dictionary);
    }
}

void IPCMessage::_appendArg (size_t argKey, int32_t argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt32Type, &argValue));
}

void IPCMessage::_appendArg (size_t argKey, int64_t argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt64Type, &argValue));
}

void IPCMessage::_appendArg (size_t argKey, size_t argValue)
{
    static_assert (sizeof (SInt64) == sizeof (size_t), "currently only implemented for 64 bit");
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberSInt64Type, &argValue));
}

void IPCMessage::_appendArg (size_t argKey, float argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberFloatType, &argValue));
}

void IPCMessage::_appendArg (size_t argKey, double argValue)
{
    _appendEncodedArg (argKey, CFNumberCreate (kCFAllocatorDefault, kCFNumberDoubleType, &argValue));
}

void IPCMessage::_appendArg (size_t argKey, const char* argValue)
{
    _appendEncodedArg (argKey, CFStringCreateWithCString (kCFAllocatorDefault, argValue, kCFStringEncodingUTF8));
}

void IPCMessage::_appendArg (size_t argKey, const std::vector<uint8_t>& argValue)
{
    _appendEncodedArg (argKey, CFDataCreate (kCFAllocatorDefault, argValue.data (), (CFIndex) argValue.size ()));
}

void IPCMessage::_appendArg (size_t argKey, const IPCMessage& argValue)
{
    _appendEncodedArg (argKey, (CFDictionaryRef) CFRetain (argValue._dictionary));  // since _dictionary is immutable after creation, we can reference it instead of copy
}

void IPCMessage::_appendEncodedArg (size_t argKey, CFTypeRef argObject)
{
    ARA_INTERNAL_ASSERT (argObject);
    if (!_isWritable && _dictionary)
    {
        auto old = _dictionary;
        _dictionary = CFDictionaryCreateMutableCopy (kCFAllocatorDefault, 0, _dictionary);
        CFRelease (old);
    }
    _isWritable = true;
    if (!_dictionary)
        _dictionary = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue ((CFMutableDictionaryRef) _dictionary, _getEncodedKey (argKey), argObject);
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

void IPCMessage::_readArg (size_t argKey, int32_t& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (number != nullptr, didFindKey))
    {
        argValue = 0;
        return;
    }
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt32Type, &argValue);
}

void IPCMessage::_readArg (size_t argKey, int64_t& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (number != nullptr, didFindKey))
    {
        argValue = 0;
        return;
    }
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt64Type, &argValue);
}

void IPCMessage::_readArg (size_t argKey, size_t& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    static_assert (sizeof (SInt64) == sizeof (size_t), "currently only implemented for 64 bit");
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (number != nullptr, didFindKey))
    {
        argValue = 0;
        return;
    }
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberSInt64Type, &argValue);
}

void IPCMessage::_readArg (size_t argKey, float& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (number != nullptr, didFindKey))
    {
        argValue = 0.0f;
        return;
    }
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberFloatType, &argValue);
}

void IPCMessage::_readArg (size_t argKey, double& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto number { (CFNumberRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (number != nullptr, didFindKey))
    {
        argValue = 0.0;
        return;
    }
    ARA_INTERNAL_ASSERT (number && (CFGetTypeID (number) == CFNumberGetTypeID ()));
    CFNumberGetValue (number, kCFNumberDoubleType, &argValue);
}

void IPCMessage::_readArg (size_t argKey, const char*& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto string { (CFStringRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (string != nullptr, didFindKey))
    {
        argValue = nullptr;
        return;
    }
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
}

void IPCMessage::_readArg (size_t argKey, std::vector<uint8_t>& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto bytes { (CFDataRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (bytes != nullptr, didFindKey))
    {
        argValue.clear ();
        return;
    }
    ARA_INTERNAL_ASSERT (bytes && (CFGetTypeID (bytes) == CFDataGetTypeID ()));
    const auto length { CFDataGetLength (bytes) };
    argValue.resize (static_cast<size_t> (length));
    CFDataGetBytes (bytes, CFRangeMake (0, length), argValue.data ());
}

void IPCMessage::_readArg (size_t argKey, IPCMessage& argValue, bool* didFindKey) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto dictionary { (CFDictionaryRef) CFDictionaryGetValue (_dictionary, _getEncodedKey (argKey)) };
    if (!_checkKeyFound (dictionary != nullptr, didFindKey))
    {
        argValue = {};
        return;
    }
    ARA_INTERNAL_ASSERT (dictionary && (CFGetTypeID (dictionary) == CFDictionaryGetTypeID ()));
    argValue._setup (dictionary, false);
}

bool IPCMessage::_checkKeyFound (bool found, bool* didFindKey)
{
    if (didFindKey)
    {
        *didFindKey = found;
        return found;
    }
    return true;
}

_Pragma ("GCC diagnostic pop")
