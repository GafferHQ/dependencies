# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'bindings_core_v8_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/bindings/core/v8',
    'bindings_core_v8_generated_union_type_files': [
      '<(bindings_core_v8_output_dir)/UnionTypesCore.cpp',
      '<(bindings_core_v8_output_dir)/UnionTypesCore.h',
    ],

    'conditions': [
      ['OS=="win" and buildtype=="Official"', {
        # On Windows Official release builds, we try to preserve symbol
        # space.
        'bindings_core_v8_generated_aggregate_files': [
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings.cpp',
        ],
      }, {
        'bindings_core_v8_generated_aggregate_files': [
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings01.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings02.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings03.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings04.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings05.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings06.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings07.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings08.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings09.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings10.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings11.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings12.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings13.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings14.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings15.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings16.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings17.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings18.cpp',
          '<(bindings_core_v8_output_dir)/V8GeneratedCoreBindings19.cpp',
        ],
      }],
    ],
  },
}
