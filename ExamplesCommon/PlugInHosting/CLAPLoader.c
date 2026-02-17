//------------------------------------------------------------------------------
//! \file       CLAPLoader.c
//!             CLAP specific ARA implementation for the SDK's hosting examples
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2022-2026, Celemony Software GmbH, All Rights Reserved.
//!             Developed in cooperation with Timo Kaluza (defiantnerd)
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

#include "CLAPLoader.h"

#include "ARA_API/ARACLAP.h"
#include "ARA_Library/Debug/ARADebug.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <conio.h>
#elif defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#elif defined(__linux__)
    #include <dlfcn.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>


#if defined(_MSC_VER)
   __pragma(warning(disable : 4100)) /*unreferenced formal parameter*/
   __pragma(warning(disable : 4201)) /*nonstandard extension used : nameless struct/union*/
#elif defined(__GNUC__)
   _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#endif


struct _CLAPBinary
{
#if defined(_WIN32)
    HMODULE libHandle;
#elif defined(__APPLE__)
    CFBundleRef libHandle;
#elif defined(__linux__)
    void* libHandle;
#endif
    const clap_plugin_entry_t * entry;
};

struct _CLAPPlugIn
{
    const clap_plugin_t * plugin;
    uint32_t channelCount;
    double sampleRate;
};


CLAPBinary CLAPLoadBinary(const char * binaryName)
{
#if defined (__APPLE__) || defined (__linux__)
    bool freeBinaryName = false;
    // if the binary name contains no '/', dlopen() searches the system library paths for the file,
    // and ignores the current directory - to prevent this, we prefix with "./" if needed
    if (!strchr(binaryName, '/'))
    {
        const char * prefix = "./";
        const size_t prefixLength = strlen(prefix);
        const size_t binaryNameLength = strlen(binaryName);
        char * temp = (char *)malloc(prefixLength + binaryNameLength + 1);
        ARA_INTERNAL_ASSERT(temp);
        memcpy(temp, prefix, prefixLength);
        memcpy(temp + prefixLength, binaryName, binaryNameLength + 1);  // including termination
        binaryName = temp;
        freeBinaryName = true;
    }
#endif

    CLAPBinary clapBinary = malloc(sizeof(struct _CLAPBinary));
    ARA_INTERNAL_ASSERT(clapBinary);
    clapBinary->libHandle = NULL;
    clapBinary->entry = NULL;

#if defined (_WIN32)
    clapBinary->libHandle = LoadLibraryA(binaryName);
    ARA_INTERNAL_ASSERT(clapBinary->libHandle);

    clapBinary->entry = (const clap_plugin_entry_t *)GetProcAddress(clapBinary->libHandle, "clap_entry");

#elif defined(__APPLE__)
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)binaryName, (CFIndex)strlen(binaryName), true);
    ARA_INTERNAL_ASSERT (url);
    clapBinary->libHandle = CFBundleCreate(kCFAllocatorDefault, url);
    CFRelease (url);
    ARA_INTERNAL_ASSERT(clapBinary->libHandle);

    Boolean ARA_MAYBE_UNUSED_VAR(didLoad);
    didLoad = CFBundleLoadExecutable(clapBinary->libHandle);
    ARA_INTERNAL_ASSERT(didLoad);

    clapBinary->entry = (const clap_plugin_entry_t *)CFBundleGetFunctionPointerForName(clapBinary->libHandle, CFSTR("clap_entry"));

#elif defined(__linux__)
    clapBinary->libHandle = dlopen(binaryName, RTLD_LOCAL | RTLD_LAZY);
    ARA_INTERNAL_ASSERT(clapBinary->libHandle);

    clapBinary->entry = (const clap_plugin_entry_t *)dlsym(clapBinary->libHandle, "clap_entry");
#endif

    ARA_INTERNAL_ASSERT(clapBinary->entry != NULL);
    ARA_INTERNAL_ASSERT(clap_version_is_compatible(clapBinary->entry->clap_version));

    bool success = clapBinary->entry->init(binaryName);
    ARA_INTERNAL_ASSERT(success);

#if defined (__APPLE__) || defined (__linux__)
    if (freeBinaryName)
        free((char *)binaryName);
#endif

    return clapBinary;
}

void CLAPValidateDescHasARA(CLAPBinary clapBinary, const char * plugin_id)
{
    const clap_plugin_factory_t * factory = (const clap_plugin_factory_t *)clapBinary->entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    ARA_INTERNAL_ASSERT(factory != NULL);

    const uint32_t plugin_count = factory->get_plugin_count(factory);
    ARA_INTERNAL_ASSERT(plugin_count > 0);

    for (uint32_t i = 0; i < plugin_count; ++i)
    {
        const clap_plugin_descriptor_t * descAtIndex = factory->get_plugin_descriptor(factory, i);
        ARA_INTERNAL_ASSERT(descAtIndex != NULL);
        if (strcmp(plugin_id, descAtIndex->id) == 0)
        {
            const char * const * feature = descAtIndex->features;
            while (feature)
            {
                if (strcmp(*(feature++), CLAP_PLUGIN_FEATURE_ARA_SUPPORTED) == 0)
                    return;
            }
            ARA_INTERNAL_ASSERT(false && "CLAP ARA effect not tagged as such in features");
        }
    }
}

const ARAFactory * CLAPGetARAFactory(CLAPBinary clapBinary, const char * optionalPlugInName)
{
    const clap_ara_factory_t * ara_factory = (const clap_ara_factory_t *)clapBinary->entry->get_factory(CLAP_EXT_ARA_FACTORY);
    if (ara_factory == NULL)
        return NULL;

    const uint32_t plugin_count = ara_factory->get_factory_count(ara_factory);
    ARA_INTERNAL_ASSERT(plugin_count > 0);

    if (optionalPlugInName)
    {
        for (uint32_t i = 0; i < plugin_count; ++i)
        {
            const ARAFactory * factory = ara_factory->get_ara_factory(ara_factory, i);
            ARA_INTERNAL_ASSERT(factory != NULL);
            if (strcmp(optionalPlugInName, factory->plugInName) == 0)
            {
                CLAPValidateDescHasARA(clapBinary, ara_factory->get_plugin_id(ara_factory, i));
                return factory;
            }
        }
        return NULL;
    }
    else
    {
        const ARAFactory * factory =  ara_factory->get_ara_factory(ara_factory, 0);
        ARA_INTERNAL_ASSERT(factory != NULL);
        CLAPValidateDescHasARA(clapBinary, ara_factory->get_plugin_id(ara_factory, 0));
        return factory;
    }
}

const void * host_get_extension(const clap_host_t * ARA_MAYBE_UNUSED_ARG(host), const char * ARA_MAYBE_UNUSED_ARG(extension_id))
{
    return NULL;
}

void host_request_dummy(const clap_host_t * ARA_MAYBE_UNUSED_ARG(host))
{
}

#define _IN_QUOTES_HELPER(x) #x
#define IN_QUOTES(x) _IN_QUOTES_HELPER(x)
static const clap_host_t clap_host =
{
    .clap_version = CLAP_VERSION_INIT,
    .host_data = NULL,
    .name = "ARA SDK Host Examples",
    .vendor = "ARA SDK Examples",
    .url =  "https://www.ara-audio.org/examples",
    .version = IN_QUOTES(ARA_MAJOR_VERSION) "." IN_QUOTES(ARA_MINOR_VERSION) "." IN_QUOTES(ARA_PATCH_VERSION),
    .get_extension = host_get_extension,
    .request_restart = host_request_dummy,
    .request_process = host_request_dummy,
    .request_callback = host_request_dummy
};
#undef _IN_QUOTES_HELPER
#undef IN_QUOTES

CLAPPlugIn CLAPCreatePlugIn(CLAPBinary clapBinary, const char* optionalPlugInName)
{
    const clap_plugin_factory_t * factory = (const clap_plugin_factory_t *)clapBinary->entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    ARA_INTERNAL_ASSERT(factory != NULL);

    const uint32_t plugin_count = factory->get_plugin_count(factory);
    ARA_INTERNAL_ASSERT(plugin_count > 0);

    const clap_plugin_descriptor_t * desc = NULL;
    if (optionalPlugInName)
    {
        for (uint32_t i = 0; i < plugin_count; ++i)
        {
            const clap_plugin_descriptor_t * descAtIndex = factory->get_plugin_descriptor(factory, i);
            ARA_INTERNAL_ASSERT(descAtIndex != NULL);
            if (strcmp(optionalPlugInName, descAtIndex->name) == 0)
            {
                desc = descAtIndex;
                break;
            }
        }
        if (desc == NULL)
            return NULL;
    }
    else
    {
        desc = factory->get_plugin_descriptor(factory, 0);
        ARA_INTERNAL_ASSERT(desc != NULL);
    }

    CLAPPlugIn clapPlugIn = malloc(sizeof(struct _CLAPPlugIn));
    ARA_INTERNAL_ASSERT(clapPlugIn);
    clapPlugIn->plugin = factory->create_plugin(factory, &clap_host, desc->id);
    clapPlugIn->plugin->init(clapPlugIn->plugin);
    return clapPlugIn;
}

const ARAPlugInExtensionInstance * CLAPBindToARADocumentController(CLAPPlugIn clapPlugIn, ARADocumentControllerRef controllerRef, ARAPlugInInstanceRoleFlags assignedRoles)
{
    const clap_ara_plugin_extension_t * ara_extension = clapPlugIn->plugin->get_extension(clapPlugIn->plugin, CLAP_EXT_ARA_PLUGINEXTENSION);
    if (ara_extension == NULL)
        return NULL;

    const ARAPlugInInstanceRoleFlags knownRoles = kARAPlaybackRendererRole | kARAEditorRendererRole | kARAEditorViewRole;
    ARA_INTERNAL_ASSERT((assignedRoles | knownRoles) == knownRoles);
    ARA_VALIDATE_API_CONDITION(!strcmp(clapPlugIn->plugin->desc->name, ara_extension->get_factory(clapPlugIn->plugin)->plugInName));
    return ara_extension->bind_to_document_controller(clapPlugIn->plugin, controllerRef, knownRoles, assignedRoles);
}

void CLAPStartRendering(CLAPPlugIn clapPlugIn, uint32_t channelCount, uint32_t maxBlockSize, double sampleRate)
{
    ARA_INTERNAL_ASSERT(clapPlugIn->channelCount == 0);
    clapPlugIn->channelCount = channelCount;
    clapPlugIn->sampleRate = sampleRate;

    bool ARA_MAYBE_UNUSED_VAR(success);

    // we require that ARA plug-ins are capable of handling mono and stereo
    const clap_plugin_configurable_audio_ports_t * configurable_audio_ports = clapPlugIn->plugin->get_extension(clapPlugIn->plugin, CLAP_EXT_CONFIGURABLE_AUDIO_PORTS);
    if (configurable_audio_ports)
    {
        clap_audio_port_configuration_request_t inputRequest;
        switch (channelCount)
        {
            default:
            {
                ARA_INTERNAL_ASSERT(false && "no default format defined for given channel count");
                // no break; intended here
            }
            case 1:
            {
                clap_audio_port_configuration_request_t input1 = { true, 0, 1, CLAP_PORT_MONO, NULL };
                inputRequest = input1;
                break;
            }
            case 2:
            {
                clap_audio_port_configuration_request_t input2 = { true, 0, 2, CLAP_PORT_STEREO, NULL };
                inputRequest = input2;
                break;
            }
            case 3:
            {
                const uint8_t channelMap3[3] = { CLAP_SURROUND_FL, CLAP_SURROUND_FR, CLAP_SURROUND_FC };
                clap_audio_port_configuration_request_t input3 = { true, 0, 3, CLAP_PORT_STEREO, channelMap3 };
                inputRequest = input3;
                break;
            }
            case 4:
            {
                const uint8_t channelMap4[4] = { CLAP_SURROUND_FL, CLAP_SURROUND_FR, CLAP_SURROUND_BL, CLAP_SURROUND_BR };
                clap_audio_port_configuration_request_t input4 = { true, 0, 4, CLAP_PORT_STEREO, channelMap4 };
                inputRequest = input4;
                break;
            }
            case 5:
            {
                const uint8_t channelMap5[5] = { CLAP_SURROUND_FL, CLAP_SURROUND_FR, CLAP_SURROUND_FC, CLAP_SURROUND_BL, CLAP_SURROUND_BR };
                clap_audio_port_configuration_request_t input5 = { true, 0, 5, CLAP_PORT_STEREO, channelMap5 };
                inputRequest = input5;
                break;
            }
            case 6:
            {
                const uint8_t channelMap6[6] = { CLAP_SURROUND_FL, CLAP_SURROUND_FR, CLAP_SURROUND_FC, CLAP_SURROUND_LFE, CLAP_SURROUND_BL, CLAP_SURROUND_BR };
                clap_audio_port_configuration_request_t input6 = { true, 0, 6, CLAP_PORT_STEREO, channelMap6 };
                inputRequest = input6;
                break;
            }
        }
    
        clap_audio_port_configuration_request_t outputRequest = inputRequest;
        outputRequest.is_input = false;

        clap_audio_port_configuration_request_t requests[] = { inputRequest, outputRequest };
        success = configurable_audio_ports->apply_configuration(clapPlugIn->plugin, requests, (uint32_t) sizeof(requests) / sizeof(requests[0]));
        ARA_INTERNAL_ASSERT(success);
    }
    else
    {
        const clap_plugin_audio_ports_config_t * audio_ports_config = clapPlugIn->plugin->get_extension(clapPlugIn->plugin, CLAP_EXT_AUDIO_PORTS_CONFIG);
        if (audio_ports_config)
        {
            uint32_t count = audio_ports_config->count(clapPlugIn->plugin);
            ARA_INTERNAL_ASSERT(count >= 2);
            bool ARA_MAYBE_UNUSED_VAR(foundMatchingConfig);
            foundMatchingConfig = false;
            for (uint32_t i = 0; i < count; ++i)
            {
                clap_audio_ports_config_t config;
                success = audio_ports_config->get(clapPlugIn->plugin, i, &config);
                ARA_INTERNAL_ASSERT(success);
                if (config.has_main_input &&
                    config.has_main_output &&
                    config.main_input_channel_count == channelCount &&
                    config.main_output_channel_count == channelCount)
                {
                    success = audio_ports_config->select(clapPlugIn->plugin, config.id);
                    ARA_INTERNAL_ASSERT(success);
                    foundMatchingConfig = true;
                    break;
                }
            }
            ARA_INTERNAL_ASSERT(foundMatchingConfig);
        }
    }

    // validate audio port info
    const clap_plugin_audio_ports_t * audio_ports = clapPlugIn->plugin->get_extension(clapPlugIn->plugin, CLAP_EXT_AUDIO_PORTS);
    ARA_INTERNAL_ASSERT(audio_ports);
    ARA_INTERNAL_ASSERT(audio_ports->count(clapPlugIn->plugin, true) > 0);
    ARA_INTERNAL_ASSERT(audio_ports->count(clapPlugIn->plugin, false) > 0);
    clap_audio_port_info_t in_info, out_info;
    success = audio_ports->get(clapPlugIn->plugin, 0, true, &in_info);
    ARA_INTERNAL_ASSERT(success);
    success = audio_ports->get(clapPlugIn->plugin, 0, false, &out_info);
    ARA_INTERNAL_ASSERT(success);
    // we require that ARA plug-ins are capable of in-place processing (typically used for editor rendering)
    ARA_INTERNAL_ASSERT(in_info.in_place_pair == out_info.id);
    ARA_INTERNAL_ASSERT(in_info.id == out_info.in_place_pair);
    // this simple loader only supports mono
    ARA_INTERNAL_ASSERT(in_info.channel_count == 1);
    ARA_INTERNAL_ASSERT(out_info.channel_count == 1);

    clapPlugIn->plugin->activate(clapPlugIn->plugin, sampleRate, 1, maxBlockSize);
    clapPlugIn->plugin->start_processing(clapPlugIn->plugin);
}

uint32_t CLAP_ABI input_events_size(const struct clap_input_events* list)
{
    return 0;
}

const clap_event_header_t* CLAP_ABI input_events_get(const struct clap_input_events* list, uint32_t index)
{
    return NULL;
}

bool CLAP_ABI output_events_try_push(const struct clap_output_events* list, const clap_event_header_t* event)
{
  return false;
}

void CLAPRenderBuffer(CLAPPlugIn clapPlugIn, uint32_t blockSize, int64_t samplePosition, float** buffers)
{
    ARA_INTERNAL_ASSERT(clapPlugIn->channelCount != 0);
    ARA_INTERNAL_ASSERT(blockSize >= 1);

    // events
    const clap_input_events_t input_events =
    {
        .ctx = NULL, // reserved pointer for the list
        .size = input_events_size,
        .get = input_events_get
    };

    clap_output_events_t output_events =
    {
        .ctx = NULL,
        .try_push = output_events_try_push
    };

    // transport
    const clap_event_transport_t transport =
    {
        .header =
        {
            .type = CLAP_EVENT_TRANSPORT,
            .space_id = CLAP_CORE_EVENT_SPACE_ID,
            .size = sizeof(clap_event_transport_t),
            .time = 0,
            .flags = 0,
        },
        .flags = CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_IS_PLAYING,
        .song_pos_seconds = (int64_t)round(((double)CLAP_SECTIME_FACTOR) * (((double)samplePosition) / clapPlugIn->sampleRate))
    };

    // I/O
    const clap_audio_buffer_t audio_inputs =
    {
        .data32 = buffers,
        .channel_count = clapPlugIn->channelCount,
        .latency = 0,
        .constant_mask = UINT64_MAX
    };

    clap_audio_buffer_t audio_outputs =
    {
        .data32 = buffers,
        .channel_count = clapPlugIn->channelCount,
        .latency = 0,
        .constant_mask = 0
    };

    for (uint32_t i = 0; i < clapPlugIn->channelCount; ++i)
        memset(buffers[i], 0, blockSize * sizeof(float));

    // process
    clap_process_t process =
    {
        .steady_time = -1,
        .frames_count = blockSize,
        .transport = &transport,
        .audio_inputs = &audio_inputs,
        .audio_outputs = &audio_outputs,
        .audio_inputs_count = 1,
        .audio_outputs_count = 1,
        .in_events = &input_events,
        .out_events = &output_events
    };

    clap_process_status ARA_MAYBE_UNUSED_VAR(status);
    status = clapPlugIn->plugin->process(clapPlugIn->plugin, &process);
    ARA_INTERNAL_ASSERT(status != CLAP_PROCESS_ERROR);
}

void CLAPStopRendering(CLAPPlugIn clapPlugIn)
{
    ARA_INTERNAL_ASSERT(clapPlugIn->channelCount != 0);
    clapPlugIn->plugin->stop_processing(clapPlugIn->plugin);
    clapPlugIn->plugin->deactivate(clapPlugIn->plugin);
    clapPlugIn->channelCount = 0;
}

void CLAPDestroyPlugIn(CLAPPlugIn clapPlugIn)
{
    clapPlugIn->plugin->destroy(clapPlugIn->plugin);
    free(clapPlugIn);
}

void CLAPUnloadBinary(CLAPBinary clapBinary)
{
    clapBinary->entry->deinit();

#if defined(_WIN32)
    FreeLibrary(clapBinary->libHandle);
#elif defined(__APPLE__)
    CFRelease(clapBinary->libHandle);
#elif defined(__linux__)
    dlclose(clapBinary->libHandle);
#endif
    free(clapBinary);
}
