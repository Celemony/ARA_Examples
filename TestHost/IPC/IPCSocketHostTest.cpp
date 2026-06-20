//------------------------------------------------------------------------------
//! \file       IPCSocketHostTest.cpp
//!             Proof-of-concept Linux IPC host: forks a plugin process, connects
//!             over a socketpair, and exercises the ARA factory API via ProxyPlugIn.
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

// Usage:
//   IPCSocketHostTest <plugin_binary> [vst3_so_path]
//
// <plugin_binary> is the path to IPCSocketPlugInTest (or any compatible process
// that accepts <main_fd> <other_fd> <ready_fd> [vst3_so_path] as arguments).

#define ARA_ENABLE_IPC 1

#include "ARA_Library/IPC/ARAIPCProxyPlugIn.h"
#include "ARA_Library/IPC/ARAIPCConnection.h"
#include "IPC/IPCSocketChannel.h"
#include "IPC/IPCSocketEncoding.h"
#include "ARA_API/ARAInterface.h"
#include "ARA_Library/Debug/ARADebug.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>


static std::unique_ptr<ARA::IPC::Connection> _makeConnection (int mainFd, int otherFd)
{
    ARA::IPC::Connection* connPtr = nullptr;

    auto conn = std::make_unique<ARA::IPC::Connection> (
        [] () -> std::unique_ptr<ARA::IPC::MessageEncoder> { return std::make_unique<IPCSocketMessageEncoder> (); },
        ARA::IPC::ProxyPlugIn::handleReceivedMessage,
        /*receiverEndianessMatches=*/ true,
        [&connPtr] () { if (connPtr) connPtr->processPendingMessageOnCreationThreadIfNeeded (); }
    );
    connPtr = conn.get ();

    conn->setMainThreadChannel   (std::make_unique<IPCSocketChannel> (mainFd));
    conn->setOtherThreadsChannel (std::make_unique<IPCSocketChannel> (otherFd));
    return conn;
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: IPCSocketHostTest <plugin_binary> [vst3_so_path]\n");
        return 1;
    }

    const char* pluginBin = argv[1];
    const char* vst3Path  = (argc >= 3) ? argv[2] : nullptr;

    std::printf ("[host] plugin binary: %s\n", pluginBin);
    std::fflush (stdout);

    int mainSv[2], otherSv[2], ready[2];
    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, mainSv)  != 0 ||
        ::socketpair (AF_UNIX, SOCK_STREAM, 0, otherSv) != 0 ||
        ::pipe (ready) != 0)
    {
        std::perror ("socketpair/pipe");
        return 1;
    }

    pid_t child = ::fork ();
    ARA_INTERNAL_ASSERT (child >= 0);

    if (child == 0)
    {
        ::close (mainSv[0]);
        ::close (otherSv[0]);
        ::close (ready[0]);

        char mainArg[32], otherArg[32], readyArg[32];
        std::snprintf (mainArg,  sizeof (mainArg),  "%d", mainSv[1]);
        std::snprintf (otherArg, sizeof (otherArg), "%d", otherSv[1]);
        std::snprintf (readyArg, sizeof (readyArg), "%d", ready[1]);

        if (vst3Path)
            ::execlp (pluginBin, pluginBin, mainArg, otherArg, readyArg, vst3Path, (char*) nullptr);
        else
            ::execlp (pluginBin, pluginBin, mainArg, otherArg, readyArg, (char*) nullptr);

        std::perror ("execlp");
        _exit (127);
    }

    ::close (mainSv[1]);
    ::close (otherSv[1]);
    ::close (ready[1]);

    std::printf ("[host] child pid=%d, waiting for ready signal...\n", (int) child);
    std::fflush (stdout);

    char readyByte = 0;
    if (::read (ready[0], &readyByte, 1) <= 0)
    {
        std::fprintf (stderr, "[host] plugin never became ready\n");
        ::kill (child, SIGTERM);
        ::waitpid (child, nullptr, 0);
        return 1;
    }
    ::close (ready[0]);

    std::printf ("[host] plugin ready, building connection...\n");
    std::fflush (stdout);

    auto conn      = _makeConnection (mainSv[0], otherSv[0]);
    auto proxyPlugIn = std::make_unique<ARA::IPC::ProxyPlugIn> (std::move (conn));
    auto proxyRef  = reinterpret_cast<ARA::IPC::ARAIPCProxyPlugInRef> (proxyPlugIn.get ());

    std::printf ("[host] querying factory count...\n");
    std::fflush (stdout);

    size_t count = ARA::IPC::ARAIPCProxyPlugInGetFactoriesCount (proxyRef);
    std::printf ("[host] factory count = %zu\n", count);
    std::fflush (stdout);
    ARA_INTERNAL_ASSERT (count > 0);

    const ARA::ARAFactory* factory = ARA::IPC::ARAIPCProxyPlugInGetFactoryAtIndex (proxyRef, 0);
    std::printf ("[host] factory name = %s\n",
                 factory && factory->plugInName ? factory->plugInName : "(null)");
    std::fflush (stdout);

    std::printf ("[host] calling initializeARA...\n");
    std::fflush (stdout);
    ARA::IPC::ARAIPCProxyPlugInInitializeARA (proxyRef, factory->factoryID, ARA::kARAAPIGeneration_2_0_Final);
    std::printf ("[host] initializeARA returned\n");
    std::fflush (stdout);

    ARA::IPC::ARAIPCProxyPlugInUninitializeARA (proxyRef, factory->factoryID);
    std::printf ("[host] uninitializeARA returned\n");
    std::fflush (stdout);

    ::usleep (100'000);

    proxyPlugIn.reset ();

    ::kill (child, SIGTERM);
    int status = 0;
    ::waitpid (child, &status, 0);
    std::printf ("[host] child exited with status %d\n", WEXITSTATUS (status));

    return 0;
}
