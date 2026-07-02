//------------------------------------------------------------------------------
//! \file       IPCSocketPlugInTest.cpp
//!             Proof-of-concept Linux IPC plugin process: loads a Linux VST3 via
//!             dlopen, registers its ARAFactory with ProxyHost, and serves messages.
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
//   IPCSocketPlugInTest <main_fd> <other_fd> <ready_fd> [vst3_so_path]
//
// Writes one byte to ready_fd when the ProxyHost is ready to receive messages.
// Default vst3_so_path must be supplied at build time via -DARA_DEFAULT_VST3_PATH.

#define ARA_ENABLE_IPC 1

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>
#include <dlfcn.h>

#include "ARA_API/ARAVST3.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
ARA_DISABLE_VST3_WARNINGS_END

#include "ARA_Library/IPC/ARAIPCProxyHost.h"
#include "ARA_Library/IPC/ARAIPCConnection.h"
#include "ARA_Library/Debug/ARADebug.h"
#include "IPC/IPCSocketChannel.h"
#include "IPC/IPCSocketEncoding.h"

DEF_CLASS_IID (Steinberg::IPluginFactory)
DEF_CLASS_IID (ARA::IMainFactory)
DEF_CLASS_IID (ARA::IPlugInEntryPoint)
DEF_CLASS_IID (ARA::IPlugInEntryPoint2)

using namespace Steinberg;
using namespace ARA;


//------------------------------------------------------------------------------
// VST3 loading
//------------------------------------------------------------------------------

typedef bool (*ModuleEntryFunc)(void*);
typedef bool (*ModuleExitFunc)();

struct LoadedVST3
{
    void*             handle      { nullptr };
    IPluginFactory*   factory     { nullptr };
    IMainFactory*     mainFactory { nullptr };
    const ARAFactory* araFactory  { nullptr };
};
static LoadedVST3 g_vst3;

static const ARAFactory* _loadARAFactory (const char* soPath)
{
    std::printf ("[plugin] loading VST3: %s\n", soPath);
    std::fflush (stdout);

    g_vst3.handle = ::dlopen (soPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!g_vst3.handle)
    {
        std::fprintf (stderr, "[plugin] dlopen failed: %s\n", ::dlerror ());
        return nullptr;
    }

    auto moduleEntry = reinterpret_cast<ModuleEntryFunc> (::dlsym (g_vst3.handle, "ModuleEntry"));
    if (moduleEntry && !moduleEntry (g_vst3.handle))
    {
        std::fprintf (stderr, "[plugin] ModuleEntry returned false\n");
        ::dlclose (g_vst3.handle); g_vst3.handle = nullptr;
        return nullptr;
    }

    auto factoryProc = reinterpret_cast<GetFactoryProc> (::dlsym (g_vst3.handle, "GetPluginFactory"));
    if (!factoryProc)
    {
        std::fprintf (stderr, "[plugin] GetPluginFactory not found\n");
        ::dlclose (g_vst3.handle); g_vst3.handle = nullptr;
        return nullptr;
    }
    g_vst3.factory = factoryProc ();
    if (!g_vst3.factory)
    {
        std::fprintf (stderr, "[plugin] GetPluginFactory returned nullptr\n");
        ::dlclose (g_vst3.handle); g_vst3.handle = nullptr;
        return nullptr;
    }

    std::printf ("[plugin] scanning %d classes...\n", (int) g_vst3.factory->countClasses ());
    std::fflush (stdout);

    for (int32 i = 0; i < g_vst3.factory->countClasses (); ++i)
    {
        PClassInfo info;
        if (g_vst3.factory->getClassInfo (i, &info) != kResultOk)
            continue;

        if (std::strcmp (info.category, kARAMainFactoryClass) != 0)
            continue;

        IMainFactory* mf = nullptr;
        tresult r = g_vst3.factory->createInstance (info.cid, IMainFactory::iid, reinterpret_cast<void**> (&mf));
        if (r != kResultOk || !mf)
        {
            std::fprintf (stderr, "[plugin] createInstance(IMainFactory) failed\n");
            continue;
        }

        const ARAFactory* af = mf->getFactory ();
        if (!af)
        {
            std::fprintf (stderr, "[plugin] IMainFactory::getFactory returned nullptr\n");
            mf->release ();
            continue;
        }

        std::printf ("[plugin] found ARAFactory: plugInName='%s' factoryID='%s'\n",
                     af->plugInName ? af->plugInName : "(null)",
                     af->factoryID  ? af->factoryID  : "(null)");
        std::fflush (stdout);

        g_vst3.mainFactory = mf;
        g_vst3.araFactory  = af;
        return af;
    }

    std::fprintf (stderr, "[plugin] no ARA::IMainFactory found\n");
    return nullptr;
}

static void _unloadVST3 ()
{
    if (g_vst3.mainFactory) { g_vst3.mainFactory->release (); g_vst3.mainFactory = nullptr; }
    g_vst3.araFactory = nullptr;
    if (g_vst3.factory)     { g_vst3.factory->release ();     g_vst3.factory     = nullptr; }
    if (g_vst3.handle)
    {
        auto moduleExit = reinterpret_cast<ModuleExitFunc> (::dlsym (g_vst3.handle, "ModuleExit"));
        if (moduleExit) moduleExit ();
        ::dlclose (g_vst3.handle);
        g_vst3.handle = nullptr;
    }
}


//------------------------------------------------------------------------------
// ProxyHost subclass
//------------------------------------------------------------------------------

class TestProxyHost : public ARA::IPC::ProxyHost
{
public:
    static std::unique_ptr<TestProxyHost> create (int mainFd, int otherFd)
    {
        return std::unique_ptr<TestProxyHost> (new TestProxyHost (mainFd, otherFd));
    }

    ARA::IPC::Connection* connection () const
    {
        return ARA::IPC::RemoteCaller::getConnection ();
    }

private:
    explicit TestProxyHost (int mainFd, int otherFd)
        : ARA::IPC::ProxyHost (buildConnection (this, mainFd, otherFd))
    {}

    static std::unique_ptr<ARA::IPC::Connection> buildConnection (TestProxyHost* self, int mainFd, int otherFd)
    {
        auto conn = std::make_unique<ARA::IPC::Connection> (
            [] () -> std::unique_ptr<ARA::IPC::MessageEncoder> { return std::make_unique<IPCSocketMessageEncoder> (); },
            [self] (const ARA::IPC::MessageID msgID,
                    const ARA::IPC::MessageDecoder* decoder,
                    ARA::IPC::MessageEncoder* replyEncoder)
            {
                self->handleReceivedMessage (msgID, decoder, replyEncoder);
            },
            /*receiverEndianessMatches=*/ true
        );
        conn->setMainThreadChannel   (std::make_unique<IPCSocketChannel> (mainFd));
        conn->setOtherThreadsChannel (std::make_unique<IPCSocketChannel> (otherFd));
        return conn;
    }
};


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::fprintf (stderr,
            "usage: IPCSocketPlugInTest <main_fd> <other_fd> <ready_fd> [vst3_so_path]\n");
        return 1;
    }

    int mainFd  = std::atoi (argv[1]);
    int otherFd = std::atoi (argv[2]);
    int readyFd = std::atoi (argv[3]);

#ifdef ARA_DEFAULT_VST3_PATH
    const char* soPath = (argc >= 5) ? argv[4] : ARA_DEFAULT_VST3_PATH;
#else
    if (argc < 5)
    {
        std::fprintf (stderr, "[plugin] no vst3_so_path provided and ARA_DEFAULT_VST3_PATH not set\n");
        return 1;
    }
    const char* soPath = argv[4];
#endif

    std::printf ("[plugin] starting, main_fd=%d other_fd=%d\n", mainFd, otherFd);
    std::fflush (stdout);

    const ARAFactory* araFactory = _loadARAFactory (soPath);
    if (!araFactory)
    {
        std::fprintf (stderr, "[plugin] failed to load ARAFactory\n");
        return 1;
    }
    ARA::IPC::ARAIPCProxyHostAddFactory (araFactory);

    auto proxyHost = TestProxyHost::create (mainFd, otherFd);

    std::printf ("[plugin] proxy host ready\n");
    std::fflush (stdout);

    char byte = 1;
    ::write (readyFd, &byte, 1);
    ::close (readyFd);

    ARA::IPC::Connection* conn = proxyHost->connection ();
    while (true)
    {
        conn->processPendingMessageOnCreationThreadIfNeeded ();
        ::usleep (1000);
    }

    _unloadVST3 ();
    return 0;
}
