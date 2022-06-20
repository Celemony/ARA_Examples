//------------------------------------------------------------------------------
//! \file       ARAIPCProxyHost.h
//!             implementation of host-side ARA IPC proxy host
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

#ifndef ARAIPCProxyHost_h
#define ARAIPCProxyHost_h

#include "ARA_Library/Dispatch/ARAPlugInDispatch.h"

#include "IPCPort.h"


#if ARA_SUPPORT_VERSION_1
    #error "This proxy does not support ARA 1."
#endif


namespace ARA {
namespace IPC {
namespace ProxyHost {


// static configuration: add the ARA factories that the proxy host will wrap
void addFactory (const ARAFactory* factory);

// static configuration: set the callback port that the proxy host will use
void setPlugInCallbacksPort (IPCPort* plugInCallbacksPort);

// static dispatcher: the host command handler that controls the proxy host
IPCMessage hostCommandHandler (const int32_t messageID, const IPCMessage& message);

// translation needed when establishing the binding to the remote documentController
ARADocumentControllerRef getDocumentControllerRefForRemoteRef (ARADocumentControllerRef remoteRef);

// creating and deleting plug-in extension instances associated with the proxy host
ARAPlugInExtensionRef createPlugInExtension (const ARAPlugInExtensionInstance* instance);
void destroyPlugInExtension (ARAPlugInExtensionRef plugInExtensionRef);

    
}   // namespace ProxyHost
}   // namespace IPC
}   // namespace ARA

#endif // ARAIPCProxyHost_h
