# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'webp_transcode',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ios/net/ios_net.gyp:ios_net',
        '../net/net.gyp:net',
        '../third_party/libwebp/libwebp.gyp:libwebp_dec',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'webp_transcode/webp_decoder.h',
        'webp_transcode/webp_decoder.mm',
        'webp_transcode/webp_network_client.h',
        'webp_transcode/webp_network_client.mm',
        'webp_transcode/webp_network_client_factory.h',
        'webp_transcode/webp_network_client_factory.mm',
       ],
    },
  ],
}
