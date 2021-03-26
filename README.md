# ARA Audio Random Access: Examples.

Copyright (c) 2012-2021, [Celemony Software GmbH](https://www.celemony.com), All Rights Reserved.
Published under the Apache 2.0 license.

The ARA_Examples git repository contains various sample code demonstrating how to both implement and host ARA enabled plug-ins. They are provided both as a mean to debug your implementation and as a blueprint for building your own ARA applications.

Developers should use the [full ARA_SDK installer](https://github.com/Celemony/ARA_SDK) which includes this repository along with the core ARA API specification, an extensive set of C++ classes and utility functions to ease ARA development, and full documentation.


## Companion APIs

Creating ARA-enabled products relies on using at least one of the various established audio plug-in formats as "Companion APIs".
The CMake code allows for settting path variables to your local copies of these SDKs: `ARA_VST3_SDK_DIR` and `ARA_AUDIO_UNIT_SDK_DIR`.
If needed, installing the SDKs can be done using the [ARA SDK Installer](https://github.com/Celemony/ARA_SDK).


## 3rd party submodules

For generic tasks such as reading .wav files or applying Base64 encodings, some examples rely on external dependencies, see separate [3rd party README](3rdParty/README.md). These dependencies have separate licensing conditions as laid out in each project, butt they should be fully compatible with ARA's terms and conditions for typical use cases.

The external projects are integrated as shallow git submodules. When using CMake these will be automatically be fetched, otherwise this can be triggered explicitly with this git command:

    git submodule update --init --recursive


## Examples overview

- Mini Host
The ARAMiniHost example is a very minimal ARA host application written using the pure C ARA API that demonstrates the basic ARA plug-in life cycle in a host.

- Test Plug-In
This Audio Unit and VST3 plug-in utilizes the `ARAPlug` classes from the ARA library and can be used as a template for creating ARA-enabled plug-ins. It executes a (fake) analysis of the audio material including proper export of the resulting ARA content data and provides pass-through ARA rendering and proper persistency.
It also features extensive validation and logging capabilities that make it very useful when developing ARA host applications.

- Test Host
The ARATestHost is a command line utility that can be used to load and debug ARA plug-ins. It implements many test scenarios that relate to real-life use cases - please see `main.cpp` for details on the tests and how to run them conveniently.
It also serves as a demonstration for how to implement the various ARA hosting interfaces, and for how to load ARA enabled plug-ins using the VST3 or AudioUnit companion APIs.

- IPC Demo
The macOS-only IPC demo illustrates how ARA's use of abstract `ARARefs` allows to easily separate ARA plug-ins into different processes/address spaces.


## Building and running the examples

Building the ARA examples relies on CMake 3.12 or newer. Using CMake 3.19 or newer is highly recommended, debugger support requires at least 3.13 for Visual Studio and 3.15 for Xcode).
To create a project for your devlopment environment of choice, from within the ARA_Examples folder execute this CMake command line (in older Cmake versions, omit the white spaces after -B/-G/-A):

    cmake -B <desired output directory, e.g. ./build> -G <desired generator for your development environment, e.g. Xcode> -A <optional architecture>

This assume you've installed the Companion API SDKs using the optional install scripts from the ARA SDK installer. If you want to instead provide local copies of those SDKs, specify them like so:

    cmake -D ARA_VST3_SDK_DIR=/path/to/VST3_SDK/ -D ARA_AUDIO_UNIT_SDK_DIR=/path/to/AudioUnit_SDK/ <then other arguments as shown above>

Note that while it is possible to directly open the ARA_Examples folder in Visual Studio 2017 or newer utilizing its integrated CMake support, we still recommend creating an explicit solution due to its superior project layout and debugger support, e.g. for VS2019:

    cmake -B build -G "Visual Studio 16 2019" -A x64

To properly handle signing and packaging on macOS, the CMake support is currently explicitly limited to creating an Xcode project:

    cmake -G "Xcode" -B build

On Linux, the examples in general are still in an experimental state, but have been successfully tested on Ubuntu 18.04 with GCC 8.3.0 and clang 7.0 using make as build system:

    cd ARA_SDK/ARA_Examples   # Enter the ARA_Examples directory
    mkdir build               # Create a CMake 'out of source' build directory
    cd build                  # Enter the build directory
    cmake ..                  # Execute cmake command to generate a GNU Makefile
    make                      # Compile the examples


To run the VST3 mini host (which automatically loads ARATestPlugIn.vst3), from within your build directory execute:

    ./bin/Debug/ARAMiniHost

To run the test host example e.g. using the VST3 variant of the ARATestPlugIn:

    ./bin/Debug/TestHost -vst3 ARATestPlugIn.vst3

Or for the Audio Unit version on macOS:

    ./bin/Debug/TestHost -au aufx AraT ADeC

Where applicable (Visual Studio solution, Xcode project), debugging a plug-in target will per default launch the validator application provided by the companion API. Debugging with the ARATestHost will per default launch the SDK's ARATestPlugIn example in its VST3 incarnation.
Note that on macOS, debugging anAudio Unit with Apple's auval tool might not be possible on your machine due to System Integrity Protection. You will need to run auvaltool without attaching the debugger in that case.
