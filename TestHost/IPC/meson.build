# meson.build — Linux IPC socket test binaries for ARA SDK Examples
#
# Configure and build from ARA_Examples/:
#
#   meson setup build TestHost/IPC/
#   ninja -C build
#   build/IPCSocketHostTest build/IPCSocketPlugInTest [/path/to/plugin.so]
#
# Assumes the ARA_Library and ARA_API are siblings of ARA_Examples:
#   ara-sdk/
#     ARA_API/
#     ARA_Library/
#     ARA_Examples/   ← project root
#
# Also requires the VST3 SDK at a sibling path (../../../vst3 relative to
# ARA_Examples), adjustable via the vst3_sdk_path build option.

project ('ara-ipc-socket-test', 'c', 'cpp',
  default_options: [
    'cpp_std=c++20',
    'c_std=c11',
    'warning_level=1',
    'buildtype=debug',
  ]
)

vst3_sdk_path = get_option ('vst3_sdk_path')
if vst3_sdk_path == ''
    vst3_sdk_path = '../../../vst3'
endif

ara_inc = include_directories (
    '..',               # ARA_Examples/TestHost/ (so 'IPC/IPCSocket*.h' resolves)
    '../..',            # ARA_Examples/           (so 'ExamplesCommon/...' resolves)
    '../../..',         # ARA_Library/            (so 'ARA_Library/IPC/...' resolves)
    '../../../..',      # ara-sdk/                (so 'ARA_API/ARAInterface.h' resolves)
    vst3_sdk_path,      # VST3 SDK
)

ara_defines = ['-DARA_ENABLE_IPC=1']

ara_lib_dir = '../../../'  # ARA_Library/ relative to IPC/

ara_lib_sources = files (
    ara_lib_dir + 'IPC/ARAIPCConnection.cpp',
    ara_lib_dir + 'IPC/ARAIPCProxyHost.cpp',
    ara_lib_dir + 'IPC/ARAIPCProxyPlugIn.cpp',
    ara_lib_dir + 'Dispatch/ARAHostDispatch.cpp',
    ara_lib_dir + 'Dispatch/ARAPlugInDispatch.cpp',
    ara_lib_dir + 'Utilities/ARAChannelFormat.cpp',
    ara_lib_dir + 'Utilities/ARAPitchInterpretation.cpp',
    ara_lib_dir + 'Debug/ARADebug.c',
    vst3_sdk_path + '/pluginterfaces/base/funknown.cpp',
)

socket_sources = files (
    'IPCSocketEncoding.cpp',
    'IPCSocketChannel.cpp',
)

ara_lib = static_library ('ara_ipc',
    ara_lib_sources + socket_sources,
    include_directories: ara_inc,
    c_args:   ara_defines,
    cpp_args: ara_defines,
)

executable ('IPCSocketPlugInTest',
    files ('IPCSocketPlugInTest.cpp'),
    include_directories: ara_inc,
    cpp_args: ara_defines,
    link_with: ara_lib,
    link_args: ['-ldl'],
)

executable ('IPCSocketHostTest',
    files ('IPCSocketHostTest.cpp'),
    include_directories: ara_inc,
    cpp_args: ara_defines,
    link_with: ara_lib,
)
