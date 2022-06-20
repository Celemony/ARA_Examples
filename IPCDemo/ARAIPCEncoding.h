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
// This is a brief proof-of-concept demo that hooks up an ARA capable plug-in
// in a separate process using IPC.
// This educational example is not suitable for production code -
// see MainProcess.cpp for a list of issues.
//------------------------------------------------------------------------------

#pragma once

#include "IPCMessage.h"

#include "ARA_API/ARAInterface.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace ARA
{

//------------------------------------------------------------------------------
// various private helpers
//------------------------------------------------------------------------------

// private key to identify methods
constexpr auto _methodIDKey = "methodID";

// private key to identify the element count of an array
constexpr auto _arrayCountKey = "count";

// private key to mark return values for scalar results
constexpr auto _returnValueKey = "result";


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


// private helper to provide reusable static key strings when encoding arrays
inline const char* _getKeyForArrayIndex (size_t index)
{
    static std::vector<std::string> cache;
    for (auto i = cache.size (); i <= index; ++i)
        cache.emplace_back (std::to_string (i));
    return cache[index].c_str ();
}


//------------------------------------------------------------------------------
// private helper templates to encode ARA API values as IPCMessage data and decode back
// mapping is 1:1 except for ARA (host)refs which are encoded as size_t and aggregate types
// (i.e. ARA structs or std::vector<> of types other than ARAByte), which are encoded as sub-messages
// encoding works using templated overloads of the function _encodeValue()
// decoding differs by return type, so it uses an templated _ValueDecoder struct which provides
// both a decode() function and the EncodedType.
//------------------------------------------------------------------------------


// primitive for appending an argument to a message
template <typename ArgT>
void _appendToMessage (IPCMessage& message, const char* argKey, ArgT argValue);

// primitives for reading an (optional) argument from a message
template <typename ArgT>
ArgT _readFromMessage (const IPCMessage& message, const char* argKey);
template <typename ArgT>
std::pair<ArgT, bool> _readOptionalFromMessage (const IPCMessage& message, const char* argKey);


// generic type encodings
template<typename T, typename std::enable_if<!_IsRefType<T>::value, bool>::type = true>
inline T _encodeValue (T value)             // overload for basic types (numbers, strings) and raw bytes
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
    result.append (_arrayCountKey, value.size ());
    for (auto i { 0u }; i < value.size (); ++i)
        result.append (_getKeyForArrayIndex (i), _encodeValue (value[i]));
    return result;
}


// generic type decodings
template<typename ValueT>                   // primary template for basic types (numbers, strings) and ref types
struct _ValueDecoder
{
private:
    template<typename RetT, typename ArgT, typename std::enable_if<std::is_same<RetT, ArgT>::value, bool>::type = true>
    static inline RetT _convertValue (ArgT value)
    {
        return value;
    }
    template<typename RetT, typename ArgT, typename std::enable_if<_IsRefType<RetT>::value, bool>::type = true>
    static inline RetT _convertValue (ArgT value)
    {
        return reinterpret_cast<RetT> (value);
    }

public:
    using EncodedType = typename std::conditional<_IsRefType<ValueT>::value, size_t, ValueT>::type;
    static inline ValueT decode (EncodedType value)
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
        const auto count { _readFromMessage<size_t> (message, _arrayCountKey) };
        result.reserve (count);
        for (auto i { 0u }; i < count; ++i)
            result.push_back (_readFromMessage<ElementT> (message, _getKeyForArrayIndex (i)));
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
    IPCMessage result;
#define _ARA_ENCODE_MEMBER(member)                                                              \
    _appendToMessage (result, #member, data.member);
#define _ARA_ENCODE_EMBEDDED_ARRAY(member)                                                      \
    _appendToMessage (result, #member, std::vector<std::remove_extent<decltype (data.member)>::type> \
                { data.member, data.member + std::extent<decltype (data.member)>::value });
#define _ARA_ENCODE_VARIABLE_ARRAY(member, count)                                               \
    if ((data.count > 0) && (data.member != nullptr)) {                                         \
        using ElementType = typename std::remove_const<std::remove_pointer<decltype (data.member)>::type>::type; \
        _appendToMessage (result, #member, std::vector<ElementType> { data.member, data.member + data.count }); \
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
        _appendToMessage (result, #member, _encodeValue (*data.member));
#define _ARA_END_ENCODE                                                                         \
    return result;                                                                              \
}


#define _ARA_BEGIN_DECODE(StructT)                                                              \
template<> struct _ValueDecoder<StructT>    /* specialization for given struct */               \
{                                                                                               \
    using EncodedType = IPCMessage;                                                             \
    static inline StructT decode (const IPCMessage& message)                                    \
    {                                                                                           \
        StructT result {};
#define _ARA_BEGIN_DECODE_SIZED(StructT)                                                        \
        _ARA_BEGIN_DECODE (StructT)                                                             \
        result.structSize = k##StructT##MinSize;
#define _ARA_DECODE_MEMBER(member)                                                              \
        result.member = _readFromMessage<decltype (result.member)> (message, #member);
#define _ARA_DECODE_EMBEDDED_ARRAY(member)                                                      \
        const auto tmp_##member = _readFromMessage<std::vector<std::remove_extent<decltype (result.member)>::type>> (message, #member); \
        std::memcpy (result.member, tmp_##member.data (), sizeof (result.member));
#define _ARA_DECODE_VARIABLE_ARRAY(member, count, updateCount)                                  \
        const auto tmp_##member { _readOptionalFromMessage<std::vector<typename                 \
                std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type>> (message, #member) }; \
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
        const auto tmp_##member { _readOptionalFromMessage<decltype (result.member)> (message, #member) }; \
        if (tmp_##member.second) {                                                              \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
            result.member = tmp_##member.first;                                                 \
        }
#define _ARA_DECODE_OPTIONAL_STRUCT_PTR(member)                                                 \
        result.member = nullptr;    /* set to null because other members may follow */          \
        const auto tmp_##member { _readOptionalFromMessage<IPCMessage> (message, #member) };     \
        if (tmp_##member.second) {                                                              \
            _ARA_UPDATE_STRUCT_SIZE_FOR_OPTIONAL (member);                                      \
            /* \todo the outer struct contains a pointer to the inner struct, so we need some */\
            /* place to store it - this static only works as long as this is single-threaded! */\
            static std::remove_const<std::remove_pointer<decltype (result.member)>::type>::type cache; \
            cache = _ValueDecoder<decltype(cache)>::decode (tmp_##member.first);                \
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


// ARAAudioAccessControllerInterface::readAudioSamples() must return the output sample data in addition
// to the actual return value, we need a special struct to encode this through IPC.
// The samples are transferred as raw bytes, with an endianness flag to handle swapping if needed.
// \todo If the communication channel could tell the receiver whether swapping was needed,
//       we would not need this struct and could instead just send an array of bytes..
struct ARAIPCReadSamplesReply
{
    ARASize dataCount;      // dataCount == 0 indicates failure, receiver then must zero-out buffers
    const ARAByte* data;
    ARABool isLittleEndian;
};
_ARA_BEGIN_ENCODE (ARAIPCReadSamplesReply)
    _ARA_ENCODE_VARIABLE_ARRAY (data, dataCount)
    _ARA_ENCODE_MEMBER (isLittleEndian)
_ARA_END_ENCODE
_ARA_BEGIN_DECODE (ARAIPCReadSamplesReply)
    _ARA_DECODE_VARIABLE_ARRAY (data, dataCount, true)
    _ARA_DECODE_MEMBER (isLittleEndian)
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
inline void _appendToMessage (IPCMessage& message, const char* argKey, ArgT argValue)
{
    message.append (argKey, _encodeValue (argValue));
}

template <typename ArgT>
inline ArgT _readFromMessage (const IPCMessage& message, const char* argKey)
{
    return _ValueDecoder<ArgT>::decode (message.getArgValue<typename _ValueDecoder<ArgT>::EncodedType> (argKey));
}
template <typename ArgT>
inline std::pair<ArgT, bool> _readOptionalFromMessage (const IPCMessage& message, const char* argKey)
{
    const auto result { message.getOptionalArgValue<typename _ValueDecoder<ArgT>::EncodedType> (argKey) };
    if (result.second)
        return { _ValueDecoder<ArgT>::decode (result.first), true };
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
inline void _encodeArgumentsHelper (IPCMessage& message, size_t n)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, size_t n, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    _appendToMessage (message, _getKeyForArrayIndex (n), argValue);
    _encodeArgumentsHelper (message, n + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsStructPointerArg<ArgT>::type::value, bool>::type = true>
inline void _encodeArgumentsHelper (IPCMessage& message, size_t n, const ArgT& argValue, const MoreArgs &... moreArgs)
{
    if (argValue != nullptr)
        _appendToMessage (message, _getKeyForArrayIndex (n), *argValue);
    _encodeArgumentsHelper (message, n + 1, moreArgs...);
}
template<typename... MoreArgs>
inline void _encodeArgumentsHelper (IPCMessage& message, size_t n, const std::nullptr_t& argValue, const MoreArgs &... moreArgs)
{
    _encodeArgumentsHelper (message, n + 1, moreArgs...);
}

inline void _decodeArgumentsHelper (const IPCMessage& message, size_t n)
{
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<!_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, size_t n, ArgT& argValue, MoreArgs &... moreArgs)
{
    argValue = _readFromMessage<ArgT> (message, _getKeyForArrayIndex (n));
    _decodeArgumentsHelper (message, n + 1, moreArgs...);
}
template<typename ArgT, typename... MoreArgs, typename std::enable_if<_IsOptionalArgument<ArgT>::value, bool>::type = true>
inline void _decodeArgumentsHelper (const IPCMessage& message, size_t n, ArgT& argValue, MoreArgs &... moreArgs)
{
    argValue = _readOptionalFromMessage<typename ArgT::first_type> (message, _getKeyForArrayIndex (n));
    _decodeArgumentsHelper (message, n + 1, moreArgs...);
}


//------------------------------------------------------------------------------
// actual client API
//------------------------------------------------------------------------------

// caller side: create a message with the specified arguments
template<typename... Args>
inline IPCMessage encodeArguments (const char* methodName, const Args &... args)
{
    IPCMessage result;
    _appendToMessage (result, _methodIDKey, methodName);
    _encodeArgumentsHelper (result, 0, args...);
    return result;
}

// caller side: decode the received reply to a sent message
template<typename RetT, typename std::enable_if<std::is_scalar<RetT>::value, bool>::type = true>
inline RetT decodeReply (const IPCMessage& message)
{
    return _readFromMessage<RetT> (message, _returnValueKey);
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, std::vector<uint8_t>>::value, bool>::type = true>
inline RetT decodeReply (const IPCMessage& message)
{
    return _readFromMessage<RetT> (message, 0);
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


// callee side: test whether the given message is encoding the given method call
inline bool isMethodCall (const IPCMessage& message, const char* methodID)
{
    return (0 == std::strcmp (methodID, _readFromMessage<const char*> (message, _methodIDKey)));
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
    _appendToMessage (result, _returnValueKey, data);
    return result;
}
template<typename RetT, typename std::enable_if<std::is_same<RetT, std::vector<uint8_t>>::value, bool>::type = true>
inline IPCMessage encodeReply (const RetT& data)
{
    IPCMessage result;
    _appendToMessage (result, 0, data);
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

}   // namespace ARA
