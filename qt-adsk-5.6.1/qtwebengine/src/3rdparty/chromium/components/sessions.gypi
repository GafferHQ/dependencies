# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Core sources shared by sessions_content and sessions_ios. These can't
    # be a separate shared library since one symbol is implemented higher up in
    # the sessions_content/ios layer.
    'sessions_core_sources': [
      'sessions/base_session_service.cc',
      'sessions/base_session_service.h',
      'sessions/base_session_service_commands.cc',
      'sessions/base_session_service_commands.h',
      'sessions/base_session_service_delegate.h',
      'sessions/core/serialized_navigation_driver.h',
      'sessions/serialized_navigation_entry.cc',
      'sessions/serialized_navigation_entry.h',
      'sessions/session_backend.cc',
      'sessions/session_backend.h',
      'sessions/session_command.cc',
      'sessions/session_command.h',
      'sessions/session_id.cc',
      'sessions/session_id.h',
      'sessions/session_service_commands.cc',
      'sessions/session_service_commands.h',
      'sessions/session_types.cc',
      'sessions/session_types.h',
    ],
  },
  'targets': [
    {
      # GN version: //components/sessions:test_support
      'target_name': 'sessions_test_support',
      'type': 'static_library',
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'sessions/serialized_navigation_entry_test_helper.cc',
        'sessions/serialized_navigation_entry_test_helper.h',
      ],
      'conditions': [
        ['OS!="ios" and OS!="android"', {
          'sources': [
            'sessions/base_session_service_test_helper.cc',
            'sessions/base_session_service_test_helper.h',
           ],
        }],
      ],
    },
  ],

  # Platform-specific targets.
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          # GN version: //components/sessions
          'target_name': 'sessions_content',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../content/content.gyp:content_browser',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../url/url.gyp:url_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'SESSIONS_IMPLEMENTATION',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            '<@(sessions_core_sources)',

            'sessions/content/content_serialized_navigation_builder.cc',
            'sessions/content/content_serialized_navigation_builder.h',
            'sessions/content/content_serialized_navigation_driver.cc',
            'sessions/content/content_serialized_navigation_driver.h',
          ],
        },
      ],
    }, {  # OS==ios
      'targets': [
        {
          'target_name': 'sessions_ios',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ios/web/ios_web.gyp:ios_web',
            '../sync/sync.gyp:sync',
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../url/url.gyp:url_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'SESSIONS_IMPLEMENTATION',
          ],
          'sources': [
            '<@(sessions_core_sources)',

            'sessions/ios/ios_serialized_navigation_builder.cc',
            'sessions/ios/ios_serialized_navigation_builder.h',
            'sessions/ios/ios_serialized_navigation_driver.cc',
            'sessions/ios/ios_serialized_navigation_driver.h',
          ],
        },
      ],
    }],
  ],

}
