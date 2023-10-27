//------------------------------------------------------------------------------
//! \file       Archives.h
//!             archive classes used by e.g. ARATestHost to save/restore plug-in state
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2023, Celemony Software GmbH, All Rights Reserved.
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

#include <string>
#include <fstream>
#include <sstream>

class ArchiveBase
{
protected:
    ArchiveBase (std::iostream& dataStream, const std::string& documentArchiveID) : _dataStream { dataStream }, _documentArchiveID { documentArchiveID } {}

public:
    std::streamsize getArchiveSize () noexcept;
    bool readBytes (std::streamoff position, std::streamsize length, char buffer[]) noexcept;
    bool writeBytes (std::streamoff position, std::streamsize length, const char buffer[]) noexcept;

    const std::string& getDocumentArchiveID () const noexcept { return _documentArchiveID; }

private:
    std::iostream& _dataStream;
    const std::string _documentArchiveID;
};

/*******************************************************************************/

// In-memory archive, using the optional provided std::string as initial data.
// \todo With C++20 the interface could be much more like in the file case:
//       an external string would always be required and it would be directly used
//       as backing for the stream, preventing all the current temporary copies.
class MemoryArchive : public ArchiveBase
{
public:
    // create empty
    MemoryArchive (const std::string& documentArchiveID) : ArchiveBase { _stream, documentArchiveID } {}
    // create with copy of existing data
    MemoryArchive (const std::string& data, const std::string& documentArchiveID) : ArchiveBase { _stream, documentArchiveID }, _stream { data } {}

    // copy current data
    operator std::string () const noexcept { return _stream.str (); }

private:
    std::stringstream _stream;
};

/*******************************************************************************/

// File-based archive, using the file at the provided path as backing.
class FileArchive : public ArchiveBase
{
public:
    FileArchive (std::string path, const std::string& documentArchiveID);

private:
    std::fstream _stream;
};
