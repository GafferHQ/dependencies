# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'xps',
      'product_name': 'skia_xps',
      'type': 'static_library',
      'standalone_static_library': 1,
      'conditions': [
        [ 'skia_os == "win"', {
          'defines': [ 'SK_XPS_USE_DETERMINISTIC_IDS', ],
          'dependencies': [
            'skia_lib.gyp:skia_lib',
          ],
          'include_dirs': [
            '../include/device/xps',
            '../include/utils/win',
            '../src/core', # needed to get SkGlyphCache.h
            '../src/utils', # needed to get SkBitSet.h
          ],
          'sources': [
            '../include/device/xps/SkConstexprMath.h',
            '../include/device/xps/SkXPSDevice.h',
            '../src/device/xps/SkXPSDevice.cpp',
            '../src/doc/SkDocument_XPS.cpp',
          ],
          'link_settings': {
            'libraries': [
              '-lt2embed.lib',
              '-lfontsub.lib',
            ],
          },
          'direct_dependent_settings': {
            'defines': [ 'SK_XPS_USE_DETERMINISTIC_IDS', ],
            'include_dirs': [
              '../include/device/xps',
              '../src/utils', # needed to get SkBitSet.h
            ],
          },
        },{ #else if 'skia_os != "win"'
          'sources': [ '../src/doc/SkDocument_XPS_None.cpp', ],
          'dependencies': [ 'skia_lib.gyp:skia_lib', ],
        }],
      ],
    },
  ],
}
