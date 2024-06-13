//------------------------------------------------------------------------------
//! \file       TestCLAPPlugIn.cpp
//!             CLAP implementation for the ARA test plug-in,
//!             based on the plugin-template.c from the CLAP SDK
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2022-2024, Celemony Software GmbH, All Rights Reserved.
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

#include "ARA_API/ARACLAP.h"

#include "ARATestDocumentController.h"
#include "ARATestPlaybackRenderer.h"
#include "TestPlugInConfig.h"
#include "ARA_Library/Utilities/ARASamplePositionConversion.h"


// since we're actually using C++ not plain C, replace NULL with nullptr
#if defined(NULL)
    #undef NULL
#endif
#define NULL nullptr


static CLAP_CONSTEXPR const char CLAP_TEST_PLUGIN_ID[] = "org.ara-audio.examples.testplugin.clap";


static const char* const s_my_features[] = {
   CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
   CLAP_PLUGIN_FEATURE_ARA_SUPPORTED,
   CLAP_PLUGIN_FEATURE_ARA_REQUIRED,
   NULL
};

static const clap_plugin_descriptor_t s_my_plug_desc = {
   .clap_version = CLAP_VERSION_INIT,
   .id = CLAP_TEST_PLUGIN_ID,
   .name = TEST_PLUGIN_NAME,
   .vendor = TEST_MANUFACTURER_NAME,
   .url = TEST_INFORMATION_URL,
   .manual_url = TEST_INFORMATION_URL,
   .support_url = TEST_INFORMATION_URL,
   .version = TEST_VERSION_STRING,
   .description = "ARA Examples: ARA Test Plug-In",
   .features = s_my_features
};

typedef struct my_plug {
   clap_plugin_t                   plugin;
   const clap_host_t              *host;
   const clap_host_latency_t      *host_latency;
   const clap_host_log_t          *host_log;
   const clap_host_thread_check_t *host_thread_check;
   const clap_host_state_t        *host_state;

   uint32_t channel_count = 1;
   double   sample_rate = 44100.0;
   uint32_t max_frames_count = 0;

   ARA::PlugIn::PlugInExtension ara_extension;
} my_plug_t;

/////////////////////////////
// clap_plugin_audio_ports //
/////////////////////////////

static uint32_t my_plug_audio_ports_count(const clap_plugin_t *plugin, bool is_input) {
   // We just declare 1 audio input and 1 audio output
   return 1;
}

static bool my_plug_audio_ports_get(const clap_plugin_t    *plugin,
                                    uint32_t                index,
                                    bool                    is_input,
                                    clap_audio_port_info_t *info) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   if (index > 0)
      return false;
   info->id = 0;
   snprintf(info->name, sizeof(info->name), "%s", "My Port Name");
   info->channel_count = plug->channel_count;
   info->flags = CLAP_AUDIO_PORT_IS_MAIN;
   info->port_type = (plug->channel_count == 1) ? CLAP_PORT_MONO : (plug->channel_count == 2) ? CLAP_PORT_MONO : NULL;
   info->in_place_pair = info->id;
   return true;
}

static const clap_plugin_audio_ports_t s_my_plug_audio_ports = {
   .count = my_plug_audio_ports_count,
   .get = my_plug_audio_ports_get,
};

//////////////////////////////////////////
// clap_plugin_configurable_audio_ports //
//////////////////////////////////////////

// internal helper that makes sure the requests describes a valid configuration with ins == outs
// returns 0 on failure
uint32_t my_plug_get_validated_channel_count_for_configuration(const my_plug_t                                    *plugin,
                                                               const struct clap_audio_port_configuration_request *requests,
                                                               uint32_t                                            request_count) {
   if (request_count > 2)
      return 0;

   uint32_t input_channel_count = 0;
   uint32_t output_channel_count = 0;
   for (uint32_t i = 0; i < request_count; ++i) {
      if (requests[i].port_index != 0)
         return 0;

      if (!strcmp(requests[i].port_type, CLAP_PORT_MONO)) {
         if (requests[i].channel_count != 1)
            return 0;
         if (requests[i].port_details != nullptr)
            return 0;
      } else if (!strcmp(requests[i].port_type, CLAP_PORT_STEREO)) {
         if (requests[i].channel_count != 2)
            return 0;
         if (requests[i].port_details != nullptr)
            return 0;
      }

      if (requests[i].is_input) {
         if (input_channel_count != 0)
            return 0;
         input_channel_count = requests[i].channel_count;
      }
      else {
         if (output_channel_count != 0)
            return 0;
         output_channel_count = requests[i].channel_count;
      }
   }

   if (input_channel_count != output_channel_count)
     return 0;

   return output_channel_count;
}

static bool my_plug_configurable_audio_ports_can_apply_configuration(const clap_plugin_t                                *plugin,
                                                                     const struct clap_audio_port_configuration_request *requests,
                                                                     uint32_t                                            request_count) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   return my_plug_get_validated_channel_count_for_configuration(plug, requests, request_count) != 0;
}

static bool my_plug_configurable_audio_ports_apply_configuration(const clap_plugin_t                                *plugin,
                                                                 const struct clap_audio_port_configuration_request *requests,
                                                                 uint32_t                                            request_count) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   uint32_t channel_count = my_plug_get_validated_channel_count_for_configuration(plug, requests, request_count);
   if (channel_count == 0)
      return false;
    plug->channel_count = channel_count;
   return true;
}

static clap_plugin_configurable_audio_ports_t s_my_plug_configurable_audio_ports = {
   .can_apply_configuration = my_plug_configurable_audio_ports_can_apply_configuration,
   .apply_configuration = my_plug_configurable_audio_ports_apply_configuration,
};

//////////////////
// clap_latency //
//////////////////

static uint32_t my_plug_latency_get(const clap_plugin_t *plugin) {
   return 0;   // ARA plug-ins have no latency because they can compensate it internally via random access
}

static const clap_plugin_latency_t s_my_plug_latency = {
   .get = my_plug_latency_get,
};

///////////////////
// ARA extension //
///////////////////

static const ARA::ARAFactory *my_plug_ara_get_factory(const clap_plugin_t *plugin) {
   return ARATestDocumentController::getARAFactory();
}

static const ARA::ARAPlugInExtensionInstance *my_plug_ara_bind_to_document_controller(const clap_plugin_t            *plugin,
                                                                                      ARA::ARADocumentControllerRef   documentControllerRef,
                                                                                      ARA::ARAPlugInInstanceRoleFlags knownRoles,
                                                                                      ARA::ARAPlugInInstanceRoleFlags assignedRoles) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   return plug->ara_extension.bindToARA(documentControllerRef, knownRoles, assignedRoles);
}

static clap_ara_plugin_extension_t ara_plugin_extension = {
   .get_factory = my_plug_ara_get_factory,
   .bind_to_document_controller = my_plug_ara_bind_to_document_controller,
};

/////////////////
// clap_plugin //
/////////////////

static bool my_plug_init(const struct clap_plugin *plugin) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;

   // Fetch host's extensions here
   // Make sure to check that the interface functions are not null pointers
   plug->host_log = (const clap_host_log_t *)plug->host->get_extension(plug->host, CLAP_EXT_LOG);
   plug->host_thread_check = (const clap_host_thread_check_t *)plug->host->get_extension(plug->host, CLAP_EXT_THREAD_CHECK);
   plug->host_latency = (const clap_host_latency_t *)plug->host->get_extension(plug->host, CLAP_EXT_LATENCY);
   plug->host_state = (const clap_host_state_t *)plug->host->get_extension(plug->host, CLAP_EXT_STATE);
   return true;
}

static void my_plug_destroy(const struct clap_plugin *plugin) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   delete plug;
}

static bool my_plug_activate(const struct clap_plugin *plugin,
                             double                    sample_rate,
                             uint32_t                  min_frames_count,
                             uint32_t                  max_frames_count) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   plug->sample_rate = sample_rate;
   plug->max_frames_count = max_frames_count;

   if (auto *playbackRenderer = plug->ara_extension.getPlaybackRenderer<ARATestPlaybackRenderer>())
      playbackRenderer->enableRendering(sample_rate, (ARA::ARAChannelCount)plug->channel_count, max_frames_count, true);

   return true;
}

static void my_plug_deactivate(const struct clap_plugin *plugin) {
   my_plug_t *plug = (my_plug_t *)plugin->plugin_data;
   if (auto* playbackRenderer = plug->ara_extension.getPlaybackRenderer<ARATestPlaybackRenderer>())
      playbackRenderer->disableRendering();
}

static bool my_plug_start_processing(const struct clap_plugin *plugin) { return true; }

static void my_plug_stop_processing(const struct clap_plugin *plugin) {}

static void my_plug_reset(const struct clap_plugin *plugin) {}

static clap_process_status my_plug_process(const struct clap_plugin *plugin,
                                           const clap_process_t     *process) {
   my_plug_t     *plug = (my_plug_t *)plugin->plugin_data;
   const uint32_t nframes = process->frames_count;

   if (!process->audio_outputs || !process->audio_outputs[0].channel_count)
      return CLAP_PROCESS_CONTINUE;

   auto *playbackRenderer = plug->ara_extension.getPlaybackRenderer<ARATestPlaybackRenderer>();
   if (playbackRenderer && process->transport) {   // we need transport info
      // if we're an ARA playback renderer, calculate ARA playback output
      const auto position = ARA::samplePositionAtTime(((double)process->transport->song_pos_seconds) / ((double)CLAP_SECTIME_FACTOR), plug->sample_rate);
      playbackRenderer->renderPlaybackRegions(process->audio_outputs[0].data32, position, nframes,
                                              (process->transport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0);
   }
   else {
      // if we're no ARA playback renderer, we're just copying the inputs to the outputs, which is
      // appropriate both when being only an ARA editor renderer, or when being used in non-ARA mode.
      for (uint32_t c = 0; c < process->audio_outputs[0].channel_count; ++c)
         memcpy(process->audio_outputs[0].data32[c], process->audio_inputs[0].data32[c], sizeof(float) * nframes);
   }

   return CLAP_PROCESS_CONTINUE;
}

static const void *my_plug_get_extension(const struct clap_plugin *plugin, const char *id) {
   if (!strcmp(id, CLAP_EXT_LATENCY))
      return &s_my_plug_latency;
   if (!strcmp(id, CLAP_EXT_AUDIO_PORTS))
      return &s_my_plug_audio_ports;
   if (!strcmp(id, CLAP_EXT_CONFIGURABLE_AUDIO_PORTS))
      return &s_my_plug_configurable_audio_ports;
   if (!strcmp(id, CLAP_EXT_ARA_PLUGINEXTENSION))
      return &ara_plugin_extension;
   return NULL;
}

static void my_plug_on_main_thread(const struct clap_plugin *plugin) {}

static clap_plugin_t *my_plug_create(const clap_host_t *host) {
   my_plug_t *p = new my_plug_t;
   p->host = host;
   p->plugin.desc = &s_my_plug_desc;
   p->plugin.plugin_data = p;
   p->plugin.init = my_plug_init;
   p->plugin.destroy = my_plug_destroy;
   p->plugin.activate = my_plug_activate;
   p->plugin.deactivate = my_plug_deactivate;
   p->plugin.start_processing = my_plug_start_processing;
   p->plugin.stop_processing = my_plug_stop_processing;
   p->plugin.reset = my_plug_reset;
   p->plugin.process = my_plug_process;
   p->plugin.get_extension = my_plug_get_extension;
   p->plugin.on_main_thread = my_plug_on_main_thread;

   // Don't call into the host here

   return &p->plugin;
}

/////////////////////////
// clap_plugin_factory //
/////////////////////////

static struct {
   const clap_plugin_descriptor_t *desc;
   clap_plugin_t *(CLAP_ABI *create)(const clap_host_t *host);
} s_plugins[] = {
   {
      /* \todo causes an internal compiler error in MSVC .desc =*/ &s_my_plug_desc,
      /*.create =*/ my_plug_create
   },
};

static uint32_t plugin_factory_get_plugin_count(const struct clap_plugin_factory *factory) {
   return sizeof(s_plugins) / sizeof(s_plugins[0]);
}

static const clap_plugin_descriptor_t *
plugin_factory_get_plugin_descriptor(const struct clap_plugin_factory *factory, uint32_t index) {
   return s_plugins[index].desc;
}

static const clap_plugin_t *plugin_factory_create_plugin(const struct clap_plugin_factory *factory,
                                                         const clap_host_t                *host,
                                                         const char *plugin_id) {
   if (!clap_version_is_compatible(host->clap_version)) {
      return NULL;
   }

   const int N = sizeof(s_plugins) / sizeof(s_plugins[0]);
   for (int i = 0; i < N; ++i)
      if (!strcmp(plugin_id, s_plugins[i].desc->id))
         return s_plugins[i].create(host);

   return NULL;
}

static const clap_plugin_factory_t s_plugin_factory = {
   .get_plugin_count = plugin_factory_get_plugin_count,
   .get_plugin_descriptor = plugin_factory_get_plugin_descriptor,
   .create_plugin = plugin_factory_create_plugin,
};

//////////////////////
// clap_ara_factory //
//////////////////////

static uint32_t ara_factory_get_factory_count(const struct clap_ara_factory* factory) {
   return 1;
}

static const ARA::ARAFactory *ara_factory_get_ara_factory(const struct clap_ara_factory* factory, uint32_t index) {
   return ARATestDocumentController::getARAFactory();
}

static const char *ara_factory_get_plugin_id(const struct clap_ara_factory *factory, uint32_t index) {
   return CLAP_TEST_PLUGIN_ID;
}

static clap_ara_factory_t s_ara_factory =
{
   .get_factory_count = ara_factory_get_factory_count,
   .get_ara_factory = ara_factory_get_ara_factory,
   .get_plugin_id = ara_factory_get_plugin_id,
};

////////////////
// clap_entry //
////////////////

static bool entry_init(const char *plugin_path) {
   // called only once, and very first
   return true;
}

static void entry_deinit(void) {
   // called before unloading the DSO
}

static const void *entry_get_factory(const char *factory_id) {
   if (!strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID))
      return &s_plugin_factory;
   if (!strcmp(factory_id, CLAP_EXT_ARA_FACTORY))
      return &s_ara_factory;
   return NULL;
}

// This symbol will be resolved by the host
CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {
   .clap_version = CLAP_VERSION_INIT,
   .init = entry_init,
   .deinit = entry_deinit,
   .get_factory = entry_get_factory,
};
