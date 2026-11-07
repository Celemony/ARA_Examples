//------------------------------------------------------------------------------
//! \file       IPCXMLEncoding.cpp
//!             Proof-of-concept pugixml-based implementation of ARAIPCMessageEn-/Decoder
//!             for the ARA SDK TestHost (error handling is limited to assertions).
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2026, Celemony Software GmbH, All Rights Reserved.
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

#include "IPCXMLEncoding.h"
#include "ARA_Library/Debug/ARADebug.h"

#include "3rdParty/cpp-base64/base64.h"

#include <limits>
#include <set>
#include <sstream>
#include <map>


IPCXMLMessage::IPCXMLMessage ()
{
    _dictionary.reset (new pugi::xml_document);
    _root = *_dictionary;
}

IPCXMLMessage::IPCXMLMessage (const char* data, const size_t dataSize)
{
    _dictionary.reset (new pugi::xml_document);
    _dictionary->load_buffer (data, dataSize, pugi::parse_minimal | pugi::parse_escapes, pugi::encoding_utf8);
    _root = *_dictionary;

    ARA_INTERNAL_ASSERT (_root.type () == pugi::node_document);
}

IPCXMLMessage::IPCXMLMessage (std::shared_ptr<pugi::xml_document> dictionary, pugi::xml_node root)
: _dictionary { std::move (dictionary) },
  _root { root }
{}

void IPCXMLMessageEncoder::appendInt32 (const int32_t argValue)
{
    _appendAttribute ("int32").set_value (argValue);
}

void IPCXMLMessageEncoder::appendInt64 (const int64_t argValue)
{
    _appendAttribute ("int64").set_value (argValue);
}

void IPCXMLMessageEncoder::appendSize (const size_t argValue)
{
    _appendAttribute ("size").set_value (argValue);
}

void IPCXMLMessageEncoder::appendFloat (const float argValue)
{
    _appendAttribute ("float").set_value (argValue);
}

void IPCXMLMessageEncoder::appendDouble (const double argValue)
{
    _appendAttribute ("double").set_value (argValue);
}

void IPCXMLMessageEncoder::appendString (const char* const argValue)
{
    ARA_INTERNAL_ASSERT (argValue);
    _appendAttribute ("string").set_value (argValue);
}

void IPCXMLMessageEncoder::appendBytes (const uint8_t* argValue, const size_t argSize, const bool /*copy*/)
{
    ARA_INTERNAL_ASSERT ((argValue != nullptr) || (argSize == 0));
    const auto encodedData { base64_encode (argValue, argSize, false) };
    _appendAttribute ("bytes").set_value (encodedData.c_str ());
}

pugi::xml_attribute IPCXMLMessageEncoder::_appendAttribute (const char* attributeName)
{
    return _root.append_child ().append_attribute (attributeName);
}

void IPCXMLMessageEncoder::appendSubMessage (std::unique_ptr<MessageEncoder>&& argValue)
{
    const auto subMessage { dynamic_cast<IPCXMLMessageEncoder*> (argValue.release ()) };
    ARA_INTERNAL_ASSERT (subMessage != nullptr);

    for (const auto& node : subMessage->_root)
        _root.append_copy (node);

    delete subMessage;
}

std::unique_ptr<ARA::IPC::MessageEncoder> IPCXMLMessageEncoder::createEncoder ()
{
    return IPCXMLMessageEncoder::create ();
}

#if defined (__APPLE__)
__attribute__((cf_returns_retained)) CFDataRef IPCXMLMessageEncoder::createEncodedMessage () const
#else
std::string IPCXMLMessageEncoder::createEncodedMessage () const
#endif
{
    if (!_root.first_attribute () &&    // empty () does not work here because the name "msg" will be set
        !_root.first_child ())
#if defined (__APPLE__)
        return nullptr;
#else
        return {};
#endif

    std::ostringstream writer;
    _dictionary->save (writer, "", pugi::format_raw | pugi::format_no_declaration);

#if defined (__APPLE__)
    auto result { CFDataCreate (kCFAllocatorDefault, reinterpret_cast<const UInt8*> (writer.str ().c_str ()),
                                static_cast<CFIndex> (writer.str ().size ())) };
    ARA_INTERNAL_ASSERT (result);
    return result;
#else
    return writer.str ();
#endif
}


#if defined (__APPLE__)
std::unique_ptr<IPCXMLMessageDecoder> IPCXMLMessageDecoder::createWithMessageData (CFDataRef data)
{
    const auto dataSize { static_cast<size_t> (CFDataGetLength (data)) };
    if (dataSize == 0)
        return nullptr;

    return std::unique_ptr<IPCXMLMessageDecoder> { new IPCXMLMessageDecoder { reinterpret_cast<const char *> (CFDataGetBytePtr (data)), dataSize } };
}
#else
std::unique_ptr<IPCXMLMessageDecoder> IPCXMLMessageDecoder::createWithMessageData (const char* data, const size_t dataSize)
{
    if (dataSize == 0)
        return nullptr;

    return std::unique_ptr<IPCXMLMessageDecoder> { new IPCXMLMessageDecoder { data, dataSize } };
}
#endif

template <typename ArgType, typename XMLType>
bool IPCXMLMessageDecoder::_readValue (ArgType& argValue, const char* attributeName, XMLType (pugi::xml_attribute::* getter) (XMLType) const) const
{
    argValue = ArgType {};

    ARA_INTERNAL_ASSERT (!_root.empty ());
    if (_node_iterator == _root.end ())
        return false;

    const auto attribute { _node_iterator++->attribute (attributeName) };
    if (attribute.empty ())
        return false;
    
    argValue = (attribute.*getter) (ArgType {});
    return true;
}

bool IPCXMLMessageDecoder::readInt32 (int32_t& argValue) const
{
    static_assert (sizeof (int) >= sizeof (int32_t), "integer type needs adjustment for this compiler setup");
    return _readValue (argValue, "int32", &pugi::xml_attribute::as_int);
}

bool IPCXMLMessageDecoder::readInt64 (int64_t& argValue) const
{
    static_assert (sizeof (long long) >= sizeof (int64_t), "integer type needs adjustment for this compiler setup");
    return _readValue (argValue, "int64", &pugi::xml_attribute::as_llong);
}

bool IPCXMLMessageDecoder::readSize (size_t& argValue) const
{
    static_assert (sizeof (unsigned long long) >= sizeof (size_t), "integer type needs adjustment for this compiler setup");
    return _readValue (argValue, "size", &pugi::xml_attribute::as_ullong);
}

bool IPCXMLMessageDecoder::readFloat (float& argValue) const
{
    return _readValue (argValue, "float", &pugi::xml_attribute::as_float);
}

bool IPCXMLMessageDecoder::readDouble (double& argValue) const
{
    return _readValue (argValue, "double", &pugi::xml_attribute::as_double);
}

bool IPCXMLMessageDecoder::readString (const char*& argValue) const
{
    return _readValue (argValue, "string", &pugi::xml_attribute::as_string);
}

bool IPCXMLMessageDecoder::readBytes (const uint8_t*& argValue, size_t& argSize) const
{
    const char* cstr {};
    if (!_readValue (cstr, "bytes", &pugi::xml_attribute::as_string))
    {
        argValue = nullptr;
        argSize = 0;
        return false;
    }

    _decodedBytesCache.emplace_back (base64_decode (std::string_view { cstr }, false));
    // back() does not return return a proper reference in some implementations, need to use rbegin() instead
    //const auto decodedBytes { _decodedBytesCache.back () };
    const auto decodedBytes { _decodedBytesCache.rbegin () };
    ARA_INTERNAL_ASSERT (decodedBytes->c_str () == _decodedBytesCache[_decodedBytesCache.size () - 1].c_str ());
    argValue = reinterpret_cast<const uint8_t*> (decodedBytes->c_str ());
    argSize = decodedBytes->size ();
    return true;
}
