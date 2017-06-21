# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'internal_ozone_platform_deps': [
      'ozone_platform_gbm',
    ],
    'internal_ozone_platforms': [
      'gbm',
    ],
    'use_mesa_platform_null%': 0,
  },
  'targets': [
    {
      'target_name': 'ozone_platform_gbm',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../build/linux/system.gyp:libdrm',
        '../../build/linux/system.gyp:gbm',
        '../../skia/skia.gyp:skia',
        '../../third_party/khronos/khronos.gyp:khronos_headers',
        '../base/ui_base.gyp:ui_base',
        '../events/events.gyp:events',
        '../events/ozone/events_ozone.gyp:events_ozone',
        '../events/ozone/events_ozone.gyp:events_ozone_evdev',
        '../events/ozone/events_ozone.gyp:events_ozone_layout',
        '../gfx/gfx.gyp:gfx',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'gpu/gbm_buffer.cc',
        'gpu/gbm_buffer.h',
        'gpu/gbm_buffer_base.cc',
        'gpu/gbm_buffer_base.h',
        'gpu/gbm_device.cc',
        'gpu/gbm_device.h',
        'gpu/gbm_surface.cc',
        'gpu/gbm_surface.h',
        'gpu/gbm_surface_factory.cc',
        'gpu/gbm_surface_factory.h',
        'gpu/gbm_surfaceless.cc',
        'gpu/gbm_surfaceless.h',
        'ozone_platform_gbm.cc',
        'ozone_platform_gbm.h',
      ],
      'conditions': [
        ['use_mesa_platform_null==1', {
          'defines': ['USE_MESA_PLATFORM_NULL'],
        }],
      ],
    },
  ],
}
