//------------------------------------------------------------------------------
//! \file       IPCXMLEncoding.h
//!             Proof-of-concept pugixml-based implementation of ARAIPCMessageEn-/Decoder
//!             for the ARA SDK TestHost (error handling is limited to assertions).
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
    using MessageKey = ARA::IPC::ARAIPCMessageKey;

protected:
    IPCXMLMessage ();
    IPCXMLMessage (std::shared_ptr<pugi::xml_document> dictionary, pugi::xml_node root);
    IPCXMLMessage (const char* data, const size_t dataSize);

    static const char* _getEncodedKey (const MessageKey argKey);

protected:
    std::shared_ptr<pugi::xml_document> _dictionary {};
    pugi::xml_node _root {};
};


class IPCXMLMessageEncoder : public IPCXMLMessage, public ARA::IPC::ARAIPCMessageEncoder
{
public:
    IPCXMLMessageEncoder () = default;

    void appendInt32 (MessageKey argKey, int32_t argValue) override;
    void appendInt64 (MessageKey argKey, int64_t argValue) override;
    void appendSize (MessageKey argKey, size_t argValue) override;
    void appendFloat (MessageKey argKey, float argValue) override;
    void appendDouble (MessageKey argKey, double argValue) override;
    void appendString (MessageKey argKey, const char * argValue) override;
    void appendBytes (MessageKey argKey, const uint8_t * argValue, size_t argSize, bool copy) override;
    ARA::IPC::ARAIPCMessageEncoder* appendSubMessage (MessageKey argKey) override;

    // to be used by IPCMessageChannel only: encoding to channel-internal datas format
#if defined (__APPLE__)
    __attribute__((cf_returns_retained)) CFDataRef createEncodedMessage () const;
#else
    std::string createEncodedMessage () const;
#endif

private:
    using IPCXMLMessage::IPCXMLMessage;

    pugi::xml_attribute _appendAttribute (const MessageKey argKey);
};

class IPCXMLMessageDecoder : public IPCXMLMessage, public ARA::IPC::ARAIPCMessageDecoder
{
public:
    // to be used by IPCMessageChannel only: encoding from channel-internal datas format
#if defined (__APPLE__)
    static IPCXMLMessageDecoder* createWithMessageData(CFDataRef data);
#else
    static IPCXMLMessageDecoder* createWithMessageData(const char* data, const size_t dataSize);
#endif

    bool readInt32 (MessageKey argKey, int32_t* argValue) const override;
    bool readInt64 (MessageKey argKey, int64_t* argValue) const override;
    bool readSize (MessageKey argKey, size_t* argValue) const override;
    bool readFloat (MessageKey argKey, float* argValue) const override;
    bool readDouble (MessageKey argKey, double* argValue) const override;
    bool readString (MessageKey argKey, const char** argValue) const override;
    bool readBytesSize (MessageKey argKey, size_t* argSize) const override;
    void readBytes (MessageKey argKey, uint8_t* argValue) const override;
    ARAIPCMessageDecoder* readSubMessage (const MessageKey argKey) const override;
    bool hasDataForKey (MessageKey argKey) const override;

private:
    using IPCXMLMessage::IPCXMLMessage;

    mutable std::string _bytesCacheData {};
    mutable MessageKey _bytesCacheKey { std::numeric_limits<MessageKey>::max () };
};

#endif // ARA_ENABLE_IPC
