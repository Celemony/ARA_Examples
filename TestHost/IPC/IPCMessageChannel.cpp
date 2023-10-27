//------------------------------------------------------------------------------
//! \file       IPCMessageChannel.cpp
//!             Proof-of-concept implementation of ARAIPCMessageChannel
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

#include "IPCMessageChannel.h"
#include "ARA_Library/Debug/ARADebug.h"

#if USE_ARA_CF_ENCODING
    #include "ARA_Library/IPC/ARAIPCCFEncoding.h"
#else
    #include "IPC/IPCXMLEncoding.h"
#endif


#if defined (__APPLE__)
    #include <sys/posix_shm.h>
    #include <sys/stat.h>
#endif


#if defined (NDEBUG)
    constexpr auto messageTimeout { 500 /* milliseconds */};
#else
    // increase to 5 min while debugging so that stopping in the debugger does not break program flow
    constexpr auto messageTimeout { 5 * 60 * 1000 };
#endif

//------------------------------------------------------------------------------
#if defined (_WIN32)
//------------------------------------------------------------------------------

IPCMessageChannel::~IPCMessageChannel ()
{
    if (_sharedMemory)
        ::UnmapViewOfFile (_sharedMemory);
    if (_fileMapping)
        ::CloseHandle (_fileMapping);
    if (_dataReceived)
        ::CloseHandle (_dataReceived);
    if (_dataAvailable)
        ::CloseHandle (_dataAvailable);
    if (_writeLock)
        ::CloseHandle (_writeLock);

    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);
    ARA_INTERNAL_ASSERT (_callbackLevel == 0);
    ARA_INTERNAL_ASSERT (!_awaitsReply);
    ARA_INTERNAL_ASSERT (_replyHandler == nullptr);
}

IPCMessageChannel::IPCMessageChannel (const std::string& channelID)
{
    _writeLock = ::CreateMutexA (NULL, FALSE, (std::string { "Write" } + channelID).c_str ());
    ARA_INTERNAL_ASSERT (_writeLock != nullptr);
    _dataAvailable = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Available" } + channelID).c_str ());
    ARA_INTERNAL_ASSERT (_dataAvailable != nullptr);
    _dataReceived = ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Received" } + channelID).c_str ());
    ARA_INTERNAL_ASSERT (_dataReceived != nullptr);
}

IPCMessageChannel* IPCMessageChannel::createPublishingID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel { channelID } };
    channel->_receiveCallback = callback;

    const auto mapKey { std::string { "Map" } + channelID };
    channel->_fileMapping = ::CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (SharedMemory), mapKey.c_str ());
    ARA_INTERNAL_ASSERT (channel->_fileMapping != nullptr);
    channel->_sharedMemory = (SharedMemory*) ::MapViewOfFile (channel->_fileMapping, FILE_MAP_WRITE, 0, 0, sizeof (SharedMemory));
    ARA_INTERNAL_ASSERT (channel->_sharedMemory != nullptr);

    return channel;
}

IPCMessageChannel* IPCMessageChannel::createConnectedToID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel { channelID } };
    channel->_receiveCallback = callback;

    const auto mapKey { std::string { "Map" } + channelID };
    while (!channel->_fileMapping)
    {
        ::Sleep (100);
        channel->_fileMapping = ::OpenFileMappingA (FILE_MAP_WRITE, FALSE, mapKey.c_str ());
    }
    channel->_sharedMemory = (SharedMemory*) ::MapViewOfFile (channel->_fileMapping, FILE_MAP_WRITE, 0, 0, 0);
    ARA_INTERNAL_ASSERT (channel->_sharedMemory != nullptr);

    return channel;
}

void IPCMessageChannel::_sendRequest (const ARA::IPC::ARAIPCMessageID messageID, const std::string& messageData)
{
    _readLock.lock ();
    _sharedMemory->messageID = messageID;
    _sharedMemory->messageSize = messageData.size ();
    std::memcpy (_sharedMemory->messageData, messageData.c_str (), messageData.size ());

    ::SetEvent (_dataAvailable);
    const auto waitResult { ::WaitForSingleObject (_dataReceived, messageTimeout) };
    ARA_INTERNAL_ASSERT (waitResult == WAIT_OBJECT_0);
    _readLock.unlock ();
}

void IPCMessageChannel::sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder,
                              ReplyHandler const replyHandler, void* replyHandlerUserData)
{
    const auto messageData { static_cast<const IPCXMLMessageEncoder*> (encoder)->createEncodedMessage () };
    ARA_INTERNAL_ASSERT (messageData.size () <= SharedMemory::maxMessageSize);

    const bool isOnCreationThread { std::this_thread::get_id () == _creationThreadID };
    const bool needsLock { !isOnCreationThread || (_callbackLevel == 0) };
//  ARA_LOG ("IPCMessageChannel sends message %i%s%s", messageID, (!isOnCreationThread) ? " from other thread" : "",
//                                           (needsLock) ? " starting new transaction" : (_callbackLevel > 0) ? " while handling message" : "");
    if (needsLock)
    {
        if (isOnCreationThread)
        {
            while (true)
            {
                const auto waitWriteMutex { ::WaitForSingleObject (_writeLock, 0) };
                if (waitWriteMutex == WAIT_OBJECT_0)
                    break;

                ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_TIMEOUT);
                runReceiveLoop (1);
            }
        }
        else
        {
            const auto waitWriteMutex { ::WaitForSingleObject (_writeLock, messageTimeout) };
            ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_OBJECT_0);
        }
    }

    _sendRequest (messageID, messageData);
    delete encoder;

    const auto previousAwaitsReply { _awaitsReply };
    const auto previousReplyHandler { _replyHandler };
    const auto previousReplyHandlerUserData { _replyHandlerUserData };
    _awaitsReply = true;
    _replyHandler = replyHandler;
    _replyHandlerUserData = replyHandlerUserData;
    do
    {
        if (isOnCreationThread)
            runReceiveLoop (messageTimeout);
        else
            std::this_thread::yield (); // \todo maybe it would be better to sleep for some longer interval here instead of simply yielding?
    } while (_awaitsReply);
    _awaitsReply = previousAwaitsReply;
    _replyHandler = previousReplyHandler;
    _replyHandlerUserData = previousReplyHandlerUserData;
//  ARA_LOG ("IPCMessageChannel received reply to message %i%s", messageID, (needsLock) ? " ending transaction" : "");

    if (needsLock)
        ::ReleaseMutex (_writeLock);
}

ARA::IPC::ARAIPCMessageEncoder* IPCMessageChannel::createEncoder ()
{
    return new IPCXMLMessageEncoder {};
}

void IPCMessageChannel::runReceiveLoop (int32_t milliseconds)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);

    const auto waitRequest { ::WaitForSingleObject (_dataAvailable, milliseconds) };
    if (waitRequest == WAIT_TIMEOUT)
        return;
    ARA_INTERNAL_ASSERT (waitRequest == WAIT_OBJECT_0);

    if (!_readLock.try_lock ())    // if we're concurrently sending from another thread, back out and re-set _dataAvailable
    {
        ::SetEvent (_dataAvailable);
        return;
    }

    const auto messageID { _sharedMemory->messageID };
    const std::pair <const char*, size_t> messageData { _sharedMemory->messageData, _sharedMemory->messageSize };
    ::SetEvent (_dataReceived);

    if (messageID != 0)
    {
        ++_callbackLevel;

//      ARA_LOG ("IPCMessageChannel received message with ID %i%s", messageID, (_awaitsReply) ? " while awaiting reply" : "");
        const auto decoder { IPCXMLMessageDecoder::createWithMessageData (messageData.first, messageData.second) };
        auto replyEncoder { createEncoder () };
        _receiveCallback (this, messageID, decoder, replyEncoder);

//      ARA_LOG ("IPCMessageChannel replies to message with ID %i", messageID);
        const auto replyData { static_cast<const IPCXMLMessageEncoder*> (replyEncoder)->createEncodedMessage () };
        ARA_INTERNAL_ASSERT (replyData.size () <= SharedMemory::maxMessageSize);
        _sendRequest (0, replyData);

        delete replyEncoder;
        delete decoder;
        --_callbackLevel;
    }
    else
    {
        ARA_INTERNAL_ASSERT (_awaitsReply);
        if (_replyHandler)
        {
            const auto replyDecoder { IPCXMLMessageDecoder::createWithMessageData (messageData.first, messageData.second) };
            (*_replyHandler) (replyDecoder, _replyHandlerUserData);
            delete replyDecoder;
        }
        _awaitsReply = false;
    }
    
    _readLock.unlock ();
}

//------------------------------------------------------------------------------
#elif defined (__APPLE__)
//------------------------------------------------------------------------------

_Pragma ("GCC diagnostic push")
_Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"")

IPCMessageChannel::~IPCMessageChannel ()
{
    if (_sendPort)
    {
        CFMessagePortInvalidate (_sendPort);
        CFRelease (_sendPort);
    }
    if (_receivePort)
    {
        CFMessagePortInvalidate (_receivePort);
        CFRelease (_receivePort);
    }
    if (_writeSemaphore)
        sem_close (_writeSemaphore);

    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);
    ARA_INTERNAL_ASSERT (_callbackLevel == 0);
    ARA_INTERNAL_ASSERT (!_awaitsReply);
    ARA_INTERNAL_ASSERT (_replyHandler == nullptr);
}

CFDataRef IPCMessageChannel::_portCallback (CFMessagePortRef /*port*/, SInt32 messageID, CFDataRef messageData, void* info)
{
    auto channel { (IPCMessageChannel*) info };
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == channel->_creationThreadID);

    if (messageID != 0)
    {
        ++channel->_callbackLevel;

//      ARA_LOG ("IPCMessageChannel received message with ID %i%s", messageID, (channel->_awaitsReply) ? " while awaiting reply" : "");
#if USE_ARA_CF_ENCODING
        const auto decoder { ARA::IPC::ARAIPCCFCreateMessageDecoder (messageData) };
#else
        const auto decoder { IPCXMLMessageDecoder::createWithMessageData (messageData) };
#endif
        auto replyEncoder { channel->createEncoder () };
        channel->_receiveCallback (channel, messageID, decoder, replyEncoder);

//      ARA_LOG ("IPCMessageChannel replies to message with ID %i", messageID);
#if USE_ARA_CF_ENCODING
        const auto replyData { ARAIPCCFCreateMessageEncoderData (replyEncoder) };
#else
        const auto replyData { static_cast<const IPCXMLMessageEncoder*> (replyEncoder)->createEncodedMessage () };
#endif
        const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (channel->_sendPort, 0, replyData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
        ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);
        if (replyData)
            CFRelease (replyData);

        delete replyEncoder;
        delete decoder;
        --channel->_callbackLevel;
    }
    else
    {
        ARA_INTERNAL_ASSERT (channel->_awaitsReply);
        if (channel->_replyHandler)
        {
            ARA_INTERNAL_ASSERT (messageData != nullptr);
#if USE_ARA_CF_ENCODING
            const auto replyDecoder { ARA::IPC::ARAIPCCFCreateMessageDecoder (messageData) };
#else
            const auto replyDecoder { IPCXMLMessageDecoder::createWithMessageData (messageData) };
#endif
            (*channel->_replyHandler) (replyDecoder, channel->_replyHandlerUserData);
            delete replyDecoder;
        }
        channel->_awaitsReply = false;
    }

    return nullptr;
}

sem_t* _openSemaphore (const std::string& channelID, bool create)
{
    const auto previousUMask { umask (0) };

    std::string semName { "/" };
    semName += channelID;
    if (semName.length () > PSHMNAMLEN - 1)
        semName.erase (10, semName.length () - PSHMNAMLEN + 1);

    auto result { sem_open (semName.c_str (), ((create) ? O_CREAT | O_EXCL : 0), S_IRUSR | S_IWUSR, 0) };
    ARA_INTERNAL_ASSERT (result != SEM_FAILED);

    if (!create)
        sem_unlink (semName.c_str ());

    umask (previousUMask);

    return result;
}

CFMessagePortRef __attribute__ ((cf_returns_retained)) IPCMessageChannel::_createMessagePortPublishingID (const std::string& portID, IPCMessageChannel* channel)
{
    auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

    CFMessagePortContext portContext { 0, channel, nullptr, nullptr, nullptr };
    auto result = CFMessagePortCreateLocal (kCFAllocatorDefault, wrappedPortID, &_portCallback, &portContext, nullptr);
    ARA_INTERNAL_ASSERT (result != nullptr);

    CFRelease (wrappedPortID);

    CFRunLoopSourceRef runLoopSource { CFMessagePortCreateRunLoopSource (kCFAllocatorDefault, result, 0) };
    CFRunLoopAddSource (CFRunLoopGetCurrent (), runLoopSource, kCFRunLoopDefaultMode);
    CFRelease (runLoopSource);

    return result;
}

CFMessagePortRef __attribute__ ((cf_returns_retained)) IPCMessageChannel::_createMessagePortConnectedToID (const std::string& portID)
{
    CFMessagePortRef result {};

    auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

    auto timeout { 5.0 };
    while (timeout > 0.0)
    {
        if ((result = CFMessagePortCreateRemote (kCFAllocatorDefault, wrappedPortID)))
            break;

        constexpr auto waitTime { 0.01 };
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, true);
        timeout -= waitTime;
    }
    ARA_INTERNAL_ASSERT (result != nullptr);

    CFRelease (wrappedPortID);

    return result;
}

IPCMessageChannel* IPCMessageChannel::createPublishingID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel };
    channel->_receiveCallback = callback;
    channel->_writeSemaphore = _openSemaphore (channelID, true);
    channel->_sendPort = _createMessagePortConnectedToID (channelID + ".from_server");
    channel->_receivePort = _createMessagePortPublishingID (channelID + ".to_server", channel);
    return channel;
}

IPCMessageChannel* IPCMessageChannel::createConnectedToID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel };
    channel->_receiveCallback = callback;
    channel->_receivePort = _createMessagePortPublishingID (channelID + ".from_server", channel);
    channel->_sendPort = _createMessagePortConnectedToID (channelID + ".to_server");
    channel->_writeSemaphore = _openSemaphore (channelID, false);
    sem_post (channel->_writeSemaphore);
    return channel;
}

void IPCMessageChannel::sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder,
                              ReplyHandler const replyHandler, void* replyHandlerUserData)
{
#if USE_ARA_CF_ENCODING
    const auto messageData { ARAIPCCFCreateMessageEncoderData (encoder) };
#else
    const auto messageData { static_cast<const IPCXMLMessageEncoder*> (encoder)->createEncodedMessage () };
#endif

    const bool isOnCreationThread { std::this_thread::get_id () == _creationThreadID };
    const bool needsLock { !isOnCreationThread || (_callbackLevel == 0) };
//  ARA_LOG ("IPCMessageChannel sends message %i%s%s", messageID, (!isOnCreationThread) ? " from other thread" : "",
//                                           (needsLock) ? " starting new transaction" : (_callbackLevel > 0) ? " while handling message" : "");
    if (needsLock)
    {
        if (isOnCreationThread)
        {
            while (sem_trywait (_writeSemaphore) != 0)
            {
                if (errno != EINTR)
                    runReceiveLoop (1);
            }
        }
        else
        {
            while (sem_wait (_writeSemaphore) != 0)
                ARA_INTERNAL_ASSERT (errno == EINTR);
        }
    }

    const auto ARA_MAYBE_UNUSED_VAR (portSendResult) { CFMessagePortSendRequest (_sendPort, messageID, messageData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
    ARA_INTERNAL_ASSERT (portSendResult == kCFMessagePortSuccess);

    if (messageData)
        CFRelease (messageData);
    delete encoder;

    const auto previousAwaitsReply { _awaitsReply };
    const auto previousReplyHandler { _replyHandler };
    const auto previousReplyHandlerUserData { _replyHandlerUserData };
    _awaitsReply = true;
    _replyHandler = replyHandler;
    _replyHandlerUserData = replyHandlerUserData;
    do
    {
        if (isOnCreationThread)
            runReceiveLoop (messageTimeout);
        else
            std::this_thread::yield (); // \todo maybe it would be better to sleep for some longer interval here instead of simply yielding?
    } while (_awaitsReply);
    _awaitsReply = previousAwaitsReply;
    _replyHandler = previousReplyHandler;
    _replyHandlerUserData = previousReplyHandlerUserData;
//  ARA_LOG ("IPCMessageChannel received reply to message %i%s", messageID, (needsLock) ? " ending transaction" : "");

    if (needsLock)
        sem_post (_writeSemaphore);
}

ARA::IPC::ARAIPCMessageEncoder* IPCMessageChannel::createEncoder ()
{
#if USE_ARA_CF_ENCODING
    return ARA::IPC::ARAIPCCFCreateMessageEncoder ();
#else
    return new IPCXMLMessageEncoder {};
#endif
}

void IPCMessageChannel::runReceiveLoop (int32_t milliseconds)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, true);
}

_Pragma ("GCC diagnostic pop")

//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------
