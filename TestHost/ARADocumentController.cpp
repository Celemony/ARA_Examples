//------------------------------------------------------------------------------
//! \file       ARADocumentController.cpp
//!             provides access the plug-in document controller
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

#include "ARADocumentController.h"

#include "ARAHostInterfaces/ARAAudioAccessController.h"
#include "ARAHostInterfaces/ARAArchivingController.h"
#include "ARAHostInterfaces/ARAContentAccessController.h"
#include "ARAHostInterfaces/ARAModelUpdateController.h"
#include "ARAHostInterfaces/ARAPlaybackController.h"

#include "ARA_Library/Dispatch/ARAHostDispatch.h"
#include "ARA_Library/Debug/ARAContentLogger.h"

#include <thread>
#include <cmath>
#include <cstring>

#if defined (__APPLE__)
    #include <ApplicationServices/ApplicationServices.h>
#endif

/*******************************************************************************/

ARADocumentController::ARADocumentController (Document* document, const ARA::ARAFactory* araFactory)
: _document { document },
  _documentControllerHostInstance { new ARAAudioAccessController (this),
                                    new ARAArchivingController (this),
                                    new ARAContentAccessController (this),
                                    new ARAModelUpdateController (this),
                                    new ARAPlaybackController (this) }
{
    const auto documentProperties { getDocumentProperties () };
    auto documentControllerInstance { araFactory->createDocumentControllerWithDocument (&_documentControllerHostInstance, &documentProperties) };
    ARA_VALIDATE_API_STRUCT_PTR (documentControllerInstance, ARADocumentControllerInstance);

    _documentController = std::make_unique<ARA::Host::DocumentController> (documentControllerInstance);
    ARA_VALIDATE_API_INTERFACE (_documentController->getInterface (), ARADocumentControllerInterface);

    ARA_VALIDATE_API_CONDITION (_documentController->getFactory () == araFactory);
}

ARADocumentController::~ARADocumentController ()
{
    ARA_INTERNAL_ASSERT (!_isEditingDocument);
    _documentController->destroyDocumentController ();
    delete _documentControllerHostInstance.getAudioAccessController ();
    delete _documentControllerHostInstance.getArchivingController ();
    delete _documentControllerHostInstance.getContentAccessController ();
    delete _documentControllerHostInstance.getModelUpdateController ();
    delete _documentControllerHostInstance.getPlaybackController ();
}

/*******************************************************************************/

void ARADocumentController::beginEditing ()
{
    ARA_INTERNAL_ASSERT (!_isEditingDocument);
    _isEditingDocument = true;
    _documentController->beginEditing ();
}

void ARADocumentController::endEditing ()
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->endEditing ();
    _isEditingDocument = false;
}

/*******************************************************************************/
const DocumentProperties ARADocumentController::getDocumentProperties () const
{
    return {
        _document->getName ().c_str ()
    };
}

void ARADocumentController::updateDocumentProperties ()
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto documentProperties { getDocumentProperties () };
    _documentController->updateDocumentProperties (&documentProperties);
}

/*******************************************************************************/

const MusicalContextProperties ARADocumentController::getMusicalContextProperties (const MusicalContext* musicalContext) const
{
    return {
        musicalContext->getName ().c_str (),
        musicalContext->getOrderIndex (),
        &musicalContext->getColor ()
    };
}

void ARADocumentController::addMusicalContext (MusicalContext* musicalContext)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto musicalContextProperties { getMusicalContextProperties (musicalContext) };
    _musicalContextRefs[musicalContext] = _documentController->createMusicalContext (toHostRef (musicalContext), &musicalContextProperties);
}

void ARADocumentController::removeMusicalContext (MusicalContext* musicalContext)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->destroyMusicalContext (getRef (musicalContext));
    _musicalContextRefs.erase (musicalContext);
}

void ARADocumentController::updateMusicalContextProperties (MusicalContext* musicalContext)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto musicalContextProperties { getMusicalContextProperties (musicalContext) };
    _documentController->updateMusicalContextProperties (getRef (musicalContext), &musicalContextProperties);
}

void ARADocumentController::updateMusicalContextContent (MusicalContext* musicalContext, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->updateMusicalContextContent (getRef (musicalContext), range, scopeFlags);
}

/*******************************************************************************/

const RegionSequenceProperties ARADocumentController::getRegionSequenceProperties (const RegionSequence* regionSequence) const
{
    return {
        regionSequence->getName ().c_str (),
        regionSequence->getOrderIndex (),
        getRef (regionSequence->getMusicalContext ()),
        &regionSequence->getColor ()
    };
}

void ARADocumentController::addRegionSequence (RegionSequence* regionSequence)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto regionSequenceProperties { getRegionSequenceProperties (regionSequence) };
    _regionSequenceRefs[regionSequence] = _documentController->createRegionSequence (toHostRef (regionSequence), &regionSequenceProperties);
}

void ARADocumentController::removeRegionSequence (RegionSequence* regionSequence)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->destroyRegionSequence (getRef (regionSequence));
    _regionSequenceRefs.erase (regionSequence);
}

void ARADocumentController::updateRegionSequenceProperties (RegionSequence* regionSequence)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto regionSequenceProperties { getRegionSequenceProperties (regionSequence) };
    _documentController->updateRegionSequenceProperties (getRef (regionSequence), &regionSequenceProperties);
}

/*******************************************************************************/

const AudioSourceProperties ARADocumentController::getAudioSourceProperties (const AudioSource* audioSource) const
{
    return {
        audioSource->getName ().c_str (),
        audioSource->getPersistentID ().c_str (),
        audioSource->getSampleCount (),
        audioSource->getSampleRate (),
        audioSource->getChannelCount (),
        audioSource->merits64BitSamples ()
    };
}

void ARADocumentController::addAudioSource (AudioSource* audioSource)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto audioSourceProperties { getAudioSourceProperties (audioSource) };
    _audioSourceRefs[audioSource] = _documentController->createAudioSource (toHostRef (audioSource), &audioSourceProperties);
}

void ARADocumentController::removeAudioSource (AudioSource* audioSource)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->destroyAudioSource (getRef (audioSource));
    _audioSourceRefs.erase (audioSource);
}

void ARADocumentController::updateAudioSourceProperties (AudioSource* audioSource)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto audioSourceProperties { getAudioSourceProperties (audioSource) };
    _documentController->updateAudioSourceProperties (getRef (audioSource), &audioSourceProperties);
}

void ARADocumentController::updateAudioSourceContent (AudioSource* audioSource, const ARA::ARAContentTimeRange* range, ARA::ContentUpdateScopes scopeFlags)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->updateAudioSourceContent (getRef (audioSource), range, scopeFlags);
}

/*******************************************************************************/

const AudioModificationProperties ARADocumentController::getAudioModificationProperties (const AudioModification* audioModification) const
{
    return {
        audioModification->getName ().c_str (),
        audioModification->getPersistentID ().c_str ()
    };
}

void ARADocumentController::addAudioModification (AudioModification* audioModification)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto audioModificationProperties { getAudioModificationProperties (audioModification) };
    _audioModificationRefs[audioModification] = _documentController->createAudioModification (getRef (audioModification->getAudioSource ()), toHostRef (audioModification), &audioModificationProperties);
}

void ARADocumentController::cloneAudioModification (AudioModification* sourceAudioModification, AudioModification* clonedAudioModification)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto cloneProperties { getAudioModificationProperties (clonedAudioModification) };
    _audioModificationRefs[clonedAudioModification] = _documentController->cloneAudioModification (getRef (sourceAudioModification), toHostRef (clonedAudioModification), &cloneProperties);
}

void ARADocumentController::removeAudioModification (AudioModification* audioModification)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->destroyAudioModification (getRef (audioModification));
    _audioModificationRefs.erase (audioModification);
}

void ARADocumentController::updateAudioModificationProperties (AudioModification* audioModification)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto audioModificationProperties { getAudioModificationProperties (audioModification) };
    _documentController->updateAudioModificationProperties (getRef (audioModification), &audioModificationProperties);
}

/*******************************************************************************/

const PlaybackRegionProperties ARADocumentController::getPlaybackRegionProperties (const PlaybackRegion* playbackRegion) const
{
    return {
        playbackRegion->getTransformationFlags (),
        playbackRegion->getStartInModificationTime (),
        playbackRegion->getDurationInModificationTime (),
        playbackRegion->getStartInPlaybackTime (),
        playbackRegion->getDurationInPlaybackTime (),
        getRef (playbackRegion->getRegionSequence ()->getMusicalContext ()),    // deprecated, but set for ARA 1 backwards compatibility
        getRef (playbackRegion->getRegionSequence ()),
        playbackRegion->getName ().c_str (),
        &playbackRegion->getColor ()
    };
}

void ARADocumentController::addPlaybackRegion (PlaybackRegion* playbackRegion)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    const auto playbackRegionProperties { getPlaybackRegionProperties (playbackRegion) };
    _playbackRegionRefs[playbackRegion] = _documentController->createPlaybackRegion (getRef (playbackRegion->getAudioModification ()), toHostRef (playbackRegion), &playbackRegionProperties);
}

void ARADocumentController::removePlaybackRegion (PlaybackRegion* playbackRegion)
{
    ARA_INTERNAL_ASSERT (_isEditingDocument);
    _documentController->destroyPlaybackRegion (getRef (playbackRegion));
    _playbackRegionRefs.erase (playbackRegion);
}

void ARADocumentController::updatePlaybackRegionProperties (PlaybackRegion* playbackRegion)
{
    const auto playbackRegionProperties { getPlaybackRegionProperties (playbackRegion) };
    _documentController->updatePlaybackRegionProperties (getRef (playbackRegion), &playbackRegionProperties);
}

/*******************************************************************************/

bool ARADocumentController::supportsPartialPersistency ()
{
    return _documentController->supportsPartialPersistency ();
}

bool ARADocumentController::storeObjectsToArchive (ARAArchive* archive, const ARA::ARAStoreObjectsFilter* filter)
{
    ARA_INTERNAL_ASSERT (_currentArchive == nullptr);
    _currentArchive = archive;
    const auto result { _documentController->storeObjectsToArchive (toHostRef (archive), filter) };
    _currentArchive = nullptr;
    return result;
}

bool ARADocumentController::restoreObjectsFromArchive (const ARAArchive* archive, const ARA::ARARestoreObjectsFilter* filter)
{
    ARA_INTERNAL_ASSERT (_currentArchive == nullptr);
    _currentArchive = archive;
    const auto result { _documentController->restoreObjectsFromArchive (toHostRef (archive), filter) };
    _currentArchive = nullptr;
    return result;
}

bool ARADocumentController::storeDocumentToArchive (ARAArchive* archive)
{
    ARA_INTERNAL_ASSERT (_currentArchive == nullptr);
    _currentArchive = archive;
    const auto result { _documentController->storeDocumentToArchive (toHostRef (archive)) };
    _currentArchive = nullptr;
    return result;
}

bool ARADocumentController::beginRestoringDocumentFromArchive (const ARAArchive* archive)
{
    ARA_INTERNAL_ASSERT (_currentArchive == nullptr);
    _currentArchive = archive;
    _isEditingDocument = true;
    return _documentController->beginRestoringDocumentFromArchive (toHostRef (archive));
}

bool ARADocumentController::endRestoringDocumentFromArchive (const ARAArchive* archive)
{
    ARA_INTERNAL_ASSERT (_currentArchive == archive);
    const auto result { _documentController->endRestoringDocumentFromArchive (toHostRef (archive)) };
    _isEditingDocument = false;
    _currentArchive = nullptr;
    return result;
}

bool ARADocumentController::isUsingArchive (const ARAArchive* archive)
{
    return ((_currentArchive != nullptr) && (archive == nullptr || archive == _currentArchive));
}

/*******************************************************************************/

void ARADocumentController::enableAudioSourceSamplesAccess (AudioSource* audioSource, bool enable)
{
    _documentController->enableAudioSourceSamplesAccess (getRef (audioSource), enable);
}

void ARADocumentController::getPlaybackRegionHeadAndTailTime (PlaybackRegion* playbackRegion, double* headTime, double* tailTime)
{
    _documentController->getPlaybackRegionHeadAndTailTime (getRef (playbackRegion), headTime, tailTime);
}

void ARADocumentController::requestAudioSourceContentAnalysis (AudioSource* audioSource, size_t contentTypesCount, const ARA::ARAContentType contentTypes[], bool bWaitUntilFinish)
{
    // check license first without opening UI
    auto isLicensed { _documentController->isLicensedForCapabilities (false, contentTypesCount, contentTypes, ARA::kARAPlaybackTransformationNoChanges) };
    if (!isLicensed)
    {
        // an actual host would now inform the user about the missing license and ask whether they
        // want to run licensing now - for testing purposes we here just assume they do.
#if defined (__APPLE__)
        // on macOS, our command line tool must be transformed into a UI task if we want to show dialogs
        bool canShowDialog { false };

    _Pragma ("GCC diagnostic push")
    _Pragma ("GCC diagnostic ignored \"-Wdeprecated\"")
        ProcessSerialNumber psn { 0, kCurrentProcess };
        ProcessInfoRec pInfo;
        memset (&pInfo, 0, sizeof (ProcessInfoRec));
        if (GetProcessInformation (&psn, &pInfo) == noErr)
        {
            if (pInfo.processMode & modeOnlyBackground)
            {
                if (TransformProcessType (&psn, kProcessTransformToForegroundApplication) == noErr)
                    canShowDialog = true;
            }
        }
    _Pragma ("GCC diagnostic pop")

        if (canShowDialog)
#endif
            isLicensed = _documentController->isLicensedForCapabilities (true, contentTypesCount, contentTypes, ARA::kARAPlaybackTransformationNoChanges);
    }

    if (!isLicensed)
        return;

    _documentController->requestAudioSourceContentAnalysis (getRef (audioSource), contentTypesCount, contentTypes);

    if (!bWaitUntilFinish)
        return;

    // Now we've got to wait for analysis to complete -
    // normally this would be done asynchronously, but in this simple test code we'll just
    // spin in a crude "update loop" until our requested analysis is complete
    while (true)
    {
        // Because this is our update loop, query the document controller for model updates here
        _documentController->notifyModelUpdates ();

        // Check if all analyses are done for the available analysis content types
        bool allDone { true };
        for (auto t { 0U }; t < contentTypesCount; ++t)
        {
            if (_documentController->isAudioSourceContentAnalysisIncomplete (getRef (audioSource), contentTypes[t]))
            {
                allDone = false;
                break;
            }
        }
        if (allDone)
            return;

        // Sleep while we wait
        std::this_thread::sleep_for (std::chrono::milliseconds { 50 });
    }
}

/*******************************************************************************/

int ARADocumentController::getProcessingAlgorithmsCount ()
{
    return _documentController->getProcessingAlgorithmsCount ();
}

const ARA::ARAProcessingAlgorithmProperties* ARADocumentController::getProcessingAlgorithmProperties (int algorithmIndex)
{
    return _documentController->getProcessingAlgorithmProperties (algorithmIndex);
}

int ARADocumentController::getProcessingAlgorithmForAudioSource (AudioSource* audioSource)
{
    return _documentController->getProcessingAlgorithmForAudioSource (getRef (audioSource));
}

void ARADocumentController::requestProcessingAlgorithmForAudioSource (AudioSource* audioSource, int algorithmIndex)
{
    return _documentController->requestProcessingAlgorithmForAudioSource (getRef (audioSource), algorithmIndex);
}

/*******************************************************************************/

void ARADocumentController::setMinimalContentUpdateLogging (bool flag)
{
    getModelUpdateController ()->setMinimalContentUpdateLogging (flag);
}

/*******************************************************************************/

ARAAudioAccessController* ARADocumentController::getAudioAccessController () const noexcept
{
    return static_cast<ARAAudioAccessController*> (_documentControllerHostInstance.getAudioAccessController ());
}
ARAArchivingController* ARADocumentController::getArchivingController () const noexcept
{
    return static_cast<ARAArchivingController*> (_documentControllerHostInstance.getArchivingController ());
}
ARAContentAccessController* ARADocumentController::getContentAccessController () const noexcept
{
    return static_cast<ARAContentAccessController*> (_documentControllerHostInstance.getContentAccessController ());
}
ARAModelUpdateController* ARADocumentController::getModelUpdateController () const noexcept
{
    return static_cast<ARAModelUpdateController*> (_documentControllerHostInstance.getModelUpdateController ());
}
ARAPlaybackController* ARADocumentController::getPlaybackController () const noexcept
{
    return static_cast<ARAPlaybackController*> (_documentControllerHostInstance.getPlaybackController ());
}
