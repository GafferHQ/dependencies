# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'wallpaper',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../content/content.gyp:content',
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines':[
        'WALLPAPER_IMPLEMENTATION',
      ],
      'sources': [
        'wallpaper/wallpaper_layout.h',
        'wallpaper/wallpaper_resizer.cc',
        'wallpaper/wallpaper_resizer.h',
        'wallpaper/wallpaper_resizer_observer.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
            '../components/components.gyp:user_manager',
          ],
          'sources': [
            'wallpaper/wallpaper_manager_base.cc',
            'wallpaper/wallpaper_manager_base.h',
          ],
        }],
      ],
    },
  ],
}
