//------------------------------------------------------------------------------
//! \file       TestAUv3AudioUnit.h
//!             Audio Unit App Extension implementation,
//!             created via the Xcode 11 project template for Audio Unit App Extensions.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2024, Celemony Software GmbH, All Rights Reserved.
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

#import <AudioToolbox/AudioToolbox.h>

#import "ARA_API/ARAAudioUnit_v3.h"

@interface TestAUv3AudioUnit : AUAudioUnit<ARAAudioUnit>

@end
