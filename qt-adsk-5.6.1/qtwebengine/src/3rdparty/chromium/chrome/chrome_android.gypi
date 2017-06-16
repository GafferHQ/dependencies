# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'conditions': [
      ['component != "shared_library" and target_arch != "arm64" and target_arch != "x64" and profiling_full_stack_frames != 1', {
        # Only enable the chromium linker on regular builds, since the
        # component build crashes on Android 4.4. See b/11379966
        'use_chromium_linker': '1',
      }],
    ],
  },
  'includes': [
    'chrome_android_paks.gypi', # Included for the list of pak resources.
    'chrome_shell.gypi', # Built atop chrome_android_core (defined here)
  ],
  'targets': [
    {
      # GN: //chrome:chrome_android_core
      'target_name': 'chrome_android_core',
      'type': 'static_library',
      'dependencies': [
        'chrome.gyp:browser',
        'chrome.gyp:browser_ui',
        'chrome.gyp:child',
        'chrome.gyp:plugin',
        'chrome.gyp:renderer',
        'chrome.gyp:utility',
        # TODO(kkimlabs): Move this to chrome.gyp:browser when the dependent
        #                 is upstreamed.
        '../components/components.gyp:enhanced_bookmarks',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_both',
      ],
      'include_dirs': [
        '..',
        '<(android_ndk_include)',
      ],
      'sources': [
        'app/android/chrome_android_initializer.cc',
        'app/android/chrome_android_initializer.h',
        'app/android/chrome_jni_onload.cc',
        'app/android/chrome_jni_onload.h',
        'app/android/chrome_main_delegate_android.cc',
        'app/android/chrome_main_delegate_android.h',
        'app/chrome_main_delegate.cc',
        'app/chrome_main_delegate.h',
      ],
      'link_settings': {
        'libraries': [
          '-landroid',
          '-ljnigraphics',
        ],
      },
    },
    {
      # GYP: //chrome/android:chrome_version_java
      'target_name': 'chrome_version_java',
      'type': 'none',
      'variables': {
        'template_input_path': 'android/java/ChromeVersionConstants.java.version',
        'version_path': 'VERSION',
        'version_py_path': '<(DEPTH)/build/util/version.py',
        'output_path': '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)/org/chromium/chrome/browser/ChromeVersionConstants.java',

        'conditions': [
          ['branding == "Chrome"', {
            'branding_path': 'app/theme/google_chrome/BRANDING',
          }, {
            'branding_path': 'app/theme/chromium/BRANDING',
          }],
        ],
      },
      'direct_dependent_settings': {
        'variables': {
          # Ensure that the output directory is used in the class path
          # when building targets that depend on this one.
          'generated_src_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)',
          ],
          # Ensure dependents are rebuilt when the generated Java file changes.
          'additional_input_paths': [
            '<(output_path)',
          ],
        },
      },
      'actions': [
        {
          'action_name': 'chrome_version_java_template',
          'inputs': [
            '<(template_input_path)',
            '<(version_path)',
            '<(branding_path)',
            '<(version_py_path)',
          ],
          'outputs': [
            '<(output_path)',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(version_path)',
            '-f', '<(branding_path)',
            '-e', 'CHANNEL=str.upper("<(android_channel)")',
            '<(template_input_path)',
            '<(output_path)',
          ],
        },
      ],
    },
  ],
}
