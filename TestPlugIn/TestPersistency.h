//------------------------------------------------------------------------------
//! \file       TestPersistency.h
//!             archiver/unarchiver implementation for the ARA test plug-in
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
#include <functional>

// Archiver/Unarchiver
// Actual plug-ins will already feature some persistency implementation which is independent of ARA -
// the following code merely drafts such an implementation, it cannot be used in actual products!

enum class TestArchiveState
{
    noError = 0,
    iOError,                // could not read or write bytes
                            // in ARA, the host handles I/O and will display a proper error message in this case
    unknownFormatError,     // archive was written by future version of the program
                            // in ARA, hosts should handle the version matching based on documentArchiveIDs -
                            // but some hosts behave incorrect here, so actual plug-ins should handle this
                            // as safety measure and will display a proper error message in this case
    incompatibleDataError   // archive contains numbers that cannot be represented on the current architecture
                            // (e.g. 64 bit archive with size_t that exceeds 32 bit architecture)
                            // in ARA, actual plug-ins will display a proper error message in this case
};

/*******************************************************************************/
// encoder class
class TestArchiver
{
public:
    // \todo this should be noexcept but recent clang/stl versions fail with this.
    using ArchivingFunction = std::function<bool (size_t, size_t, const uint8_t[]) /*noexcept*/>;
    TestArchiver (const ArchivingFunction& writeFunction) noexcept;

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
    const ArchivingFunction& _writeFunction;
    size_t _location { 0 };
    TestArchiveState _state { TestArchiveState::noError };
};

/*******************************************************************************/
// decoder class
class TestUnarchiver
{
public:
    // \todo this should be noexcept but recent clang/stl versions fail with this.
    using UnarchivingFunction = std::function<bool (size_t, size_t, uint8_t[]) /*noexcept*/>;
    TestUnarchiver (const UnarchivingFunction& readFunction) noexcept;

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
    const UnarchivingFunction& _readFunction;
    size_t _location { 0 };
    TestArchiveState _state { TestArchiveState::noError };
};
