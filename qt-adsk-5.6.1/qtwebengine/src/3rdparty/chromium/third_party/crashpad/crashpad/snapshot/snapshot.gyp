# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'includes': [
    '../build/crashpad.gypi',
  ],
  'targets': [
    {
      'target_name': 'crashpad_snapshot',
      'type': 'static_library',
      'dependencies': [
        '../client/client.gyp:crashpad_client',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpu_architecture.h',
        'cpu_context.cc',
        'cpu_context.h',
        'crashpad_info_client_options.cc',
        'crashpad_info_client_options.h',
        'exception_snapshot.h',
        'mac/cpu_context_mac.cc',
        'mac/cpu_context_mac.h',
        'mac/exception_snapshot_mac.cc',
        'mac/exception_snapshot_mac.h',
        'mac/mach_o_image_annotations_reader.cc',
        'mac/mach_o_image_annotations_reader.h',
        'mac/mach_o_image_reader.cc',
        'mac/mach_o_image_reader.h',
        'mac/mach_o_image_segment_reader.cc',
        'mac/mach_o_image_segment_reader.h',
        'mac/mach_o_image_symbol_table_reader.cc',
        'mac/mach_o_image_symbol_table_reader.h',
        'mac/memory_snapshot_mac.cc',
        'mac/memory_snapshot_mac.h',
        'mac/module_snapshot_mac.cc',
        'mac/module_snapshot_mac.h',
        'mac/process_reader.cc',
        'mac/process_reader.h',
        'mac/process_snapshot_mac.cc',
        'mac/process_snapshot_mac.h',
        'mac/process_types.cc',
        'mac/process_types.h',
        'mac/process_types/all.proctype',
        'mac/process_types/crashpad_info.proctype',
        'mac/process_types/crashreporterclient.proctype',
        'mac/process_types/custom.cc',
        'mac/process_types/dyld_images.proctype',
        'mac/process_types/flavors.h',
        'mac/process_types/internal.h',
        'mac/process_types/loader.proctype',
        'mac/process_types/nlist.proctype',
        'mac/process_types/traits.h',
        'mac/system_snapshot_mac.cc',
        'mac/system_snapshot_mac.h',
        'mac/thread_snapshot_mac.cc',
        'mac/thread_snapshot_mac.h',
        'minidump/minidump_simple_string_dictionary_reader.cc',
        'minidump/minidump_simple_string_dictionary_reader.h',
        'minidump/minidump_string_list_reader.cc',
        'minidump/minidump_string_list_reader.h',
        'minidump/minidump_string_reader.cc',
        'minidump/minidump_string_reader.h',
        'minidump/module_snapshot_minidump.cc',
        'minidump/module_snapshot_minidump.h',
        'minidump/process_snapshot_minidump.cc',
        'minidump/process_snapshot_minidump.h',
        'memory_snapshot.h',
        'module_snapshot.h',
        'process_snapshot.h',
        'system_snapshot.h',
        'thread_snapshot.h',
        'win/memory_snapshot_win.cc',
        'win/memory_snapshot_win.h',
        'win/module_snapshot_win.cc',
        'win/module_snapshot_win.h',
        'win/pe_image_annotations_reader.cc',
        'win/pe_image_annotations_reader.h',
        'win/pe_image_reader.cc',
        'win/pe_image_reader.h',
        'win/process_reader_win.cc',
        'win/process_reader_win.h',
        'win/process_snapshot_win.cc',
        'win/process_snapshot_win.h',
        'win/system_snapshot_win.cc',
        'win/system_snapshot_win.h',
        'win/thread_snapshot_win.cc',
        'win/thread_snapshot_win.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lpowrprof.lib',
              '-lversion.lib',
            ],
          },
        }],
      ]
    },
  ],
}
