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
      'target_name': 'crashpad_minidump_test',
      'type': 'executable',
      'dependencies': [
        'minidump.gyp:crashpad_minidump',
        '../snapshot/snapshot_test.gyp:crashpad_snapshot_test_lib',
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
        'minidump_context_writer_test.cc',
        'minidump_crashpad_info_writer_test.cc',
        'minidump_exception_writer_test.cc',
        'minidump_file_writer_test.cc',
        'minidump_memory_writer_test.cc',
        'minidump_misc_info_writer_test.cc',
        'minidump_module_crashpad_info_writer_test.cc',
        'minidump_module_writer_test.cc',
        'minidump_rva_list_writer_test.cc',
        'minidump_simple_string_dictionary_writer_test.cc',
        'minidump_string_writer_test.cc',
        'minidump_system_info_writer_test.cc',
        'minidump_thread_id_map_test.cc',
        'minidump_thread_writer_test.cc',
        'minidump_writable_test.cc',
        'test/minidump_context_test_util.cc',
        'test/minidump_context_test_util.h',
        'test/minidump_file_writer_test_util.cc',
        'test/minidump_file_writer_test_util.h',
        'test/minidump_memory_writer_test_util.cc',
        'test/minidump_memory_writer_test_util.h',
        'test/minidump_rva_list_test_util.cc',
        'test/minidump_rva_list_test_util.h',
        'test/minidump_string_writer_test_util.cc',
        'test/minidump_string_writer_test_util.h',
        'test/minidump_writable_test_util.cc',
        'test/minidump_writable_test_util.h',
      ],
    },
  ],
}
