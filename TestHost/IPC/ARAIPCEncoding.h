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
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace ARA
{

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
// \todo we might choose to use overloads on IPCMessage and just add the
//       type encoding to the function names in the C wrapper,
//       which would allow to just drop these wrappers here.
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
inline void _appendToMessage (IPCMessage& message, const int32_t argKey, const std::vector<uint8_t>& argValue)
{
    message.appendBytes (argKey, argValue);
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
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, std::vector<uint8_t>& argValue)
{
    return message.readBytes (argKey, argValue);
}
inline bool _readFromMessage (const IPCMessage& message, const int32_t argKey, IPCMessage& argValue)
{
    return message.readMessage (argKey, argValue);
}


//------------------------------------------------------------------------------
// private helper templates to encode ARA API values as IPCMessage data and decode back
// mapping is 1:1 except for ARA (host)refs which are encoded as size_t and aggregate types
// (i.e. ARA structs or std::vector<> of types other than ARAByte), which are encoded as sub-messages
// encoding works using templated overloads of the function _encodeValue()
// decoding differs by return type, so it uses an templated _ValueDecoder struct which provides
// both a decode() function and the EncodedType.
//------------------------------------------------------------------------------


// templated wrapper for reading an (optional) argument from a message
template <typename ArgT>
ArgT _decodeValue (const IPCMessage& message, const int32_t argKey);
template <typename ArgT>
std::pair<ArgT, bool> _decodeOptionalValue (const IPCMessage& message, const int32_t argKey);


// generic type encodings
template<typename T, typename std::enable_if<!_IsRefType<T>::value, bool>::type = true>
inline T _encodeValue (const T& value)      // overload for basic types (numbers, strings) and raw bytes
{
    return value;
}
template<typename T, typename std::enable_if<_IsRefType<T>::value, bool>::type = true>
inline size_t _encodeValue (T value)        // overload for ref types
{
    return reinterpret_cast<size_t> (value);
}
template<typename ElementT, typename std::enable_if<!std::is_same<ElementT, uint8_t>::value, bool>::type = true>
inline IPCMessage _encodeValue (const std::vector<ElementT>& value)
{                                           // overload for arrays
    IPCMessage result;
    ARA_INTERNAL_ASSERT (value.size () < static_cast<size_t> (std::numeric_limits<int32_t>::max ()));
    const auto count { static_cast<int32_t> (value.size ()) };
    _appendToMessage (result, 0, count);
    for (auto i { 0 }; i < count; ++i)
        _appendToMessage (result, i + 1, _encodeValue (value[static_cast<size_t> (i)]));
    return result;
}


// generic type decodings
template<typename ValueT>                   // primary template for basic types (numbers, strings) and ref types
struct _ValueDecoder
{
private:
    template<typename RetT, typename ArgT, typename std::enable_if<std::is_same<RetT, ArgT>::value, bool>::type = true>
    static inline RetT _convertValue (const ArgT& value)
    {
        return value;
    }
    template<typename RetT, typename ArgT, typename std::enable_if<_IsRefType<RetT>::value, bool>::type = true>
    static inline RetT _convertValue (const ArgT& value)
    {
        return reinterpret_cast<RetT> (value);
    }

public:
    using EncodedType = typename std::conditional<_IsRefType<ValueT>::value, size_t, ValueT>::type;
    static inline ValueT decode (const EncodedType& value)
    {
        return _ValueDecoder::_convertValue<ValueT, EncodedType> (value);
    }
};

template<typename ElementT>                 // specialization for arrays
struct _ValueDecoder<std::vector<ElementT>>
{
    using EncodedType = IPCMessage;
    static inline std::vector<ElementT> decode (const IPCMessage& message)
    {
        std::vector<ElementT> result;
        const auto count { _decodeValue<int32_t> (message, 0) };
        result.reserve (static_cast<size_t> (count));
        for (auto i { 0 }; i < count; ++i)
            result.emplace_back (_decodeValue<ElementT> (message, i + 1));
        return result;
    }
};

template<>                                  // specialization for raw bytes
struct _ValueDecoder<std::vector<uint8_t>>
{
    using EncodedType = std::vector<uint8_t>;
    static inline EncodedType decode (const EncodedType& value)
    {
        return value;
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
#define _ARA_ENCODE_EMBEDDED_ARRAY(member)                                                      \
    _appendToMessage (result, offsetof (StructType, member), _encodeValue (std::vector<std::remove_extent<decltype (data.member)>::type> \
                { data.member, data.member + std::extent<decltype (data.member)>::value }));
#define _ARA_ENCODE_VARIABLE_ARRAY(member, count)                                               \
    if ((data.count > 0) && (data.member != nullptr)) {                                         \
        using ElementType = typename std::remove_const<std::remove_pointer<decltype (data.member)>::type>::type; \
        _appendToMessage (result, offsetof (StructType, member), _encodeValue (std::vector<ElementType> { data.member, data.member + data.count })); \
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
template<> struct _ValueDecoder<StructT>    /* specialization for given struct */               \
{                                                                                               \
    using StructType = StructT;                                                                 \
    using EncodedType = IPCMessage;                                                             \
    static inline StructT decode (const IPCMessage& message)                                    \
    {                                                                                           \
        StructT result {};
#define _ARA_BEGIN_DECODE_SIZED(StructT)                                                        \
        _ARA_BEGIN_DECODE (StructT)                                                             \
        result.structSize = k##StructT##MinSize;
#define _ARA_DECODE_MEMBER(member)                                                              \
        result.member = _decodeValue<decltype (result.member)> (message, offsetof (StructType, member));
#define _ARA_DECODE_EMBEDDED_ARRAY(member)                                                      \
        const auto tmp_##member = _decodeValue<std::vector<std::remove_extent<decltype (result.member)>::type>> (message, offsetof (StructType, member)); \
        std::memcpy (result.member, tmp_##member.data (), sizeof (result.member));
#define _ARA_DECODE_VARIABLE_ARRAY(member, count, updateCount)                                  \
        const auto tmp_##member { _decodeOptionalValue<std::vector<typename                     \
                std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type>> (message, offsetof (StructType, member)) }; \
        if (tmp_##member.second) {                                                              \
            /* \todo the outer struct contains a pointer to the inner array, so we need some */ \
            /* place to store it - this static only works as long as this is single-threaded! */\
            static decltype (tmp_##member.first) cache;                                         \
            cache = tmp_##member.first;                                                         \
            result.member = cache.data ();                                                      \
            if (updateCount) { result.count = cache.size (); }                                  \
        } else {                                                                                \
            result.member = nullptr;                                                            \
            if (updateCount) { result.count = 0; }                                              \
        }
#define _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL(member)                                            \
            /* \todo ARA_IMPLEMENTED_STRUCT_SIZE decorates the type with the ARA:: namespace, */\
            /* this conflicts with decltype's result - this copied version drops the ARA::    */\
            constexpr auto size { offsetof (decltype (result), member) + sizeof (static_cast<decltype (result)*> (nullptr)->member) }; \
            result.structSize = std::max (result.structSize, size);
#define _ARA_DECODE_OPTIONAL_MEMBER(member)                                                     \
        const auto tmp_##member { _decodeOptionalValue<decltype (result.member)> (message, offsetof (StructType, member)) }; \
        if (tmp_##member.second) {                                                              \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
            result.member = tmp_##member.first;                                                 \
        }
#define _ARA_DECODE_OPTIONAL_STRUCT_PTR(member)                                                 \
        result.member = nullptr;    /* set to null because other members may follow */          \
        const auto tmp_##member { _decodeOptionalValue<IPCMessage> (message, offsetof (StructType, member)) }; \
        if (tmp_##member.second) {                                                              \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
            /* \todo the outer struct contains a pointer to the inner struct, so we need some */\
            /* place to store it - this static only works as long as this is single-threaded! */\
            static std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type cache; \
            cache = _ValueDecoder<decltype (cache)>::decode (tmp_##member.first);               \
            result.member = &cache;                                                             \
        }
#define _ARA_END_DECODE                                                                         \
        return result;                                                                          \
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
    _ARA_ENCODE_EMBEDDED_ARRAY (intervals)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (position)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentKeySignature)
    _ARA_DECODE_MEMBER (root)
    _ARA_DECODE_EMBEDDED_ARRAY (intervals)
    _ARA_DECODE_MEMBER (name)
    _ARA_DECODE_MEMBER (position)
_ARA_END_DECODE

_ARA_BEGIN_ENCODE (ARAContentChord)
    _ARA_ENCODE_MEMBER (root)
    _ARA_ENCODE_MEMBER (bass)
    _ARA_ENCODE_EMBEDDED_ARRAY (intervals)
    _ARA_ENCODE_MEMBER (name)
    _ARA_ENCODE_MEMBER (position)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAContentChord)
    _ARA_DECODE_MEMBER (root)
    _ARA_DECODE_MEMBER (bass)
    _ARA_DECODE_EMBEDDED_ARRAY (intervals)
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
    _ARA_DECODE_MEMBER (plugInName)
    _ARA_DECODE_MEMBER (manufacturerName)
    _ARA_DECODE_MEMBER (informationURL)
    _ARA_DECODE_MEMBER (version)
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
#undef _ARA_ENCODE_EMBEDDED_ARRAY
#undef _ARA_ENCODE_VARIABLE_ARRAY
#undef _ARA_ENCODE_OPTIONAL_MEMBER
#undef _ARA_ENCODE_OPTIONAL_STRUCT_PTR
#undef _ARA_END_ENCODE

#undef _ARA_BEGIN_DECODE
#undef _ARA_BEGIN_DECODE_SIZED
#undef _ARA_DECODE_MEMBER
#undef _ARA_DECODE_EMBEDDED_ARRAY
#undef _ARA_DECODE_VARIABLE_ARRAY
#undef _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL
#undef _ARA_DECODE_OPTIONAL_MEMBER
#undef _ARA_DECODE_OPTIONAL_STRUCT_PTR
#undef _ARA_END_DECODE


//------------------------------------------------------------------------------

// definitions of the primitive templates declare at the start of this header
template <typename ArgT>
inline ArgT _decodeValue (const IPCMessage& message, const int32_t argKey)
{
    typename _ValueDecoder<ArgT>::EncodedType value;
    const auto success { _readFromMessage (message, argKey, value) };
    ARA_INTERNAL_ASSERT (success);
    return _ValueDecoder<ArgT>::decode (value);
}
template <typename ArgT>
inline std::pair<ArgT, bool> _decodeOptionalValue (const IPCMessage& message, const int32_t argKey)
{
    typename _ValueDecoder<ArgT>::EncodedType value;
    if (_readFromMessage (message, argKey, value))
        return { _ValueDecoder<ArgT>::decode (value), true };
    else
        return { {}, false };
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
    argValue = _decodeValue<ArgT> (message, argKey);
    _decodeArgumentsHelper (message, argKey + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, const int32_t argKey, ArgT& argValue, MoreArgs &... moreArgs)
{
    argValue = _decodeOptionalValue<typename ArgT::first_type> (message, argKey);
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
template<typename RetT, typename std::enable_if<std::is_scalar<RetT>::value, bool>::type = true>
inline RetT decodeReply (const IPCMessage& message)
{
    return _decodeValue<RetT> (message, 0);
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, std::vector<uint8_t>>::value, bool>::type = true>
inline RetT decodeReply (const IPCMessage& message)
{
    return _decodeValue<RetT> (message, 0);
}
template<typename RetT, typename std::enable_if<std::is_class<RetT>::value && std::is_pod<RetT>::value, bool>::type = true>
inline RetT decodeReply (const IPCMessage& message)
{
    return _ValueDecoder<RetT>::decode (message);
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, IPCMessage>::value, bool>::type = true>
inline IPCMessage decodeReply (const IPCMessage& message)
{
    return message;
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
template<typename RetT, typename std::enable_if<std::is_scalar<RetT>::value, bool>::type = true>
inline IPCMessage encodeReply (const RetT& data)
{
    IPCMessage result;
    _appendToMessage (result, 0, _encodeValue (data));
    return result;
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, std::vector<uint8_t>>::value, bool>::type = true>
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
        *reinterpret_cast<ContentT*> (&this->_eventStorage) = decodeReply<ContentT> (message);
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
    RetT remoteCallWithReply (const int32_t methodID, const Args &... args)
    {
        return decodeReply<RetT> (_port.sendAndAwaitReply (methodID, encodeArguments (args...)));
    }

    bool portEndianessMatches () { return _port.endianessMatches (); }

private:
    IPCPort& _port;
};

}   // namespace ARA
