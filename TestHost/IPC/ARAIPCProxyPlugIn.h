//------------------------------------------------------------------------------
//! \file       ARAIPCProxyPlugIn.h
//!             implementation of host-side ARA IPC proxy plug-in
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

#ifndef ARAIPCProxyPlugIn_h
#define ARAIPCProxyPlugIn_h

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include "IPCPort.h"


#if ARA_SUPPORT_VERSION_1
    #error "This proxy does not support ARA 1."
#endif


namespace ARA {
namespace IPC {
namespace ProxyPlugIn {


class Factory;


// static initialization
size_t initializeFactories (IPCPort& hostCommandsPort);

// access proxies to the factories provided by the remote plug-in
Factory* getFactoryAtIndex (size_t index);

// get copy of the remote factory data, with all function calls removed
const ARAFactory* getFactoryData (Factory* proxyFactory);

// proxy document controller creation call, to be used instead of ARAFactory.createDocumentControllerWithDocument ()
const ARADocumentControllerInstance* createDocumentControllerWithDocument (Factory* proxyFactory,
                                                                           const ARADocumentControllerHostInstance* hostInstance,
                                                                           const ARADocumentProperties* properties);

// static handler of received messages
IPCMessage plugInCallbacksDispatcher (const MessageID messageID, const IPCMessage& message);

// \todo to perform the binding to the remote plug-in instance, the host needs access to this translation...
ARADocumentControllerRef getDocumentControllerRemoteRef (ARADocumentControllerRef documentControllerRef);

// create the plug-in extension when performing the binding to the remote plug-in instance
const ARAPlugInExtensionInstance* createPlugInExtensionInstance (size_t remoteExtensionRef, IPCPort& port, ARADocumentControllerRef documentControllerRef,
                                                                    ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles);
// destroy the plug-in extension when destroying the remote plug-in instance
void destroyPlugInExtensionInstance (const ARAPlugInExtensionInstance* plugInExtensionInstance);


}   // namespace ProxyPlugIn
}   // namespace IPC
}   // namespace ARA

#endif // ARAIPCProxyPlugIn_h
