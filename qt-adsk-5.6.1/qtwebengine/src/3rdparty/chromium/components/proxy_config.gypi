# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/proxy_config
      'target_name': 'proxy_config',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'PROXY_CONFIG_IMPLEMENTATION',
      ],
      'sources': [
        'proxy_config/proxy_config_dictionary.cc',
        'proxy_config/proxy_config_dictionary.h',
        'proxy_config/proxy_config_export.h',
        'proxy_config/proxy_prefs.cc',
        'proxy_config/proxy_prefs.h',
      ],
    },
  ],
}
