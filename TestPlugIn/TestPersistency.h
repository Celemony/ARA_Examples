//------------------------------------------------------------------------------
//! \file       TestPersistency.h
//!             archiver/unarchiver implementation for the ARA test plug-in
//!             Actual plug-ins will typically have a persistency implementation which is
//!             independent of ARA - this code is also largely decoupled from ARA.
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

#pragma once

#include <string>
#include <cstdint>

namespace ARA
{
namespace PlugIn
{
    class HostArchiveWriter;
    class HostArchiveReader;
}
}

// Archiver/Unarchiver
// actual plug-ins will already feature some persistency implementation which is independent of ARA.
// some adapter can usually be written to hook up this existing code to ARA's archive readers/writers.
// the following code merely drafts such an implementation, it cannot be used in actual products!

enum class TestArchiveState
{
    noError = 0,
    iOError,                // could not read or write bytes
                            // in ARA, host handles I/O and will display proper error message in this case
    unkownFormatError,      // archive was written by future version of the program
                            // in ARA, actual plug-ins will display a proper error message in this case
    incompatibleDataError   // archive contains numbers that cannot be represented on the current architecture
                            // (e.g. 64 bit archive with size_t that exceeds 32 bit architecture)
                            // in ARA, actual plug-ins will display a proper error message in this case
};

/*******************************************************************************/
// encoder class
class TestArchiver
{
public:
    TestArchiver (ARA::PlugIn::HostArchiveWriter* archiveWriter) noexcept;

    void writeBool (bool data) noexcept;
    void writeDouble (double data) noexcept;
    void writeInt64 (int64_t data) noexcept;
    void writeSize (size_t data) noexcept;
    void writeString (std::string data) noexcept;

    TestArchiveState getState () const noexcept { return _state; }
    bool didSucceed () const noexcept { return (_state == TestArchiveState::noError); }

private:
    void write8ByteData (uint64_t data) noexcept;

private:
    ARA::PlugIn::HostArchiveWriter* const _archiveWriter;
    size_t _location { 0 };
    TestArchiveState _state { TestArchiveState::noError };
};

/*******************************************************************************/
// decoder class
class TestUnarchiver
{
public:
    TestUnarchiver (ARA::PlugIn::HostArchiveReader* archiveReader) noexcept;

    bool readBool () noexcept;
    double readDouble () noexcept;
    int64_t readInt64 () noexcept;
    size_t readSize () noexcept;
    std::string readString ();

    TestArchiveState getState () const noexcept { return _state; }
    bool didSucceed () const noexcept { return (_state == TestArchiveState::noError); }

private:
    uint64_t read8ByteData () noexcept;

private:
    ARA::PlugIn::HostArchiveReader* const _archiveReader;
    size_t _location { 0 };
    TestArchiveState _state { TestArchiveState::noError };
};
