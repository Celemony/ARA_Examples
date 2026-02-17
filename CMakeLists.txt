# ======================
# ARA Examples CMake Integration
#
# Copyright (c) 2020-2026, Celemony Software GmbH, All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ======================

# See accompanying README.md for details about the CMake integration of the ARA examples.

# ======================
# cmake compatibility

# You may be able to use older versions by commenting out this line, but we do not support it.
cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
if(APPLE)
    if(NOT CMAKE_GENERATOR MATCHES "Xcode")
        message(FATAL_ERROR "CMake support for Apple platforms is currently limited to Xcode, add '-G Xcode'.")
    endif()
elseif(NOT WIN32)
    message(STATUS "Careful: ARA CMake support for this system is currently experimental.")
endif()


# ======================
# supported build configurations

get_property(ARA_IS_MULTI_CONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(ARA_IS_MULTI_CONFIG_GENERATOR)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build Configurations" FORCE)
else()
    if(CMAKE_BUILD_TYPE)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
            message(FATAL_ERROR "CMAKE_BUILD_TYPE must be either 'Debug' or 'Release', value '${CMAKE_BUILD_TYPE}' is invalid.")
        endif()
    else()
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type (either 'Debug' or 'Release')" FORCE)
        message(STATUS "No build type specified, defaulting to '${CMAKE_BUILD_TYPE}'.")
    endif()
endif()

# these paths match Steinberg's default paths used in the VST SDK
set(optional_build_type_subfolder "")
if(NOT ARA_IS_MULTI_CONFIG_GENERATOR AND NOT WIN32)
    set(optional_build_type_subfolder "/${CMAKE_BUILD_TYPE}")
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib${optional_build_type_subfolder}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib${optional_build_type_subfolder}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin${optional_build_type_subfolder}")
unset(optional_build_type_subfolder)


# ======================
# enable universal binaries on Apple platforms

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)" CACHE STRING "macOS architecture(s) - defaults to Xcode standard archs instead of CMake build platform" FORCE)
endif()


# ======================
# ARA SDK paths

if(ARA_API_DIR)
    # normalize path if provided (and if relative path, assume it's relative to the top-level project)
    get_filename_component(ARA_API_DIR "${ARA_API_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
else()
    # if no path provided, assume ARA_API is right next to our ARA_Examples directory
    get_filename_component(ARA_API_DIR ../ARA_API ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set(ARA_API_DIR "${ARA_API_DIR}" CACHE PATH "directory of the ARA_API project")

if(ARA_LIBRARY_DIR)
    # normalize path if provided (and if relative path, assume it's relative to the top-level project)
    get_filename_component(ARA_LIBRARY_DIR "${ARA_LIBRARY_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
else()
    # if no path provided, assume ARA_Library is right next to our ARA_Examples directory
    get_filename_component(ARA_LIBRARY_DIR ../ARA_Library ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set(ARA_LIBRARY_DIR "${ARA_LIBRARY_DIR}" CACHE PATH "directory of the ARA_Library project")


# ======================
# Companion APIs paths

if(ARA_VST3_SDK_DIR)
    # normalize path if provided (and if relative path, assume it's relative to the top-level project)
    get_filename_component(ARA_VST3_SDK_DIR "${ARA_VST3_SDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
else()
    # if no path provided, assume vst3sdk is right next to our ARA_Examples directory
    get_filename_component(ARA_VST3_SDK_DIR ../vst3sdk ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set(ARA_VST3_SDK_DIR "${ARA_VST3_SDK_DIR}" CACHE PATH "directory of the VST3 SDK")
if(EXISTS "${ARA_VST3_SDK_DIR}")
    set(ARA_ENABLE_VST3 TRUE)
else()
    message(STATUS "No VST3 SDK installed, will not generate VST3 related examples.")
    set(ARA_ENABLE_VST3 FALSE)
endif()

if(APPLE)
    if(ARA_AUDIO_UNIT_SDK_DIR)
        # normalize path if provided (and if relative path, assume it's relative to the top-level project)
        get_filename_component(ARA_AUDIO_UNIT_SDK_DIR "${ARA_AUDIO_UNIT_SDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    else()
        # if no path provided, assume AudioUnitSDK is right next to our ARA_Examples directory
        get_filename_component(ARA_AUDIO_UNIT_SDK_DIR ../AudioUnitSDK ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    set(ARA_AUDIO_UNIT_SDK_DIR "${ARA_AUDIO_UNIT_SDK_DIR}" CACHE PATH "directory of the Audio Unit SDK")
    if(EXISTS "${ARA_AUDIO_UNIT_SDK_DIR}")
        set(ARA_ENABLE_AUDIO_UNIT TRUE)
    else()
        message(STATUS "No Audio Unit SDK installed, will not generate Audio Unit related examples.")
        set(ARA_ENABLE_AUDIO_UNIT FALSE)
    endif()
else()
    set(ARA_ENABLE_AUDIO_UNIT FALSE)
endif()

if(ARA_CLAP_SDK_DIR)
    # normalize path if provided (and if relative path, assume it's relative to the top-level project)
    get_filename_component(ARA_CLAP_SDK_DIR "${ARA_CLAP_SDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
else()
    # if no path provided, assume clap is right next to our ARA_Examples directory
    get_filename_component(ARA_CLAP_SDK_DIR ../clap ABSOLUTE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
set(ARA_CLAP_SDK_DIR "${ARA_CLAP_SDK_DIR}" CACHE PATH "directory of the CLAP SDK")
if(EXISTS "${ARA_CLAP_SDK_DIR}")
    set(ARA_ENABLE_CLAP TRUE)
else()
    message(STATUS "No CLAP SDK installed, will not generate CLAP related examples.")
    set(ARA_ENABLE_CLAP FALSE)
endif()

if(NOT (ARA_ENABLE_VST3 OR ARA_ENABLE_AUDIO_UNIT OR ARA_ENABLE_CLAP))
    message(FATAL_ERROR "No companion API SDK installed - at least one of VST3, Audio Unit or CLAP SDK is required.")
endif()


# ======================
# project globals

include("${ARA_API_DIR}/ARA_Version.cmake")
include("${ARA_API_DIR}/ARA_API_Helpers.cmake")

# set this to OFF when running on build servers, ON for developer machines where you intend to debug.
# this may require building as admin, depending on the access rights needed to write to the audio plug-in folder(s)
option(ARA_SETUP_DEBUGGING "Prepare for debugging (configure debugger, install audio plug-ins into system, etc.)" ON)

# regenerating the project while building causes various of issues, esp. in Xcode, plus these examples are typically not modified
set(CMAKE_SUPPRESS_REGENERATION ON)

set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY ON)
set(CMAKE_XCODE_GENERATE_SCHEME ${ARA_SETUP_DEBUGGING})

set(CMAKE_SKIP_INSTALL_RULES YES)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# https://stackoverflow.com/questions/41695993/ignore-warnings-in-external-modules-when-using-cmake
if(POLICY CMP0077)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    cmake_policy(SET CMP0077 NEW)
endif()
if(POLICY CMP0091)
    set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
    cmake_policy(SET CMP0091 NEW)
endif()


# helpful for debugging the build if necessary
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE STRING "Generate compile commands" FORCE)

project(ARA_Examples
    DESCRIPTION "ARA Audio Random Access: Examples"
    HOMEPAGE_URL "https://github.com/Celemony/ARA_Examples"
    VERSION ${ARA_MAJOR_VERSION}.${ARA_MINOR_VERSION}.${ARA_PATCH_VERSION}.${ARA_BUILD_VERSION}
    LANGUAGES C CXX
)


# ======================
# C and C++ language standard

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

# sync C++ standard between Xcode and CMake (unfortunately the CMake Xcode generator uses OTHER_CPLUSPLUSFLAGS)
# \todo similar logic should be done above for CMAKE_C_... based on GCC_C_LANGUAGE_STANDARD
if(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD)
    if(CMAKE_CXX_STANDARD)
        if(NOT CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD MATCHES ${CMAKE_CXX_STANDARD})
            message(FATAL_ERROR "Conflicting C++ standards set in general versus Xcode-specific configuration.")
        endif()
    else()
        string(REGEX MATCH "[0123456789][0123456789]" CMAKE_CXX_STANDARD CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD)
    endif()

    string(REGEX MATCH "[Cc]\\+\\+" temp CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD)
    if (CMAKE_CXX_EXTENSIONS)
        if((temp AND NOT CMAKE_CXX_EXTENSIONS) OR
           (NOT temp AND CMAKE_CXX_EXTENSIONS))
            message(FATAL_ERROR "Conflicting C++ standards set in general versus Xcode-specific configuration.")
        endif()
    else()
        if(temp)
            set(CMAKE_CXX_EXTENSIONS OFF)
        else()
            set(CMAKE_CXX_EXTENSIONS ON)
        endif()
    endif()
    unset(temp)
endif()

if(CMAKE_CXX_STANDARD)
    if(NOT(CMAKE_CXX_STANDARD VERSION_GREATER_EQUAL 17 AND
           CMAKE_CXX_STANDARD VERSION_LESS 98))
       message(FATAL_ERROR "This project require C++17 or newer.")
    endif()
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# ======================
# support for OBJECT libraries:
# test if we can use object targets or must fall back to or static targets
# older CMake can't link to object targets, must build static lib instead
# \todo when building universal binaries for macOS, the paths to the object files are broken...
#       to work around, we need to stick with static libs for now.
#       CMake 3.20 promises a fix for the issue...
#if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
#    set(ARA_SHARED_TARGET_TYPE OBJECT)
#else()
    set(ARA_SHARED_TARGET_TYPE STATIC)
#endif()


# ======================
# target platform

if(WIN32)
    # no special setup for Windows needed at this point.
elseif(APPLE)
    if (CMAKE_OSX_DEPLOYMENT_TARGET AND
        CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.9")
        message(FATAL_ERROR "CMAKE_OSX_DEPLOYMENT_TARGET must be '10.9' or higher.")
    endif()
    if(NOT CMAKE_OSX_SYSROOT MATCHES ".+[Mm][Aa][Cc][Oo][Ss][X?x?]?[0123456789]+\.[0123456789]+\.sdk")
        message(FATAL_ERROR "CMake support for Apple platforms is currently limited to macOS.")
    endif()
elseif(UNIX)
    set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
else()
    message(FATAL_ERROR "Unsupported target system.")
endif()


# ======================
# Compiler-specific default settings

# check GNU-style compilers (GCC, Clang, AppleClang)
if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(GCC_STYLE_COMPILER TRUE)
else()
    set(GCC_STYLE_COMPILER FALSE)
endif()

if(MSVC)
    # explicitly remove debug format set by some Windows CMake generators to enable edit&continue
    # (see also similar workaround when configuring warings)
    string(REGEX REPLACE "/Z[7iI]" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/Z[7iI]" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    add_compile_options(
        /MP
        $<IF:$<CONFIG:Debug>,/ZI,/Zi>
        /diagnostics:column
    )

    # /Zc:__cplusplus, added in Visual Studio 2017 version 15.7, is required to make __cplusplus
    # accurate, see https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
    if(MSVC_VERSION GREATER_EQUAL 1914)
        add_compile_options(
            /Zc:__cplusplus
        )
    endif()

elseif(GCC_STYLE_COMPILER)
    add_compile_options(
        $<IF:$<CONFIG:Debug>,-O0,-Os>
    )

    # Architectures \todo CMake bug? This does not properly propagate to all targets, must be set again per-target...
    set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>)
    # Build Options
    set(CMAKE_XCODE_ATTRIBUTE_ENABLE_TESTABILITY $<IF:$<CONFIG:Debug>,YES,NO>)
    # Search Paths
    set(CMAKE_XCODE_ATTRIBUTE_ALWAYS_SEARCH_USER_PATHS NO)
    # Preprocessing
    set(CMAKE_XCODE_ATTRIBUTE_ENABLE_NS_ASSERTIONS $<IF:$<CONFIG:Debug>,YES,NO>)
    set(CMAKE_XCODE_ATTRIBUTE_ENABLE_STRICT_OBJC_MSGSEND YES)
endif()


# ======================
# Apple signing (defaults to signing locally for debug and not signing for release)
# Set CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY and CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
# in your CMakeCache.txt if building actual products based on these examples,
# or inject them when generating from the command line.
if(APPLE AND NOT CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY)
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "$<$<CONFIG:Debug>:->")
    message(WARNING "CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY not set via command line or CMakeCache.txt, binaries will not be signed correctly")
endif()
if(APPLE AND NOT CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM)
    message(WARNING "CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM not set via command line or CMakeCache.txt, binaries will not be signed correctly")
endif()
# \todo this should be set per-target, but for some reason CMake ignores it when set only there...
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.entitlements")
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)


# ======================
# external dependency: VST3 SDK
if(ARA_ENABLE_VST3)

    message(STATUS "Importing VST3 SDK")

    if(POLICY CMP0077)
        set(SMTG_RENAME_ASSERT OFF)
        set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF)
        set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF)
        set(SMTG_ENABLE_VSTGUI_SUPPORT OFF)
    else()
        set(SMTG_RENAME_ASSERT OFF CACHE BOOL "Disable SMTG_RENAME_ASSERT per default")
        set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Disable SMTG_ENABLE_VST3_PLUGIN_EXAMPLES per default")
        set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "Disable SMTG_ENABLE_VST3_HOSTING_EXAMPLES per default")
        set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "Disable SMTG_ENABLE_VSTGUI_SUPPORT per default")
    endif()

    # when adding these paths, the VST3 SDK CMake file implicitly adds the corresponding wrapper targets,
    # which we currently do not want because we build these plug-ins natively
    #set(SMTG_COREAUDIO_SDK_PATH "${ARA_AUDIO_UNIT_SDK_DIR}/CoreAudio")
    #set(SMTG_AAX_SDK_PATH "${ARA_AAX_SDK_DIR}")
    add_subdirectory("${ARA_VST3_SDK_DIR}" "${PROJECT_BINARY_DIR}/vst3sdk" EXCLUDE_FROM_ALL)

    macro(get_all_targets_recursive targets dir)
        get_directory_property(subdirectories DIRECTORY ${dir} SUBDIRECTORIES)
        foreach(subdir ${subdirectories})
            get_all_targets_recursive(${targets} ${subdir})
        endforeach()

        get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        list(APPEND ${targets} ${current_targets})
    endmacro()

    set(vst3_sdk_targets)
    get_all_targets_recursive(vst3_sdk_targets ${ARA_VST3_SDK_DIR})
    foreach(target ${vst3_sdk_targets})
        set_target_properties(${target} PROPERTIES
            FOLDER "3rdParty/vst3sdk"
            XCODE_GENERATE_SCHEME OFF
            XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>
            # \todo workaround for having to globally set the entitlements, see CMAKE_XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME above
            XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
        )

        # disable some warnings that do not go along with the VST3 coding style
        get_target_property(type ${target} TYPE)
        if(NOT ${type} MATCHES "UTILITY")
            if(MSVC)
                target_compile_options(${target}
                    PRIVATE
                        "/wd4127"
                )
            endif()
            if(GCC_STYLE_COMPILER)
                target_compile_options(${target}
                    PRIVATE
                        -Wno-deprecated
                )
            endif()
        endif()
    endforeach()

    function(ara_add_vst3_main target)
        get_target_property(source_dir ${target} SOURCE_DIR)
        get_target_property(sources_old ${target} SOURCES)

        set(public_sdk_SOURCE_DIR "${ARA_VST3_SDK_DIR}/public.sdk")
        smtg_target_add_library_main(${target})

        get_target_property(sources_new ${target} SOURCES)
        list(REMOVE_ITEM sources_new ${sources_old})
        source_group("VST3 SDK/main" FILES ${sources_new})

        foreach(source ${sources_new})
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                set_source_files_properties("${source}" PROPERTIES COMPILE_FLAGS
                    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-old-style-cast> -Wno-zero-as-null-pointer-constant -Wno-reserved-id-macro -Wno-undef"
                )
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                set_source_files_properties("${source}" PROPERTIES COMPILE_FLAGS
                    # \todo CMake 3.10 on Linux struggles with this, but since the SDK only adds .cpp files we can skip the generator expression for now
                    #"$<$<COMPILE_LANGUAGE:CXX>:-Wno-old-style-cast>  -Wno-undef"
                    "-Wno-old-style-cast -Wno-undef"
                )
            endif()
        endforeach()
    endfunction()

endif(ARA_ENABLE_VST3)


# ======================
# external dependency: Audio Unit SDK
if(ARA_ENABLE_AUDIO_UNIT)

    message(STATUS "Importing AudioUnitSDK")

    add_library(AudioUnitSDK ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
        "${ARA_AUDIO_UNIT_SDK_DIR}/readme.md"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUBase.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUBase.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUBuffer.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUBuffer.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUBufferAllocator.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUEffectBase.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUEffectBase.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUInputElement.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUInputElement.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUOutputElement.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUOutputElement.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUPlugInDispatch.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUPlugInDispatch.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUScopeElement.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUScopeElement.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUSilentTimeout.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/AUUtility.h"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/ComponentBase.cpp"
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source/ComponentBase.h"
    )

    target_include_directories(AudioUnitSDK PUBLIC
        "${ARA_AUDIO_UNIT_SDK_DIR}/Source"
    )

    target_compile_definitions(AudioUnitSDK PUBLIC
        -DCA_USE_AUDIO_PLUGIN_ONLY=1
    )

    target_compile_options(AudioUnitSDK
        PUBLIC
            -Wno-undef
            -Wno-shadow
            -Wno-switch-enum
            -Wno-sign-conversion
            -Wno-nullable-to-nonnull-conversion
            -Wno-deprecated
            -Wno-extra-semi
        PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -Wno-conversion
            $<$<CXX_COMPILER_ID:AppleClang>:-Wno-newline-eof>
    )

    target_link_libraries(AudioUnitSDK PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
    )

    set_target_properties(AudioUnitSDK PROPERTIES
        FOLDER "3rdParty"
        XCODE_GENERATE_SCHEME OFF
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>
    )

endif(ARA_ENABLE_AUDIO_UNIT)


# ======================
# optional external dependency: CLAP

if(ARA_ENABLE_CLAP)
    message(STATUS "Importing CLAP SDK")

    add_subdirectory("${ARA_CLAP_SDK_DIR}" "${PROJECT_BINARY_DIR}/clap" EXCLUDE_FROM_ALL)

    set_target_properties(clap clap-tests PROPERTIES
        FOLDER "3rdParty/clap"
        XCODE_GENERATE_SCHEME OFF
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>
    )
endif(ARA_ENABLE_CLAP)


# ======================
# external dependency: some general purpose 3rdParty libraries used in the ARA examples
message(STATUS "Importing more 3rd party libraries")

function(configure_3rdParty_target target)

    if(MSVC)
        target_compile_options(${target}
            PRIVATE
                /wd4244
                /wd4267
                /wd4996
                /wd6001
                /wd6054
                /wd6326
                /wd6385
                /wd6386
        )
    endif()
    if(GCC_STYLE_COMPILER)
        target_compile_options(${target}
            PRIVATE
                -Wno-conversion
                -Wno-unused-result
        )
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target}
            PUBLIC
                -Wno-reserved-id-macro
                -Wno-deprecated-dynamic-exception-spec
            PRIVATE
                -Wno-shorten-64-to-32
        )
    endif()

    set_target_properties(${target} PROPERTIES
        FOLDER "3rdParty"
        XCODE_GENERATE_SCHEME OFF
        XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>
    )

endfunction()

# fetch Git submodules if needed
if((NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-base64/base64.h") OR
   (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/AudioFile.h") OR
   (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/src/pugixml.hpp"))
    message(STATUS "Updating Git submodules")
    execute_process(COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" submodule update --init --recursive RESULT_VARIABLE result)
    if(result)
        message(FATAL_ERROR "Couldn't fetch/update Git submodules: ${result}.")
    endif()
    unset(result)
endif()

add_library(cpp-base64 ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-base64/base64.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-base64/base64.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-base64/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-base64/LICENSE"
)
configure_3rdParty_target(cpp-base64)

add_library(ICST_AudioFile ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/Common.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/MathDefs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/AudioFile.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/AudioFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/ICST_AudioFile/LICENSE.txt"
)
configure_3rdParty_target(ICST_AudioFile)

add_library(pugixml ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/src/pugiconfig.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/src/pugixml.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/src/pugixml.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/pugixml/LICENSE.md"
)
configure_3rdParty_target(pugixml)


# ======================
# enable all warnings appropriate for the ARA SDK code

if(MSVC)
    # explicitly remove default warning level set by some Windows CMake generators so we can configure it without conflict later on
    # see https://github.com/Microsoft/llvm/commit/daa55a66a09167498fc8b35b6cdece3636d88b7e
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(
        /Wall
        # warnings that are triggered by Windows or Visual Studio system headers
        # \todo see https://devblogs.microsoft.com/cppblog/broken-warnings-theory/
        #       we should be able to disable them only for the system headers
        #       this could also be used for warnings from 3rd party and companion includes
        #       CMake also offers SYSTEM includes for libraries, this would achieve the same.
        /wd4255 # missing prototype
        /wd4365 # signed/unsigned conversion
        /wd4548 # expression before comma has no effect
        /wd4668 # undefined macro
        /wd5039 # potentially throwing function passed to 'extern "C"'
        /wd5204 # trivial destructor is not virtual
        # warnings that only provide additional information for valid code:
        /wd4514 # removed unreferenced inline function
        /wd4710 # function not inlined
        /wd4711 # automatic inline expansion
        /wd4820 # struct padding
        /wd4623 # default constructor implicitly deleted
        /wd4625 # copy constructor implicitly deleted
        /wd4626 # assignment operator implicitly deleted
        /wd5027 # move assignment operator implicitly deleted
        /wd5026 # move constructor was implicitly defined as deleted
        /wd5220 # volatile member no longer implies trivial copy/move constructors/assignment operators
        /wd5045 # Spectre mitigation
        # warnings that are too broad
        /wd4355 # 'this' used in base member initializer list
                # this is only a problem if the pointer is used, but not if the pointer is merely stored for later use
    )
    if(MSVC_VERSION VERSION_LESS 1920)
        add_compile_options(
            # warnings that are triggered by Visual Studio older than 2019 system headers
            /wd4571 # catch(...) semantics changed
            /wd4774 # format string mismatch
        )
    endif()
elseif(GCC_STYLE_COMPILER)
    add_compile_options(
        -Wall
        -Wextra
        # this cannot be used for gcc because the necessary -Wno-variadic-macros below does not work, moved to clang-only
        #-Wpedantic
        -Wconversion
        -Wuninitialized
        -Wcast-align
        -Wpointer-arith
        -Wundef
        -Wpacked
        -Wswitch-enum
        -Wunreachable-code
        -Wdeprecated-declarations
        -Wno-missing-declarations
        -Wformat=2
        $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
        $<$<COMPILE_LANGUAGE:C>:-Wimplicit>
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpessimizing-move>
        $<$<COMPILE_LANGUAGE:CXX>:-Wredundant-move>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
        $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wpedantic
            -Wgcc-compat
            -Wgnu
            -Wnon-gcc
            -Wno-gnu-zero-variadic-macro-arguments
            -Wno-vla-extension
            -Wno-four-char-constants
            -Wmicrosoft
            -Wpragmas
            -Wshadow-all
            -Wsigned-enum-bitfield
            -Wnullable-to-nonnull-conversion
            -Wvector-conversion
            -Wshift-sign-overflow
            -Wloop-analysis
            -Wconditional-uninitialized
            -Wunreachable-code-aggressive
            -Wpedantic-core-features
            -Watomic-properties
            -Wimplicit-fallthrough
            -Wimplicit-function-declaration
            -Wimplicit-retain-self
            -Wmethod-signatures
            -Wmissing-noreturn
            -Wassign-enum
            -Wover-aligned
            -Wlong-long
            -Wredundant-parens
            -Wcomma
            -Wnewline-eof
            -Wformat-pedantic
            -Wformat-non-iso
            -Wdeprecated
            -Wdeprecated-implementations
            -Wdeprecated-objc-isa-usage
            -Wdeprecated-objc-pointer-introspection
            -Wdeprecated-objc-pointer-introspection-performSelector
            -Wblock-capture-autoreleasing
            -Wrange-loop-analysis
            -Wstatic-in-inline
            -Wthread-safety
            -Wthread-safety-beta
            -Wthread-safety-negative
            -Wthread-safety-verbose
            -Wobjc-missing-property-synthesis
            -Wobjc-interface-ivars
            -Wobjc-messaging-id
            -Wobjc-missing-property-synthesis
            -Wobjc-property-assign-on-object-type
            -Wreceiver-forward-class
            -Wselector
            -Wundeclared-selector
            -Wstrict-selector-match
            -Wsuper-class-method-mismatch
            -Wduplicate-decl-specifier
            -Wmain
            -Wused-but-marked-unused
            -Wavailability
            -Wunguarded-availability
            -Wheader-hygiene
            -Wnonportable-system-include-path
            -Wquoted-include-in-framework-header
            -Winvalid-or-nonexistent-directory
            -Wvariadic-macros
            -Wreserved-id-macro
            -Wreserved-user-defined-literal
            -Widiomatic-parentheses
            -Wincomplete-module
            -Wimplicit-retain-self
            -Wquoted-include-in-framework-header
            -Watomic-implicit-seq-cst
            -Wabstract-vbase-init
            -Winconsistent-missing-destructor-override
            -Woverriding-method-mismatch
            -Wundefined-func-template
            -Wundefined-reinterpret-cast
            -Wunsupported-dll-base-class-template
            -Wunused-exception-parameter
            -Wunused-member-function
            -Wunused-template
            -Wweak-template-vtables
            -Wzero-as-null-pointer-constant
            -Wprofile-instr-missing
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(
            -Wno-variadic-macros
            -Wno-vla
            -Wshadow
        )
    endif()
endif()


# ======================
# enable code analysis for the given target
function(ara_enable_target_analyzer target)

    if(MSVC)
#       file(TO_NATIVE_PATH "${CMAKE_CURRENT_LIST_DIR}" ARA_RULESET_DIR)
#       set(ARA_RULESET_FILE "${ARA_RULESET_DIR}\\ARA_Library.ruleset")
        target_compile_options(${target}
            PRIVATE
                /analyze
# \todo for some reason the compiler neither accepts this:
#               /analyze:rulesetdirectory "${ARA_RULESET_DIR}"
#               /analyze:ruleset ARA_Library.ruleset
#       nor that:
#               /analyze:ruleset "${ARA_RULESET_FILE}"
        )
    elseif(XCODE)
        set_target_properties(${target} PROPERTIES
            # Static Analyzer (Analysis Policy, Apple APIs, Security)
            XCODE_ATTRIBUTE_RUN_CLANG_STATIC_ANALYZER YES
            XCODE_ATTRIBUTE_CLANG_STATIC_ANALYZER_MODE $<IF:$<CONFIG:Debug>,shallow,deep>
            XCODE_ATTRIBUTE_CLANG_ANALYZER_SECURITY_FLOATLOOPCOUNTER YES
            XCODE_ATTRIBUTE_CLANG_ANALYZER_SECURITY_INSECUREAPI_RAND YES
            XCODE_ATTRIBUTE_CLANG_ANALYZER_SECURITY_INSECUREAPI_STRCPY YES
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # \todo instead of "cloning" the target, it may be more reliable to just run a post-build
        #       script that directly calls underlying xcodebuild/clang directly to build the
        #       target w/ analysis enabled (and obj file creation/link disabled)?
        #       But unfortunately OBJECT targets cannot have post-builds for some obscure reason...
        get_target_property(sources ${target} SOURCES)
        get_target_property(options ${target} COMPILE_OPTIONS)
        get_target_property(definitions ${target} COMPILE_DEFINITIONS)
        get_target_property(features ${target} COMPILE_FEATURES)
        get_target_property(includes ${target} INCLUDE_DIRECTORIES)
        get_target_property(libraries ${target} LINK_LIBRARIES)

        # analysis only deals with object files, so we can always build an object libraray
        # regardless of the original target's type
        set(analysis_target ${target}_analysis)
        add_library(${analysis_target} OBJECT
            ${sources}
        )
        set_target_properties(${analysis_target} PROPERTIES
            FOLDER "Static Analysis"
            EXCLUDE_FROM_ALL ON
            LINKER_LANGUAGE NONE
        )
        # \todo we need to suppress linking or else the analysis build fails -
        #       unfortunately clearing the linker language above is not sufficent for this.
        #       until this is solved, we bypass automatic analysis when building and run it manually...
        #add_dependencies(${target} ${analysis_target})

        target_compile_options(${analysis_target} PRIVATE
            PRIVATE
                ${options}
                --analyze
                # \todo do we need to pass -D__clang_analyzer__? Xcode does...
                -D__clang_analyzer__
                "SHELL:-Xclang -analyzer-config -Xclang mode=$<IF:$<CONFIG:Debug>,shallow,deep>"
                "SHELL:-Xclang -analyzer-output=text"
                "SHELL:-Xclang -analyzer-checker -Xclang core"
                "SHELL:-Xclang -analyzer-checker -Xclang deadcode"
                "SHELL:-Xclang -analyzer-checker -Xclang nullability"
                "SHELL:-Xclang -analyzer-checker -Xclang optin"
                "SHELL:-Xclang -analyzer-checker -Xclang security"
                $<$<COMPILE_LANGUAGE:CXX>: "SHELL:-Xclang -analyzer-checker -Xclang cplusplus" >
                $<$<PLATFORM_ID:Linux>: "SHELL:-Xclang -analyzer-checker -Xclang unix" >
                $<$<PLATFORM_ID:Darwin>: "SHELL:-Xclang -analyzer-checker -Xclang osx" >
        )
        if(definitions)
            target_compile_definitions(${analysis_target} PRIVATE ${definitions})
        endif()
        if(features)
            target_compile_features(${analysis_target} PRIVATE ${features})
        endif()
        if(includes)
            target_include_directories(${analysis_target} PRIVATE ${includes})
        endif()
        if(libraries)
            target_link_libraries(${analysis_target} PRIVATE ${libraries})
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
            target_compile_options(${target}
                PRIVATE
                    # \todo with default settings in debug, the gcc analyzer eventually just dies because of memory usage.
                    #       we need to test which analysis is causing this and disable it to make this usable
                    #-fanalyzer
                    $<$<NOT:$<CONFIG:Debug>>:-fanalyzer>
                    # \todo there's too many of those right now, need to figure out why...
                    #-Wanalyzer-too-complex
            )
        else()
            message(WARNING "Static code analysis requires gcc 10 an up.")
        endif()
    else()
        message(WARNING "Code analysis is not implemented for this compiler.")
    endif()

endfunction()


# ======================
# set up common settings used for all the ARA examples
function(configure_ARA_Examples_target target)

    # language standards
    target_compile_features(${target} PRIVATE
        cxx_std_14
        c_std_11
    )

    # hide symbols per default
    if(XCODE)
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN YES
            XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN YES
        )
    else()
        set_target_properties(${target} PROPERTIES
            C_VISIBILITY_PRESET hidden
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
    endif()

    # defines
    target_compile_definitions(${target} PRIVATE
        -DARA_TARGET_NAME=\"${target}\"
        -DARA_TARGET_TYPE_$<TARGET_PROPERTY:${target},TYPE>
        -DARA_MAJOR_VERSION=${ARA_MAJOR_VERSION}
        -DARA_MINOR_VERSION=${ARA_MINOR_VERSION}
        -DARA_PATCH_VERSION=${ARA_PATCH_VERSION}
        -DARA_BUILD_VERSION=${ARA_BUILD_VERSION}
        $<$<NOT:$<CONFIG:Debug>>: -DNDEBUG>
        # for these examples, we want to always enable validation, assertions and logging (also for release)
        # actual products would not do this, and let the ARA SDK enable these for debug builds only per default
        -DARA_VALIDATE_API_CALLS=1
        -DARA_ENABLE_INTERNAL_ASSERTS=1
        -DARA_ENABLE_DEBUG_OUTPUT=1
    )

    # platform-specific settings
    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            -DNOMINMAX=1
        )
    elseif(APPLE)
        set(bundle_identifier org.ara-audio.examples.${target})
        set_target_properties(${target} PROPERTIES
            ARA_PRODUCT_BUNDLE_IDENTIFIER ${bundle_identifier}
        )
        target_compile_definitions(${target} PRIVATE
            -DARA_PRODUCT_BUNDLE_IDENTIFIER="$<TARGET_PROPERTY:${target},ARA_PRODUCT_BUNDLE_IDENTIFIER>"
        )
        unset(bundle_identifier)
    endif()

    ara_disable_unwanted_warnings(${target})

    ara_enable_target_analyzer(${target})

    ara_group_target_files(${target})

    # stripping (may not be desired this way for actual products since these typically
    # require more complex symbol handling for later crash log symbolication etc.)
    if(MSVC)
        # no debug symbol stripping needed here since MSVC creates separate .pdb files
        # \todo any strip flags needed for dead stripping etc?
    elseif(XCODE)
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES
            XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING $<IF:$<CONFIG:Debug>,NO,YES>
            XCODE_ATTRIBUTE_COPY_PHASE_STRIP $<IF:$<CONFIG:Debug>,NO,YES>
        )
    else()
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set_target_properties(${target} PROPERTIES
                LINK_OPTIONS "-dead_strip"
            )
        endif()

        # \todo for some reason, the Ninja and make generators do not fully expand the generator expression.
        if(ARA_IS_MULTI_CONFIG_GENERATOR)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_STRIP}" $<$<NOT:$<CONFIG:Debug>>:-x -S> "$<TARGET_FILE:${target}>"
            )
        else()
            if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "${CMAKE_STRIP}" -x -S "$<TARGET_FILE:${target}>"
                )
            endif()
        endif()
    endif()

    # Xcode build&scheme defaults for debugging
    if(ARA_SETUP_DEBUGGING)
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>

            # unfortunately due to how CMake and Xcode interact we cannot set these per configuration,
            # so we switch the sanitizers off by default for now (\todo would be great to enable them for debug).
            XCODE_SCHEME_DISABLE_MAIN_THREAD_CHECKER OFF
            XCODE_SCHEME_MAIN_THREAD_CHECKER_STOP ON
            XCODE_SCHEME_ADDRESS_SANITIZER OFF
            XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN ON
            XCODE_SCHEME_MALLOC_SCRIBBLE ON
            XCODE_SCHEME_THREAD_SANITIZER OFF
            XCODE_SCHEME_THREAD_SANITIZER_STOP ON
            XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER OFF
            XCODE_SCHEME_UNDEFINED_BEHAVIOUR_SANITIZER_STOP ON
        )
    endif()

endfunction()


# ======================
# set up common settings used for all targets that produce "shipping" binaries
function(ara_configure_binary_package target extension apple_plist_file)

    # resources, bundle and signing
    set(target_resources "")
    if(WIN32)
        list(APPEND target_resources
            "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/Windows/ARAExamples.rc"
        )
    elseif(APPLE)
        list(APPEND target_resources
            ${apple_plist_file}
        )
        if (NOT ${extension} MATCHES "framework")
            list(APPEND target_resources
                "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.icns"
            )
        endif()
        if(NOT ${extension} MATCHES "appex")
            list(APPEND target_resources
                "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.entitlements"
            )
        endif()
    endif()
    target_sources(${target} PRIVATE
        ${target_resources}
    )
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${target_resources})

    if(APPLE)
        if(${extension} MATCHES "framework")
            set_target_properties(${target} PROPERTIES
                FRAMEWORK ON
                FRAMEWORK_EXTENSION ${extension}
                MACOSX_FRAMEWORK_INFO_PLIST "${apple_plist_file}"
            )
        else()
            set_target_properties(${target} PROPERTIES
                BUNDLE ON
                BUNDLE_EXTENSION ${extension}
                MACOSX_BUNDLE_INFO_PLIST "${apple_plist_file}"
                RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.icns"
            )
        endif()

        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_INFOPLIST_PREPROCESS YES
            XCODE_ATTRIBUTE_GENERATE_PKGINFO_FILE YES
            XCODE_ATTRIBUTE_WRAPPER_EXTENSION ${extension}
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER $<TARGET_PROPERTY:${target},ARA_PRODUCT_BUNDLE_IDENTIFIER>

            XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        )
        if(NOT ${extension} MATCHES "appex")
            set_target_properties(${target} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.entitlements"
            )
        endif()


        # \todo this causes codesign to fail, Finder info and resource forks are forbidden these days...
        #if(${extension} MATCHES "component|vst3")
        #   add_custom_command(TARGET ${target} POST_BUILD
        #       COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/AddFolderIcon.sh" "$<TARGET_BUNDLE_DIR:${target}>"
        #   )
        #endif()
    else()
        set_target_properties(${target} PROPERTIES
            PREFIX ""
            SUFFIX ".${extension}"
        )
    endif()

   if(MSVC)
        target_link_options(${target} PRIVATE
            /NOEXP
        )
    endif()

    # even though our plug-ins are technically libs, they are distributed as user-visible binaries so we place them next to the apps
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )

endfunction()


# ======================
# add post-build step that installs an audio plug-in into the system so that it can be debugged w/ various hosts
# this may require building as admin, depending on the access rights needed to write to the plug-in folder
if(ARA_SETUP_DEBUGGING)
    function(ara_postbuild_install_for_debugging target install_target_dir create_symlink)

        if(APPLE)
            set(source_package "$<TARGET_BUNDLE_DIR:${target}>")
            set(package_name "$<TARGET_FILE_NAME:${target}>.$<TARGET_PROPERTY:${target},BUNDLE_EXTENSION>")
        else()
            set(source_package "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>")
            set(package_name "$<TARGET_FILE_NAME:${target}>")
        endif()

        if(create_symlink)
            set(install_command create_symlink)
        else()
            set(install_command copy_directory)
        endif()

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                make_directory "${install_target_dir}"
            COMMAND "${CMAKE_COMMAND}" -E
                rm -Rf "${install_target_dir}${package_name}"
            COMMAND "${CMAKE_COMMAND}" -E
                ${install_command} "${source_package}" "${install_target_dir}${package_name}"
        )

        add_custom_command(TARGET ARATestPlugInUninstall POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                make_directory "${install_target_dir}"
            COMMAND "${CMAKE_COMMAND}" -E
                rm -Rf "${install_target_dir}${package_name}"
        )
        
    endfunction()
endif()


# ======================
# ARA SDK dependencies (after defining the warnings so the SDK code is compiled with them)

# ARA_Library (including ARA_API)
message(STATUS "Importing ARA_Library (including ARA_API)")
add_subdirectory("${ARA_LIBRARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ARA_Library.build" EXCLUDE_FROM_ALL)
set_target_properties(ARA_Host_Library ARA_PlugIn_Library ARA_IPC_Library PROPERTIES
    FOLDER "ARA_Library"
    XCODE_GENERATE_SCHEME OFF
    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH $<IF:$<CONFIG:Debug>,YES,NO>
)

# for debug builds, these are enabled automatically by the ARA SDK, as is appropriate for actual products
# - but for our examples/tests here, we want to enable these also for release builds
foreach(target ARA_Host_Library ARA_PlugIn_Library ARA_IPC_Library)
    target_compile_definitions(${target} PUBLIC
        -DARA_VALIDATE_API_CALLS=1
        -DARA_ENABLE_INTERNAL_ASSERTS=1
        -DARA_ENABLE_DEBUG_OUTPUT=1
    )

    ara_enable_target_analyzer(${target})
endforeach()


# ======================
# common code shared to varying degrees between the various host and plug-in targets

add_library(ARAExamplesCommon ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/Utilities/StdUniquePtrUtilities.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/Archives/Archives.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/Archives/Archives.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/AudioFiles/AudioFiles.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/AudioFiles/AudioFiles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/SignalProcessing/PulsedSineSignal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/SignalProcessing/PulsedSineSignal.c"
)
if(ARA_ENABLE_VST3)
    target_sources(ARAExamplesCommon PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/VST3Loader.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/VST3Loader.cpp"
    )
    target_include_directories(ARAExamplesCommon
        PRIVATE
            "${ARA_VST3_SDK_DIR}"
    )
endif()
if(ARA_ENABLE_AUDIO_UNIT)
    target_sources(ARAExamplesCommon PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/AudioUnitLoader.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/AudioUnitLoader.m"
    )
endif()
if(ARA_ENABLE_CLAP)
    target_sources(ARAExamplesCommon PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/CLAPLoader.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/PlugInHosting/CLAPLoader.c"
    )
    target_link_libraries(ARAExamplesCommon PRIVATE
        clap
    )
endif()

set_target_properties(ARAExamplesCommon PROPERTIES
    FOLDER "Examples"
    XCODE_GENERATE_SCHEME OFF
)

target_link_libraries(ARAExamplesCommon PUBLIC
    cpp-base64
    ICST_AudioFile
    pugixml
)

if(APPLE)
    target_link_libraries(ARAExamplesCommon PUBLIC
        "-framework CoreServices"
        "-framework AVFoundation"
        "-framework AudioUnit"
        "-framework AudioToolbox"
    )
elseif(UNIX)
    target_link_libraries(ARAExamplesCommon PUBLIC
        "m"
        "dl"
    )
endif()

target_include_directories(ARAExamplesCommon
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${ARA_LIBRARY_DIR}/.."
)

configure_ARA_Examples_target(ARAExamplesCommon)


# ======================
# common code shared between the various TestPlugIn targets

add_library(ARATestPlugInCommon ${ARA_SHARED_TARGET_TYPE} EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestAudioSource.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestAudioSource.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestDocumentController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestDocumentController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestPlaybackRenderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/ARATestPlaybackRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/TestAnalysis.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/TestAnalysis.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/TestPersistency.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/TestPersistency.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/TestPlugInConfig.h"
)

set_target_properties(ARATestPlugInCommon PROPERTIES
    FOLDER "Examples/ARATestPlugIn"
    XCODE_GENERATE_SCHEME OFF
)

target_link_libraries(ARATestPlugInCommon PUBLIC
    ARA_PlugIn_Library
    ARAExamplesCommon
)

# \todo add IPC implementation for Linux
if(APPLE OR WIN32)
    target_link_libraries(ARATestPlugInCommon PUBLIC
        ARA_IPC_Library
    )
endif()

target_include_directories(ARATestPlugInCommon PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn"
)

configure_ARA_Examples_target(ARATestPlugInCommon)


# ======================
# debug uninstall target for all variants of the Test Plug-In

if(ARA_SETUP_DEBUGGING)
    add_custom_target(ARATestPlugInUninstall)
    set_target_properties(ARATestPlugInUninstall PROPERTIES
        FOLDER "Examples/ARATestPlugIn"
        XCODE_GENERATE_SCHEME ON
        EXCLUDE_FROM_ALL ON
    )
endif()


# ======================
# audio plug-in target VST3 Test Plug-In

if (ARA_ENABLE_VST3)

    if(APPLE)
        add_library(ARATestPlugInVST3 MODULE "")
    else()
        add_library(ARATestPlugInVST3 SHARED "")
    endif()

    target_sources(ARATestPlugInVST3 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/VST3/ARATestMainFactory.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/VST3/TestVST3Entry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/VST3/TestVST3Processor.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/VST3/TestVST3Processor.cpp"
    )

    set_target_properties(ARATestPlugInVST3 PROPERTIES
        FOLDER "Examples/ARATestPlugIn"
        OUTPUT_NAME "ARATestPlugIn"
        PDB_NAME "ARATestPlugIn_vst3"
    )

    target_link_libraries(ARATestPlugInVST3 PRIVATE
        ARATestPlugInCommon
        sdk
    )
    if(APPLE)
        target_link_libraries(ARATestPlugInVST3 PRIVATE
            "-framework CoreFoundation"
        )
    elseif(UNIX)
        target_link_libraries(ARATestPlugInVST3 PRIVATE
            "pthread"
            "atomic"
        )
    endif()

    configure_ARA_Examples_target(ARATestPlugInVST3)

    ara_add_vst3_main(ARATestPlugInVST3)

    ara_configure_binary_package(ARATestPlugInVST3 "vst3" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/VST3/TestVST3Info.plist")

    if(ARA_SETUP_DEBUGGING)
        if(WIN32)
            ara_postbuild_install_for_debugging(ARATestPlugInVST3 "$ENV{CommonProgramW6432}\\VST3\\" ON)
        elseif(APPLE)
            ara_postbuild_install_for_debugging(ARATestPlugInVST3 "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/" ON)
        endif()

        add_dependencies(ARATestPlugInVST3 validator)
        get_target_property(validator_dir validator RUNTIME_OUTPUT_DIRECTORY)
        get_target_property(plugin_dir ARATestPlugInVST3 RUNTIME_OUTPUT_DIRECTORY)
        if(ARA_IS_MULTI_CONFIG_GENERATOR)
            # \todo always using Debug here...
            # Xcode scheme uses Debug as default config for its "Run" action, regardless of the currently selected configuration.
            # Also, generator expressions are not evaluated when creating the schemes, so we can't really know what is selected.
            set(validator_dir "${validator_dir}/Debug")
            set(plugin_dir "${plugin_dir}/Debug")
        endif()
        set_target_properties(ARATestPlugInVST3 PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY  "${plugin_dir}"
            XCODE_SCHEME_WORKING_DIRECTORY "${plugin_dir}"
            VS_DEBUGGER_COMMAND     "${validator_dir}/validator"
            XCODE_SCHEME_EXECUTABLE "${validator_dir}/validator"
            VS_DEBUGGER_COMMAND_ARGUMENTS "ARATestPlugIn.vst3"
            XCODE_SCHEME_ARGUMENTS        "ARATestPlugIn.vst3"
        )
        unset(validator_dir)
    endif()

endif(ARA_ENABLE_VST3)


# ======================
# audio plug-in target Audio Unit Test Plug-In

if(ARA_ENABLE_AUDIO_UNIT)

    add_library(ARATestPlugInAudioUnit MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit/TestAudioUnit.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit/TestAudioUnit.cpp"
    )

    set_target_properties(ARATestPlugInAudioUnit PROPERTIES
        FOLDER "Examples/ARATestPlugIn"
        OUTPUT_NAME "ARATestPlugIn"
    )

    target_compile_options(ARATestPlugInAudioUnit PRIVATE
        -Wno-reserved-id-macro
        -Wno-old-style-cast
        -Wno-zero-as-null-pointer-constant
    )

    target_link_libraries(ARATestPlugInAudioUnit PRIVATE
        ARATestPlugInCommon
        AudioUnitSDK
    )

    configure_ARA_Examples_target(ARATestPlugInAudioUnit)

    ara_configure_binary_package(ARATestPlugInAudioUnit "component" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit/TestAudioUnitInfo.plist")

    if(ARA_SETUP_DEBUGGING)
        ara_postbuild_install_for_debugging(ARATestPlugInAudioUnit "$ENV{HOME}/Library/Audio/Plug-Ins/Components/" OFF)
        # \todo Due to SIP, debugging with auvaltool is no longer allowed.
        # On Intel Macs, it's possible to copy it out of /usr/bin,but this works no longer on Apple Silicon.
        # As workaround, disable "Debug executable" in the "Info" tab of the scheme.
        #if(NOT EXISTS "/usr/local/bin/auvaltool")
        #   execute_process(COMMAND sudo cp /usr/bin/auvaltool /usr/local/bin)
        #endif()
        set_target_properties(ARATestPlugInAudioUnit PROPERTIES
            #XCODE_SCHEME_EXECUTABLE "/usr/local/bin/auvaltool"
            XCODE_SCHEME_EXECUTABLE "/usr/bin/auvaltool"
            XCODE_SCHEME_ARGUMENTS "-v aufx AraT ADeC"
        )
    endif()

    # the `${INFOPLIST_FILE}` variable is provided via Xcode's build system environment
    add_custom_command(TARGET ARATestPlugInAudioUnit PRE_BUILD
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit/mangle-plugin-version.sh "${ARA_API_DIR}/ARA_Version.cmake" \${INFOPLIST_FILE}
    )

endif(ARA_ENABLE_AUDIO_UNIT)


# ======================
# audio plug-in target Audio Unit v3 App Extension Test Plug-In, including the required dummy app

if(APPLE)

    add_library(ARATestPlugInAUv3Framework SHARED
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/BufferedAudioBus.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3AudioUnit.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3AudioUnit.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3DSPKernel.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3Factory.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3Factory.m"
    )

    set_target_properties(ARATestPlugInAUv3Framework PROPERTIES
        FOLDER "Examples/ARATestPlugIn/ARATestPlugInAUv3"
        OUTPUT_NAME "ARATestPlugInAUv3"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES
        XCODE_GENERATE_SCHEME OFF
    )

    target_compile_options(ARATestPlugInAUv3Framework PRIVATE
        -Wno-switch-enum
        -Wno-sign-compare
        -Wno-sign-conversion
        -Wno-non-virtual-dtor
        -Wno-nullable-to-nonnull-conversion
        -Wno-old-style-cast
        -Wno-unused-parameter
        -Wno-objc-missing-property-synthesis
        -Wno-atomic-properties
    )

    target_link_libraries(ARATestPlugInAUv3Framework PRIVATE
        ARATestPlugInCommon
        "-framework CoreServices"
        "-framework AVFoundation"
        "-framework AudioToolbox"
        "-framework AudioUnit"
    )

    configure_ARA_Examples_target(ARATestPlugInAUv3Framework)

    ara_configure_binary_package(ARATestPlugInAUv3Framework "framework" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Framework/TestAUv3FrameworkInfo.plist")


    add_executable(ARATestPlugInAUv3Extension MACOSX_BUNDLE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Extension/TestAUv3Extension.m"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Extension/TestAUv3Extension.entitlements"
    )

    set_target_properties(ARATestPlugInAUv3Extension PROPERTIES
        FOLDER "Examples/ARATestPlugIn/ARATestPlugInAUv3"
        OUTPUT_NAME "ARATestPlugInAUv3"
        XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Extension/TestAUv3Extension.entitlements"
        XCODE_GENERATE_SCHEME OFF
    )

    target_link_libraries(ARATestPlugInAUv3Extension PRIVATE
        ARATestPlugInAUv3Framework
    )

    configure_ARA_Examples_target(ARATestPlugInAUv3Extension)

    ara_configure_binary_package(ARATestPlugInAUv3Extension "appex" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/Extension/TestAUv3ExtensionInfo.plist")

    # hotfix: bundle identifier of extension must start with identifier of container application, patch accordingly
    set(auv3_bundle_id org.ara-audio.examples.ARATestPlugInAUv3App.AUv3Extension)
    # \todo this would be cleaner:
    #set(auv3_bundle_id $<TARGET_PROPERTY:ARATestPlugInAUv3App,ARA_PRODUCT_BUNDLE_IDENTIFIER>.AUv3Extension)
    set_target_properties(ARATestPlugInAUv3Extension PROPERTIES
        ARA_PRODUCT_BUNDLE_IDENTIFIER ${auv3_bundle_id}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${auv3_bundle_id}
    )
    target_compile_definitions(ARATestPlugInAUv3Extension PRIVATE
        -DARA_PRODUCT_BUNDLE_IDENTIFIER="${auv3_bundle_id}"
    )
    unset(auv3_bundle_id)

    add_dependencies(ARATestPlugInAUv3Extension ARATestPlugInAUv3Framework)


    add_executable(ARATestPlugInAUv3App MACOSX_BUNDLE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/App/TestAUv3App.m"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/App/TestAUv3App.xib"
    )

    set_target_properties(ARATestPlugInAUv3App PROPERTIES
        FOLDER "Examples/ARATestPlugIn/ARATestPlugInAUv3"
        OUTPUT_NAME "ARATestPlugInAUv3"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_EMBED_APP_EXTENSIONS ARATestPlugInAUv3Extension
    )

    target_link_libraries(ARATestPlugInAUv3App PRIVATE
        "-framework Cocoa"
    )

    configure_ARA_Examples_target(ARATestPlugInAUv3App)

    ara_configure_binary_package(ARATestPlugInAUv3App "app" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/App/TestAUv3AppInfo.plist")

    # \todo this is the only target with actual resources in addition to the icon, improve ara_configure_binary_package to handle this correctly
    set(ARATestPlugInAUv3App_Resources
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioUnit_v3/App/TestAUv3App.xib"
        "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/macOS/ARAExamples.icns"
    )
    set_target_properties(ARATestPlugInAUv3App PROPERTIES
        RESOURCE "${ARATestPlugInAUv3App_Resources}"
    )

    add_dependencies(ARATestPlugInAUv3App ARATestPlugInAUv3Extension)

    # copy framework and extension into application
    set(app_dir "$<TARGET_BUNDLE_DIR:ARATestPlugInAUv3App>")
    set(source_dir "$<TARGET_BUNDLE_DIR:ARATestPlugInAUv3Extension>")
    set(source_name "$<TARGET_FILE_NAME:ARATestPlugInAUv3Extension>.$<TARGET_PROPERTY:ARATestPlugInAUv3Extension,BUNDLE_EXTENSION>")
    add_custom_command(TARGET ARATestPlugInAUv3App POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            rm -Rf "${app_dir}/Contents/PlugIns/${source_name}"
        COMMAND "${CMAKE_COMMAND}" -E
            copy_directory "${source_dir}" "${app_dir}/Contents/PlugIns/${source_name}"
    )
    set(source_dir "$<TARGET_BUNDLE_DIR:ARATestPlugInAUv3Framework>")
    set(source_name "$<TARGET_FILE_NAME:ARATestPlugInAUv3Framework>.$<TARGET_PROPERTY:ARATestPlugInAUv3Framework,FRAMEWORK_EXTENSION>")
    add_custom_command(TARGET ARATestPlugInAUv3App POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            rm -Rf "${app_dir}/Contents/Frameworks/${source_name}"
        COMMAND "${CMAKE_COMMAND}" -E
            copy_directory "${source_dir}" "${app_dir}/Contents/Frameworks/${source_name}"
    )
    unset(source_name)
    unset(source_dir)
    unset(app_dir)

    if(ARA_SETUP_DEBUGGING)
        add_custom_command(TARGET ARATestPlugInUninstall POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E
                rm -Rf "$<TARGET_BUNDLE_DIR:ARATestPlugInAUv3App>"
        )
    endif()

endif(APPLE)


# ======================
# optional audio plug-in target CLAP Test Plug-In

if(ARA_ENABLE_CLAP)

    if (APPLE)
        add_library(ARATestPlugInCLAP MODULE "")
    else()
        add_library(ARATestPlugInCLAP SHARED "")
    endif()

    target_sources(ARATestPlugInCLAP PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/CLAP/TestCLAPPlugIn.cpp"
    )

    set_target_properties(ARATestPlugInCLAP PROPERTIES
        FOLDER "Examples/ARATestPlugIn"
        OUTPUT_NAME "ARATestPlugIn"
        PDB_NAME "ARATestPlugIn_clap"
    )

    target_compile_features(ARATestPlugInCLAP PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(ARATestPlugInCLAP
            PRIVATE
                /wd4100
        )
    elseif(GCC_STYLE_COMPILER)
        target_compile_options(ARATestPlugInCLAP
            PRIVATE
                -Wno-unused-parameter
                -Wno-old-style-cast
        )
    endif()

    target_link_libraries(ARATestPlugInCLAP PRIVATE
        ARATestPlugInCommon
        clap
    )

    if(APPLE)
        target_link_libraries(ARATestPlugInCLAP PRIVATE
            "-framework CoreFoundation"
        )
    elseif(UNIX)
        target_link_libraries(ARATestPlugInCLAP PRIVATE
            "pthread"
            "atomic"
        )
    endif()

    configure_ARA_Examples_target(ARATestPlugInCLAP)

    ara_configure_binary_package(ARATestPlugInCLAP "clap" "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/CLAP/TestCLAPInfo.plist")

    if(ARA_SETUP_DEBUGGING)
        if(WIN32)
            ara_postbuild_install_for_debugging(ARATestPlugInCLAP "$ENV{CommonProgramW6432}\\CLAP\\" ON)
        elseif(APPLE)
            ara_postbuild_install_for_debugging(ARATestPlugInCLAP "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP/" ON)
        elseif(UNIX)
            ara_postbuild_install_for_debugging(ARATestPlugInCLAP "$ENV{HOME}/.clap" ON)
        endif()
    endif()

endif(ARA_ENABLE_CLAP)


# ======================
# command line tool target for ARA audio file chunk authoring for the TestPlugIn

add_executable(ARATestChunkWriter
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/SignalProcessing/PulsedSineSignal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ExamplesCommon/SignalProcessing/PulsedSineSignal.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestPlugIn/AudioFileChunkWriter/ARATestChunkWriter.cpp"
)

set_target_properties(ARATestChunkWriter PROPERTIES
    FOLDER "Examples/ARATestPlugIn"
)

target_link_libraries(ARATestChunkWriter PRIVATE
    ARATestPlugInCommon
)

configure_ARA_Examples_target(ARATestChunkWriter)

if(WIN32)
    ara_configure_binary_package(ARATestChunkWriter "exe" "")
endif()

if(ARA_SETUP_DEBUGGING)
    set(ARATestChunkWriter_Dbg_Arguments "")
    # optionally, enable openAutomatically:
    #string(APPEND ARATestChunkWriter_Dbg_Arguments "-openAutomatically ")
    string(APPEND ARATestChunkWriter_Dbg_Arguments "TestSineWithARAChunk.wav")
    set_target_properties(ARATestChunkWriter PROPERTIES
        VS_DEBUGGER_COMMAND_ARGUMENTS ${ARATestChunkWriter_Dbg_Arguments}
        XCODE_SCHEME_ARGUMENTS ${ARATestChunkWriter_Dbg_Arguments}
    )
endif()


# ======================
# command line tool target Mini Host (C only)

add_executable(ARAMiniHost
    "${CMAKE_CURRENT_SOURCE_DIR}/MiniHost/MiniHost.c"
)

set_target_properties(ARAMiniHost PROPERTIES
    LINKER_LANGUAGE C
    FOLDER "Examples"
)

target_link_libraries(ARAMiniHost PRIVATE
    ARA_API
    ARAExamplesCommon
    ARA_Host_Library
    ARA_IPC_Library
)

if(ARA_ENABLE_VST3)
    target_compile_definitions(ARAMiniHost PRIVATE
        -DPLUGIN_FORMAT=PLUGIN_FORMAT_VST3
    )
elseif(ARA_ENABLE_AUDIO_UNIT)
    target_compile_definitions(ARAMiniHost PRIVATE
        -DPLUGIN_FORMAT=PLUGIN_FORMAT_AU
    )
elseif(ARA_ENABLE_CLAP)
    target_compile_definitions(ARAMiniHost PRIVATE
        -DPLUGIN_FORMAT=PLUGIN_FORMAT_CLAP
    )
endif()

if(MSVC)
    target_compile_options(ARAMiniHost PRIVATE
        /wd4100
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ARAMiniHost PRIVATE
        -Wno-unused-parameter
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(ARAMiniHost PRIVATE
        -Wno-unused-parameter
    )
endif()

configure_ARA_Examples_target(ARAMiniHost)

if(WIN32)
    ara_configure_binary_package(ARAMiniHost "exe" "")
endif()

if(ARA_SETUP_DEBUGGING)
    if(ARA_ENABLE_VST3)
        add_dependencies(ARAMiniHost ARATestPlugInVST3)
    elseif(ARA_ENABLE_AUDIO_UNIT)
        add_dependencies(ARAMiniHost ARATestPlugInAudioUnit)
    elseif(ARA_ENABLE_CLAP)
        add_dependencies(ARAMiniHost ARATestPlugInCLAP)
    endif()
endif()


# ======================
# command line tool target Test Host

add_executable(ARATestHost
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAArchivingController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAArchivingController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAAudioAccessController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAAudioAccessController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAContentAccessController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAContentAccessController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAModelUpdateController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAModelUpdateController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAPlaybackController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARAHostInterfaces/ARAPlaybackController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARADocumentController.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ARADocumentController.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/CompanionAPIs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/CompanionAPIs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ModelObjects.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/ModelObjects.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/TestHost.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/TestHost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/TestCases.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/TestCases.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/main.cpp"
)

set_target_properties(ARATestHost PROPERTIES
    FOLDER "Examples"
)

target_include_directories(ARATestHost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/TestHost"
)

if(ARA_ENABLE_VST3)
    target_compile_definitions(ARATestHost PRIVATE
        -DARA_ENABLE_VST3=1
    )
else()
    target_compile_definitions(ARATestHost PRIVATE
        -DARA_ENABLE_VST3=0
    )
endif()

if(ARA_ENABLE_CLAP)
    target_compile_definitions(ARATestHost PRIVATE
        -DARA_ENABLE_CLAP=1
    )
    target_link_libraries(ARATestHost PRIVATE
        clap
    )
else()
    target_compile_definitions(ARATestHost PRIVATE
        -DARA_ENABLE_CLAP=0
    )
endif()

target_link_libraries(ARATestHost PRIVATE
    ARAExamplesCommon
    ARA_Host_Library
)
if(APPLE)
    target_link_libraries(ARATestHost PRIVATE
        "-framework ApplicationServices"
    )
elseif(UNIX)
    target_link_libraries(ARATestHost PRIVATE
        "pthread"
    )
endif()

# \todo add IPC implementation for Linux
if(APPLE OR WIN32)
    target_sources(ARATestHost PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/IPC/IPCMessageChannel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/IPC/IPCMessageChannel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/IPC/IPCXMLEncoding.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestHost/IPC/IPCXMLEncoding.cpp"
    )

    target_link_libraries(ARATestHost PRIVATE
        ARA_IPC_Library
    )
endif()

configure_ARA_Examples_target(ARATestHost)

if(WIN32)
    ara_configure_binary_package(ARATestHost "exe" "")
endif()

if(ARA_SETUP_DEBUGGING)
    if(ARA_ENABLE_VST3)
        add_dependencies(ARATestHost ARATestPlugInVST3)
    endif()
    if(ARA_ENABLE_AUDIO_UNIT)
        add_dependencies(ARATestHost ARATestPlugInAudioUnit)
    endif()
    if(APPLE)
        add_dependencies(ARATestHost ARATestPlugInAUv3App)
    endif()
    if(ARA_ENABLE_CLAP)
        add_dependencies(ARATestHost ARATestPlugInCLAP)
    endif()

    set(ARATestHost_Dbg_Arguments "")
    if(WIN32)
        # per default, test with VST3 example plug-in
        string(APPEND ARATestHost_Dbg_Arguments "-vst3 ARATestPlugIn.vst3")
        # alternatively for testing with VST3 example plug-in in a remote process via IPC:
        #string(APPEND ARATestHost_Dbg_Arguments "-ipc_vst3 ARATestPlugIn.vst3")
        # alternatively for testing with Melodyne VST3:
        #string(APPEND ARATestHost_Dbg_Arguments "-vst3 \"$ENV{CommonProgramW6432}\\VST3\\Celemony\\Melodyne\\Melodyne.vst3\"")
        # alternatively for testing with JUCE_ARA VST3 example:
        #string(APPEND ARATestHost_Dbg_Arguments "-vst3 \"$ENV{CommonProgramW6432}\\VST3\\ARAPluginDemo.vst3\"")
        # alternatively for testing with optional CLAP example plug-in:
        #string(APPEND ARATestHost_Dbg_Arguments "-clap ARATestPlugIn.clap")
        # alternatively for testing with optional CLAP example plug-in in a remote process via IPC:
        #string(APPEND ARATestHost_Dbg_Arguments "-ipc_clap ARATestPlugIn.clap")
    elseif(APPLE)
        # per default, test with VST3 example plug-in
        string(APPEND ARATestHost_Dbg_Arguments "-vst3 ARATestPlugIn.vst3")
        # alternatively for testing with VST3 example plug-in in a remote process via IPC:
        #string(APPEND ARATestHost_Dbg_Arguments "-ipc_vst3 ARATestPlugIn.vst3")
        # alternatively for testing with Audio Unit example plug-in:
        #string(APPEND ARATestHost_Dbg_Arguments "-au aufx AraT ADeC")
        # alternatively for testing with Audio Unit example plug-in in a remote process via IPC:
        #string(APPEND ARATestHost_Dbg_Arguments "-ipc_au aufx AraT ADeC")
        # alternatively for testing with Audio Unit v3 example plug-in:
        #string(APPEND ARATestHost_Dbg_Arguments "-au aufx Ara3 ADeC")
        # alternatively for testing with Melodyne VST3 or Audio Unit:
        #string(APPEND ARATestHost_Dbg_Arguments "-vst3 \"/Library/Audio/Plug-Ins/VST3/Melodyne.vst3\"")
        #string(APPEND ARATestHost_Dbg_Arguments "-au aumf MPLG CLMY")
        # alternatively for testing with JUCE_ARA VST3 or Audio Unit example:
        #string(APPEND ARATestHost_Dbg_Arguments "-vst3 \"$ENV{HOME}/Library/Audio/Plug-Ins/VST3/ARAPluginDemo.vst3\"")
        #string(APPEND ARATestHost_Dbg_Arguments "-au aufx AraD ADeC")
        # alternatively for testing with optional CLAP example plug-in:
        #string(APPEND ARATestHost_Dbg_Arguments "-clap ARATestPlugIn.clap")
        # alternatively for testing with optional CLAP example plug-in in a remote process via IPC:
        #string(APPEND ARATestHost_Dbg_Arguments "-ipc_clap ARATestPlugIn.clap")
    else()
        string(APPEND ARATestHost_Dbg_Arguments "-vst3 ARATestPlugIn.vst3")
    endif()
    # optionally, limit testing to selected test:
    #string(APPEND ARATestHost_Dbg_Arguments " -test PropertyUpdates")
    #string(APPEND ARATestHost_Dbg_Arguments " -test ContentUpdates")
    #string(APPEND ARATestHost_Dbg_Arguments " -test ContentReading")
    #string(APPEND ARATestHost_Dbg_Arguments " -test ModificationCloning")
    #string(APPEND ARATestHost_Dbg_Arguments " -test Archiving")
    #string(APPEND ARATestHost_Dbg_Arguments " -test SplitArchives")
    #string(APPEND ARATestHost_Dbg_Arguments " -test DragAndDrop")
    #string(APPEND ARATestHost_Dbg_Arguments " -test PlaybackRendering")
    #string(APPEND ARATestHost_Dbg_Arguments " -test EditorView")
    #string(APPEND ARATestHost_Dbg_Arguments " -test Algorithms")
    #string(APPEND ARATestHost_Dbg_Arguments " -test AudioFileChunkSaving")
    #string(APPEND ARATestHost_Dbg_Arguments " -test AudioFileChunkLoading")
    # optionally, choose specific audio file(s) to selected test:
    #string(APPEND ARATestHost_Dbg_Arguments " -file /some/path/audiofile.wav")
    set_target_properties(ARATestHost PROPERTIES
        VS_DEBUGGER_COMMAND_ARGUMENTS ${ARATestHost_Dbg_Arguments}
        XCODE_SCHEME_ARGUMENTS ${ARATestHost_Dbg_Arguments}
    )
endif()


# ======================
# dummy target: Documentation

set(api_docs
    "${ARA_API_DIR}/ARA_API.pdf"
    "${ARA_API_DIR}/ChangeLog.txt"
)
set(library_docs
    "${ARA_LIBRARY_DIR}/ARA_Library.html"
    "${ARA_LIBRARY_DIR}/ChangeLog.txt"
)
set(examples_docs
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/ChangeLog.txt"
)

add_custom_target(Documentation SOURCES ${api_docs} ${library_docs} ${examples_docs})
set_target_properties(Documentation PROPERTIES
    XCODE_GENERATE_SCHEME OFF
    EXCLUDE_FROM_ALL ON
)

source_group(TREE "${ARA_API_DIR}/.." FILES ${api_docs})
source_group(TREE "${ARA_LIBRARY_DIR}/.." FILES ${library_docs})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${examples_docs})

unset(api_docs)
unset(library_docs)
unset(examples_docs)
