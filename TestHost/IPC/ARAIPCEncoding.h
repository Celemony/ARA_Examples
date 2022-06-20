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
class BytesEncoder : public std::function<void (const uint8_t*&, size_t&)>
{
public:
    BytesEncoder (const uint8_t* const bytes, const size_t size)
    : std::function<void (const uint8_t*&, size_t&)> {
        [bytes, size] (const uint8_t*& bytesPtr, size_t& bytesSize) -> void
        {
            bytesPtr = bytes;
            bytesSize = size;
        }}
    {}
    BytesEncoder (const std::vector<uint8_t>& bytes)
    : BytesEncoder { bytes.data (), bytes.size () }
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
#define _ARA_SPECIALIZE_FOR_REF_TYPE(Type)  \
template<>                                  \
struct _IsRefType<Type>                     \
{                                           \
    static constexpr bool value { true };   \
};
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAMusicalContextRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARARegionSequenceRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioSourceRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioModificationRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRegionRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAContentReaderRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARADocumentControllerRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRendererRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAEditorRendererRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAEditorViewRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAPlugInExtensionRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAMusicalContextHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARARegionSequenceHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioSourceHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioModificationHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackRegionHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAContentReaderHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioAccessControllerHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAAudioReaderHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAArchivingControllerHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAArchiveReaderHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAArchiveWriterHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAContentAccessControllerHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAModelUpdateControllerHostRef)
_ARA_SPECIALIZE_FOR_REF_TYPE (ARAPlaybackControllerHostRef)
#undef _ARA_SPECIALIZE_FOR_REF_TYPE


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
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const int32_t argValue)
{
    message.appendInt32 (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const int64_t argValue)
{
    message.appendInt64 (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const size_t argValue)
{
    message.appendSize (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const float argValue)
{
    message.appendFloat (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const double argValue)
{
    message.appendDouble (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const char* const argValue)
{
    message.appendString (argKey, argValue);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const BytesEncoder& argValue)
{
    // \todo we should enable "noCopy" for all call arguments (but leave it off for all replies)
    const uint8_t* bytes;
    size_t size;
    argValue (bytes, size);
    message.appendBytes (argKey, bytes, size);
}
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const IPCMessage& argValue)
{
    message.appendMessage (argKey, argValue);
}

// primitives for reading an (optional) argument from a message
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, int32_t& argValue)
{
    return message.readInt32 (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, int64_t& argValue)
{
    return message.readInt64 (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, size_t& argValue)
{
    return message.readSize (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, float& argValue)
{
    return message.readFloat (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, double& argValue)
{
    return message.readDouble (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, const char*& argValue)
{
    return message.readString (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, BytesDecoder& argValue)
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
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, IPCMessage& argValue)
{
    return message.readMessage (argKey, argValue);
}


//------------------------------------------------------------------------------
// overloads of the IPCMessage primitives for types that can be directly mapped to a primitive type
//------------------------------------------------------------------------------


// templated overloads of the IPCMessage primitives for ARA (host) ref types,
// which are stored as size_t
template<typename T, typename std::enable_if<_IsRefType<T>::value, bool>::type = true>
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const T argValue)
{
    message.appendSize (argKey, reinterpret_cast<size_t> (argValue));
}
template<typename T, typename std::enable_if<_IsRefType<T>::value, bool>::type = true>
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, T& argValue)
{
    // \todo is there a safe/proper way across all compilers for this cast to avoid the copy?
//  return message.readSize (argKey, *reinterpret_cast<size_t*> (&argValue));
    size_t tmp;
    const auto success { message.readSize (argKey, tmp) };
    argValue = reinterpret_cast<T> (tmp);
    return success;
}

/*  instead of using _ARA_ENCODE_EMBEDDED_BYTES below, we could instead allow
    sending arrays of ARABytes via this overload (seems simpler but less efficient):
// to read and write arrays of ARAByte (not raw bytes but e.g. ARAKeySignatureIntervalUsage),
// we use int32_t to keep the IPCMessage API small
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const ARAByte argValue)
{
    message.appendInt32 (argKey, static_cast<int32_t> (argValue));
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, ARAByte& argValue)
{
    int32_t tmp;
    const auto result { message.readInt32 (argKey, tmp) };
    ARA_INTERNAL_ASSERT ((0 <= tmp) && (tmp <= static_cast<int32_t> (std::numeric_limits<ARAByte>::max ())));
    argValue = static_cast<ARAByte> (tmp);
    return result;
}
*/


//------------------------------------------------------------------------------
// private helper templates to encode ARA API values as IPCMessage data and decode back
// The mapping is 1:1 except for ARA (host)refs which are encoded as size_t, and aggregate types
// (i.e. ARA structs or std::vector<> of types other than ARAByte), which are expressed as sub-messages.
// Encoding works by calling templated overloads of the function _encodeValue(), then appending
// the result to a message.
// Decoding basically follows the same pattern, but needs to be split between reading a value
// from the message and decoding an aggregate type, so that such types can be sent as "root"
// messages without a key. It uses a _readValue() function which internally forwards to a
// _ValueDecoder struct to perform the read&decode, but additionally provides a separate decoding
// function if the data is encoded at "root" level.
//------------------------------------------------------------------------------


// generic type encodings (specific overloads for each ARA struct are defined below)

// overloads for basic types (numbers, strings, (host)refs and raw bytes) and nested messages
template<typename T>
inline const T& _encodeValue (const T& value)
{
    return value;
}

// overloads for arrays
template<typename ElementT>
inline IPCMessage _encodeValue (const ArrayArgument<ElementT>& value)
{
    IPCMessage result;
    ARA_INTERNAL_ASSERT (value.count <= static_cast<size_t> (std::numeric_limits<int32_t>::max ()));
    const auto count { static_cast<int32_t> (value.count) };
    _appendToMessage (result, 0, count);
    for (auto i { 0 }; i < count; ++i)
        _appendToMessage (result, i + 1, _encodeValue (value.elements[static_cast<size_t> (i)]));
    return result;
}

// generic type decodings (specializations for each ARA struct are defined below)

// primary template for basic types (numbers, strings, (host)refs and raw bytes) and nested messages
template<typename ValueT>
struct _ValueDecoder
{
    static inline bool readAndDecode (ValueT& result, const IPCMessage& message, const int32_t argKey)
    {
        return _readFromMessage (message, argKey, result);
    }
};
template <typename ArgT>
inline bool _readValue (ArgT& result, const IPCMessage& message, const int32_t argKey)
{
    return _ValueDecoder<ArgT>::readAndDecode (result, message, argKey);
}

// base class for compound types encoded via nested messages, providing the generic readAndDecode()
template<typename ValueT>
struct _CompoundValueDecoderBase
{
    static inline bool readAndDecode (ValueT& result, const IPCMessage& message, const int32_t argKey)
    {
        IPCMessage subMessage;
        if (!_readFromMessage (message, argKey, subMessage))
            return false;
        return _ValueDecoder<ValueT>::decode (result, subMessage);
    }
};

// specialization for fixed-size arrays
template<typename ElementT>
struct _ValueDecoder<ArrayArgument<ElementT>> : public _CompoundValueDecoderBase<ArrayArgument<ElementT>>
{
    static inline bool decode (ArrayArgument<ElementT>& result, const IPCMessage& message)
    {
        bool success { true };
        int32_t count;
        success &= _readValue (count, message, 0);
        success &= (count == static_cast<int32_t> (result.count));
        if (count > static_cast<int32_t> (result.count))
            count = static_cast<int32_t> (result.count);

        for (auto i { 0 }; i < count; ++i)
            success &= _readValue (result.elements[static_cast<size_t> (i)], message, i + 1);
        return success;
    }
};

// specialization for variable arrays
template<typename ElementT>
struct _ValueDecoder<std::vector<ElementT>> : public _CompoundValueDecoderBase<std::vector<ElementT>>
{
    static inline bool decode (std::vector<ElementT>& result, const IPCMessage& message)
    {
        bool success { true };
        int32_t count;
        success &= _readValue (count, message, 0);
        result.resize (static_cast<size_t> (count));
        for (auto i { 0 }; i < count; ++i)
            success &= _readValue (result[static_cast<size_t> (i)], message, i + 1);
        return success;
    }
};


// en/decoding of compound types

#define _ARA_BEGIN_ENCODE(StructT)                                                              \
inline IPCMessage _encodeValue (const StructT& data)                                            \
{                                           /* overload for given struct */                     \
    using StructType = StructT;                                                                 \
    IPCMessage result;
#define _ARA_ENCODE_MEMBER(member)                                                              \
    _appendToMessage (result, offsetof (StructType, member), _encodeValue (data.member));
#define _ARA_ENCODE_EMBEDDED_BYTES(member)                                                      \
    const BytesEncoder tmp_##member { reinterpret_cast<const uint8_t*> (data.member), sizeof (data.member) }; \
    _appendToMessage (result, offsetof (StructType, member), _encodeValue (tmp_##member));
#define _ARA_ENCODE_EMBEDDED_ARRAY(member)                                                      \
    const ArrayArgument<const std::remove_extent<decltype (data.member)>::type> tmp_##member { data.member, std::extent<decltype (data.member)>::value }; \
    _appendToMessage (result, offsetof (StructType, member), _encodeValue (tmp_##member));
#define _ARA_ENCODE_VARIABLE_ARRAY(member, count)                                               \
    if ((data.count > 0) && (data.member != nullptr)) {                                         \
        const ArrayArgument<const std::remove_pointer<decltype (data.member)>::type> tmp_##member { data.member, data.count }; \
        _appendToMessage (result, offsetof (StructType, member), _encodeValue (tmp_##member));  \
    }
#define _ARA_HAS_OPTIONAL_MEMBER(member)                                                        \
        /* \todo ARA_IMPLEMENTS_FIELD decorates the type with the ARA:: namespace,     */       \
        /* this conflicts with decltype's result - this copied version drops the ARA:: */       \
        (data.structSize > offsetof (std::remove_reference<decltype (data)>::type, member))
#define _ARA_ENCODE_OPTIONAL_MEMBER(member)                                                     \
    if (_ARA_HAS_OPTIONAL_MEMBER (member))                                                      \
        _ARA_ENCODE_MEMBER (member)
#define _ARA_ENCODE_OPTIONAL_STRUCT_PTR(member)                                                 \
    if (_ARA_HAS_OPTIONAL_MEMBER (member) && (data.member != nullptr))                          \
        _appendToMessage (result, offsetof (StructType, member), _encodeValue (*data.member));
#define _ARA_END_ENCODE                                                                         \
    return result;                                                                              \
}


#define _ARA_BEGIN_DECODE(StructT)                                                              \
template<> struct _ValueDecoder<StructT> : public _CompoundValueDecoderBase<StructT>            \
{                                           /* specialization for given struct */               \
    using StructType = StructT;                                                                 \
    static inline bool decode (StructType& result, const IPCMessage& message)                   \
    {                                                                                           \
        bool success { true };
#define _ARA_BEGIN_DECODE_SIZED(StructT)                                                        \
        _ARA_BEGIN_DECODE (StructT)                                                             \
        result.structSize = k##StructT##MinSize;
#define _ARA_DECODE_MEMBER(member)                                                              \
        success &= _readValue (result.member, message, offsetof (StructType, member));          \
        ARA_INTERNAL_ASSERT (success);
#define _ARA_DECODE_EMBEDDED_BYTES(member)                                                      \
        auto resultSize_##member { sizeof (result.member) };                                    \
        BytesDecoder tmp_##member { reinterpret_cast<uint8_t*> (result.member), resultSize_##member }; \
        success &= _readValue (tmp_##member, message, offsetof (StructType, member));           \
        success &= (resultSize_##member == sizeof (result.member));                             \
        ARA_INTERNAL_ASSERT (success);
#define _ARA_DECODE_EMBEDDED_ARRAY(member)                                                      \
        ArrayArgument<std::remove_extent<decltype (result.member)>::type> tmp_##member { result.member, std::extent<decltype (result.member)>::value }; \
        success &= _readValue (tmp_##member, message, offsetof (StructType, member));           \
        ARA_INTERNAL_ASSERT (success);
#define _ARA_DECODE_VARIABLE_ARRAY(member, count, updateCount)                                  \
        /* \todo the outer struct contains a pointer to the inner array, so we need some */     \
        /* place to store it - this static only works as long as this is single-threaded! */    \
        static std::vector<typename std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type> tmp_##member; \
        if (_readValue (tmp_##member, message, offsetof (StructType, member))) {                \
            result.member = tmp_##member.data ();                                               \
            if (updateCount) { result.count = tmp_##member.size (); }                           \
        } else {                                                                                \
            result.member = nullptr;                                                            \
            if (updateCount) { result.count = 0; }                                              \
        }
#define _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL(member)                                            \
            /* \todo ARA_IMPLEMENTED_STRUCT_SIZE decorates the type with the ARA:: namespace, */\
            /* conflicting with the local alias StructType - this copy simply drops the ARA:: */\
            constexpr auto size { offsetof (StructType, member) + sizeof (static_cast<StructType*> (nullptr)->member) }; \
            result.structSize = std::max (result.structSize, size);
#define _ARA_DECODE_OPTIONAL_MEMBER(member)                                                     \
        if (_readValue (result.member, message, offsetof (StructType, member))) {               \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
        }
#define _ARA_DECODE_OPTIONAL_STRUCT_PTR(member)                                                 \
        result.member = nullptr;    /* set to null because other members may follow */          \
        IPCMessage tmp_##member;                                                                \
        if (_readValue (tmp_##member, message, offsetof (StructType, member))) {                \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
            /* \todo the outer struct contains a pointer to the inner struct, so we need some */\
            /* place to store it - this static only works as long as this is single-threaded! */\
            static std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type cache; \
            success &= _ValueDecoder<decltype (cache)>::decode (cache, tmp_##member);           \
            ARA_INTERNAL_ASSERT (success);                                                      \
            result.member = &cache;                                                             \
        }
#define _ARA_END_DECODE                                                                         \
        return success;                                                                         \
    }                                                                                           \
};


_ARA_BEGIN_ENCODE (ARAColor)
    _ARA_ENCODE_MEMBER (r)
    _ARA_ENCODE_MEMBER (g)
    _ARA_ENCODE_MEMBER (b)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAColor)
    _ARA_DECODE_MEMBER (r)
    _ARA_DECODE_MEMBER (g)
    _ARA_DECODE_MEMBER (b)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARADocumentProperties)
    _ARA_ENCODE_MEMBER (name)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARADocumentProperties)
    _ARA_DECODE_MEMBER (name)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAMusicalContextProperties)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_OPTIONAL_MEMBER (orderIndex)
    _ARA_ENCODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAMusicalContextProperties)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_OPTIONAL_MEMBER (orderIndex)
    _ARA_DECODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARARegionSequenceProperties)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (orderIndex)
    _ARA_ENCODE_MEMBER (musicalContextRef)
    _ARA_ENCODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARARegionSequenceProperties)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (orderIndex)
    _ARA_DECODE_MEMBER (musicalContextRef)
    _ARA_DECODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAAudioSourceProperties)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (persistentID)
    _ARA_ENCODE_MEMBER (sampleCount)
    _ARA_ENCODE_MEMBER (sampleRate)
    _ARA_ENCODE_MEMBER (channelCount)
    _ARA_ENCODE_MEMBER (merits64BitSamples)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAAudioSourceProperties)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (persistentID)
    _ARA_DECODE_MEMBER (sampleCount)
    _ARA_DECODE_MEMBER (sampleRate)
    _ARA_DECODE_MEMBER (channelCount)
    _ARA_DECODE_MEMBER (merits64BitSamples)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAAudioModificationProperties)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (persistentID)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAAudioModificationProperties)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (persistentID)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAPlaybackRegionProperties)
    _ARA_ENCODE_MEMBER (transformationFlags)
    _ARA_ENCODE_MEMBER (startInModificationTime)
    _ARA_ENCODE_MEMBER (durationInModificationTime)
    _ARA_ENCODE_MEMBER (startInPlaybackTime)
    _ARA_ENCODE_MEMBER (durationInPlaybackTime)
    _ARA_ENCODE_MEMBER (musicalContextRef)
    _ARA_ENCODE_OPTIONAL_MEMBER (regionSequenceRef)
    _ARA_ENCODE_OPTIONAL_MEMBER (name)
    _ARA_ENCODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAPlaybackRegionProperties)
    _ARA_DECODE_MEMBER (transformationFlags)
    _ARA_DECODE_MEMBER (startInModificationTime)
    _ARA_DECODE_MEMBER (durationInModificationTime)
    _ARA_DECODE_MEMBER (startInPlaybackTime)
    _ARA_DECODE_MEMBER (durationInPlaybackTime)
    _ARA_DECODE_MEMBER (musicalContextRef)
    _ARA_DECODE_OPTIONAL_MEMBER (regionSequenceRef)
    _ARA_DECODE_OPTIONAL_MEMBER (name)
    _ARA_DECODE_OPTIONAL_STRUCT_PTR (color)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentTimeRange)
    _ARA_ENCODE_MEMBER (start)
    _ARA_ENCODE_MEMBER (duration)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentTimeRange)
    _ARA_DECODE_MEMBER (start)
    _ARA_DECODE_MEMBER (duration)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentTempoEntry)
    _ARA_ENCODE_MEMBER (timePosition)
    _ARA_ENCODE_MEMBER (quarterPosition)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentTempoEntry)
    _ARA_DECODE_MEMBER (timePosition)
    _ARA_DECODE_MEMBER (quarterPosition)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentBarSignature)
    _ARA_ENCODE_MEMBER (numerator)
    _ARA_ENCODE_MEMBER (denominator)
    _ARA_ENCODE_MEMBER (position)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentBarSignature)
    _ARA_DECODE_MEMBER (numerator)
    _ARA_DECODE_MEMBER (denominator)
    _ARA_DECODE_MEMBER (position)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentNote)
    _ARA_ENCODE_MEMBER (frequency)
    _ARA_ENCODE_MEMBER (pitchNumber)
    _ARA_ENCODE_MEMBER (volume)
    _ARA_ENCODE_MEMBER (startPosition)
    _ARA_ENCODE_MEMBER (attackDuration)
    _ARA_ENCODE_MEMBER (noteDuration)
    _ARA_ENCODE_MEMBER (signalDuration)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentNote)
    _ARA_DECODE_MEMBER (frequency)
    _ARA_DECODE_MEMBER (pitchNumber)
    _ARA_DECODE_MEMBER (volume)
    _ARA_DECODE_MEMBER (startPosition)
    _ARA_DECODE_MEMBER (attackDuration)
    _ARA_DECODE_MEMBER (noteDuration)
    _ARA_DECODE_MEMBER (signalDuration)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentTuning)
    _ARA_ENCODE_MEMBER (concertPitchFrequency)
    _ARA_ENCODE_MEMBER (root)
    _ARA_ENCODE_EMBEDDED_ARRAY (tunings)
    _ARA_ENCODE_MEMBER (name)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentTuning)
    _ARA_DECODE_MEMBER (concertPitchFrequency)
    _ARA_DECODE_MEMBER (root)
    _ARA_DECODE_EMBEDDED_ARRAY (tunings)
    _ARA_DECODE_MEMBER (name)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentKeySignature)
    _ARA_ENCODE_MEMBER (root)
    _ARA_ENCODE_EMBEDDED_BYTES (intervals)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (position)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentKeySignature)
    _ARA_DECODE_MEMBER (root)
    _ARA_DECODE_EMBEDDED_BYTES (intervals)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (position)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentChord)
    _ARA_ENCODE_MEMBER (root)
    _ARA_ENCODE_MEMBER (bass)
    _ARA_ENCODE_EMBEDDED_BYTES (intervals)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (position)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentChord)
    _ARA_DECODE_MEMBER (root)
    _ARA_DECODE_MEMBER (bass)
    _ARA_DECODE_EMBEDDED_BYTES (intervals)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (position)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARARestoreObjectsFilter)
    _ARA_ENCODE_MEMBER (documentData)
    _ARA_ENCODE_VARIABLE_ARRAY (audioSourceArchiveIDs, audioSourceIDsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (audioSourceCurrentIDs, audioSourceIDsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (audioModificationArchiveIDs, audioModificationIDsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (audioModificationCurrentIDs, audioModificationIDsCount)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARARestoreObjectsFilter)
    _ARA_DECODE_MEMBER (documentData)
    _ARA_DECODE_VARIABLE_ARRAY (audioSourceArchiveIDs, audioSourceIDsCount, true)
    _ARA_DECODE_VARIABLE_ARRAY (audioSourceCurrentIDs, audioSourceIDsCount, false)
    _ARA_DECODE_VARIABLE_ARRAY (audioModificationArchiveIDs, audioModificationIDsCount, true)
    _ARA_DECODE_VARIABLE_ARRAY (audioModificationCurrentIDs, audioModificationIDsCount, false)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAStoreObjectsFilter)
    _ARA_ENCODE_MEMBER (documentData)
    _ARA_ENCODE_VARIABLE_ARRAY (audioSourceRefs, audioSourceRefsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (audioModificationRefs, audioModificationRefsCount)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAStoreObjectsFilter)
    _ARA_DECODE_MEMBER (documentData)
    _ARA_DECODE_VARIABLE_ARRAY (audioSourceRefs, audioSourceRefsCount, true)
    _ARA_DECODE_VARIABLE_ARRAY (audioModificationRefs, audioModificationRefsCount, true)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAProcessingAlgorithmProperties)
    _ARA_ENCODE_MEMBER (persistentID)
    _ARA_ENCODE_MEMBER (name)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAProcessingAlgorithmProperties)
    _ARA_DECODE_MEMBER (persistentID)
    _ARA_DECODE_MEMBER (name)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAViewSelection)
    _ARA_ENCODE_VARIABLE_ARRAY (playbackRegionRefs, playbackRegionRefsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (regionSequenceRefs, regionSequenceRefsCount)
    _ARA_ENCODE_OPTIONAL_STRUCT_PTR (timeRange)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAViewSelection)
    _ARA_DECODE_VARIABLE_ARRAY (playbackRegionRefs, playbackRegionRefsCount, true)
    _ARA_DECODE_VARIABLE_ARRAY (regionSequenceRefs, regionSequenceRefsCount, true)
    _ARA_DECODE_OPTIONAL_STRUCT_PTR (timeRange)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAFactory)
    _ARA_ENCODE_MEMBER (lowestSupportedApiGeneration)
    _ARA_ENCODE_MEMBER (highestSupportedApiGeneration)
    _ARA_ENCODE_MEMBER (factoryID)
    _ARA_ENCODE_MEMBER (plugInName)
    _ARA_ENCODE_MEMBER (manufacturerName)
    _ARA_ENCODE_MEMBER (informationURL)
    _ARA_ENCODE_MEMBER (version)
    _ARA_ENCODE_MEMBER (documentArchiveID)
    _ARA_ENCODE_VARIABLE_ARRAY (compatibleDocumentArchiveIDs, compatibleDocumentArchiveIDsCount)
    _ARA_ENCODE_VARIABLE_ARRAY (analyzeableContentTypes, analyzeableContentTypesCount)
    _ARA_ENCODE_MEMBER (supportedPlaybackTransformationFlags)
    _ARA_ENCODE_OPTIONAL_MEMBER (supportsStoringAudioFileChunks)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE_SIZED (ARAFactory)
    _ARA_DECODE_MEMBER (lowestSupportedApiGeneration)
    _ARA_DECODE_MEMBER (highestSupportedApiGeneration)
    _ARA_DECODE_MEMBER (factoryID)
    result.initializeARAWithConfiguration = nullptr;
    result.uninitializeARA = nullptr;
    _ARA_DECODE_MEMBER (plugInName)
    _ARA_DECODE_MEMBER (manufacturerName)
    _ARA_DECODE_MEMBER (informationURL)
    _ARA_DECODE_MEMBER (version)
    result.createDocumentControllerWithDocument = nullptr;
    _ARA_DECODE_MEMBER (documentArchiveID)
    _ARA_DECODE_VARIABLE_ARRAY (compatibleDocumentArchiveIDs, compatibleDocumentArchiveIDsCount, true)
    _ARA_DECODE_VARIABLE_ARRAY (analyzeableContentTypes, analyzeableContentTypesCount, true)
    _ARA_DECODE_MEMBER (supportedPlaybackTransformationFlags)
    _ARA_DECODE_OPTIONAL_MEMBER (supportsStoringAudioFileChunks)
_ARA_END_DECODE


// ARADocumentControllerInterface::storeAudioSourceToAudioFileChunk() must return the documentArchiveID and the
// openAutomatically flag in addition to the return value, we need a special struct to encode this through IPC.
struct ARAIPCStoreAudioSourceToAudioFileChunkReply
{
    ARABool result;
    ARAPersistentID documentArchiveID;
    ARABool openAutomatically;
};
_ARA_BEGIN_ENCODE (ARAIPCStoreAudioSourceToAudioFileChunkReply)
    _ARA_ENCODE_MEMBER (result)
    _ARA_ENCODE_MEMBER (documentArchiveID)
    _ARA_ENCODE_MEMBER (openAutomatically)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAIPCStoreAudioSourceToAudioFileChunkReply)
    _ARA_DECODE_MEMBER (result)
    _ARA_DECODE_MEMBER (documentArchiveID)
    _ARA_DECODE_MEMBER (openAutomatically)
_ARA_END_DECODE

// ARADocumentControllerInterface::getPlaybackRegionHeadAndTailTime() must return both head- and tailtime.
struct ARAIPCGetPlaybackRegionHeadAndTailTimeReply
{
    ARATimeDuration headTime;
    ARATimeDuration tailTime;
};
_ARA_BEGIN_ENCODE (ARAIPCGetPlaybackRegionHeadAndTailTimeReply)
    _ARA_ENCODE_MEMBER (headTime)
    _ARA_ENCODE_MEMBER (tailTime)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAIPCGetPlaybackRegionHeadAndTailTimeReply)
    _ARA_DECODE_MEMBER (headTime)
    _ARA_DECODE_MEMBER (tailTime)
_ARA_END_DECODE


#undef _ARA_BEGIN_ENCODE
#undef _ARA_HAS_OPTIONAL_MEMBER
#undef _ARA_ENCODE_MEMBER
#undef _ARA_ENCODE_EMBEDDED_BYES
#undef _ARA_ENCODE_EMBEDDED_ARRAY
#undef _ARA_ENCODE_VARIABLE_ARRAY
#undef _ARA_ENCODE_OPTIONAL_MEMBER
#undef _ARA_ENCODE_OPTIONAL_STRUCT_PTR
#undef _ARA_END_ENCODE

#undef _ARA_BEGIN_DECODE
#undef _ARA_BEGIN_DECODE_SIZED
#undef _ARA_DECODE_MEMBER
#undef _ARA_DECODE_EMBEDDED_BYTES
#undef _ARA_DECODE_EMBEDDED_ARRAY
#undef _ARA_DECODE_VARIABLE_ARRAY
#undef _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL
#undef _ARA_DECODE_OPTIONAL_MEMBER
#undef _ARA_DECODE_OPTIONAL_STRUCT_PTR
#undef _ARA_END_DECODE

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
inline void _encodeArgumentsHelper (IPCMessage& /*message*/, int32_t /*argKey*/)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, const int32_t argKey, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    _appendToMessage (message, argKey, _encodeValue (argValue));
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, const int32_t argKey, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    if (argValue != nullptr)
        _appendToMessage (message, argKey, _encodeValue (*argValue));
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename... MoreArgs>
inline void _encodeArgumentsHelper (IPCMessage& message, const int32_t argKey, const std::nullptr_t& /*argValue*/, const MoreArgs &... moreArgs)
{
    _encodeArgumentsHelper (message, argKey + 1, moreArgs...);
}

inline void _decodeArgumentsHelper (const IPCMessage& /*message*/, int32_t /*argKey*/)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, const int32_t argKey, ArgT& argValue, MoreArgs &... moreArgs)
{
    _readValue (argValue, message, argKey);
    _decodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, const int32_t argKey, ArgT& argValue, MoreArgs &... moreArgs)
{
    argValue.second = _readValue (argValue.first, message, argKey);
    _decodeArgumentsHelper (message, argKey + 1, moreArgs...);
}

// private helpers for HOST_METHOD_ID and PLUGIN_METHOD_ID
template<typename StructT>
constexpr int32_t _getHostInterfaceID ();
template<>
constexpr int32_t _getHostInterfaceID<ARAAudioAccessControllerInterface> () { return 0; }
template<>
constexpr int32_t _getHostInterfaceID<ARAArchivingControllerInterface> () { return 1; }
template<>
constexpr int32_t _getHostInterfaceID<ARAContentAccessControllerInterface> () { return 2; }
template<>
constexpr int32_t _getHostInterfaceID<ARAModelUpdateControllerInterface> () { return 3; }
template<>
constexpr int32_t _getHostInterfaceID<ARAPlaybackControllerInterface> () { return 4; }

template<typename StructT>
constexpr int32_t _getPlugInInterfaceID ();
template<>
constexpr int32_t _getPlugInInterfaceID<ARADocumentControllerInterface> () { return 0; }
template<>
constexpr int32_t _getPlugInInterfaceID<ARAPlaybackRendererInterface> () { return 1; }
template<>
constexpr int32_t _getPlugInInterfaceID<ARAEditorRendererInterface> () { return 2; }
template<>
constexpr int32_t _getPlugInInterfaceID<ARAEditorViewInterface> () { return 3; }

template<int32_t interfaceID, size_t offset>
constexpr int32_t _encodeMessageID ()
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
#define HOST_METHOD_ID(StructT, member) _encodeMessageID <_getHostInterfaceID<StructT> (), offsetof (StructT, member)> ()
#define PLUGIN_METHOD_ID(StructT, member) _encodeMessageID <_getPlugInInterfaceID<StructT> (), offsetof (StructT, member)> ()

// "global" messages that are not passed based on interface structs
constexpr int32_t kGetFactoryMethodID { 1 };
constexpr int32_t kCreateDocumentControllerMethodID { 2 };


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
    return _readValue (result, message, 0);
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
template<typename RetT, typename std::enable_if<!std::is_class<RetT>::value || !std::is_pod<RetT>::value, bool>::type = true>
inline IPCMessage encodeReply (const RetT& data)
{
    IPCMessage result;
    _appendToMessage (result, 0, _encodeValue (data));
    return result;
}
template<typename RetT, typename std::enable_if<std::is_class<RetT>::value && std::is_pod<RetT>::value, bool>::type = true>
inline IPCMessage encodeReply (const RetT& data)
{
    return _encodeValue (data);
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, IPCMessage>::value, bool>::type = true>
inline IPCMessage encodeReply (const IPCMessage& data)
{
    return data;
}


// for debugging only: decoding method IDs
inline const char* _decodeMethodID (std::map<int32_t, std::string>& cache, const char* interfaceName, const int32_t methodID)
{
    auto it { cache.find (methodID) };
    if (it == cache.end ())
        it = cache.emplace (methodID, std::string { interfaceName } + " method " + std::to_string (methodID >> 3)).first;
    return it->second.c_str();
}
inline const char* decodeHostMethodID (const int32_t messageID)
{
    static std::map<int32_t, std::string> cache;
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
    return _decodeMethodID (cache, interfaceName, messageID);
}
inline const char* decodePlugInMethodID (const int32_t messageID)
{
    static std::map<int32_t, std::string> cache;
    const char* interfaceName;
    switch (messageID & 0x7)
    {
        case 0: interfaceName = "ARADocumentControllerInterface"; break;
        case 1: interfaceName = "ARAPlaybackRendererInterface"; break;
        case 2: interfaceName = "ARAEditorRendererInterface"; break;
        case 3: interfaceName = "ARAEditorViewInterface"; break;
        default: ARA_INTERNAL_ASSERT (false); interfaceName = "(unknown)"; break;
    }
    return _decodeMethodID (cache, interfaceName, messageID);
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
    : _decoderFunction { _getDecoderFunctionForContentType (type) }
    {}

    const void* decode (const IPCMessage& message)
    {
        (this->*_decoderFunction) (message);
        return &_eventStorage;
    }

private:
    using _DecoderFunction = void (ARAIPCContentEventDecoder::*) (const IPCMessage&);

    template<typename ContentT>
    void _copyEventStringIfNeeded ()
    {}
// \todo is there a way to use decltype(memberString) instead of additionally passing in the type?
#define _SPECIALIZE_COPY_EVENT_STRING_IF_NEEEDED(ContentT, memberString) \
    template<>                                                      \
    void _copyEventStringIfNeeded<ContentT> ()                      \
    {                                                               \
        if (_eventStorage.memberString)                             \
        {                                                           \
            _stringStorage.assign (_eventStorage.memberString);     \
            _eventStorage.memberString = _stringStorage.c_str ();   \
        }                                                           \
    }
    _SPECIALIZE_COPY_EVENT_STRING_IF_NEEEDED (ARAContentTuning, _tuning.name)
    _SPECIALIZE_COPY_EVENT_STRING_IF_NEEEDED (ARAContentKeySignature, _keySignature.name)
    _SPECIALIZE_COPY_EVENT_STRING_IF_NEEEDED (ARAContentChord, _chord.name)
#undef _SPECIALIZE_COPY_EVENT_STRING_IF_NEEEDED

    template<typename ContentT>
    void _decodeContentEvent (const IPCMessage& message)
    {
        decodeReply (*reinterpret_cast<ContentT*> (&this->_eventStorage), message);
        _copyEventStringIfNeeded<ContentT> ();
    }

    _DecoderFunction _getDecoderFunctionForContentType (ARAContentType type)
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
    void remoteCallWithoutReply (const int32_t methodID, const Args &... args)
    {
        _port.sendBlocking (methodID, encodeArguments (args...));
    }
    template<typename RetT, typename... Args>
    void remoteCallWithReply (RetT& result, const int32_t methodID, const Args &... args)
    {
        decodeReply (result, _port.sendAndAwaitReply (methodID, encodeArguments (args...)));
    }

    bool portEndianessMatches () { return _port.endianessMatches (); }

private:
    IPCPort& _port;
};

}   // namespace ARA
