//------------------------------------------------------------------------------
//! \file       TestAUv3App.m
//!             Empty dummy container app for the Audio Unit v3 App Extension,
//!             based on Apple's AUv3FilterDemo found at:
//!             https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins/creating_custom_audio_effects
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

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [[NSApplication sharedApplication] orderFrontStandardAboutPanel:self];
}
@end


int main(int argc, const char * argv[])
{
    AppDelegate * appDelegate = [[AppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:appDelegate];
    return NSApplicationMain(argc, argv);
}
