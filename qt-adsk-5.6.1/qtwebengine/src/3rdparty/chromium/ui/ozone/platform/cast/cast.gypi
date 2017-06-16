# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'cast_graphics_gyp%': '../../chromecast/chromecast.gyp',
    'libcast_media_gyp%': '../../chromecast/media/media.gyp',
    'internal_ozone_platform_deps': [
      'ozone_platform_cast',
    ],
    'internal_ozone_platform_unittest_deps': [ ],
    'internal_ozone_platforms': [
      'cast',
    ],
  },
  'targets': [
    {
      'target_name': 'ozone_platform_cast',
      'type': 'static_library',
      'dependencies': [
        '<(cast_graphics_gyp):libcast_graphics_1.0',
        '<(libcast_media_gyp):libcast_media_1.0',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../../base/base.gyp:base',
        '../../chromecast/chromecast.gyp:cast_public_api',
        '../../chromecast/media/media.gyp:media_base',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'sources': [
        'gpu_platform_support_cast.cc',
        'gpu_platform_support_cast.h',
        'overlay_manager_cast.cc',
        'overlay_manager_cast.h',
        'ozone_platform_cast.cc',
        'ozone_platform_cast.h',
        'platform_window_cast.cc',
        'platform_window_cast.h',
        'surface_factory_cast.cc',
        'surface_factory_cast.h',
        'surface_ozone_egl_cast.cc',
        'surface_ozone_egl_cast.h',
      ],
    },
  ],
}
