//------------------------------------------------------------------------------
//! \file       IPCXMLEncoding.h
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

#pragma once


#include "ARA_Library/IPC/ARAIPCMessage.h"

#if ARA_ENABLE_IPC


#include <memory>
#include <type_traits>
#include <vector>

#if defined (__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#include "3rdParty/pugixml/src/pugixml.hpp"


class IPCXMLMessage
{
protected:
    IPCXMLMessage ();
    IPCXMLMessage (const uint8_t* data, const size_t dataSize);

protected:
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
};


class IPCXMLMessageEncoder : public IPCXMLMessage, public ARA::IPC::MessageEncoder
{
public:
    void appendInt32 (int32_t argValue) override;
    void appendInt64 (int64_t argValue) override;
    void appendSize (size_t argValue) override;
    void appendFloat (float argValue) override;
    void appendDouble (double argValue) override;
    void appendBytes (const uint8_t* argValue, size_t argSize, bool copy) override;

#if defined (__APPLE__)
    __attribute__((cf_returns_retained)) CFDataRef
#else
    std::vector<uint8_t>
#endif
                                                   convertToEncodedData () override;

    static std::unique_ptr<IPCXMLMessageEncoder> create () { return std::unique_ptr<IPCXMLMessageEncoder> { new IPCXMLMessageEncoder {} }; }

private:
    using IPCXMLMessage::IPCXMLMessage;

    pugi::xml_attribute _appendAttribute (const char* attributeName);
};

class IPCXMLMessageDecoder : public IPCXMLMessage, public ARA::IPC::MessageDecoder
{
public:
#if defined (__APPLE__)
    static std::unique_ptr<IPCXMLMessageDecoder> createWithMessageData (CFDataRef data);
#else
    static std::unique_ptr<IPCXMLMessageDecoder> createWithMessageData (const uint8_t* data, const size_t dataSize);
#endif

    bool readInt32 (int32_t& argValue) const override;
    bool readInt64 (int64_t& argValue) const override;
    bool readSize (size_t& argValue) const override;
    bool readFloat (float& argValue) const override;
    bool readDouble (double& argValue) const override;
    bool readBytes (const uint8_t*& argValue, size_t& argSize) const override;

private:
    using IPCXMLMessage::IPCXMLMessage;

    template <typename ArgType, typename XMLType>
    bool _readValue (ArgType& argValue, const char* attributeName, XMLType (pugi::xml_attribute::* getter) (XMLType) const) const;

private:
    mutable pugi::xml_node::iterator _node_iterator { _root.begin () };
    mutable std::vector<std::string> _decodedBytesCache;
};

#endif // ARA_ENABLE_IPC
