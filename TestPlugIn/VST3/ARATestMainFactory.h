//------------------------------------------------------------------------------
//! \file       ARATestMainFactory.h
//!             VST3 ARA Main Factory implementation for the ARA test plug-in.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2012-2025, Celemony Software GmbH, All Rights Reserved.
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

#include "ARA_API/ARAVST3.h"
#include "ARATestDocumentController.h"

ARA_DISABLE_VST3_WARNINGS_BEGIN

class ARATestMainFactory : public ARA::IMainFactory
{
public:
    ARATestMainFactory () { FUNKNOWN_CTOR }
    virtual ~ARATestMainFactory () { FUNKNOWN_DTOR }

    // Class ID
    static const Steinberg::FUID getClassFUID ()
    {
        return Steinberg::FUID (0xB761364A, 0x035149BF, 0xA580C576, 0xECD186FB);
    }

    // Create function
    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (ARA::IMainFactory*) new ARATestMainFactory ();
    }

    DECLARE_FUNKNOWN_METHODS

    //------------------------------------------------------------------------
    // ARA::IMainFactory overrides:
    //------------------------------------------------------------------------
    const ARA::ARAFactory* PLUGIN_API getFactory () SMTG_OVERRIDE
    {
        return ARATestDocumentController::getARAFactory ();
    }
};

IMPLEMENT_FUNKNOWN_METHODS (ARATestMainFactory, ARA::IMainFactory, ARA::IMainFactory::iid)

ARA_DISABLE_VST3_WARNINGS_END
