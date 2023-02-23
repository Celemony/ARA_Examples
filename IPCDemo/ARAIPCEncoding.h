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

namespace ARA
{

// key to identify methods
constexpr auto _methodIDKey = "methodID";

// create a message encoding a method call
template<typename... Args>
IPCMessage encodeMethodCall (Args... args)
{
    return { _methodIDKey, args... };
}

// test whether a message is encoding the given method call
bool isMethodCall (const IPCMessage& message, const char* methodID)
{
    std::string value { message.getArgValue<std::string> (_methodIDKey) };
    return (0 == std::strcmp (methodID, value.c_str ()));
}

}   // namespace ARA
