# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/dom_distiller/webui
      'target_name': 'dom_distiller_webui',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../url/url.gyp:url_lib',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        'dom_distiller_core',
        'dom_distiller_protos',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'dom_distiller/webui/dom_distiller_handler.cc',
        'dom_distiller/webui/dom_distiller_handler.h',
        'dom_distiller/webui/dom_distiller_ui.cc',
        'dom_distiller/webui/dom_distiller_ui.h',
      ],
    },
    {
      # GN version: //components/dom_distiller/core
      'target_name': 'dom_distiller_core',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../third_party/re2/re2.gyp:re2',
        '../third_party/dom_distiller_js/dom_distiller_js.gyp:dom_distiller_js_proto',
        'components.gyp:leveldb_proto',
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        'dom_distiller_protos',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        'dom_distiller_protos',
        '../third_party/dom_distiller_js/dom_distiller_js.gyp:dom_distiller_js_proto',
      ],
      'sources': [
        'dom_distiller/android/component_jni_registrar.cc',
        'dom_distiller/android/component_jni_registrar.h',
        'dom_distiller/core/article_attachments_data.cc',
        'dom_distiller/core/article_attachments_data.h',
        'dom_distiller/core/article_distillation_update.cc',
        'dom_distiller/core/article_distillation_update.h',
        'dom_distiller/core/article_entry.cc',
        'dom_distiller/core/article_entry.h',
        'dom_distiller/core/distillable_page_detector.cc',
        'dom_distiller/core/distillable_page_detector.h',
        'dom_distiller/core/distilled_content_store.cc',
        'dom_distiller/core/distilled_content_store.h',
        'dom_distiller/core/distilled_page_prefs.cc',
        'dom_distiller/core/distilled_page_prefs.h',
        'dom_distiller/core/distilled_page_prefs_android.cc',
        'dom_distiller/core/distilled_page_prefs_android.h',
        'dom_distiller/core/distiller.cc',
        'dom_distiller/core/distiller.h',
        'dom_distiller/core/distiller_page.cc',
        'dom_distiller/core/distiller_page.h',
        'dom_distiller/core/distiller_url_fetcher.cc',
        'dom_distiller/core/distiller_url_fetcher.h',
        'dom_distiller/core/dom_distiller_constants.cc',
        'dom_distiller/core/dom_distiller_constants.h',
        'dom_distiller/core/dom_distiller_model.cc',
        'dom_distiller/core/dom_distiller_model.h',
        'dom_distiller/core/dom_distiller_observer.h',
        'dom_distiller/core/dom_distiller_request_view_base.cc',
        'dom_distiller/core/dom_distiller_request_view_base.h',
        'dom_distiller/core/dom_distiller_service.cc',
        'dom_distiller/core/dom_distiller_service.h',
        'dom_distiller/core/dom_distiller_service_android.cc',
        'dom_distiller/core/dom_distiller_service_android.h',
        'dom_distiller/core/dom_distiller_store.cc',
        'dom_distiller/core/dom_distiller_store.h',
        'dom_distiller/core/dom_distiller_switches.cc',
        'dom_distiller/core/dom_distiller_switches.h',
        'dom_distiller/core/experiments.cc',
        'dom_distiller/core/experiments.h',
        'dom_distiller/core/external_feedback_reporter.h',
        'dom_distiller/core/feedback_reporter.cc',
        'dom_distiller/core/feedback_reporter.h',
        'dom_distiller/core/font_family_list.h',
        'dom_distiller/core/page_features.cc',
        'dom_distiller/core/page_features.h',
        'dom_distiller/core/task_tracker.cc',
        'dom_distiller/core/task_tracker.h',
        'dom_distiller/core/theme_list.h',
        'dom_distiller/core/url_constants.cc',
        'dom_distiller/core/url_constants.h',
        'dom_distiller/core/url_utils.cc',
        'dom_distiller/core/url_utils.h',
        'dom_distiller/core/url_utils_android.cc',
        'dom_distiller/core/url_utils_android.h',
        'dom_distiller/core/viewer.cc',
        'dom_distiller/core/viewer.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'dom_distiller_core_jni_headers',
          ],
        }],
      ],
    },
    {
      # GN version: components/dom_distiller/core:test_support
      'target_name': 'dom_distiller_test_support',
      'type': 'static_library',
      'dependencies': [
        'dom_distiller_core',
        'components.gyp:leveldb_proto_test_support',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'dom_distiller/core/dom_distiller_test_util.cc',
        'dom_distiller/core/dom_distiller_test_util.h',
        'dom_distiller/core/fake_distiller.cc',
        'dom_distiller/core/fake_distiller.h',
        'dom_distiller/core/fake_distiller_page.cc',
        'dom_distiller/core/fake_distiller_page.h',
        'dom_distiller/core/test_request_view_handle.h',
      ],
    },
    {
      # GN version: //components/dom_distiller/core/proto
      'target_name': 'dom_distiller_protos',
      'type': 'static_library',
      'sources': [
        'dom_distiller/core/proto/adaboost.proto',
        'dom_distiller/core/proto/distilled_article.proto',
        'dom_distiller/core/proto/distilled_page.proto',
      ],
      'variables': {
        'proto_in_dir': 'dom_distiller/core/proto',
        'proto_out_dir': 'components/dom_distiller/core/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/dom_distiller/content
          'target_name': 'dom_distiller_content',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            '../ui/gfx/gfx.gyp:gfx',
            '../url/url.gyp:url_lib',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'dom_distiller_core',
            'dom_distiller_protos',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'dom_distiller/content/distillable_page_utils.cc',
            'dom_distiller/content/distillable_page_utils.h',
            'dom_distiller/content/distillable_page_utils_android.cc',
            'dom_distiller/content/distillable_page_utils_android.h',
            'dom_distiller/content/distiller_javascript_utils.cc',
            'dom_distiller/content/distiller_javascript_utils.h',
            'dom_distiller/content/distiller_page_web_contents.cc',
            'dom_distiller/content/distiller_page_web_contents.h',
            'dom_distiller/content/dom_distiller_viewer_source.cc',
            'dom_distiller/content/dom_distiller_viewer_source.h',
            'dom_distiller/content/web_contents_main_frame_observer.cc',
            'dom_distiller/content/web_contents_main_frame_observer.h',
          ],
          'conditions': [
            ['OS == "android"', {
              'dependencies': [
                'dom_distiller_content_jni_headers',
                'dom_distiller_core_jni_headers',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS=="ios"', {
      'targets': [
        {
          'target_name': 'dom_distiller_ios',
          'type': 'static_library',
          'dependencies': [
            '../ios/provider/ios_provider_web.gyp:ios_provider_web',
            'dom_distiller_protos',
            'dom_distiller_core',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'dom_distiller/ios/distiller_page_factory_ios.h',
            'dom_distiller/ios/distiller_page_factory_ios.mm',
            'dom_distiller/ios/distiller_page_ios.h',
            'dom_distiller/ios/distiller_page_ios.mm',
          ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          # TODO(cjhopman): remove this when it is rolled downstream.
          'target_name': 'dom_distiller_core_java',
          'type': 'none',
          'dependencies': [
            'dom_distiller_java',
          ],
        },
        {
          # GN: //components/dom_distiller/android:dom_distiller_java
          'target_name': 'dom_distiller_java',
          'type': 'none',
          'dependencies': [
            'dom_distiller_core_font_family_java',
            'dom_distiller_core_theme_java',
            '../base/base.gyp:base',
            '../content/content.gyp:content_java',
          ],
          'variables': {
            'java_in_dir': 'dom_distiller/android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # GN: //components/dom_distiller/android:dom_distiller_core_font_family_javagen
          'target_name': 'dom_distiller_core_font_family_java',
          'type': 'none',
          'sources': [
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/FontFamily.template',
          ],
          'variables': {
            'package_name': 'org/chromium/components/dom_distiller/core',
            'template_deps': ['dom_distiller/core/font_family_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          # GN: //components/dom_distiller/content:jni_headers
          'target_name': 'dom_distiller_content_jni_headers',
          'type': 'none',
          'sources': [
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/content/DistillablePageUtils.java',
          ],
          'variables': {
            'jni_gen_package': 'dom_distiller_content',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/dom_distiller/core:jni_headers
          'target_name': 'dom_distiller_core_jni_headers',
          'type': 'none',
          'sources': [
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/DistilledPagePrefs.java',
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/DomDistillerService.java',
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/DomDistillerUrlUtils.java',
          ],
          'variables': {
            'jni_gen_package': 'dom_distiller_core',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/dom_distiller/android:dom_distiller_core_theme_javagen
          'target_name': 'dom_distiller_core_theme_java',
          'type': 'none',
          'sources': [
            'dom_distiller/android/java/src/org/chromium/components/dom_distiller/core/Theme.template',
          ],
          'variables': {
            'package_name': 'org/chromium/components/dom_distiller/core',
            'template_deps': ['dom_distiller/core/theme_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
      ],
    }],
  ],
}
