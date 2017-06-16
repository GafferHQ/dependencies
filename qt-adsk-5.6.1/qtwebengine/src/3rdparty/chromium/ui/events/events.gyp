# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/events:dom_keycode_converter
      'target_name': 'dom_keycode_converter',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'keycodes/dom/dom_code.h',
        'keycodes/dom/dom_key.h',
        'keycodes/dom/dom_key_data.inc',
        'keycodes/dom/keycode_converter.cc',
        'keycodes/dom/keycode_converter.h',
        'keycodes/dom/keycode_converter_data.inc',
      ],
    },
    {
      # GN version: //ui/events:events_base
      'target_name': 'events_base',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'dom_keycode_converter',
      ],
      'defines': [
        'EVENTS_BASE_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'android/scroller.cc',
        'android/scroller.h',
        'base_event_utils.cc',
        'base_event_utils.h',
        'event_constants.h',
        'event_switches.cc',
        'event_switches.h',
        'events_base_export.h',
        'gesture_curve.h',
        'gesture_event_details.cc',
        'gesture_event_details.h',
        'gestures/fling_curve.cc',
        'gestures/fling_curve.h',
        'keycodes/dom_us_layout_data.h',
        'keycodes/keyboard_code_conversion.cc',
        'keycodes/keyboard_code_conversion.h',
        'keycodes/keyboard_code_conversion_android.cc',
        'keycodes/keyboard_code_conversion_android.h',
        'keycodes/keyboard_code_conversion_mac.h',
        'keycodes/keyboard_code_conversion_mac.mm',
        'keycodes/keyboard_code_conversion_win.cc',
        'keycodes/keyboard_code_conversion_win.h',
        'keycodes/keyboard_code_conversion_x.cc',
        'keycodes/keyboard_code_conversion_x.h',
        'keycodes/keyboard_codes.h',
        'latency_info.cc',
        'latency_info.h',
        'x/keysym_to_unicode.cc',
        'x/keysym_to_unicode.h',
      ],
      'export_dependent_settings': [
        '../../ui/gfx/gfx.gyp:gfx',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_x11==1 or use_xkbcommon==1', {
          'sources': [
            'keycodes/keyboard_code_conversion_xkb.cc',
            'keycodes/keyboard_code_conversion_xkb.h',
            'keycodes/xkb_keysym.h',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/events
      'target_name': 'events',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'dom_keycode_converter',
        'events_base',
        'gesture_detection',
      ],
      'defines': [
        'EVENTS_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'cocoa/cocoa_event_utils.h',
        'cocoa/cocoa_event_utils.mm',
        'cocoa/events_mac.mm',
        'event.cc',
        'event.h',
        'event_dispatcher.cc',
        'event_dispatcher.h',
        'event_handler.cc',
        'event_handler.h',
        'event_processor.cc',
        'event_processor.h',
        'event_rewriter.h',
        'event_source.cc',
        'event_source.h',
        'event_target.cc',
        'event_target.h',
        'event_target_iterator.h',
        'event_targeter.h',
        'event_utils.cc',
        'event_utils.h',
        'events_export.h',
        'events_stub.cc',
        'gestures/gesture_provider_aura.cc',
        'gestures/gesture_provider_aura.h',
        'gestures/gesture_recognizer.h',
        'gestures/gesture_recognizer_impl.cc',
        'gestures/gesture_recognizer_impl.h',
        'gestures/gesture_recognizer_impl_mac.cc',
        'gestures/gesture_types.h',
        'gestures/motion_event_aura.cc',
        'gestures/motion_event_aura.h',
        'linux/text_edit_command_auralinux.cc',
        'linux/text_edit_command_auralinux.h',
        'linux/text_edit_key_bindings_delegate_auralinux.cc',
        'linux/text_edit_key_bindings_delegate_auralinux.h',
        'null_event_targeter.cc',
        'null_event_targeter.h',
        'ozone/events_ozone.cc',
        'win/events_win.cc',
        'x/events_x.cc',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            'devices/events_devices.gyp:events_devices',
            '../gfx/x/gfx_x11.gyp:gfx_x11',
            '../../build/linux/system.gyp:x11',
          ],
        }],
        ['use_aura==0', {
          'sources!': [
            'gestures/gesture_provider_aura.cc',
            'gestures/gesture_provider_aura.h',
            'gestures/gesture_recognizer.h',
            'gestures/gesture_recognizer_impl.cc',
            'gestures/gesture_recognizer_impl.h',
            'gestures/gesture_types.h',
            'gestures/motion_event_aura.cc',
            'gestures/motion_event_aura.h',
          ],
        }],
        # We explicitly enumerate the platforms we _do_ provide native cracking
        # for here.
        ['OS=="win" or OS=="mac" or use_x11==1 or use_ozone==1', {
          'sources!': [
            'events_stub.cc',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'linux/text_edit_command_auralinux.cc',
            'linux/text_edit_command_auralinux.h',
            'linux/text_edit_key_bindings_delegate_auralinux.cc',
            'linux/text_edit_key_bindings_delegate_auralinux.h',
          ],
        }],
        ['use_ozone==1', {
          'dependencies': [
            'ozone/events_ozone.gyp:events_ozone_layout',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/events/blink
      'target_name': 'blink',
      'type': 'static_library',
      'dependencies': [
        '../../third_party/WebKit/public/blink_headers.gyp:blink_headers',
        '../gfx/gfx.gyp:gfx_geometry',
        'events',
        'gesture_detection',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'blink/blink_event_util.cc',
        'blink/blink_event_util.h',
      ],
    },
    {
      # GN version: //ui/events/gestures/blink
      'target_name': 'gestures_blink',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/WebKit/public/blink_headers.gyp:blink_headers',
        '../gfx/gfx.gyp:gfx_geometry',
        'events',
        'gesture_detection',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'gestures/blink/web_gesture_curve_impl.cc',
        'gestures/blink/web_gesture_curve_impl.h',
      ],
    },
    {
      # GN version: //ui/events:gesture_detection
      'target_name': 'gesture_detection',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'events_base',
      ],
      'defines': [
        'GESTURE_DETECTION_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'gesture_detection/bitset_32.h',
        'gesture_detection/filtered_gesture_provider.cc',
        'gesture_detection/filtered_gesture_provider.h',
        'gesture_detection/gesture_configuration.cc',
        'gesture_detection/gesture_configuration.h',
        'gesture_detection/gesture_configuration_android.cc',
        'gesture_detection/gesture_configuration_aura.cc',
        'gesture_detection/gesture_detection_export.h',
        'gesture_detection/gesture_detector.cc',
        'gesture_detection/gesture_detector.h',
        'gesture_detection/gesture_event_data.cc',
        'gesture_detection/gesture_event_data.h',
        'gesture_detection/gesture_event_data_packet.cc',
        'gesture_detection/gesture_event_data_packet.h',
        'gesture_detection/gesture_listeners.cc',
        'gesture_detection/gesture_listeners.h',
        'gesture_detection/gesture_provider.cc',
        'gesture_detection/gesture_provider.h',
        'gesture_detection/gesture_provider_config_helper.cc',
        'gesture_detection/gesture_provider_config_helper.h',
        'gesture_detection/gesture_touch_uma_histogram.cc',
        'gesture_detection/gesture_touch_uma_histogram.h',
        'gesture_detection/motion_event.cc',
        'gesture_detection/motion_event.h',
        'gesture_detection/motion_event_buffer.cc',
        'gesture_detection/motion_event_buffer.h',
        'gesture_detection/motion_event_generic.cc',
        'gesture_detection/motion_event_generic.h',
        'gesture_detection/scale_gesture_detector.cc',
        'gesture_detection/scale_gesture_detector.h',
        'gesture_detection/scale_gesture_listeners.cc',
        'gesture_detection/scale_gesture_listeners.h',
        'gesture_detection/snap_scroll_controller.cc',
        'gesture_detection/snap_scroll_controller.h',
        'gesture_detection/touch_disposition_gesture_filter.cc',
        'gesture_detection/touch_disposition_gesture_filter.h',
        'gesture_detection/velocity_tracker.cc',
        'gesture_detection/velocity_tracker.h',
        'gesture_detection/velocity_tracker_state.cc',
        'gesture_detection/velocity_tracker_state.h',
      ],
      'conditions': [
        ['use_aura!=1 and OS!="android"', {
          'sources': [
            'gesture_detection/gesture_configuration_default.cc',
          ],
        }],
        ['use_aura==1 and OS=="android"', {
          'sources!': [
            'gesture_detection/gesture_configuration_aura.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/events:test_support
      'target_name': 'events_test_support',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx_geometry',
        'events',
        'events_base',
        'gesture_detection',
        'platform/events_platform.gyp:events_platform',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'test/cocoa_test_event_utils.h',
        'test/cocoa_test_event_utils.mm',
        'test/event_generator.cc',
        'test/event_generator.h',
        'test/events_test_utils.cc',
        'test/events_test_utils.h',
        'test/events_test_utils_x11.cc',
        'test/events_test_utils_x11.h',
        'test/motion_event_test_utils.cc',
        'test/motion_event_test_utils.h',
        'test/platform_event_waiter.cc',
        'test/platform_event_waiter.h',
        'test/test_event_handler.cc',
        'test/test_event_handler.h',
        'test/test_event_processor.cc',
        'test/test_event_processor.h',
        'test/test_event_target.cc',
        'test/test_event_target.h',
        'test/test_event_targeter.cc',
        'test/test_event_targeter.h',
      ],
      'conditions': [
        ['OS=="ios"', {
          # The cocoa files don't apply to iOS.
          'sources/': [['exclude', 'cocoa']],
        }],
        ['use_x11==1', {
          'dependencies': [
            'devices/events_devices.gyp:events_devices',
          ],
        }],
      ],
    },
    {
      # GN version: //ui/events:events_unittests
      'target_name': 'events_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        '../gfx/gfx.gyp:gfx_test_support',
        'devices/events_devices.gyp:events_devices',
        'dom_keycode_converter',
        'events',
        'events_base',
        'events_test_support',
        'gesture_detection',
        'gestures_blink',
        'platform/events_platform.gyp:events_platform',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'android/scroller_unittest.cc',
        'cocoa/events_mac_unittest.mm',
        'devices/x11/device_data_manager_x11_unittest.cc',
        'event_dispatcher_unittest.cc',
        'event_processor_unittest.cc',
        'event_rewriter_unittest.cc',
        'event_unittest.cc',
        'gesture_detection/bitset_32_unittest.cc',
        'gesture_detection/filtered_gesture_provider_unittest.cc',
        'gesture_detection/gesture_event_data_packet_unittest.cc',
        'gesture_detection/gesture_provider_unittest.cc',
        'gesture_detection/motion_event_buffer_unittest.cc',
        'gesture_detection/motion_event_generic_unittest.cc',
        'gesture_detection/snap_scroll_controller_unittest.cc',
        'gesture_detection/touch_disposition_gesture_filter_unittest.cc',
        'gesture_detection/velocity_tracker_unittest.cc',
        'gestures/blink/web_gesture_curve_impl_unittest.cc',
        'gestures/fling_curve_unittest.cc',
        'gestures/gesture_provider_aura_unittest.cc',
        'gestures/motion_event_aura_unittest.cc',
        'keycodes/dom/keycode_converter_unittest.cc',
        'keycodes/keyboard_code_conversion_unittest.cc',
        'latency_info_unittest.cc',
        'platform/platform_event_source_unittest.cc',
        'x/events_x_unittest.cc',
      ],
      'include_dirs': [
        '../../testing/gmock/include',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            '../gfx/x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_ozone==1', {
          'sources': [
            'ozone/chromeos/cursor_controller_unittest.cc',
            'ozone/evdev/event_converter_evdev_impl_unittest.cc',
            'ozone/evdev/event_converter_test_util.cc',
            'ozone/evdev/event_device_info_unittest.cc',
            'ozone/evdev/event_device_test_util.cc',
            'ozone/evdev/input_injector_evdev_unittest.cc',
            'ozone/evdev/tablet_event_converter_evdev_unittest.cc',
            'ozone/evdev/touch_event_converter_evdev_unittest.cc',
            'ozone/evdev/touch_noise/touch_noise_finder_unittest.cc',
          ],
          'dependencies': [
            'ozone/events_ozone.gyp:events_ozone',
            'ozone/events_ozone.gyp:events_ozone_evdev',
            'ozone/events_ozone.gyp:events_ozone_layout',
          ]
        }],
        ['use_xkbcommon==1', {
          'sources': [
            'ozone/layout/xkb/xkb_keyboard_layout_engine_unittest.cc',
          ]
        }],
        ['use_aura==0', {
          'sources!': [
            'gestures/gesture_provider_aura_unittest.cc',
            'gestures/motion_event_aura_unittest.cc',
          ],
        }],
        ['OS=="linux" and use_allocator!="none"', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        # Exclude tests that rely on event_utils.h for platforms that do not
        # provide native cracking, i.e., platforms that use events_stub.cc.
        ['OS!="win" and use_x11!=1 and use_ozone!=1', {
          'sources!': [
            'event_unittest.cc',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'events_unittests_apk',
          'type': 'none',
          'dependencies': [
            'events_unittests',
          ],
          'variables': {
            'test_suite_name': 'events_unittests',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'events_unittests_run',
          'type': 'none',
          'dependencies': [
            'events_unittests',
          ],
          'includes': [
            '../../build/isolate.gypi',
          ],
          'sources': [
            'events_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
