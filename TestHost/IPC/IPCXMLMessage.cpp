//------------------------------------------------------------------------------
//! \file       IPCXMLMessage.cpp
//!             XML-based messaging used for IPC in SDK IPC demo example
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2023, Celemony Software GmbH, All Rights Reserved.
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

#include "IPCXMLMessage.h"
#include "ARA_Library/Debug/ARADebug.h"

#include "3rdParty/cpp-base64/base64.h"

#include <limits>
#include <set>
#include <sstream>
#include <map>


#if defined (__APPLE__)
    _Pragma ("GCC diagnostic push")
    _Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"")
#endif


constexpr auto kRootKey { "msg" };

IPCXMLMessage& IPCXMLMessage::operator= (const IPCXMLMessage& other)
{
    if (other._isWritable)
    {
        _dictionary.reset (new pugi::xml_document);
        _root = _dictionary->append_copy (other._root);
        _root.set_name (kRootKey);
        _isWritable = true;
    }
    else
    {
        _dictionary = other._dictionary;
        _root = other._root;
        _isWritable = false;
    }

    return *this;
}

IPCXMLMessage& IPCXMLMessage::operator= (IPCXMLMessage&& other) noexcept
{
    std::swap (_dictionary, other._dictionary);
    std::swap (_root, other._root);
    std::swap (_isWritable, other._isWritable);
    return *this;
}

const char* IPCXMLMessage::_getEncodedKey (const MessageKey argKey)
{
    ARA_INTERNAL_ASSERT (argKey >= 0);
    static std::map<MessageKey, std::string> cache;
    auto existingEntry { cache.find (argKey) };
    if (existingEntry != cache.end ())
        return existingEntry->second.c_str ();
    // \todo pugixml ignores attributes with only numbers as keys - bug or feature? for now, we just prepend an underscore.
    return cache.emplace (argKey, std::string { "_"} + std::to_string (argKey)).first->second.c_str ();
}

#if defined (__APPLE__)
IPCXMLMessage::IPCXMLMessage (CFDataRef dataObject)
#else
IPCXMLMessage::IPCXMLMessage (const char* data, const size_t dataSize)
#endif
{
    _dictionary.reset (new pugi::xml_document);
#if defined (__APPLE__)
    const auto data { CFDataGetBytePtr (dataObject) };
    const auto dataSize { (size_t) CFDataGetLength (dataObject) };
#endif
    _dictionary->load_buffer (data, dataSize, pugi::parse_minimal | pugi::parse_escapes, pugi::encoding_utf8);
    _root = _dictionary->child (kRootKey);
    _isWritable = false;
}

void IPCXMLMessage::appendInt32 (const MessageKey argKey, const int32_t argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendInt64 (const MessageKey argKey, const int64_t argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendSize (const MessageKey argKey, const size_t argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendFloat (const MessageKey argKey, const float argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendDouble (const MessageKey argKey, const double argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendString (const MessageKey argKey, const char* const argValue)
{
    _appendAttribute (argKey).set_value (argValue);
}

void IPCXMLMessage::appendBytes (const MessageKey argKey, const uint8_t* argValue, const size_t argSize, const bool /*copy*/)
{
    const auto encodedData { base64_encode (argValue, argSize, false) };
    _appendAttribute (argKey).set_value (encodedData.c_str ());
}

pugi::xml_attribute IPCXMLMessage::_appendAttribute (const MessageKey argKey)
{
    ARA_INTERNAL_ASSERT (argKey >= 0);

    _makeWritableIfNeeded ();

    return _root.append_attribute (_getEncodedKey (argKey));
}

IPCXMLMessage* IPCXMLMessage::appendSubMessage (const MessageKey argKey)
{
    ARA_INTERNAL_ASSERT (argKey >= 0);

    _makeWritableIfNeeded ();

    auto result { new IPCXMLMessage };
    result->_dictionary = _dictionary;
    result->_root = _root.append_child (_getEncodedKey (argKey));
    result->_isWritable = true;
    return result;
}

void IPCXMLMessage::_makeWritableIfNeeded ()
{
    if (_isWritable)
        return;

    pugi::xml_document* dictionary { new pugi::xml_document };
    if (_root.empty ())
    {
        _root = dictionary->append_child (kRootKey);
    }
    else
    {
        _root = dictionary->append_copy (_root);
        _root.set_name (kRootKey);
    }
    _dictionary.reset (dictionary);
    _isWritable = true;
}

#if defined (__APPLE__)
__attribute__((cf_returns_retained)) CFDataRef IPCXMLMessage::createEncodedMessage () const
#else
std::string IPCXMLMessage::createEncodedMessage () const
#endif
{
    if (_root.empty ())
#if defined (__APPLE__)
        return nullptr;
#else
        return {};
#endif

    auto dictionary { _dictionary };
    if (_root != _dictionary->child (kRootKey))
    {
        dictionary.reset (new pugi::xml_document);
        dictionary->append_child (kRootKey).append_copy (_root);
    }

    std::ostringstream writer;
    dictionary->save (writer, "", pugi::format_raw | pugi::format_no_declaration);

#if defined (__APPLE__)
    auto result { CFDataCreate (kCFAllocatorDefault, (const UInt8*) writer.str ().c_str (), (CFIndex) writer.str ().size ()) };
    ARA_INTERNAL_ASSERT (result);
    return result;
#else
    return writer.str ();
#endif
}

bool IPCXMLMessage::isEmpty () const
{
    return _root.empty ();
}

bool IPCXMLMessage::readInt32 (const MessageKey argKey, int32_t& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = 0;
        return false;
    }
    static_assert (sizeof (int) >= sizeof (int32_t), "integer type needs adjustment for this compiler setup");
    argValue = attribute.as_int ();
    return true;
}

bool IPCXMLMessage::readInt64 (const MessageKey argKey, int64_t& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = 0;
        return false;
    }
    static_assert (sizeof (long long) >= sizeof (int64_t), "integer type needs adjustment for this compiler setup");
    argValue = attribute.as_llong ();
    return true;
}

bool IPCXMLMessage::readSize (const MessageKey argKey, size_t& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = 0U;
        return false;
    }
    static_assert (sizeof (unsigned long long) >= sizeof (size_t), "integer type needs adjustment for this compiler setup");
    argValue = attribute.as_ullong ();
    return true;
}

bool IPCXMLMessage::readFloat (const MessageKey argKey, float& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = 0.0f;
        return false;
    }
    argValue = attribute.as_float ();
    return true;
}

bool IPCXMLMessage::readDouble (const MessageKey argKey, double& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = 0.0;
        return false;
    }
    argValue = attribute.as_double ();
    return true;
}

bool IPCXMLMessage::readString (const MessageKey argKey, const char*& argValue) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argValue = nullptr;
        return false;
    }
    argValue = attribute.as_string ();
    return true;
}

bool IPCXMLMessage::readBytesSize (const MessageKey argKey, size_t& argSize) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    if (attribute.empty ())
    {
        argSize = 0;
        return false;
    }
    _bytesCacheKey = argKey;
#if __cplusplus >= 201703L
    _bytesCacheData = base64_decode (std::string_view { attribute.as_string () }, false);
#else
    _bytesCacheData = base64_decode (std::string { attribute.as_string () }, false);
#endif
    argSize = _bytesCacheData.size ();
    return true;
}

void IPCXMLMessage::readBytes (const MessageKey argKey, uint8_t* const argValue) const
{
    if (argKey == _bytesCacheKey)
        std::memcpy (argValue, _bytesCacheData.data (), _bytesCacheData.size ());

    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto attribute { _root.attribute (_getEncodedKey (argKey)) };
    ARA_INTERNAL_ASSERT (!attribute.empty ());

#if __cplusplus >= 201703L
    const auto decodedData { base64_decode (std::string_view { attribute.as_string () }, false) };
#else
    const auto decodedData { base64_decode (std::string { attribute.as_string () }, false) };
#endif
    std::memcpy (argValue, decodedData.c_str (), decodedData.size ());
}

IPCXMLMessage* IPCXMLMessage::readSubMessage (const MessageKey argKey) const
{
    ARA_INTERNAL_ASSERT (!_root.empty ());
    const auto child { _root.child (_getEncodedKey (argKey)) };
    if (child.empty ())
        return nullptr;

    auto result { new IPCXMLMessage };
    result->_dictionary = _dictionary;
    result->_root = child;
    result->_isWritable = false;
    return result;;
}

#if defined (__APPLE__)
    _Pragma ("GCC diagnostic pop")
#endif
