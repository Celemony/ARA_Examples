//------------------------------------------------------------------------------
//! \file       ARAContentAccessController.h
//!             implementation of the host ARAContentAccessControllerInterface
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

#pragma once

#include "ARADocumentController.h"

/*******************************************************************************/
// Simple content reader class that will be passed as ARAContentReaderHostRef
class HostDataContentReader
{
public:
    virtual ~HostDataContentReader () = default;

    virtual bool hasData () const noexcept = 0;
    virtual const void* getDataForEvent (ARA::ARAInt32 eventIndex) const noexcept = 0;
    virtual ARA::ARAInt32 getEventCount () const noexcept = 0;

protected:
    HostDataContentReader () = default;
    ARA_PLUGIN_MANAGED_OBJECT (HostDataContentReader)
};
ARA_MAP_HOST_REF (HostDataContentReader, ARA::ARAContentReaderHostRef)


/*******************************************************************************/
// Implementation of our test host's content access controller interface
// We'll use it here to give the plug-in information about the content of our musical context or audio sources
class ARAContentAccessController : public ARA::Host::ContentAccessControllerInterface
{
public:
    ARAContentAccessController (ARADocumentController* araDocumentController) noexcept
    : _araDocumentController { araDocumentController }
    {}

    bool isMusicalContextContentAvailable (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade getMusicalContextContentGrade (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentReaderHostRef createMusicalContextContentReader (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept override;

    bool isAudioSourceContentAvailable (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentGrade getAudioSourceContentGrade (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type) noexcept override;
    ARA::ARAContentReaderHostRef createAudioSourceContentReader (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type, const ARA::ARAContentTimeRange* range) noexcept override;

    ARA::ARAInt32 getContentReaderEventCount (ARA::ARAContentReaderHostRef contentReaderHostRef) noexcept override;
    const void* getContentReaderDataForEvent (ARA::ARAContentReaderHostRef contentReaderHostRef, ARA::ARAInt32 eventIndex) noexcept override;
    void destroyContentReader (ARA::ARAContentReaderHostRef contentReaderHostRef) noexcept override;

private:
    static std::unique_ptr<HostDataContentReader> createContentReader (const ContentContainer* contentContainer, const ARA::ARAContentType type);
    static bool isContentAvailable (const ContentContainer* contentContainer, const ARA::ARAContentType type);
    static ARA::ARAContentGrade getContentGrade (const ContentContainer* contentContainer, const ARA::ARAContentType type);

    Document* getDocument () const noexcept { return _araDocumentController->getDocument (); }

private:
    std::vector<std::unique_ptr<HostDataContentReader>> _hostDataContentReaders;
    ARADocumentController* _araDocumentController;
};
