//------------------------------------------------------------------------------
//! \file       TestAUv3Factory.m
//!             Audio Unit App Extension factory,
//!             created via the Xcode 11 project template for Audio Unit App Extensions.
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2021-2023, Celemony Software GmbH, All Rights Reserved.
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

#import "TestAUv3AudioUnit.h"
#import "TestAUv3Factory.h"


@implementation TestAUv3Factory

- (void)beginRequestWithExtensionContext:(NSExtensionContext *)context {

}

- (AUAudioUnit *)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError **)error {
    return [[TestAUv3AudioUnit alloc] initWithComponentDescription:desc error:error];
}

@end
