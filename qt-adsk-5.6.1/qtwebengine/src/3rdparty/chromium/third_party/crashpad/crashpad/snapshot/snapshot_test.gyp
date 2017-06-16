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
      'target_name': 'crashpad_snapshot_test_lib',
      'type': 'static_library',
      'dependencies': [
        'snapshot.gyp:crashpad_snapshot',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/test_cpu_context.cc',
        'test/test_cpu_context.h',
        'test/test_exception_snapshot.cc',
        'test/test_exception_snapshot.h',
        'test/test_memory_snapshot.cc',
        'test/test_memory_snapshot.h',
        'test/test_module_snapshot.cc',
        'test/test_module_snapshot.h',
        'test/test_process_snapshot.cc',
        'test/test_process_snapshot.h',
        'test/test_system_snapshot.cc',
        'test/test_system_snapshot.h',
        'test/test_thread_snapshot.cc',
        'test/test_thread_snapshot.h',
      ],
    },
    {
      'target_name': 'crashpad_snapshot_test',
      'type': 'executable',
      'dependencies': [
        'crashpad_snapshot_test_module',
        'snapshot.gyp:crashpad_snapshot',
        '../client/client.gyp:crashpad_client',
        '../compat/compat.gyp:crashpad_compat',
        '../test/test.gyp:crashpad_test',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpu_context_test.cc',
        'crashpad_info_client_options_test.cc',
        'mac/cpu_context_mac_test.cc',
        'mac/mach_o_image_annotations_reader_test.cc',
        'mac/mach_o_image_reader_test.cc',
        'mac/mach_o_image_segment_reader_test.cc',
        'mac/process_reader_test.cc',
        'mac/process_types_test.cc',
        'mac/system_snapshot_mac_test.cc',
        'minidump/process_snapshot_minidump_test.cc',
        'win/pe_image_annotations_reader_test.cc',
        'win/process_reader_win_test.cc',
        'win/system_snapshot_win_test.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenCL.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'crashpad_snapshot_test_module',
      'type': 'loadable_module',
      'dependencies': [
        '../client/client.gyp:crashpad_client',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'crashpad_info_client_options_test_module.cc',
      ],
    },
  ],
}
