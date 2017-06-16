# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/display/types
      'target_name': 'display_types',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'DISPLAY_TYPES_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'types/display_constants.h',
        'types/display_mode.cc',
        'types/display_mode.h',
        'types/display_snapshot.cc',
        'types/display_snapshot.h',
        'types/display_types_export.h',
        'types/gamma_ramp_rgb_entry.h',
        'types/native_display_delegate.h',
        'types/native_display_observer.h',
      ],
    },
    {
      # GN version: //ui/display
      'target_name': 'display',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        'display_util',
      ],
      'defines': [
        'DISPLAY_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'chromeos/apply_content_protection_task.cc',
        'chromeos/apply_content_protection_task.h',
        'chromeos/configure_displays_task.cc',
        'chromeos/configure_displays_task.h',
        'chromeos/display_configurator.cc',
        'chromeos/display_configurator.h',
        'chromeos/display_layout_manager.h',
        'chromeos/display_util.cc',
        'chromeos/display_util.h',
        'chromeos/ozone/display_configurator_ozone.cc',
        'chromeos/query_content_protection_task.cc',
        'chromeos/query_content_protection_task.h',
        'chromeos/update_display_configuration_task.cc',
        'chromeos/update_display_configuration_task.h',
        'chromeos/x11/display_configurator_x11.cc',
        'chromeos/x11/display_mode_x11.cc',
        'chromeos/x11/display_mode_x11.h',
        'chromeos/x11/display_snapshot_x11.cc',
        'chromeos/x11/display_snapshot_x11.h',
        'chromeos/x11/display_util_x11.cc',
        'chromeos/x11/display_util_x11.h',
        'chromeos/x11/native_display_delegate_x11.cc',
        'chromeos/x11/native_display_delegate_x11.h',
        'chromeos/x11/native_display_event_dispatcher_x11.cc',
        'chromeos/x11/native_display_event_dispatcher_x11.h',
        'display_export.h',
        'display_switches.cc',
        'display_switches.h',
      ],
      'conditions': [
        ['use_x11 == 1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../../build/linux/system.gyp:xext',
            '../../build/linux/system.gyp:xi',
            '../../build/linux/system.gyp:xrandr',
            '../../ui/events/platform/events_platform.gyp:events_platform',
          ],
        }],
        ['chromeos == 1', {
          'dependencies': [
            'display_types',
          ],
        }],
        ['chromeos == 1 and use_x11 == 1', {
          'dependencies': [
            '../gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_ozone == 1', {
          'dependencies': [
            '../../ui/ozone/ozone.gyp:ozone',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/display/util
      'target_name': 'display_util',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'DISPLAY_UTIL_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list shared with GN build.
        'util/display_util.cc',
        'util/display_util.h',
        'util/display_util_export.h',
        'util/edid_parser.cc',
        'util/edid_parser.h',
        'util/x11/edid_parser_x11.cc',
        'util/x11/edid_parser_x11.h',
      ],
      'conditions': [
        ['use_x11 == 1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../../build/linux/system.gyp:xrandr',
            '../../ui/gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['chromeos == 1', {
          'dependencies': [
            'display_types',
          ],
        }],
      ],
    },
    {
      # Used to share stubs with code outside ui/display
      #
      # GN version: //ui/display:test_util
      'target_name': 'display_test_util',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'defines': [
        'DISPLAY_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'chromeos/test/test_display_snapshot.cc',
        'chromeos/test/test_display_snapshot.h',
      ],
      'conditions': [
        ['chromeos == 1', {
          'dependencies': [
            'display_types',
          ],
        }],
      ],
    },
    # Internal utilities used by display_unittests
    {
      'target_name': 'display_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        'chromeos/test/action_logger.cc',
        'chromeos/test/action_logger.h',
        'chromeos/test/action_logger_util.cc',
        'chromeos/test/action_logger_util.h',
        'chromeos/test/test_display_layout_manager.cc',
        'chromeos/test/test_display_layout_manager.h',
        'chromeos/test/test_native_display_delegate.cc',
        'chromeos/test/test_native_display_delegate.h',
      ],
    },
    {
      # GN version: //ui/display:display_unittests
      'target_name': 'display_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../testing/gtest.gyp:gtest',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        'display_util',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'chromeos/apply_content_protection_task_unittest.cc',
        'chromeos/configure_displays_task_unittest.cc',
        'chromeos/display_configurator_unittest.cc',
        'chromeos/query_content_protection_task_unittest.cc',
        'chromeos/update_display_configuration_task_unittest.cc',
        'chromeos/x11/display_util_x11_unittest.cc',
        'chromeos/x11/native_display_event_dispatcher_x11_unittest.cc',
        'util/display_util_unittest.cc',
        'util/edid_parser_unittest.cc',
      ],
      'conditions': [
        ['chromeos == 1', {
          'dependencies': [
            'display',
            'display_test_support',
            'display_test_util',
            'display_types',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'display_unittests_run',
          'type': 'none',
          'dependencies': [
            'display_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'display_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
