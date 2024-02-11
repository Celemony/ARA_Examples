//------------------------------------------------------------------------------
//! \file       IPCXMLEncoding.h
//!             Proof-of-concept pugixml-based implementation of ARAIPCMessageEn-/Decoder
//!             for the ARA SDK TestHost (error handling is limited to assertions).
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2024, Celemony Software GmbH, All Rights Reserved.
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
public:
    using MessageArgumentKey = ARA::IPC::MessageArgumentKey;

protected:
    IPCXMLMessage ();
    IPCXMLMessage (std::shared_ptr<pugi::xml_document> dictionary, pugi::xml_node root);
    IPCXMLMessage (const char* data, const size_t dataSize);

    static const char* _getEncodedKey (const MessageArgumentKey argKey);

protected:
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
};


class IPCXMLMessageEncoder : public IPCXMLMessage, public ARA::IPC::MessageEncoder
{
public:
    IPCXMLMessageEncoder () = default;

    void appendInt32 (MessageArgumentKey argKey, int32_t argValue) override;
    void appendInt64 (MessageArgumentKey argKey, int64_t argValue) override;
    void appendSize (MessageArgumentKey argKey, size_t argValue) override;
    void appendFloat (MessageArgumentKey argKey, float argValue) override;
    void appendDouble (MessageArgumentKey argKey, double argValue) override;
    void appendString (MessageArgumentKey argKey, const char * argValue) override;
    void appendBytes (MessageArgumentKey argKey, const uint8_t * argValue, size_t argSize, bool copy) override;
    ARA::IPC::MessageEncoder* appendSubMessage (MessageArgumentKey argKey) override;

    // to be used by IPCMessageChannel only: encoding to channel-internal datas format
#if defined (__APPLE__)
    __attribute__((cf_returns_retained)) CFDataRef createEncodedMessage () const;
#else
    std::string createEncodedMessage () const;
#endif

private:
    using IPCXMLMessage::IPCXMLMessage;

    pugi::xml_attribute _appendAttribute (const MessageArgumentKey argKey);
};

class IPCXMLMessageDecoder : public IPCXMLMessage, public ARA::IPC::MessageDecoder
{
public:
    // to be used by IPCMessageChannel only: encoding from channel-internal datas format
#if defined (__APPLE__)
    static IPCXMLMessageDecoder* createWithMessageData(CFDataRef data);
#else
    static IPCXMLMessageDecoder* createWithMessageData(const char* data, const size_t dataSize);
#endif

    bool readInt32 (MessageArgumentKey argKey, int32_t* argValue) const override;
    bool readInt64 (MessageArgumentKey argKey, int64_t* argValue) const override;
    bool readSize (MessageArgumentKey argKey, size_t* argValue) const override;
    bool readFloat (MessageArgumentKey argKey, float* argValue) const override;
    bool readDouble (MessageArgumentKey argKey, double* argValue) const override;
    bool readString (MessageArgumentKey argKey, const char** argValue) const override;
    bool readBytesSize (MessageArgumentKey argKey, size_t* argSize) const override;
    void readBytes (MessageArgumentKey argKey, uint8_t* argValue) const override;
    MessageDecoder* readSubMessage (const MessageArgumentKey argKey) const override;
    bool hasDataForKey (MessageArgumentKey argKey) const override;

private:
    using IPCXMLMessage::IPCXMLMessage;

    mutable std::string _bytesCacheData {};
    mutable MessageArgumentKey _bytesCacheKey { std::numeric_limits<MessageArgumentKey>::max () };
};

#endif // ARA_ENABLE_IPC
