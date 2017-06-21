# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/accelerated_widget_mac
      'target_name': 'accelerated_widget_mac',
      'type': '<(component)',
      'sources': [
        'accelerated_widget_mac.h',
        'accelerated_widget_mac.mm',
        'accelerated_widget_mac_export.h',
        'display_link_mac.cc',
        'display_link_mac.h',
        'io_surface_context.h',
        'io_surface_context.mm',
        'io_surface_layer.h',
        'io_surface_layer.mm',
        'io_surface_texture.h',
        'io_surface_texture.mm',
        'software_layer.h',
        'software_layer.mm',
        'surface_handle_types.cc',
        'surface_handle_types.h',
      ],
      'defines': [
        'ACCELERATED_WIDGET_MAC_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/ui/events/events.gyp:events_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
      ],
      'link_settings': {
        'libraries': [
          # Required by io_surface_texture.mm.
          '$(SDKROOT)/System/Library/Frameworks/IOSurface.framework',
          '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
          '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
        ],
      },
    },
  ],
}
