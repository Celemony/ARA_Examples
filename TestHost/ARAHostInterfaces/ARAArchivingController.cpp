//------------------------------------------------------------------------------
//! \file       ARAArchivingController.cpp
//!             implementation of the host ARAArchivingControllerInterface
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2022, Celemony Software GmbH, All Rights Reserved.
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

#include "ARAArchivingController.h"

// A reference to the TestArchive instance will be passed to this function,
// which we can use to query the size of the archive's file stream
ARA::ARASize ARAArchivingController::getArchiveSize (ARA::ARAArchiveReaderHostRef archiveReaderHostRef) noexcept
{
    const auto archive = fromHostRef (archiveReaderHostRef);
    ARA_VALIDATE_API_ARGUMENT (archiveReaderHostRef, archive != nullptr);
    ARA_VALIDATE_API_STATE (_araDocumentController->isUsingArchive (archive));

    return static_cast<ARA::ARASize> (archive->getArchiveSize ());
}

// A reference to the TestArchive instance will be passed to this function,
// and we can use it to read from the test archive into the supplied output buffer
bool ARAArchivingController::readBytesFromArchive (ARA::ARAArchiveReaderHostRef archiveReaderHostRef, ARA::ARASize position, ARA::ARASize length, ARA::ARAByte buffer[]) noexcept
{
    const auto archive = fromHostRef (archiveReaderHostRef);
    ARA_VALIDATE_API_ARGUMENT (archiveReaderHostRef, archive != nullptr);
    ARA_VALIDATE_API_STATE (_araDocumentController->isUsingArchive (archive));
    ARA_VALIDATE_API_ARGUMENT (nullptr, 0 < length);
    ARA_VALIDATE_API_ARGUMENT (nullptr, position + length <= getArchiveSize (archiveReaderHostRef));

    return archive->readBytes (static_cast<std::streamoff> (position), static_cast<std::streamsize> (length), reinterpret_cast<char*> (buffer));
}

// Like the above function, but instead of streaming bytes into a data buffer we'll
// write data into our TestArchive instance.
bool ARAArchivingController::writeBytesToArchive (ARA::ARAArchiveWriterHostRef archiveWriterHostRef, ARA::ARASize position, ARA::ARASize length, const ARA::ARAByte buffer[]) noexcept
{
    auto archive = fromHostRef (archiveWriterHostRef);
    ARA_VALIDATE_API_ARGUMENT (archiveWriterHostRef, archive != nullptr);
    ARA_VALIDATE_API_STATE (_araDocumentController->isUsingArchive (archive));
    ARA_VALIDATE_API_ARGUMENT (nullptr, 0 < length);

    return archive->writeBytes (static_cast<std::streamoff> (position), static_cast<std::streamsize> (length), reinterpret_cast<const char*> (buffer));
}

// The plug-in will call these progress notification functions, which we can
// use to keep track of its archiving/unarchiving progress
void ARAArchivingController::notifyDocumentArchivingProgress (float value) noexcept
{
    ARA_LOG ("document archiving progress is %.f%%.", 100.0 * value);
    ARA_VALIDATE_API_STATE (_araDocumentController->isUsingArchive ());
}

void ARAArchivingController::notifyDocumentUnarchivingProgress (float value) noexcept
{
    ARA_VALIDATE_API_STATE (_araDocumentController->isUsingArchive ());
    ARA_LOG ("document unarchiving progress is %.f%%.", 100.0 * value);
}

ARA::ARAPersistentID ARAArchivingController::getDocumentArchiveID (ARA::ARAArchiveReaderHostRef archiveReaderHostRef) noexcept
{
    const auto archive = fromHostRef (archiveReaderHostRef);
    return archive->getDocumentArchiveID ().c_str ();
}
