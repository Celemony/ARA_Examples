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


class IPCMessagePort
{
protected:
    IPCMessagePort (const std::string& channelID)
    : _dataAvailable { ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Available" } + channelID).c_str ()) },
      _dataReceived { ::CreateEventA (NULL, FALSE, FALSE, (std::string { "Received" } + channelID).c_str ()) }
    {
        ARA_INTERNAL_ASSERT (_dataAvailable != nullptr);
        ARA_INTERNAL_ASSERT (_dataReceived != nullptr);
    }

public:
    ~IPCMessagePort ()
    {
        ::UnmapViewOfFile (_sharedMemory);
        ::CloseHandle (_fileMapping);
        ::CloseHandle (_dataReceived);
        ::CloseHandle (_dataAvailable);
    }

protected:
    struct SharedMemory
    {
        static constexpr DWORD maxMessageSize { 4 * 1024 * 1024L - 64};

        size_t messageSize;
        ARA::IPC::ARAIPCMessageID messageID;
        char messageData[maxMessageSize];
    };

    HANDLE _dataAvailable {};           // signal set by the sending side indicating new data has been placed in shared memory
    HANDLE _dataReceived {};            // signal set by the receiving side when evaluating the shared memory
    HANDLE _fileMapping {};
    SharedMemory* _sharedMemory {};

};

class IPCReceivePort : public IPCMessagePort
{
public:
    IPCReceivePort (const std::string& channelID, IPCMessageChannel* channel)
    : IPCMessagePort { channelID },
      _channel { channel }
    {
        const auto mapKey { std::string { "Map" } + channelID };
        _fileMapping = ::CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof (SharedMemory), mapKey.c_str ());
        ARA_INTERNAL_ASSERT (_fileMapping != nullptr);
        _sharedMemory = (SharedMemory*) ::MapViewOfFile (_fileMapping, FILE_MAP_WRITE, 0, 0, sizeof (SharedMemory));
        ARA_INTERNAL_ASSERT (_sharedMemory != nullptr);
    }

    void runReceiveLoop (int32_t milliseconds)
    {
        const auto waitRequest { ::WaitForSingleObject (_dataAvailable, milliseconds) };
        if (waitRequest == WAIT_TIMEOUT)
            return;
        ARA_INTERNAL_ASSERT (waitRequest == WAIT_OBJECT_0);

        const auto messageID { _sharedMemory->messageID };
        const auto decoder { _channel->createDecoderForMessage (messageID, _sharedMemory->messageData, _sharedMemory->messageSize) };

        ::SetEvent (_dataReceived);

        _channel->routeReceivedMessage (messageID, decoder);
    }

private:
    IPCMessageChannel* const _channel;
};

class IPCSendPort : public IPCMessagePort
{
public:
    IPCSendPort (const std::string& channelID)
    : IPCMessagePort { channelID }
    {
        const auto mapKey { std::string { "Map" } + channelID };
        while (!_fileMapping)
        {
            ::Sleep (100);
            _fileMapping = ::OpenFileMappingA (FILE_MAP_WRITE, FALSE, mapKey.c_str ());
        }
        _sharedMemory = (SharedMemory*) ::MapViewOfFile (_fileMapping, FILE_MAP_WRITE, 0, 0, 0);
        ARA_INTERNAL_ASSERT (_sharedMemory != nullptr);
    }

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, const std::string& messageData)
    {
        ARA_INTERNAL_ASSERT (messageData.size () <= SharedMemory::maxMessageSize);

        _sharedMemory->messageID = messageID;
        _sharedMemory->messageSize = messageData.size ();
        std::memcpy (_sharedMemory->messageData, messageData.c_str (), messageData.size ());

        ::SetEvent (_dataAvailable);
        const auto waitResult { ::WaitForSingleObject (_dataReceived, messageTimeout) };
        ARA_INTERNAL_ASSERT (waitResult == WAIT_OBJECT_0);
    }
};

//------------------------------------------------------------------------------
#elif defined (__APPLE__)
//------------------------------------------------------------------------------


class IPCReceivePort
{
public:
    IPCReceivePort (const std::string& portID, IPCMessageChannel* channel)
    {
        auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

        CFMessagePortContext portContext { 0, channel, nullptr, nullptr, nullptr };
        _port = CFMessagePortCreateLocal (kCFAllocatorDefault, wrappedPortID, _portCallback, &portContext, nullptr);
        ARA_INTERNAL_ASSERT (_port != nullptr);

        CFRelease (wrappedPortID);

        CFRunLoopSourceRef runLoopSource { CFMessagePortCreateRunLoopSource (kCFAllocatorDefault, _port, 0) };
        CFRunLoopAddSource (CFRunLoopGetCurrent (), runLoopSource, kCFRunLoopDefaultMode);
        CFRelease (runLoopSource);
    }

    ~IPCReceivePort ()
    {
        CFMessagePortInvalidate (_port);
        CFRelease (_port);
    }

    void runReceiveLoop (int32_t milliseconds)
    {
        CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001 * milliseconds, true);
    }

private:
    static CFDataRef _portCallback (CFMessagePortRef /*port*/, SInt32 messageID, CFDataRef messageData, void* info)
    {
        auto channel { static_cast<IPCMessageChannel*> (info) };
        const auto decoder { channel->createDecoderForMessage (messageID, messageData) };
        channel->routeReceivedMessage (messageID, decoder);
        return nullptr;
    }

private:
    CFMessagePortRef _port {};
};

class IPCSendPort
{
public:
    IPCSendPort (const std::string& portID)
    {
        auto wrappedPortID { CFStringCreateWithCStringNoCopy (kCFAllocatorDefault, portID.c_str (), kCFStringEncodingASCII, kCFAllocatorNull) };

        auto timeout { 0.001 * messageTimeout };
        while (timeout > 0.0)
        {
            if ((_port = CFMessagePortCreateRemote (kCFAllocatorDefault, wrappedPortID)))
                break;

            constexpr auto waitTime { 0.01 };
            CFRunLoopRunInMode (kCFRunLoopDefaultMode, waitTime, true);
            timeout -= waitTime;
        }
        ARA_INTERNAL_ASSERT (_port != nullptr);

        CFRelease (wrappedPortID);
    }

    ~IPCSendPort ()
    {
        CFMessagePortInvalidate (_port);
        CFRelease (_port);
    }

    void sendMessage (ARA::IPC::ARAIPCMessageID messageID, CFDataRef messageData)
    {
        const auto ARA_MAYBE_UNUSED_VAR (result) { CFMessagePortSendRequest (_port, messageID, messageData, 0.001 * messageTimeout, 0.0, nullptr, nullptr) };
        ARA_INTERNAL_ASSERT (result == kCFMessagePortSuccess);
    }

private:
    CFMessagePortRef _port {};
};

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


//------------------------------------------------------------------------------
#endif
//------------------------------------------------------------------------------


IPCMessageChannel::IPCMessageChannel (const ReceiveCallback& callback)
: _receiveCallback { callback }
{}

IPCMessageChannel* IPCMessageChannel::createPublishingID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel { callback } };
#if defined (_WIN32)
    channel->_transactionLock = ::CreateMutexA (NULL, FALSE, (std::string { "Transaction" } + channelID).c_str ());
    ARA_INTERNAL_ASSERT (channel->_transactionLock != nullptr);
#elif defined (__APPLE__)
    channel->_transactionLock = _openSemaphore (channelID, true);
#endif
    channel->_sendPort = new IPCSendPort { channelID + ".from_server" };
    channel->_receivePort = new IPCReceivePort { channelID + ".to_server", channel };
    return channel;
}

IPCMessageChannel* IPCMessageChannel::createConnectedToID (const std::string& channelID, const ReceiveCallback& callback)
{
    auto channel { new IPCMessageChannel { callback } };
    channel->_receivePort = new IPCReceivePort { channelID + ".from_server", channel };
    channel->_sendPort = new IPCSendPort { channelID + ".to_server" };
#if defined (_WIN32)
    channel->_transactionLock = ::CreateMutexA (NULL, FALSE, (std::string { "Transaction" } + channelID).c_str ());
    ARA_INTERNAL_ASSERT (channel->_transactionLock != nullptr);
#elif defined (__APPLE__)
    channel->_transactionLock = _openSemaphore (channelID, false);
#endif
    channel->_unlockTransaction ();
    return channel;
}

IPCMessageChannel::~IPCMessageChannel ()
{
    delete _sendPort;
    delete _receivePort;

#if defined (_WIN32)
    ::CloseHandle (_transactionLock);
#elif defined (__APPLE__)
    sem_close (_transactionLock);
#endif

    ARA_INTERNAL_ASSERT (_callbackLevel == 0);
    ARA_INTERNAL_ASSERT (!_awaitsReply);
    ARA_INTERNAL_ASSERT (_replyHandler == nullptr);
}

void IPCMessageChannel::_lockTransaction (bool isOnCreationThread)
{
    if (isOnCreationThread)
    {
#if defined (_WIN32)
        while (true)
        {
            const auto waitWriteMutex { ::WaitForSingleObject (_transactionLock, 0) };
            if (waitWriteMutex == WAIT_OBJECT_0)
                break;

            ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_TIMEOUT);
            runReceiveLoop (1);
        }
#elif defined (__APPLE__)
        while (sem_trywait (_transactionLock) != 0)
        {
            if (errno != EINTR)
                runReceiveLoop (1);
        }
#endif
    }
    else
    {
#if defined (_WIN32)
        const auto waitWriteMutex { ::WaitForSingleObject (_transactionLock, messageTimeout) };
        ARA_INTERNAL_ASSERT (waitWriteMutex == WAIT_OBJECT_0);
#elif defined (__APPLE__)
        while (sem_wait (_transactionLock) != 0)
            ARA_INTERNAL_ASSERT (errno == EINTR);
#endif
    }
}

void IPCMessageChannel::_unlockTransaction ()
{
#if defined (_WIN32)
    ::ReleaseMutex (_transactionLock);
#elif defined (__APPLE__)
    auto result { sem_post (_transactionLock) };
    ARA_INTERNAL_ASSERT (result == 0);
#endif
}

void IPCMessageChannel::_sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder)
{
#if USE_ARA_CF_ENCODING
    const auto messageData { ARAIPCCFCreateMessageEncoderData (encoder) };
#else
    const auto messageData { static_cast<const IPCXMLMessageEncoder*> (encoder)->createEncodedMessage () };
#endif

    _sendPort->sendMessage (messageID, messageData);

#if defined (__APPLE__)
    if (messageData)
        CFRelease (messageData);
#endif
}

void IPCMessageChannel::runReceiveLoop (int32_t milliseconds)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);
    _receivePort->runReceiveLoop (milliseconds);
}

#if defined (_WIN32)
const ARA::IPC::ARAIPCMessageDecoder* IPCMessageChannel::createDecoderForMessage (ARA::IPC::ARAIPCMessageID messageID, const char* data, const size_t dataSize)
{
    if ((messageID != 0) || _replyHandler)
        return IPCXMLMessageDecoder::createWithMessageData (data, dataSize);
    return nullptr;
}
#elif defined (__APPLE__)
const ARA::IPC::ARAIPCMessageDecoder* IPCMessageChannel::createDecoderForMessage (ARA::IPC::ARAIPCMessageID messageID, CFDataRef messageData)
{
    if ((messageID != 0) || _replyHandler)
#if USE_ARA_CF_ENCODING
        return ARA::IPC::ARAIPCCFCreateMessageDecoder (messageData);
#else
        return IPCXMLMessageDecoder::createWithMessageData (messageData);
#endif
    return nullptr;
}
#endif

void IPCMessageChannel::routeReceivedMessage (ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageDecoder* decoder)
{
    ARA_INTERNAL_ASSERT (std::this_thread::get_id () == _creationThreadID);

    if (messageID != 0)
    {
        _handleReceivedMessage (messageID, decoder);
    }
    else
    {
        ARA_INTERNAL_ASSERT (_awaitsReply);
        if (_replyHandler)
        {
            ARA_INTERNAL_ASSERT (decoder != nullptr);
            (*_replyHandler) (decoder, _replyHandlerUserData);
            delete decoder;
        }
        else
        {
            ARA_INTERNAL_ASSERT (decoder == nullptr);
        }
        _awaitsReply = false;
    }
}

void IPCMessageChannel::_handleReceivedMessage (ARA::IPC::ARAIPCMessageID messageID, const ARA::IPC::ARAIPCMessageDecoder* decoder)
{
    ++_callbackLevel;

    auto replyEncoder { createEncoder () };
//  ARA_LOG ("IPCMessageChannel handles message with ID %i%s", messageID, (_awaitsReply) ? " (while awaiting reply)" : "");
    _receiveCallback (this, messageID, decoder, replyEncoder);
    delete decoder;

//  ARA_LOG ("IPCMessageChannel replies to message with ID %i", messageID);
    _sendMessage (0, replyEncoder);
    delete replyEncoder;

    --_callbackLevel;
}

void IPCMessageChannel::sendMessage (ARA::IPC::ARAIPCMessageID messageID, ARA::IPC::ARAIPCMessageEncoder* encoder,
                                     ReplyHandler const replyHandler, void* replyHandlerUserData)
{
    const bool isOnCreationThread { std::this_thread::get_id () == _creationThreadID };
    const bool needsLock { !isOnCreationThread || (_callbackLevel == 0) };
//  ARA_LOG ("IPCMessageChannel sends message %i%s%s", messageID, (!isOnCreationThread) ? " from other thread" : "",
//                                              (needsLock) ? " starting new transaction" : (_callbackLevel > 0) ? " while handling message" : "");
    if (needsLock)
        _lockTransaction (isOnCreationThread);

    _sendMessage (messageID, encoder);
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
        _unlockTransaction ();
}

ARA::IPC::ARAIPCMessageEncoder* IPCMessageChannel::createEncoder ()
{
#if USE_ARA_CF_ENCODING
    return ARA::IPC::ARAIPCCFCreateMessageEncoder ();
#else
    return new IPCXMLMessageEncoder {};
#endif
}
