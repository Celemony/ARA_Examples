//------------------------------------------------------------------------------
//! \file       TestPlugInConfig.h
//!             IDs and other shared definitions for the ARATestPlugIn
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

#pragma once

#define _IN_QUOTES_HELPER(x) #x
#define IN_QUOTES(x) _IN_QUOTES_HELPER(x)

#if (!defined(ARA_MAJOR_VERSION)) || (!defined(ARA_MINOR_VERSION)) || (!defined(ARA_PATCH_VERSION))
    #error "ARA version macros not set up properly!"
#endif

#define TEST_PLUGIN_NAME "ARATestPlugIn"
#define TEST_MANUFACTURER_NAME "ARA SDK Examples"
#define TEST_INFORMATION_URL "https://www.ara-audio.org/examples"
#define TEST_MAILTO_URL "mailto:info@ara-audio.org?subject=ARATestPlugIn"
#define TEST_VERSION_STRING IN_QUOTES(ARA_MAJOR_VERSION) "." IN_QUOTES(ARA_MINOR_VERSION) "." IN_QUOTES(ARA_PATCH_VERSION)

#define TEST_FACTORY_ID "org.ara-audio.examples.testplugin.arafactory"
#define TEST_DOCUMENT_ARCHIVE_ID "org.ara-audio.examples.testplugin.aradocumentarchive.version1"
#define TEST_FILECHUNK_ARCHIVE_ID "org.ara-audio.examples.testplugin.arafilechunkarchive.version1"
