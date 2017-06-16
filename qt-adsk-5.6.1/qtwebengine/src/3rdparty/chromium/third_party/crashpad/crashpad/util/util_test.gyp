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
      'target_name': 'crashpad_util_test',
      'type': 'executable',
      'dependencies': [
        'util.gyp:crashpad_util',
        '../compat/compat.gyp:crashpad_compat',
        '../test/test.gyp:crashpad_test',
        '../third_party/gmock/gmock.gyp:gmock',
        '../third_party/gmock/gmock.gyp:gmock_main',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'file/file_io_test.cc',
        'file/string_file_test.cc',
        'mac/launchd_test.mm',
        'mac/mac_util_test.mm',
        'mac/service_management_test.mm',
        'mac/xattr_test.cc',
        'mach/child_port_handshake_test.cc',
        'mach/child_port_server_test.cc',
        'mach/composite_mach_message_server_test.cc',
        'mach/exc_client_variants_test.cc',
        'mach/exc_server_variants_test.cc',
        'mach/exception_behaviors_test.cc',
        'mach/exception_ports_test.cc',
        'mach/exception_types_test.cc',
        'mach/mach_extensions_test.cc',
        'mach/mach_message_server_test.cc',
        'mach/mach_message_test.cc',
        'mach/notify_server_test.cc',
        'mach/scoped_task_suspend_test.cc',
        'mach/symbolic_constants_mach_test.cc',
        'mach/task_memory_test.cc',
        'misc/clock_test.cc',
        'misc/initialization_state_dcheck_test.cc',
        'misc/initialization_state_test.cc',
        'misc/scoped_forbid_return_test.cc',
        'misc/uuid_test.cc',
        'net/http_body_test.cc',
        'net/http_body_test_util.cc',
        'net/http_body_test_util.h',
        'net/http_multipart_builder_test.cc',
        'net/http_transport_test.cc',
        'numeric/checked_address_range_test.cc',
        'numeric/checked_range_test.cc',
        'numeric/in_range_cast_test.cc',
        'numeric/int128_test.cc',
        'posix/process_info_test.cc',
        'posix/symbolic_constants_posix_test.cc',
        'stdlib/map_insert_test.cc',
        'stdlib/string_number_conversion_test.cc',
        'stdlib/strlcpy_test.cc',
        'stdlib/strnlen_test.cc',
        'string/split_string_test.cc',
        'synchronization/semaphore_test.cc',
        'thread/thread_log_messages_test.cc',
        'thread/thread_test.cc',
        'win/process_info_test.cc',
        'win/time_test.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS=="win"', {
          'dependencies': [
            'crashpad_util_test_process_info_test_child_x64',
            'crashpad_util_test_process_info_test_child_x86',
          ],
          'link_settings': {
            'libraries': [
              '-limagehlp.lib',
              '-lrpcrt4.lib',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'crashpad_util_test_process_info_test_child_x64',
          'type': 'executable',
          'sources': [
            'win/process_info_test_child.cc',
          ],
          'msvs_configuration_platform': 'x64',
          # Set an unusually high load address to make sure that the main
          # executable still appears as the first element in
          # ProcessInfo::Modules().
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/BASE:0x78000000',
                '/FIXED',
              ],
              'TargetMachine': '17',  # x64.
            },
          },
        },
        {
          # Same as above, but explicitly x86 to test 64->32 access.
          'target_name': 'crashpad_util_test_process_info_test_child_x86',
          'type': 'executable',
          'sources': [
            'win/process_info_test_child.cc',
          ],
          'msvs_configuration_platform': 'x86',
          # Set an unusually high load address to make sure that the main
          # executable still appears as the first element in
          # ProcessInfo::Modules().
          'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalOptions': [
                '/BASE:0x78000000',
                '/FIXED',
              ],
              'TargetMachine': '1',  # x86.
            },
          },
        },
      ]
    }],
  ],
}
