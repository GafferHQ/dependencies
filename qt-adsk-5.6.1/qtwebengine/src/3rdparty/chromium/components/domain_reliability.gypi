# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'domain_reliability/baked_in_configs.gypi',
  ],
  'targets': [
    {
      # GN version: //components/domain_reliability
      'target_name': 'domain_reliability',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../components/components.gyp:keyed_service_core',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'DOMAIN_RELIABILITY_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'domain_reliability/baked_in_configs.h',
        'domain_reliability/beacon.cc',
        'domain_reliability/beacon.h',
        'domain_reliability/clear_mode.h',
        'domain_reliability/config.cc',
        'domain_reliability/config.h',
        'domain_reliability/context.cc',
        'domain_reliability/context.h',
        'domain_reliability/context_manager.cc',
        'domain_reliability/context_manager.h',
        'domain_reliability/dispatcher.cc',
        'domain_reliability/dispatcher.h',
        'domain_reliability/domain_reliability_export.h',
        'domain_reliability/monitor.cc',
        'domain_reliability/monitor.h',
        'domain_reliability/scheduler.cc',
        'domain_reliability/scheduler.h',
        'domain_reliability/service.cc',
        'domain_reliability/service.h',
        'domain_reliability/uploader.cc',
        'domain_reliability/uploader.h',
        'domain_reliability/util.cc',
        'domain_reliability/util.h',
      ],
      'actions': [
        {
          'action_name': 'bake_in_configs',
          'variables': {
            'bake_in_configs_script': 'domain_reliability/bake_in_configs.py',
            'baked_in_configs_cc':
                '<(INTERMEDIATE_DIR)/domain_reliability/baked_in_configs.cc',
          },
          'inputs': [
            '<(bake_in_configs_script)',
            '<@(baked_in_configs)',
          ],
          'outputs': [
            '<(baked_in_configs_cc)'
          ],
          # The actual list of JSON files will overflow the command line length
          # limit on Windows, so pass the name of the .gypi file and
          # bake_in_configs.py will read the filenames out of it manually.
          'action': ['python',
                     '<(bake_in_configs_script)',
                     '.',
                     'domain_reliability/baked_in_configs.gypi',
                     '<(baked_in_configs_cc)'],
          'process_outputs_as_sources': 1,
          'message': 'Baking in Domain Reliability configs',
        },
      ],
    },
  ],
}
