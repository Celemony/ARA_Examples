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


IPCMessage::IPCMessage (CFDictionaryRef dictionary)
    : _dictionary { dictionary }
{}

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
    this->~IPCMessage ();    // cleanup current data by calling our d'tor
    _dictionary = (other._dictionary) ? (CFDictionaryRef) CFRetain (other._dictionary) : nullptr;   // since _dictionary is immutable after creation, we can reference it instead of copy
    return *this;
}

IPCMessage& IPCMessage::operator= (IPCMessage&& other) noexcept
{
    std::swap (_dictionary, other._dictionary);
    return *this;
}

IPCMessage::~IPCMessage ()
{
    if (_dictionary)
        CFRelease (_dictionary);
}

CFStringRef IPCMessage::_createEncodedTag (const char* tag)
{
    auto result { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, tag, kCFStringEncodingASCII,  kCFAllocatorNull) };
    ARA_INTERNAL_ASSERT (result);
    return result;
}

IPCMessage::IPCMessage (const char* messageID)
    : IPCMessage { CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks) }
{
    ARA_INTERNAL_ASSERT (messageID != nullptr);
    auto msgID { _createEncodedTag (messageID) };
    CFDictionarySetValue ((CFMutableDictionaryRef) _dictionary, CFSTR ("messageID"), msgID);
    CFRelease (msgID);
}

IPCMessage::IPCMessage (CFDataRef data)
    : IPCMessage { (CFDictionaryRef) CFPropertyListCreateWithData (kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr) }
{
    ARA_INTERNAL_ASSERT (_dictionary && (CFGetTypeID (_dictionary) == CFDictionaryGetTypeID ()));
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

void IPCMessage::_appendBytesArg (const char* argKey, const void* valueBytes, size_t valueSize)
{
    _appendEncodedArg (argKey, CFDataCreate (kCFAllocatorDefault, (const UInt8 *) valueBytes, (CFIndex) valueSize));
}

void IPCMessage::_appendArg (const char* argKey, const IPCMessage& argValue)
{
    _appendEncodedArg (argKey, (CFDictionaryRef) CFRetain (argValue._dictionary));  // since _dictionary is immutable after creation, we can reference it instead of copy
}

void IPCMessage::_appendEncodedArg (const char* argKey, CFTypeRef argObject)
{
    ARA_INTERNAL_ASSERT (argObject);
    auto keyObject { _createEncodedTag (argKey) };
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

bool IPCMessage::isMessageWithID (const char* messageID) const
{
    if (!_dictionary)
        return false;
    auto string { (CFStringRef) CFDictionaryGetValue (_dictionary, CFSTR ("messageID")) };
    ARA_INTERNAL_ASSERT (string && (CFGetTypeID (string) == CFStringGetTypeID ()));
    auto msgID { _createEncodedTag (messageID) };
    bool result { (CFStringCompare (string, msgID, 0) == kCFCompareEqualTo) };
    CFRelease (msgID);
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

void IPCMessage::_readArg (const char* argKey, std::string& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    argValue.clear ();
    auto key { _createEncodedTag (argKey) };
    auto string { (CFStringRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (string && (CFGetTypeID (string) == CFStringGetTypeID ()));
    const auto length { CFStringGetLength (string) };
    argValue.resize (static_cast<size_t> (length), 0);
    CFStringGetCString (string, (char *) argValue.c_str (), length + 1, kCFStringEncodingUTF8);
    CFRelease (key);
}

size_t IPCMessage::_readArrayArgCount (const char* argKey, size_t valueSize) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto bytes { (CFDataRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (bytes && (CFGetTypeID (bytes) == CFDataGetTypeID ()));
    const auto length { (size_t) CFDataGetLength (bytes) };
    ARA_INTERNAL_ASSERT (length % valueSize == 0);
    CFRelease (key);
    return length / valueSize;
}

void IPCMessage::_readArrayArgData (const char* argKey, void* data) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto bytes { (CFDataRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (bytes && (CFGetTypeID (bytes) == CFDataGetTypeID ()));
    const auto length { CFDataGetLength (bytes) };
    CFDataGetBytes (bytes, CFRangeMake (0, length), (UInt8*) data);
    CFRelease (key);
}

void IPCMessage::_readArg (const char* argKey, IPCMessage& argValue) const
{
    ARA_INTERNAL_ASSERT (_dictionary);
    auto key { _createEncodedTag (argKey) };
    auto dictionary { (CFDictionaryRef) CFDictionaryGetValue (_dictionary, key) };
    ARA_INTERNAL_ASSERT (_dictionary && (CFGetTypeID (_dictionary) == CFDictionaryGetTypeID ()));
    argValue = IPCMessage { (CFDictionaryRef) CFRetain (dictionary) };   // since _dictionary is immutable after creation, we can reference it instead of copy
    CFRelease (key);
}
