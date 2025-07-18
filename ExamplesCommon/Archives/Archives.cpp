//------------------------------------------------------------------------------
//! \file       Archives.cpp
//!             archive classes used by e.g. ARATestHost to save/restore plug-in state
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2025, Celemony Software GmbH, All Rights Reserved.
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

#include "Archives.h"

#include "ARA_Library/Debug/ARADebug.h"

#include <iostream>

/*******************************************************************************/

std::streamsize ArchiveBase::getArchiveSize () noexcept
{
    try
    {
        if (!_dataStream.good ())
            return 0;

        const auto initialPos { _dataStream.tellg () };
        _dataStream.seekg (0, _dataStream.end);
        const auto fileSize { _dataStream.tellg () };
        _dataStream.seekg (initialPos, _dataStream.beg);

        return fileSize;
    }
    catch (const std::exception& e)
    {
        ARA_WARN ("%s", e.what ());
        ARA_INTERNAL_ASSERT (false);
        return 0;
    }
}

bool ArchiveBase::readBytes (std::streamoff position, std::streamsize length, char buffer[]) noexcept
{
    try
    {
        if (!_dataStream.good ())
            return false;

        _dataStream.seekg (position);
        ARA_INTERNAL_ASSERT (_dataStream.good ());

        _dataStream.read (buffer, length);
        return _dataStream.good ();
    }
    catch (const std::exception& e)
    {
        ARA_WARN ("%s", e.what ());
        ARA_INTERNAL_ASSERT (false);
        return false;
    }
}

bool ArchiveBase::writeBytes (std::streamoff position, std::streamsize length, const char buffer[]) noexcept
{
    try
    {
        if (!_dataStream.good ())
            return false;

        _dataStream.seekg (position, std::ios::beg);
        ARA_INTERNAL_ASSERT (_dataStream.good ());

        _dataStream.write (buffer, length);
        return _dataStream.good ();
    }
    catch (const std::exception& e)
    {
        ARA_WARN ("%s", e.what ());
        ARA_INTERNAL_ASSERT (false);
        return false;
    }
}
