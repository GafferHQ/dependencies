# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'grit_base_dir': '<(SHARED_INTERMEDIATE_DIR)',
    'grit_out_dir': '<(grit_base_dir)/ios/chrome',
  },
  'targets': [
    {
      'target_name': 'ios_chrome_resources',
      'type': 'none',
      'dependencies': [
        'ios_strings_resources_gen',
        'ios_theme_resources_gen',
      ],
    },
    {
      'target_name': 'ios_strings_resources_gen',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'ios_strings_resources',
          'variables': {
            'grit_whitelist': '',
            'grit_grd_file': 'app/strings/ios_strings_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      # Override the exported include-dirs; ios_strings_resources.h should only
      # be referenceable as ios/chrome/grit/ to allow DEPS-time checking of
      # usage.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_dir)',
        ],
        'include_dirs!': [
          '<(grit_out_dir)',
        ],
      }
    },
    {
      'target_name': 'ios_theme_resources_gen',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'ios_theme_resources',
          'variables': {
            'grit_whitelist': '',
            'grit_grd_file': 'app/theme/ios_theme_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      # Override the exported include-dirs; ios_theme_resources.h should only be
      # referencable as ios/chrome/grit/ to allow DEPS-time checking of usage.
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_base_dir)',
        ],
        'include_dirs!': [
          '<(grit_out_dir)',
        ],
      },
    },
    {
      'target_name': 'ios_packed_resources',
      'type': 'none',
      'dependencies': [
        '../../components/components_strings.gyp:components_strings',
        '../../net/net.gyp:net_resources',
        '../../ui/resources/ui_resources.gyp:ui_resources',
        '../../ui/strings/ui_strings.gyp:ui_strings',
        'ios_chrome_resources',
      ],
      'actions': [
        {
          'action_name': 'repack_ios_locales',
          'variables': {
            'repack_locales_path': 'tools/build/ios_repack_locales.py',
          },
          'inputs': [
            'tools/build/ios_repack_locales.py',
            '<!@pymod_do_main(ios_repack_locales -i '
              '-s <(SHARED_INTERMEDIATE_DIR) '
              '-x <(SHARED_INTERMEDIATE_DIR)/repack_ios '
              '<(locales))'
          ],
          'outputs': [
            '<!@pymod_do_main(ios_repack_locales -o '
              '-s <(SHARED_INTERMEDIATE_DIR) '
              '-x <(SHARED_INTERMEDIATE_DIR)/repack_ios '
              '<(locales))'
          ],
          'action': [
            'python',
            'tools/build/ios_repack_locales.py',
            '-x', '<(SHARED_INTERMEDIATE_DIR)/repack_ios',
            '-s', '<(SHARED_INTERMEDIATE_DIR)',
            '<@(locales)',
          ],
        },
        {
          'action_name': 'repack_ios_resources_100_percent',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_theme_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
            ],
            'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_100_percent.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
        {
          'action_name': 'repack_ios_resources_200_percent',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_theme_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_200_percent.pak',
            ],
            'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_200_percent.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
        {
          'action_name': 'repack_ios_resources_300_percent',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_300_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ios/chrome/ios_theme_resources_300_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_300_percent.pak',
            ],
            'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack_ios/chrome_300_percent.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
        {
          'action_name': 'repack_ios_resources',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
            ],
            'pak_output': '<(SHARED_INTERMEDIATE_DIR)/repack_ios/resources.pak',
          },
          'includes': [ '../../build/repack_action.gypi' ],
        },
      ],
    },
  ],
}

