# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../skia/skia.gyp:skia',
    '../third_party/khronos/khronos.gyp:khronos_headers',
    '../ui/gl/gl.gyp:gl',
  ],
  'sources': [
    'gpu/gpu_child_thread.cc',
    'gpu/gpu_child_thread.h',
    'gpu/gpu_main.cc',
    'gpu/gpu_process.cc',
    'gpu/gpu_process.h',
    'gpu/gpu_watchdog_thread.cc',
    'gpu/gpu_watchdog_thread.h',
    'gpu/in_process_gpu_thread.cc',
    'gpu/in_process_gpu_thread.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="win" and use_qt==0', {
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
        # ANGLE libs picked up from ui/gl
        '<(angle_path)/src',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
        ],
      },
    }],
    ['qt_os=="mac"', {
      'export_dependent_settings': [
        '../third_party/khronos/khronos.gyp:khronos_headers',
      ],
    }],
    ['qt_os=="win32" and qt_gl=="angle"', {
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
          '-l<(qt_egl_library)',
          '-l<(qt_glesv2_library)',
        ],
      },
    }],
    ['target_arch!="arm" and chromeos == 1', {
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
      ],
    }],
  ],
}
