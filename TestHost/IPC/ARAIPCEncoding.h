//------------------------------------------------------------------------------
//! \file       ARAIPCEncoding.h
//!             utilities for representing ARA specific data in generic IPC messages
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "IPCPort.h"

#include "ARA_Library/Debug/ARADebug.h"
#include "ARA_Library/Dispatch/ARAContentReader.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace ARA
{

//------------------------------------------------------------------------------
// wrapper factories to efficiently handle sending and receiving raw bytes
//------------------------------------------------------------------------------

// a read function based on a ptr+size pair or std::vector
// it returns pointer to read bytes from and byte count via reference
// copy should be set to false if the bytes are guaranteed to remain valid
// until the message has been sent - this is the case for all blocking sends,
// but not for non-blocking sends and depends on context for replies
class BytesEncoder : public std::function<void (const uint8_t*&, size_t&, bool&)>
{
public:
    BytesEncoder (const uint8_t* const bytes, const size_t size, const bool copy)
    : std::function<void (const uint8_t*&, size_t&, bool&)> {
        [bytes, size, copy] (const uint8_t*& bytesPtr, size_t& bytesSize, bool& bytesCopy) -> void
        {
            bytesPtr = bytes;
            bytesSize = size;
            bytesCopy = copy;
        }}
    {}
    BytesEncoder (const std::vector<uint8_t>& bytes, const bool copy)
    : BytesEncoder { bytes.data (), bytes.size (), copy }
    {}
};

// a write function based on a ptr+size pair or std::vector
// resizes to the desired byte count and returns pointer to write bytes to
class BytesDecoder : public std::function<uint8_t* (size_t&)>
{
public:
    BytesDecoder (uint8_t* const bytes, size_t& size)
    : std::function<uint8_t* (size_t&)> {
        [bytes, &size] (size_t& bytesSize) -> uint8_t*
        {
            if (bytesSize > size)
                bytesSize = size;   // if there is more data then we can take, clip
            else
                size = bytesSize;   // otherwise store size
            return bytes;
        }}
    {}
    BytesDecoder (std::vector<uint8_t>& bytes)
    : std::function<uint8_t* (size_t&)> {
        [&bytes] (size_t& size) -> uint8_t*
        {
            bytes.resize (size);
            return bytes.data ();
        }}
    {}
};


//------------------------------------------------------------------------------
// wrapper factories to efficiently handle sending and receiving arrays
//------------------------------------------------------------------------------


template<typename ElementT>
struct ArrayArgument
{
    static_assert (sizeof(ElementT) > sizeof(ARAByte), "byte-sized arrays should be sent as raw bytes");
    ElementT* elements;
    size_t count;
};


//------------------------------------------------------------------------------
// various private helpers
//------------------------------------------------------------------------------

// private helper template to detect ARA ref types
template<typename T>
struct _IsRefType
{
    static constexpr bool value { false };
};
#define ARA_IPC_SPECIALIZE_FOR_REF_TYPE(Type)   \
template<>                                      \
struct _IsRefType<Type>                         \
{                                               \
    static constexpr bool value { true };       \
};
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAMusicalContextRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARARegionSequenceRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioSourceRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioModificationRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRegionRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAContentReaderRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARADocumentControllerRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRendererRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAEditorRendererRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAEditorViewRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAPlugInExtensionRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAMusicalContextHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARARegionSequenceHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioSourceHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioModificationHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRegionHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAContentReaderHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioAccessControllerHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAAudioReaderHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAArchivingControllerHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAArchiveReaderHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAArchiveWriterHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAContentAccessControllerHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAModelUpdateControllerHostRef)
ARA_IPC_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackControllerHostRef)
#undef ARA_IPC_SPECIALIZE_FOR_REF_TYPE


// helper template to identify pointers to ARA structs in message arguments
template<typename ArgT>
struct _IsStructPointerArg
{
    struct _False
    {
        static constexpr bool value { false };
    };
    struct _True
    {
        static constexpr bool value { true };
    };
    using type = typename std::conditional<!_IsRefType<ArgT>::value &&
                                            std::is_pointer<ArgT>::value && !std::is_same<ArgT, const char*>::value, _True, _False>::type;
};


//------------------------------------------------------------------------------
// private primitive wrappers for IPCMessage API that drop the type encoding
// from the name (which is required there for potential C compatibility)
//------------------------------------------------------------------------------


// primitives for appending an argument to a message
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const int32_t argValue)
{
    message.appendInt32 (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const int64_t argValue)
{
    message.appendInt64 (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const size_t argValue)
{
    message.appendSize (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const float argValue)
{
    message.appendFloat (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const double argValue)
{
    message.appendDouble (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const char* const argValue)
{
    message.appendString (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const BytesEncoder& argValue)
{
    const uint8_t* bytes;
    size_t size;
    bool copy;
    argValue (bytes, size, copy);
    message.appendBytes (argKey, bytes, size, copy);
}
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const IPCMessage& argValue)
{
    message.appendMessage (argKey, argValue);
}

// primitives for reading an (optional) argument from a message
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, int32_t& argValue)
{
    return message.readInt32 (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, int64_t& argValue)
{
    return message.readInt64 (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, size_t& argValue)
{
    return message.readSize (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, float& argValue)
{
    return message.readFloat (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, double& argValue)
{
    return message.readDouble (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, const char*& argValue)
{
    return message.readString (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, BytesDecoder& argValue)
{
    size_t receivedSize;
    const auto found { message.readBytesSize (argKey, receivedSize) };
    auto availableSize { receivedSize };
    const auto bytes { argValue (availableSize) };
    if (!found)
        return false;
    if (availableSize < receivedSize)
        return false;
    return message.readBytes (argKey, bytes);
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, IPCMessage& argValue)
{
    return message.readMessage (argKey, argValue);
}


//------------------------------------------------------------------------------
// overloads of the IPCMessage primitives for types that can be directly mapped to a primitive type
//------------------------------------------------------------------------------


// templated overloads of the IPCMessage primitives for ARA (host) ref types,
// which are stored as size_t
template<typename T, typename std::enable_if<_IsRefType<T>::value, bool>::type = true>
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const T argValue)
{
    message.appendSize (argKey, reinterpret_cast<size_t> (argValue));
}
template<typename T, typename std::enable_if<_IsRefType<T>::value, bool>::type = true>
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, T& argValue)
{
    // \todo is there a safe/proper way across all compilers for this cast to avoid the copy?
//  return message.readSize (argKey, *reinterpret_cast<size_t*> (&argValue));
    size_t tmp;
    const auto success { message.readSize (argKey, tmp) };
    argValue = reinterpret_cast<T> (tmp);
    return success;
}

/*  instead of using ARA_IPC_ENCODE_EMBEDDED_BYTES below, we could instead allow
    sending arrays of ARABytes via this overload (seems simpler but less efficient):
// to read and write arrays of ARAByte (not raw bytes but e.g. ARAKeySignatureIntervalUsage),
// we use int32_t to keep the IPCMessage API small
inline void _appendToMessage (IPCMessage& message, const MessageKey argKey, const ARAByte argValue)
{
    message.appendInt32 (argKey, static_cast<int32_t> (argValue));
}
inline bool _readFromMessage (const IPCMessage& message, const MessageKey argKey, ARAByte& argValue)
{
    int32_t tmp;
    const auto result { message.readInt32 (argKey, tmp) };
    ARA_INTERNAL_ASSERT ((0 <= tmp) && (tmp <= static_cast<int32_t> (std::numeric_limits<ARAByte>::max ())));
    argValue = static_cast<ARAByte> (tmp);
    return result;
}
*/


//------------------------------------------------------------------------------
// private helper templates to en-/decode ARA API values
// The mapping is 1:1 except for ARA (host)refs which are encoded as size_t, and aggregate types
// (i.e. ARA structs or std::vector<> of types other than ARAByte), which are expressed as sub-messages.
// En- and Decoding use the same implementation technique:
// To support using compound types (arrays, structs) both as indexed sub-message for call arguments
// when sending as well as as root message for replies, there's a templated _ValueEn-/Decoder struct
// for each type which provides an encode&append/read&decode call that extracts a sub-message if
// needed and then performs the en/decode via a separate plain en-/decoding call only available in
// compound types. The latter call will be used directly for compound data type replies.
// In order not to have to spell out _ValueEn-/Decoder<> explicitly, overloaded wrapper function
// templates _encodeAndAppend() and _readAndDecode() are provided.
//------------------------------------------------------------------------------


// declarations of wrapper functions to implicitly deduce _ValueEn-/Decoder<> -
// they are defined further down below, after all specializations are defined
template <typename ValueT>
inline void _encodeAndAppend (IPCMessage& message, const MessageKey argKey, const ValueT& argValue);
template <typename ValueT>
inline bool _readAndDecode (ValueT& result, const IPCMessage& message, const MessageKey argKey);


// primary templates for basic types (numbers, strings, (host)refs and raw bytes)
template<typename ValueT>
struct _ValueEncoder
{
    static inline void encodeAndAppend (IPCMessage& message, const MessageKey argKey, const ValueT& argValue)
    {
        _appendToMessage (message, argKey, argValue);
    }
};
template<typename ValueT>
struct _ValueDecoder
{
    static inline bool readAndDecode (ValueT& result, const IPCMessage& message, const MessageKey argKey)
    {
        return _readFromMessage (message, argKey, result);
    }
};


// common base classes for en-/decoding compound types (arrays, structs) via nested messages,
// providing the generic encode&append/read&decode calls
template<typename ValueT>
struct _CompoundValueEncoderBase
{
    static inline void encodeAndAppend (IPCMessage& message, const MessageKey argKey, const ValueT& argValue)
    {
        _encodeAndAppend (message, argKey, _ValueEncoder<ValueT>::encode (argValue));
    }
};
template<typename ValueT>
struct _CompoundValueDecoderBase
{
    static inline bool readAndDecode (ValueT& result, const IPCMessage& message, const MessageKey argKey)
    {
        IPCMessage subMessage;
        if (!_readFromMessage (message, argKey, subMessage))
            return false;
        return _ValueDecoder<ValueT>::decode (result, subMessage);
    }
};


// specialization for encoding arrays (variable or fixed size)
template<typename ElementT>
struct _ValueEncoder<ArrayArgument<ElementT>> : public _CompoundValueEncoderBase<ArrayArgument<ElementT>>
{
    static inline IPCMessage encode (const ArrayArgument<ElementT>& value)
    {
        IPCMessage result;
        ARA_INTERNAL_ASSERT (value.count <= static_cast<size_t> (std::numeric_limits<MessageKey>::max ()));
        const auto count { static_cast<MessageKey> (value.count) };
        _encodeAndAppend (result, 0, count);
        for (auto i { 0 }; i < count; ++i)
            _encodeAndAppend (result, i + 1, value.elements[static_cast<size_t> (i)]);
        return result;
    }
};

// specialization for decoding fixed-size arrays
template<typename ElementT>
struct _ValueDecoder<ArrayArgument<ElementT>> : public _CompoundValueDecoderBase<ArrayArgument<ElementT>>
{
    static inline bool decode (ArrayArgument<ElementT>& result, const IPCMessage& message)
    {
        bool success { true };
        MessageKey count;
        success &= _readAndDecode (count, message, 0);
        success &= (count == static_cast<MessageKey> (result.count));
        if (count > static_cast<MessageKey> (result.count))
            count = static_cast<MessageKey> (result.count);

        for (auto i { 0 }; i < count; ++i)
            success &= _readAndDecode (result.elements[static_cast<size_t> (i)], message, i + 1);
        return success;
    }
};

// specialization for decoding variable arrays
template<typename ElementT>
struct _ValueDecoder<std::vector<ElementT>> : public _CompoundValueDecoderBase<std::vector<ElementT>>
{
    static inline bool decode (std::vector<ElementT>& result, const IPCMessage& message)
    {
        bool success { true };
        MessageKey count;
        success &= _readAndDecode (count, message, 0);
        result.resize (static_cast<size_t> (count));
        for (auto i { 0 }; i < count; ++i)
            success &= _readAndDecode (result[static_cast<size_t> (i)], message, i + 1);
        return success;
    }
};


// en/decoding of compound types

#define ARA_IPC_BEGIN_ENCODE(StructT)                                                           \
template<> struct _ValueEncoder<StructT> : public _CompoundValueEncoderBase<StructT>            \
{                                               /* specialization for given struct */           \
    using StructType = StructT;                                                                 \
    static inline IPCMessage encode (const StructType& value)                                   \
    {                                                                                           \
        IPCMessage result;
#define ARA_IPC_ENCODE_MEMBER(member)                                                           \
        _encodeAndAppend (result, offsetof (StructType, member), value.member);
#define ARA_IPC_ENCODE_EMBEDDED_BYTES(member)                                                   \
        const BytesEncoder tmp_##member { reinterpret_cast<const uint8_t*> (value.member), sizeof (value.member), true }; \
        _encodeAndAppend (result, offsetof (StructType, member), tmp_##member);
#define ARA_IPC_ENCODE_EMBEDDED_ARRAY(member)                                                   \
        const ArrayArgument<const std::remove_extent<decltype (value.member)>::type> tmp_##member { value.member, std::extent<decltype (value.member)>::value }; \
        _encodeAndAppend (result, offsetof (StructType, member), tmp_##member);
#define ARA_IPC_ENCODE_VARIABLE_ARRAY(member, count)                                            \
        if ((value.count > 0) && (value.member != nullptr)) {                                   \
            const ArrayArgument<const std::remove_pointer<decltype (value.member)>::type> tmp_##member { value.member, value.count }; \
            _encodeAndAppend (result, offsetof (StructType, member), tmp_##member);             \
        }
#define ARA_IPC_HAS_OPTIONAL_MEMBER(member)                                                     \
        /* \todo ARA_IMPLEMENTS_FIELD decorates the type with the ARA:: namespace,     */       \
        /* this conflicts with decltype's result - this copied version drops the ARA:: */       \
        (value.structSize > offsetof (std::remove_reference<decltype (value)>::type, member))
#define ARA_IPC_ENCODE_OPTIONAL_MEMBER(member)                                                  \
        if (ARA_IPC_HAS_OPTIONAL_MEMBER (member))                                               \
            ARA_IPC_ENCODE_MEMBER (member)
#define ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR(member)                                              \
        if (ARA_IPC_HAS_OPTIONAL_MEMBER (member) && (value.member != nullptr))                  \
            _encodeAndAppend (result, offsetof (StructType, member), *value.member);
#define ARA_IPC_END_ENCODE                                                                      \
        return result;                                                                          \
    }                                                                                           \
};


#define ARA_IPC_BEGIN_DECODE(StructT)                                                           \
template<> struct _ValueDecoder<StructT> : public _CompoundValueDecoderBase<StructT>            \
{                                               /* specialization for given struct */           \
    using StructType = StructT;                                                                 \
    static inline bool decode (StructType& result, const IPCMessage& message)                   \
    {                                                                                           \
        bool success { true };
#define ARA_IPC_BEGIN_DECODE_SIZED(StructT)                                                     \
        ARA_IPC_BEGIN_DECODE (StructT)                                                          \
        result.structSize = k##StructT##MinSize;
#define ARA_IPC_DECODE_MEMBER(member)                                                           \
        success &= _readAndDecode (result.member, message, offsetof (StructType, member));      \
        ARA_INTERNAL_ASSERT (success);
#define ARA_IPC_DECODE_EMBEDDED_BYTES(member)                                                   \
        auto resultSize_##member { sizeof (result.member) };                                    \
        BytesDecoder tmp_##member { reinterpret_cast<uint8_t*> (result.member), resultSize_##member }; \
        success &= _readAndDecode (tmp_##member, message, offsetof (StructType, member));       \
        success &= (resultSize_##member == sizeof (result.member));                             \
        ARA_INTERNAL_ASSERT (success);
#define ARA_IPC_DECODE_EMBEDDED_ARRAY(member)                                                   \
        ArrayArgument<std::remove_extent<decltype (result.member)>::type> tmp_##member { result.member, std::extent<decltype (result.member)>::value }; \
        success &= _readAndDecode (tmp_##member, message, offsetof (StructType, member));       \
        ARA_INTERNAL_ASSERT (success);
#define ARA_IPC_DECODE_VARIABLE_ARRAY(member, count, updateCount)                               \
        /* \todo the outer struct contains a pointer to the inner array, so we need some  */    \
        /* place to store it - this static only works as long as this is single-threaded! */    \
        static std::vector<typename std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type> tmp_##member; \
        if (_readAndDecode (tmp_##member, message, offsetof (StructType, member))) {            \
            result.member = tmp_##member.data ();                                               \
            if (updateCount) { result.count = tmp_##member.size (); }                           \
        } else {                                                                                \
            result.member = nullptr;                                                            \
            if (updateCount) { result.count = 0; }                                              \
        }
#define ARA_IPC_UPDATE_STRUCT_SIZE_FOR_OPTIONAL(member)                                         \
        /* \todo ARA_IMPLEMENTED_STRUCT_SIZE decorates the type with the ARA:: namespace, */    \
        /* conflicting with the local alias StructType - this copy simply drops the ARA:: */    \
        constexpr auto size { offsetof (StructType, member) + sizeof (static_cast<StructType*> (nullptr)->member) }; \
        result.structSize = std::max (result.structSize, size);
#define ARA_IPC_DECODE_OPTIONAL_MEMBER(member)                                                  \
        if (_readAndDecode (result.member, message, offsetof (StructType, member))) {           \
            ARA_IPC_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                   \
        }
#define ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR(member)                                              \
        result.member = nullptr;    /* set to null because other members may follow */          \
        IPCMessage tmp_##member;                                                                \
        if (_readAndDecode (tmp_##member, message, offsetof (StructType, member))) {            \
            ARA_IPC_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                   \
            /* \todo the outer struct contains a pointer to the inner struct, so we need some */\
            /* place to store it - this static only works as long as this is single-threaded! */\
            static std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type cache; \
            success &= _ValueDecoder<decltype (cache)>::decode (cache, tmp_##member);           \
            ARA_INTERNAL_ASSERT (success);                                                      \
            result.member = &cache;                                                             \
        }
#define ARA_IPC_END_DECODE                                                                      \
        return success;                                                                         \
    }                                                                                           \
};


ARA_IPC_BEGIN_ENCODE (ARAColor)
    ARA_IPC_ENCODE_MEMBER (r)
    ARA_IPC_ENCODE_MEMBER (g)
    ARA_IPC_ENCODE_MEMBER (b)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAColor)
    ARA_IPC_DECODE_MEMBER (r)
    ARA_IPC_DECODE_MEMBER (g)
    ARA_IPC_DECODE_MEMBER (b)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARADocumentProperties)
    ARA_IPC_ENCODE_MEMBER (name)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARADocumentProperties)
    ARA_IPC_DECODE_MEMBER (name)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAMusicalContextProperties)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_OPTIONAL_MEMBER (orderIndex)
    ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAMusicalContextProperties)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_OPTIONAL_MEMBER (orderIndex)
    ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARARegionSequenceProperties)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_MEMBER (orderIndex)
    ARA_IPC_ENCODE_MEMBER (musicalContextRef)
    ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARARegionSequenceProperties)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_MEMBER (orderIndex)
    ARA_IPC_DECODE_MEMBER (musicalContextRef)
    ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAAudioSourceProperties)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_MEMBER (persistentID)
    ARA_IPC_ENCODE_MEMBER (sampleCount)
    ARA_IPC_ENCODE_MEMBER (sampleRate)
    ARA_IPC_ENCODE_MEMBER (channelCount)
    ARA_IPC_ENCODE_MEMBER (merits64BitSamples)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAAudioSourceProperties)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_MEMBER (persistentID)
    ARA_IPC_DECODE_MEMBER (sampleCount)
    ARA_IPC_DECODE_MEMBER (sampleRate)
    ARA_IPC_DECODE_MEMBER (channelCount)
    ARA_IPC_DECODE_MEMBER (merits64BitSamples)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAAudioModificationProperties)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_MEMBER (persistentID)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAAudioModificationProperties)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_MEMBER (persistentID)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAPlaybackRegionProperties)
    ARA_IPC_ENCODE_MEMBER (transformationFlags)
    ARA_IPC_ENCODE_MEMBER (startInModificationTime)
    ARA_IPC_ENCODE_MEMBER (durationInModificationTime)
    ARA_IPC_ENCODE_MEMBER (startInPlaybackTime)
    ARA_IPC_ENCODE_MEMBER (durationInPlaybackTime)
    ARA_IPC_ENCODE_MEMBER (musicalContextRef)
    ARA_IPC_ENCODE_OPTIONAL_MEMBER (regionSequenceRef)
    ARA_IPC_ENCODE_OPTIONAL_MEMBER (name)
    ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAPlaybackRegionProperties)
    ARA_IPC_DECODE_MEMBER (transformationFlags)
    ARA_IPC_DECODE_MEMBER (startInModificationTime)
    ARA_IPC_DECODE_MEMBER (durationInModificationTime)
    ARA_IPC_DECODE_MEMBER (startInPlaybackTime)
    ARA_IPC_DECODE_MEMBER (durationInPlaybackTime)
    ARA_IPC_DECODE_MEMBER (musicalContextRef)
    ARA_IPC_DECODE_OPTIONAL_MEMBER (regionSequenceRef)
    ARA_IPC_DECODE_OPTIONAL_MEMBER (name)
    ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR (color)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentTimeRange)
    ARA_IPC_ENCODE_MEMBER (start)
    ARA_IPC_ENCODE_MEMBER (duration)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentTimeRange)
    ARA_IPC_DECODE_MEMBER (start)
    ARA_IPC_DECODE_MEMBER (duration)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentTempoEntry)
    ARA_IPC_ENCODE_MEMBER (timePosition)
    ARA_IPC_ENCODE_MEMBER (quarterPosition)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentTempoEntry)
    ARA_IPC_DECODE_MEMBER (timePosition)
    ARA_IPC_DECODE_MEMBER (quarterPosition)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentBarSignature)
    ARA_IPC_ENCODE_MEMBER (numerator)
    ARA_IPC_ENCODE_MEMBER (denominator)
    ARA_IPC_ENCODE_MEMBER (position)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentBarSignature)
    ARA_IPC_DECODE_MEMBER (numerator)
    ARA_IPC_DECODE_MEMBER (denominator)
    ARA_IPC_DECODE_MEMBER (position)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentNote)
    ARA_IPC_ENCODE_MEMBER (frequency)
    ARA_IPC_ENCODE_MEMBER (pitchNumber)
    ARA_IPC_ENCODE_MEMBER (volume)
    ARA_IPC_ENCODE_MEMBER (startPosition)
    ARA_IPC_ENCODE_MEMBER (attackDuration)
    ARA_IPC_ENCODE_MEMBER (noteDuration)
    ARA_IPC_ENCODE_MEMBER (signalDuration)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentNote)
    ARA_IPC_DECODE_MEMBER (frequency)
    ARA_IPC_DECODE_MEMBER (pitchNumber)
    ARA_IPC_DECODE_MEMBER (volume)
    ARA_IPC_DECODE_MEMBER (startPosition)
    ARA_IPC_DECODE_MEMBER (attackDuration)
    ARA_IPC_DECODE_MEMBER (noteDuration)
    ARA_IPC_DECODE_MEMBER (signalDuration)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentTuning)
    ARA_IPC_ENCODE_MEMBER (concertPitchFrequency)
    ARA_IPC_ENCODE_MEMBER (root)
    ARA_IPC_ENCODE_EMBEDDED_ARRAY (tunings)
    ARA_IPC_ENCODE_MEMBER (name)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentTuning)
    ARA_IPC_DECODE_MEMBER (concertPitchFrequency)
    ARA_IPC_DECODE_MEMBER (root)
    ARA_IPC_DECODE_EMBEDDED_ARRAY (tunings)
    ARA_IPC_DECODE_MEMBER (name)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentKeySignature)
    ARA_IPC_ENCODE_MEMBER (root)
    ARA_IPC_ENCODE_EMBEDDED_BYTES (intervals)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_MEMBER (position)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentKeySignature)
    ARA_IPC_DECODE_MEMBER (root)
    ARA_IPC_DECODE_EMBEDDED_BYTES (intervals)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_MEMBER (position)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAContentChord)
    ARA_IPC_ENCODE_MEMBER (root)
    ARA_IPC_ENCODE_MEMBER (bass)
    ARA_IPC_ENCODE_EMBEDDED_BYTES (intervals)
    ARA_IPC_ENCODE_MEMBER (name)
    ARA_IPC_ENCODE_MEMBER (position)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAContentChord)
    ARA_IPC_DECODE_MEMBER (root)
    ARA_IPC_DECODE_MEMBER (bass)
    ARA_IPC_DECODE_EMBEDDED_BYTES (intervals)
    ARA_IPC_DECODE_MEMBER (name)
    ARA_IPC_DECODE_MEMBER (position)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARARestoreObjectsFilter)
    ARA_IPC_ENCODE_MEMBER (documentData)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioSourceArchiveIDs, audioSourceIDsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioSourceCurrentIDs, audioSourceIDsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioModificationArchiveIDs, audioModificationIDsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioModificationCurrentIDs, audioModificationIDsCount)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARARestoreObjectsFilter)
    ARA_IPC_DECODE_MEMBER (documentData)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioSourceArchiveIDs, audioSourceIDsCount, true)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioSourceCurrentIDs, audioSourceIDsCount, false)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioModificationArchiveIDs, audioModificationIDsCount, true)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioModificationCurrentIDs, audioModificationIDsCount, false)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAStoreObjectsFilter)
    ARA_IPC_ENCODE_MEMBER (documentData)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioSourceRefs, audioSourceRefsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (audioModificationRefs, audioModificationRefsCount)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAStoreObjectsFilter)
    ARA_IPC_DECODE_MEMBER (documentData)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioSourceRefs, audioSourceRefsCount, true)
    ARA_IPC_DECODE_VARIABLE_ARRAY (audioModificationRefs, audioModificationRefsCount, true)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAProcessingAlgorithmProperties)
    ARA_IPC_ENCODE_MEMBER (persistentID)
    ARA_IPC_ENCODE_MEMBER (name)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAProcessingAlgorithmProperties)
    ARA_IPC_DECODE_MEMBER (persistentID)
    ARA_IPC_DECODE_MEMBER (name)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAViewSelection)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (playbackRegionRefs, playbackRegionRefsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (regionSequenceRefs, regionSequenceRefsCount)
    ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR (timeRange)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAViewSelection)
    ARA_IPC_DECODE_VARIABLE_ARRAY (playbackRegionRefs, playbackRegionRefsCount, true)
    ARA_IPC_DECODE_VARIABLE_ARRAY (regionSequenceRefs, regionSequenceRefsCount, true)
    ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR (timeRange)
ARA_IPC_END_DECODE

ARA_IPC_BEGIN_ENCODE (ARAFactory)
    ARA_IPC_ENCODE_MEMBER (lowestSupportedApiGeneration)
    ARA_IPC_ENCODE_MEMBER (highestSupportedApiGeneration)
    ARA_IPC_ENCODE_MEMBER (factoryID)
    ARA_IPC_ENCODE_MEMBER (plugInName)
    ARA_IPC_ENCODE_MEMBER (manufacturerName)
    ARA_IPC_ENCODE_MEMBER (informationURL)
    ARA_IPC_ENCODE_MEMBER (version)
    ARA_IPC_ENCODE_MEMBER (documentArchiveID)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (compatibleDocumentArchiveIDs, compatibleDocumentArchiveIDsCount)
    ARA_IPC_ENCODE_VARIABLE_ARRAY (analyzeableContentTypes, analyzeableContentTypesCount)
    ARA_IPC_ENCODE_MEMBER (supportedPlaybackTransformationFlags)
    ARA_IPC_ENCODE_OPTIONAL_MEMBER (supportsStoringAudioFileChunks)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE_SIZED (ARAFactory)
    ARA_IPC_DECODE_MEMBER (lowestSupportedApiGeneration)
    ARA_IPC_DECODE_MEMBER (highestSupportedApiGeneration)
    ARA_IPC_DECODE_MEMBER (factoryID)
    result.initializeARAWithConfiguration = nullptr;
    result.uninitializeARA = nullptr;
    ARA_IPC_DECODE_MEMBER (plugInName)
    ARA_IPC_DECODE_MEMBER (manufacturerName)
    ARA_IPC_DECODE_MEMBER (informationURL)
    ARA_IPC_DECODE_MEMBER (version)
    result.createDocumentControllerWithDocument = nullptr;
    ARA_IPC_DECODE_MEMBER (documentArchiveID)
    ARA_IPC_DECODE_VARIABLE_ARRAY (compatibleDocumentArchiveIDs, compatibleDocumentArchiveIDsCount, true)
    ARA_IPC_DECODE_VARIABLE_ARRAY (analyzeableContentTypes, analyzeableContentTypesCount, true)
    ARA_IPC_DECODE_MEMBER (supportedPlaybackTransformationFlags)
    ARA_IPC_DECODE_OPTIONAL_MEMBER (supportsStoringAudioFileChunks)
ARA_IPC_END_DECODE


// ARADocumentControllerInterface::storeAudioSourceToAudioFileChunk() must return the documentArchiveID and the
// openAutomatically flag in addition to the return value, we need a special struct to encode this through IPC.
struct ARAIPCStoreAudioSourceToAudioFileChunkReply
{
    ARABool result;
    ARAPersistentID documentArchiveID;
    ARABool openAutomatically;
};
ARA_IPC_BEGIN_ENCODE (ARAIPCStoreAudioSourceToAudioFileChunkReply)
    ARA_IPC_ENCODE_MEMBER (result)
    ARA_IPC_ENCODE_MEMBER (documentArchiveID)
    ARA_IPC_ENCODE_MEMBER (openAutomatically)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAIPCStoreAudioSourceToAudioFileChunkReply)
    ARA_IPC_DECODE_MEMBER (result)
    ARA_IPC_DECODE_MEMBER (documentArchiveID)
    ARA_IPC_DECODE_MEMBER (openAutomatically)
ARA_IPC_END_DECODE

// ARADocumentControllerInterface::getPlaybackRegionHeadAndTailTime() must return both head- and tailtime.
struct ARAIPCGetPlaybackRegionHeadAndTailTimeReply
{
    ARATimeDuration headTime;
    ARATimeDuration tailTime;
};
ARA_IPC_BEGIN_ENCODE (ARAIPCGetPlaybackRegionHeadAndTailTimeReply)
    ARA_IPC_ENCODE_MEMBER (headTime)
    ARA_IPC_ENCODE_MEMBER (tailTime)
ARA_IPC_END_ENCODE
ARA_IPC_BEGIN_DECODE (ARAIPCGetPlaybackRegionHeadAndTailTimeReply)
    ARA_IPC_DECODE_MEMBER (headTime)
    ARA_IPC_DECODE_MEMBER (tailTime)
ARA_IPC_END_DECODE


#undef ARA_IPC_BEGIN_ENCODE
#undef ARA_IPC_HAS_OPTIONAL_MEMBER
#undef ARA_IPC_ENCODE_MEMBER
#undef ARA_IPC_ENCODE_EMBEDDED_BYES
#undef ARA_IPC_ENCODE_EMBEDDED_ARRAY
#undef ARA_IPC_ENCODE_VARIABLE_ARRAY
#undef ARA_IPC_ENCODE_OPTIONAL_MEMBER
#undef ARA_IPC_ENCODE_OPTIONAL_STRUCT_PTR
#undef ARA_IPC_END_ENCODE

#undef ARA_IPC_BEGIN_DECODE
#undef ARA_IPC_BEGIN_DECODE_SIZED
#undef ARA_IPC_DECODE_MEMBER
#undef ARA_IPC_DECODE_EMBEDDED_BYTES
#undef ARA_IPC_DECODE_EMBEDDED_ARRAY
#undef ARA_IPC_DECODE_VARIABLE_ARRAY
#undef ARA_IPC_UPDATE_STRUCT_SIZE_FOR_OPTIONAL
#undef ARA_IPC_DECODE_OPTIONAL_MEMBER
#undef ARA_IPC_DECODE_OPTIONAL_STRUCT_PTR
#undef ARA_IPC_END_DECODE


// actual definitions of wrapper functions to implicitly deduce _ValueEn-/Decoder<> -
// they are forward-declared above, before _ValueEn-/Decoder<> are defined
template <typename ValueT>
inline void _encodeAndAppend (IPCMessage& message, const MessageKey argKey, const ValueT& argValue)
{
    return _ValueEncoder<ValueT>::encodeAndAppend (message, argKey, argValue);
}
template <typename ValueT>
inline bool _readAndDecode (ValueT& result, const IPCMessage& message, const MessageKey argKey)
{
    return _ValueDecoder<ValueT>::readAndDecode (result, message, argKey);
}


//------------------------------------------------------------------------------

// private helper for decodeArguments() to deal with optional arguments
template<typename ArgT>
struct _IsOptionalArgument
{
    static constexpr bool value { false };
};
template<typename ArgT>
struct _IsOptionalArgument<std::pair<ArgT, bool>>
{
    static constexpr bool value { true };
};

// private helpers for encodeArguments() and decodeArguments() to deal with the variable arguments one at a time
inline void _encodeArgumentsHelper (IPCMessage& /*message*/, MessageKey /*argKey*/)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, const MessageKey argKey, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    _encodeAndAppend (message, argKey, argValue);
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, const MessageKey argKey, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    if (argValue != nullptr)
        _encodeAndAppend (message, argKey, *argValue);
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename... MoreArgs>
inline void _encodeArgumentsHelper (IPCMessage& message, const MessageKey argKey, const std::nullptr_t& /*argValue*/, const MoreArgs &... moreArgs)
{
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}

inline void _decodeArgumentsHelper (const IPCMessage& /*message*/, MessageKey /*argKey*/)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, const MessageKey argKey, ArgT& argValue, MoreArgs &... moreArgs)
{
    _readAndDecode (argValue, message, argKey);
    _decodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, const MessageKey argKey, ArgT& argValue, MoreArgs &... moreArgs)
{
    argValue.second = _readAndDecode (argValue.first, message, argKey);
    _decodeArgumentsHelper (message, argKey + 1, moreArgs...);
}

// private helpers for ARA_IPC_HOST_METHOD_ID and ARA_IPC_PLUGIN_METHOD_ID
template<typename StructT>
constexpr MessageID _getHostInterfaceID ();
template<>
constexpr MessageID _getHostInterfaceID<ARAAudioAccessControllerInterface> () { return 0; }
template<>
constexpr MessageID _getHostInterfaceID<ARAArchivingControllerInterface> () { return 1; }
template<>
constexpr MessageID _getHostInterfaceID<ARAContentAccessControllerInterface> () { return 2; }
template<>
constexpr MessageID _getHostInterfaceID<ARAModelUpdateControllerInterface> () { return 3; }
template<>
constexpr MessageID _getHostInterfaceID<ARAPlaybackControllerInterface> () { return 4; }

template<typename StructT>
constexpr MessageID _getPlugInInterfaceID ();
template<>
constexpr MessageID _getPlugInInterfaceID<ARADocumentControllerInterface> () { return 0; }
template<>
constexpr MessageID _getPlugInInterfaceID<ARAPlaybackRendererInterface> () { return 1; }
template<>
constexpr MessageID _getPlugInInterfaceID<ARAEditorRendererInterface> () { return 2; }
template<>
constexpr MessageID _getPlugInInterfaceID<ARAEditorViewInterface> () { return 3; }

template<MessageID interfaceID, size_t offset>
constexpr MessageID _encodeMessageID ()
{
    static_assert (offset > 0, "offset 0 is never a valid function pointer");
    static_assert ((interfaceID < 8), "currently using only 3 bits for interface ID");
#if defined (__i386__) || defined (_M_IX86)
    static_assert ((sizeof (void*) == 4), "compiler settings imply 32 bit pointers");
    static_assert (((offset & 0x3FFFFFF4) == offset), "offset is misaligned or too large");
    return (offset << 1) + interfaceID; // lower 2 bits of offset are 0 due to alignment, must shift 1 bit to store interface ID
#else
    static_assert ((sizeof (void*) == 8), "assuming 64 bit pointers per default");
    static_assert (((offset & 0x7FFFFFF8) == offset), "offset is misaligned or too large");
    return offset + interfaceID;        // lower 3 bits of offset are 0 due to alignment, can be used to store interface ID
#endif
}


//------------------------------------------------------------------------------
// actual client API
//------------------------------------------------------------------------------


// caller side: create a message ID for a given ARA method
#define ARA_IPC_HOST_METHOD_ID(StructT, member) _encodeMessageID <_getHostInterfaceID<StructT> (), offsetof (StructT, member)> ()
#define ARA_IPC_PLUGIN_METHOD_ID(StructT, member) _encodeMessageID <_getPlugInInterfaceID<StructT> (), offsetof (StructT, member)> ()

// "global" messages that are not passed based on interface structs
constexpr MessageID kGetFactoriesCountMessageID { 1 };
constexpr MessageID kGetFactoryMessageID { 2 };
constexpr MessageID kCreateDocumentControllerMessageID { 3 };


// caller side: create a message with the specified arguments
template<typename... Args>
inline IPCMessage encodeArguments (const Args &... args)
{
    IPCMessage result;
    _encodeArgumentsHelper (result, 0, args...);
    return result;
}

// caller side: decode the received reply to a sent message
template<typename RetT, typename std::enable_if<!std::is_class<RetT>::value || !std::is_pod<RetT>::value, bool>::type = true>
inline bool decodeReply (RetT& result, const IPCMessage& message)
{
    return _readAndDecode (result, message, 0);
}
template<typename RetT, typename std::enable_if<std::is_class<RetT>::value && std::is_pod<RetT>::value, bool>::type = true>
inline bool decodeReply (RetT& result, const IPCMessage& message)
{
    return _ValueDecoder<RetT>::decode (result, message);
}
inline bool decodeReply (IPCMessage& result, const IPCMessage& message)
{
    result = message;
    return true;
}


// callee side: decode the arguments of a received message
template<typename... Args>
inline void decodeArguments (const IPCMessage& message, Args &... args)
{
    _decodeArgumentsHelper (message, 0, args...);
}

// callee side: wrapper for optional method arguments: first is the argument value, second if it was present
template<typename ArgT>
using OptionalArgument = typename std::pair<typename std::remove_pointer<ArgT>::type, bool>;

// callee side: encode the reply to a received message
template<typename ValueT, typename std::enable_if<!std::is_class<ValueT>::value || !std::is_pod<ValueT>::value, bool>::type = true>
inline IPCMessage encodeReply (const ValueT& value)
{
    IPCMessage result;
    _encodeAndAppend (result, 0, value);
    return result;
}
template<typename ValueT, typename std::enable_if<std::is_class<ValueT>::value && std::is_pod<ValueT>::value, bool>::type = true>
inline IPCMessage encodeReply (const ValueT& value)
{
    return _ValueEncoder<ValueT>::encode (value);
}


// for debugging only: decoding method IDs
inline const char* _decodeMessageID (std::map<MessageID, std::string>& cache, const char* interfaceName, const MessageID messageID)
{
    auto it { cache.find (messageID) };
    if (it == cache.end ())
        it = cache.emplace (messageID, std::string { interfaceName } + " method " + std::to_string (messageID >> 3)).first;
    return it->second.c_str();
}
inline const char* decodeHostMessageID (const MessageID messageID)
{
    static std::map<MessageID, std::string> cache;
    const char* interfaceName;
    switch (messageID & 0x7)
    {
        case 0: interfaceName = "ARAAudioAccessControllerInterface"; break;
        case 1: interfaceName = "ARAArchivingControllerInterface"; break;
        case 2: interfaceName = "ARAContentAccessControllerInterface"; break;
        case 3: interfaceName = "ARAModelUpdateControllerInterface"; break;
        case 4: interfaceName = "ARAPlaybackControllerInterface"; break;
        default: ARA_INTERNAL_ASSERT (false); interfaceName = "(unknown)"; break;
    }
    return _decodeMessageID (cache, interfaceName, messageID);
}
inline const char* decodePlugInMessageID (const MessageID messageID)
{
    static std::map<MessageID, std::string> cache;
    const char* interfaceName;
    switch (messageID & 0x7)
    {
        case 0: interfaceName = "ARADocumentControllerInterface"; break;
        case 1: interfaceName = "ARAPlaybackRendererInterface"; break;
        case 2: interfaceName = "ARAEditorRendererInterface"; break;
        case 3: interfaceName = "ARAEditorViewInterface"; break;
        default: ARA_INTERNAL_ASSERT (false); interfaceName = "(unknown)"; break;
    }
    return _decodeMessageID (cache, interfaceName, messageID);
}

//------------------------------------------------------------------------------
// support for content readers
//------------------------------------------------------------------------------

inline IPCMessage encodeContentEvent (const ARAContentType type, const void* eventData)
{
    switch (type)
    {
        case kARAContentTypeNotes:          return encodeReply (*static_cast<const ARAContentNote*> (eventData));
        case kARAContentTypeTempoEntries:   return encodeReply (*static_cast<const ARAContentTempoEntry*> (eventData));
        case kARAContentTypeBarSignatures:  return encodeReply (*static_cast<const ARAContentBarSignature*> (eventData));
        case kARAContentTypeStaticTuning:   return encodeReply (*static_cast<const ARAContentTuning*> (eventData));
        case kARAContentTypeKeySignatures:  return encodeReply (*static_cast<const ARAContentKeySignature*> (eventData));
        case kARAContentTypeSheetChords:    return encodeReply (*static_cast<const ARAContentChord*> (eventData));
        default:                            ARA_INTERNAL_ASSERT (false && "content type not implemented yet"); return {};
    }
}

class ARAIPCContentEventDecoder
{
public:
    ARAIPCContentEventDecoder (ARAContentType type)
    : _decoderFunction { _getDecoderFunctionForContentType (type) },
      _contentString { _findStringMember (type) }
    {}

    const void* decode (const IPCMessage& message)
    {
        (this->*_decoderFunction) (message);
        return &_eventStorage;
    }

private:
    using _DecoderFunction = void (ARAIPCContentEventDecoder::*) (const IPCMessage&);

    template<typename ContentT>
    void _decodeContentEvent (const IPCMessage& message)
    {
        decodeReply (*reinterpret_cast<ContentT*> (&this->_eventStorage), message);
        if (this->_contentString != nullptr)
        {
            this->_stringStorage.assign (*this->_contentString);
            *this->_contentString = this->_stringStorage.c_str ();
        }
    }

    static inline
#if __cplusplus >= 201402L
                  constexpr
#endif
                            _DecoderFunction _getDecoderFunctionForContentType (const ARAContentType type)
    {
        switch (type)
        {
            case kARAContentTypeNotes: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeNotes>::DataType>;
            case kARAContentTypeTempoEntries: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeTempoEntries>::DataType>;
            case kARAContentTypeBarSignatures: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeBarSignatures>::DataType>;
            case kARAContentTypeStaticTuning: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeStaticTuning>::DataType>;
            case kARAContentTypeKeySignatures: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeKeySignatures>::DataType>;
            case kARAContentTypeSheetChords: return &ARAIPCContentEventDecoder::_decodeContentEvent<ContentTypeMapper<kARAContentTypeSheetChords>::DataType>;
            default: ARA_INTERNAL_ASSERT (false); return nullptr;
        }
    }

    static inline
#if __cplusplus >= 201402L
                  constexpr
#endif
                            size_t _getStringMemberOffsetForContentType (const ARAContentType type)
    {
        switch (type)
        {
            case kARAContentTypeStaticTuning: return offsetof (ARAContentTuning, name);
            case kARAContentTypeKeySignatures: return offsetof (ARAContentKeySignature, name);
            case kARAContentTypeSheetChords: return offsetof (ARAContentChord, name);
            default: return 0;
        }
    }

    inline ARAUtf8String* _findStringMember (const ARAContentType type)
    {
        const auto offset { ARAIPCContentEventDecoder::_getStringMemberOffsetForContentType (type) };
        if (offset == 0)
            return nullptr;
        return reinterpret_cast<ARAUtf8String*> (reinterpret_cast<uint8_t *> (&this->_eventStorage) + offset);
    }

private:
    _DecoderFunction const _decoderFunction;    // instead of performing the switch (type) for each event,
                                                // we select a templated member function in the c'tor
    union
    {
        ARAContentTempoEntry _tempoEntry;
        ARAContentBarSignature _barSignature;
        ARAContentNote _note;
        ARAContentTuning _tuning;
        ARAContentKeySignature _keySignature;
        ARAContentChord _chord;
    } _eventStorage {};

    ARAUtf8String* _contentString {};
    std::string _stringStorage {};
};

//------------------------------------------------------------------------------
// implementation helpers
//------------------------------------------------------------------------------

// helper base class to create and send messages, decoding reply if applicable
// it's possible to specify IPCMessage as reply type to access an undecoded reply if needed
class ARAIPCMessageSender
{
public:
    ARAIPCMessageSender (IPCPort& port) noexcept : _port { port } {}

    template<typename... Args>
    void remoteCallWithoutReply (const MessageID messageID, const Args &... args)
    {
        _port.sendBlocking (messageID, encodeArguments (args...));
    }
    template<typename RetT, typename... Args>
    void remoteCallWithReply (RetT& result, const MessageID messageID, const Args &... args)
    {
        decodeReply (result, _port.sendAndAwaitReply (messageID, encodeArguments (args...)));
    }

    bool portEndianessMatches () { return _port.endianessMatches (); }

private:
    IPCPort& _port;
};

}   // namespace ARA
