# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/shell_dialogs
      'target_name': 'shell_dialogs',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../strings/ui_strings.gyp:ui_strings',
      ],
      'defines': [
        'SHELL_DIALOGS_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'android/shell_dialogs_jni_registrar.cc',
        'android/shell_dialogs_jni_registrar.h',
        'base_shell_dialog.cc',
        'base_shell_dialog.h',
        'base_shell_dialog_win.cc',
        'base_shell_dialog_win.h',
        'linux_shell_dialog.cc',
        'linux_shell_dialog.h',
        'select_file_dialog.cc',
        'select_file_dialog.h',
        'select_file_dialog_android.cc',
        'select_file_dialog_android.h',
        'select_file_dialog_factory.cc',
        'select_file_dialog_factory.h',
        'select_file_dialog_mac.h',
        'select_file_dialog_mac.mm',
        'select_file_dialog_win.cc',
        'select_file_dialog_win.h',
        'select_file_policy.cc',
        'select_file_policy.h',
        'selected_file_info.cc',
        'selected_file_info.h',
      ],
      'conditions': [
        ['use_aura==1',
          {
            'dependencies': [
              '../aura/aura.gyp:aura',
            ],
            'sources!': [
              'select_file_dialog_mac.mm',
            ],
          }
        ],
        ['OS=="android"',
          {
            'dependencies': [
              '../android/ui_android.gyp:ui_android',
              '../android/ui_android.gyp:ui_java',
              '../base/ui_base.gyp:ui_base_jni_headers',
            ],
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui',
            ],
            'link_settings': {
              'libraries': [
                '-ljnigraphics',
              ],
            },
          }
        ],
        ['OS=="win"',
          {
            'dependencies': [
              '../../win8/win8.gyp:metro_viewer',
            ],
          }
        ],
      ],
    },  # target_name: shell_dialogs
    {
      # GN version: //ui/shell_dialogs:shell_dialogs_unittests
      'target_name': 'shell_dialogs_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
        'shell_dialogs',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'select_file_dialog_win_unittest.cc',
      ],
    },
  ],
}
