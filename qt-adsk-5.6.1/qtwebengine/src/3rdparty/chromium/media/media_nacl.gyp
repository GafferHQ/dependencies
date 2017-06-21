# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'shared_memory_support_nacl',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libshared_memory_support_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl',
            '../base/base_nacl.gyp:base_nacl_nonsfi',
          ],
          'defines': [
            'MEDIA_IMPLEMENTATION',
          ],
          'include_dirs': [
            '..',
          ],
          'includes': [
            'shared_memory_support.gypi',
          ],
          'sources': [
            '<@(shared_memory_support_sources)',
          ],
        },  # end of target 'shared_memory_support_nacl'
        {
          'target_name': 'media_yuv_nacl',
          'type': 'none',
          'variables': {
            'nlib_target': 'libmedia_yuv_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_pnacl_newlib': 1,
          },
          'sources': [
            'base/media.cc',
            'base/media.h',
            'base/simd/convert_rgb_to_yuv.h',
            'base/simd/convert_rgb_to_yuv_c.cc',
            'base/simd/convert_yuv_to_rgb.h',
            'base/simd/convert_yuv_to_rgb_c.cc',
            'base/simd/filter_yuv.h',
            'base/simd/filter_yuv_c.cc',
            'base/yuv_convert.cc',
            'base/yuv_convert.h',
          ],
          'defines': [
            'MEDIA_DISABLE_FFMPEG',
          ],
        },  # end of target 'media_yuv_nacl'
      ],
    }],
  ],
}
