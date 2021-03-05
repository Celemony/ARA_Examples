//------------------------------------------------------------------------------
//! \file       ARAArchivingController.h
//!             implementation of the host ARAArchivingControllerInterface
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2021, Celemony Software GmbH, All Rights Reserved.
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
// This is a brief test app that hooks up an ARA capable plug-in using a choice
// of several companion APIs, creates a small model, performs various tests and
// sanity checks and shuts everything down again.
// This educational example is not suitable for production code - for the sake
// of readability of the code, proper error handling or dealing with optional
// ARA API elements is left out.
//------------------------------------------------------------------------------

#pragma once

#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include "ARADocumentController.h"

/*******************************************************************************/
// Implementation of our test host's archiving controller interface
// The plug-in will call these functions when reading and writing its document archive
// and to notify the host of archiving progress notifications while reading / writing
class ARAArchivingController : public ARA::Host::ArchivingControllerInterface
{
public:
    ARAArchivingController (ARADocumentController* araDocumentController) noexcept
    : _araDocumentController { araDocumentController }
    {}

    ARA::ARASize getArchiveSize (ARA::ARAArchiveReaderHostRef archiveReaderHostRef) noexcept override;
    bool readBytesFromArchive (ARA::ARAArchiveReaderHostRef archiveReaderHostRef, ARA::ARASize position, ARA::ARASize length, ARA::ARAByte buffer[]) noexcept override;
    bool writeBytesToArchive (ARA::ARAArchiveWriterHostRef archiveWriterHostRef, ARA::ARASize position, ARA::ARASize length, const ARA::ARAByte buffer[]) noexcept override;
    void notifyDocumentArchivingProgress (float value) noexcept override;
    void notifyDocumentUnarchivingProgress (float value) noexcept override;
    ARA::ARAPersistentID getDocumentArchiveID (ARA::ARAArchiveReaderHostRef archiveReaderHostRef) noexcept override;

private:
    ARADocumentController* _araDocumentController;
};
