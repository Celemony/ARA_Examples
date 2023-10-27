//------------------------------------------------------------------------------
//! \file       ARAContentAccessController.cpp
//!             implementation of the host ARAContentAccessControllerInterface
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

#include "ARAContentAccessController.h"

/*******************************************************************************/
// Template-based implementations of HostDataContentReader
template <typename ContentType>
class ContentReaderImplementation : public HostDataContentReader
{
public:
    explicit ContentReaderImplementation (const ContentContainer::EntryData<ContentType>& entries)
    : _entries { entries }
    {}

    bool hasData () const noexcept override { return _entries != nullptr; }
    const void* getDataForEvent (ARA::ARAInt32 eventIndex) const noexcept override { return &this->_entries->at (static_cast<size_t> (eventIndex)); }
    ARA::ARAInt32 getEventCount () const noexcept override { return static_cast<ARA::ARAInt32> (this->_entries->size ()); }

private:
    const ContentContainer::EntryData<ContentType>& _entries;
};

/*******************************************************************************/

std::unique_ptr<HostDataContentReader> ARAContentAccessController::createContentReader (const ContentContainer* contentContainer, const ARA::ARAContentType type)
{
    switch (type)
    {
        case ARA::kARAContentTypeNotes: return std::make_unique<ContentReaderImplementation<ARA::ARAContentNote>> (contentContainer->getNotes ());
        case ARA::kARAContentTypeTempoEntries: return std::make_unique<ContentReaderImplementation<ARA::ARAContentTempoEntry>> (contentContainer->getTempoEntries ());
        case ARA::kARAContentTypeBarSignatures: return std::make_unique<ContentReaderImplementation<ARA::ARAContentBarSignature>> (contentContainer->getBarSignatures ());
        case ARA::kARAContentTypeStaticTuning: return std::make_unique<ContentReaderImplementation<ARA::ARAContentTuning>> (contentContainer->getTuning ());
        case ARA::kARAContentTypeKeySignatures: return std::make_unique<ContentReaderImplementation<ARA::ARAContentKeySignature>> (contentContainer->getKeySignatures ());
        case ARA::kARAContentTypeSheetChords: return std::make_unique<ContentReaderImplementation<ARA::ARAContentChord>> (contentContainer->getChords ());
        default: return nullptr;
    }
}

bool ARAContentAccessController::isContentAvailable (const ContentContainer* contentContainer, const ARA::ARAContentType type)
{
    return createContentReader (contentContainer, type)->hasData ();
}

// For the available content we can indicate a "grade" of how reliable the content data is -
// in this test host the content is "adjusted" because we simulate that the end user described this through some UI.
ARA::ARAContentGrade ARAContentAccessController::getContentGrade (const ContentContainer* contentContainer, const ARA::ARAContentType type)
{
    if (isContentAvailable (contentContainer, type))
        return ARA::kARAContentGradeAdjusted;

    return ARA::kARAContentGradeInitial;
}

bool ARAContentAccessController::isMusicalContextContentAvailable (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type) noexcept
{
    const auto musicalContext = fromHostRef (musicalContextHostRef);
    ARA_VALIDATE_API_ARGUMENT (musicalContext, ARA::contains (getDocument ()->getMusicalContexts (), musicalContext));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return isContentAvailable (musicalContext, type);
}

ARA::ARAContentGrade ARAContentAccessController::getMusicalContextContentGrade (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type) noexcept
{
    const auto musicalContext = fromHostRef (musicalContextHostRef);
    ARA_VALIDATE_API_ARGUMENT (musicalContext, ARA::contains (getDocument ()->getMusicalContexts (), musicalContext));
    ARA_VALIDATE_API_STATE (isContentAvailable (musicalContext, type));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return getContentGrade (musicalContext, type);
}

ARA::ARAContentReaderHostRef ARAContentAccessController::createMusicalContextContentReader (ARA::ARAMusicalContextHostRef musicalContextHostRef, ARA::ARAContentType type, const ARA::ARAContentTimeRange* /*range*/) noexcept
{
    const auto musicalContext = fromHostRef (musicalContextHostRef);
    ARA_VALIDATE_API_ARGUMENT (musicalContextHostRef, ARA::contains (getDocument ()->getMusicalContexts (), musicalContext));
    ARA_VALIDATE_API_STATE (isContentAvailable (musicalContext, type));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    if (auto contentReader { createContentReader (musicalContext, type) })
    {
        const auto hostRef { toHostRef (contentReader.get ()) };
        _hostDataContentReaders.emplace_back (std::move (contentReader));
        return hostRef;
    }

    return nullptr;
}

bool ARAContentAccessController::isAudioSourceContentAvailable (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSource, ARA::contains (getDocument ()->getAudioSources (), audioSource));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return isContentAvailable (audioSource, type);
}

ARA::ARAContentGrade ARAContentAccessController::getAudioSourceContentGrade (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSource, ARA::contains (getDocument ()->getAudioSources (), audioSource));
    ARA_VALIDATE_API_STATE (isContentAvailable (audioSource, type));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return getContentGrade (audioSource, type);
}

ARA::ARAContentReaderHostRef ARAContentAccessController::createAudioSourceContentReader (ARA::ARAAudioSourceHostRef audioSourceHostRef, ARA::ARAContentType type, const ARA::ARAContentTimeRange* /*range*/) noexcept
{
    const auto audioSource = fromHostRef (audioSourceHostRef);
    ARA_VALIDATE_API_ARGUMENT (audioSource, ARA::contains (getDocument ()->getAudioSources (), audioSource));
    ARA_VALIDATE_API_STATE (isContentAvailable (audioSource, type));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    if (auto contentReader { createContentReader (audioSource, type) })
    {
        const auto hostRef { toHostRef (contentReader.get ()) };
        _hostDataContentReaders.emplace_back (std::move (contentReader));
        return hostRef;
    }

    return nullptr;
}

ARA::ARAInt32 ARAContentAccessController::getContentReaderEventCount (ARA::ARAContentReaderHostRef contentReaderHostRef) noexcept
{
    const auto hostDataContentReader = fromHostRef (contentReaderHostRef);
    ARA_VALIDATE_API_ARGUMENT (contentReaderHostRef, ARA::contains (_hostDataContentReaders, hostDataContentReader));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return hostDataContentReader->getEventCount ();
}

const void* ARAContentAccessController::getContentReaderDataForEvent (ARA::ARAContentReaderHostRef contentReaderHostRef, ARA::ARAInt32 eventIndex) noexcept
{
    const auto hostDataContentReader = fromHostRef (contentReaderHostRef);
    ARA_VALIDATE_API_ARGUMENT (contentReaderHostRef, ARA::contains (_hostDataContentReaders, hostDataContentReader));
    ARA_VALIDATE_API_ARGUMENT (nullptr, 0 <= eventIndex);
    ARA_VALIDATE_API_ARGUMENT (nullptr, eventIndex < hostDataContentReader->getEventCount ());
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    return hostDataContentReader->getDataForEvent (eventIndex);
}

void ARAContentAccessController::destroyContentReader (ARA::ARAContentReaderHostRef contentReaderHostRef) noexcept
{
    const auto hostDataContentReader = fromHostRef (contentReaderHostRef);
    ARA_VALIDATE_API_ARGUMENT (contentReaderHostRef, ARA::contains (_hostDataContentReaders, hostDataContentReader));
    ARA_VALIDATE_API_THREAD (_araDocumentController->wasCreatedOnCurrentThread ());

    ARA::find_erase (_hostDataContentReaders, hostDataContentReader);
}
