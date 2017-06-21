# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'android_support_v13_target%':
        '../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
    'cast_build_release': 'internal/build/cast_build_release',
    'cast_is_debug_build%': 0,
    # Refers to enum CastProductType in components/metrics/proto/cast_logs.proto
    'cast_product_type%': 0,  # CAST_PRODUCT_TYPE_UNKNOWN
    'chromium_code': 1,
    'chromecast_branding%': 'Chromium',
    'disable_display%': 0,
    'enable_default_cast_graphics%': 1,
    'ozone_platform_cast%': 0,
    'use_chromecast_webui%': 0,
  },
  'includes': [
    'chromecast_tests.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',  # Root of Chromium checkout
    ],
    'conditions': [
      ['disable_display==1', {
        'defines': ['DISABLE_DISPLAY'],
      }],
    ],
  },
  'targets': [
    # Public API target for OEM partners to replace shlibs.
    {
      'target_name': 'cast_public_api',
      'type': '<(component)',
      'sources': [
        'public/cast_egl_platform.h',
        'public/cast_egl_platform_shlib.h',
        'public/cast_media_shlib.h',
        'public/cast_sys_info.h',
        'public/chromecast_export.h',
        'public/graphics_properties_shlib.h',
        'public/graphics_types.h',
        'public/media/decoder_config.h',
        'public/media/stream_id.h',
        'public/osd_plane.h',
        'public/osd_plane_shlib.h',
        'public/osd_surface.h',
        'public/video_plane.h',
      ],
    },
    {
      'target_name': 'cast_base',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'base/cast_paths.cc',
        'base/cast_paths.h',
        'base/chromecast_switches.cc',
        'base/chromecast_switches.h',
        'base/error_codes.cc',
        'base/error_codes.h',
        'base/metrics/cast_histograms.h',
        'base/metrics/cast_metrics_helper.cc',
        'base/metrics/cast_metrics_helper.h',
        'base/metrics/grouped_histogram.cc',
        'base/metrics/grouped_histogram.h',
        'base/path_utils.cc',
        'base/path_utils.h',
        'base/process_utils.cc',
        'base/process_utils.h',
        'base/serializers.cc',
        'base/serializers.h'
      ],
    },  # end of target 'cast_base'
    {
      'target_name': 'cast_crash',
      'type': '<(component)',
      'include_dirs': [
        # TODO(gfhuang): we should not need to include this directly, but
        # somehow depending on component.gyp:breakpad_component is not
        # working as expected.
        '../breakpad/src',
      ],
      'dependencies': [
        'cast_base',
        'cast_version_header',
        '../breakpad/breakpad.gyp:breakpad_client',
      ],
      'sources': [
        'crash/app_state_tracker.cc',
        'crash/app_state_tracker.h',
        'crash/cast_crash_keys.cc',
        'crash/cast_crash_keys.h',
        'crash/cast_crashdump_uploader.cc',
        'crash/cast_crashdump_uploader.h',
        'crash/linux/crash_util.cc',
        'crash/linux/crash_util.h',
        'crash/linux/dummy_minidump_generator.cc',
        'crash/linux/dummy_minidump_generator.h',
        'crash/linux/dump_info.cc',
        'crash/linux/dump_info.h',
        'crash/linux/minidump_generator.h',
        'crash/linux/synchronized_minidump_manager.cc',
        'crash/linux/synchronized_minidump_manager.h',
        'crash/linux/minidump_params.cc',
        'crash/linux/minidump_params.h',
        'crash/linux/minidump_writer.cc',
        'crash/linux/minidump_writer.h',
      ],
    },  # end of target 'cast_crash'
    {
      'target_name': 'cast_crash_client',
      'type': '<(component)',
      'dependencies': [
        'cast_crash',
        '../components/components.gyp:crash_component',
        '../content/content.gyp:content_common',
      ],
      'sources' : [
        # TODO(slan): Move android crash_client here as well.
        'app/linux/cast_crash_reporter_client.cc',
        'app/linux/cast_crash_reporter_client.h',
      ],
    },  # end of target 'cast_crash_client'
    {
      'target_name': 'cast_net',
      'type': '<(component)',
      'sources': [
        'net/connectivity_checker.cc',
        'net/connectivity_checker.h',
        'net/connectivity_checker_impl.cc',
        'net/connectivity_checker_impl.h',
        'net/fake_connectivity_checker.cc',
        'net/fake_connectivity_checker.h',
        'net/net_switches.cc',
        'net/net_switches.h',
        'net/net_util_cast.cc',
        'net/net_util_cast.h',
      ],
      'conditions': [
        ['OS!="android"', {
          'sources': [
            'net/network_change_notifier_factory_cast.cc',
            'net/network_change_notifier_factory_cast.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_shell_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast',
      },
      'actions': [
        {
          'action_name': 'cast_shell_resources',
          'variables': {
            'grit_grd_file': 'app/resources/shell_resources.grd',
            'grit_resource_ids': 'app/resources/resource_ids',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'cast_shell_pak',
      'type': 'none',
      'dependencies': [
        'cast_shell_resources',
        '../content/app/resources/content_resources.gyp:content_resources',
        '../content/app/strings/content_strings.gyp:content_strings',
        '../net/net.gyp:net_resources',
        '../third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '../ui/resources/ui_resources.gyp:ui_resources',
        '../ui/strings/ui_strings.gyp:ui_strings',
      ],
      'actions': [
        {
          'action_name': 'repack_cast_shell_pak',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/chromecast/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
            ],
            'conditions': [
              ['chromecast_branding=="Chrome" and use_chromecast_webui==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/chromecast/app_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/chromecast/cast_webui_resources.pak',
                ],
              }],
            ],
            'pak_output': '<(PRODUCT_DIR)/assets/cast_shell.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
      'conditions': [
        ['chromecast_branding=="Chrome" and use_chromecast_webui==1', {
          'dependencies': [
            'internal/chromecast_resources.gyp:chromecast_app_resources',
            'internal/chromecast_resources.gyp:chromecast_webui_resources',
          ],
        }],
      ],
    },  # end of target 'cast_shell_pak'
    # This target contains all content-embedder implementation that is
    # non-platform-specific.
    {
      'target_name': 'cast_shell_common',
      'type': '<(component)',
      'dependencies': [
        'cast_base',
        'cast_crash_client',
        'cast_net',
        'cast_shell_pak',
        'cast_shell_resources',
        'cast_sys_info',
        'cast_version_header',
        'chromecast_locales.gyp:chromecast_locales_pak',
        'chromecast_locales.gyp:chromecast_settings',
        'media/media.gyp:media_base',
        'media/media.gyp:media_cdm',
        '../base/base.gyp:base',
        '../components/components.gyp:breakpad_host',
        '../components/components.gyp:cdm_renderer',
        '../components/components.gyp:component_metrics_proto',
        '../components/components.gyp:crash_component',
        '../components/components.gyp:devtools_discovery',
        '../components/components.gyp:devtools_http_handler',
        '../components/components.gyp:network_hints_browser',
        '../components/components.gyp:network_hints_renderer',
        '../components/components.gyp:metrics',
        '../components/components.gyp:metrics_gpu',
        '../components/components.gyp:metrics_net',
        '../components/components.gyp:metrics_profiler',
        '../content/content.gyp:content',
        '../content/content.gyp:content_app_both',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'app/cast_main_delegate.cc',
        'app/cast_main_delegate.h',
        'browser/cast_browser_context.cc',
        'browser/cast_browser_context.h',
        'browser/cast_browser_main_parts.cc',
        'browser/cast_browser_main_parts.h',
        'browser/cast_browser_process.cc',
        'browser/cast_browser_process.h',
        'browser/cast_content_browser_client.cc',
        'browser/cast_content_browser_client.h',
        'browser/cast_content_window.cc',
        'browser/cast_content_window.h',
        'browser/cast_download_manager_delegate.cc',
        'browser/cast_download_manager_delegate.h',
        'browser/cast_http_user_agent_settings.cc',
        'browser/cast_http_user_agent_settings.h',
        'browser/cast_net_log.cc',
        'browser/cast_net_log.h',
        'browser/cast_network_delegate.cc',
        'browser/cast_network_delegate.h',
        'browser/cast_permission_manager.cc',
        'browser/cast_permission_manager.h',
        'browser/cast_quota_permission_context.cc',
        'browser/cast_quota_permission_context.h',
        'browser/cast_resource_dispatcher_host_delegate.cc',
        'browser/cast_resource_dispatcher_host_delegate.h',
        'browser/devtools/cast_dev_tools_delegate.cc',
        'browser/devtools/cast_dev_tools_delegate.h',
        'browser/devtools/remote_debugging_server.cc',
        'browser/devtools/remote_debugging_server.h',
        'browser/geolocation/cast_access_token_store.cc',
        'browser/geolocation/cast_access_token_store.h',
        'browser/media/cast_media_client_android.cc',
        'browser/media/cast_media_client_android.h',
        'browser/metrics/cast_metrics_prefs.cc',
        'browser/metrics/cast_metrics_prefs.h',
        'browser/metrics/cast_metrics_service_client.cc',
        'browser/metrics/cast_metrics_service_client.h',
        'browser/metrics/cast_stability_metrics_provider.cc',
        'browser/metrics/cast_stability_metrics_provider.h',
        'browser/metrics/platform_metrics_providers.h',
        'browser/pref_service_helper.cc',
        'browser/pref_service_helper.h',
        'browser/service/cast_service.cc',
        'browser/service/cast_service.h',
        'browser/url_request_context_factory.cc',
        'browser/url_request_context_factory.h',
        'common/cast_content_client.cc',
        'common/cast_content_client.h',
        'common/cast_resource_delegate.cc',
        'common/cast_resource_delegate.h',
        'common/media/cast_messages.h',
        'common/media/cast_message_generator.cc',
        'common/media/cast_message_generator.h',
        'common/platform_client_auth.h',
        'common/pref_names.cc',
        'common/pref_names.h',
        'renderer/cast_content_renderer_client.cc',
        'renderer/cast_content_renderer_client.h',
        'renderer/cast_media_load_deferrer.cc',
        'renderer/cast_media_load_deferrer.h',
        'renderer/cast_render_process_observer.cc',
        'renderer/cast_render_process_observer.h',
        'renderer/key_systems_cast.cc',
        'renderer/key_systems_cast.h',
        'renderer/media/capabilities_message_filter.cc',
        'renderer/media/capabilities_message_filter.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:cast_shell_internal',
          ],
        }, {
          'sources': [
            'browser/cast_content_browser_client_simple.cc',
            'browser/cast_network_delegate_simple.cc',
            'browser/devtools/remote_debugging_server_simple.cc',
            'browser/metrics/platform_metrics_providers_simple.cc',
            'browser/pref_service_helper_simple.cc',
            'common/platform_client_auth_simple.cc',
            'renderer/cast_content_renderer_client_simple.cc',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'browser/service/cast_service_android.cc',
                'browser/service/cast_service_android.h',
              ],
            }, {
              'sources': [
                'browser/media/cast_browser_cdm_factory_simple.cc',
                'browser/service/cast_service_simple.cc',
                'browser/service/cast_service_simple.h',
              ],
            }],
          ],
        }],
        # ExternalMetrics not necessary on Android and (as of this writing) uses
        # non-portable filesystem operations. Also webcrypto is not used on
        # Android either.
        ['OS=="linux"', {
          'sources': [
            'browser/metrics/external_metrics.cc',
            'browser/metrics/external_metrics.h',
            'graphics/cast_screen.cc',
            'graphics/cast_screen.h',
          ],
          'dependencies': [
            '../components/components.gyp:metrics_serialization',
            '../ui/aura/aura.gyp:aura_test_support',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../components/components.gyp:cdm_browser',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_sys_info',
      'type': '<(component)',
      'dependencies': [
        'cast_public_api',
        '../base/base.gyp:base',
      ],
      'sources': [
        'base/cast_sys_info_util.h',
        'base/cast_sys_info_dummy.cc',
        'base/cast_sys_info_dummy.h',
      ],
      'conditions': [
        ['chromecast_branding!="Chrome" and OS!="android"', {
          'sources': [
            'base/cast_sys_info_util_simple.cc',
          ],
        }],
      ],
    },  # end of target 'cast_sys_info'
    {
      'target_name': 'cast_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'inputs': [
            '<(version_path)',
            'base/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chromecast/base/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-e', 'VERSION_FULL="<(version_full)"',
            # CAST_BUILD_INCREMENTAL is taken from buildbot if available;
            # otherwise, a dev string is used.
            '-e', 'CAST_BUILD_INCREMENTAL="<!(echo ${CAST_BUILD_INCREMENTAL:="<!(date +%Y%m%d.%H%M%S)"})"',
            # CAST_BUILD_RELEASE is taken from cast_build_release file if exist;
            # otherwise, a dev string is used.
            '-e', 'CAST_BUILD_RELEASE="<!(if test -f <(cast_build_release); then cat <(cast_build_release); else echo eng.${USER}; fi)"',
            '-e', 'CAST_IS_DEBUG_BUILD=1 if "<(CONFIGURATION_NAME)" == "Debug" or <(cast_is_debug_build) == 1 else 0',
            '-e', 'CAST_PRODUCT_TYPE=<(cast_product_type)',
            'base/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../build/util/version.gypi',
          ],
        },
      ],
    },
  ],  # end of targets

  # Targets for Android receiver.
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'cast_shell_icudata',
          'type': 'none',
          'dependencies': [
            '../third_party/icu/icu.gyp:icudata',
            '../v8/tools/gyp/v8.gyp:v8_external_snapshot',
          ],
          'copies': [{
            'destination': '<(PRODUCT_DIR)/assets',
            'files': [
              '<(PRODUCT_DIR)/icudtl.dat',
              '<(PRODUCT_DIR)/natives_blob.bin',
              '<(PRODUCT_DIR)/snapshot_blob.bin',
            ],
          }],
        },
        {
          'target_name': 'libcast_shell_android',
          'type': 'shared_library',
          'dependencies': [
            'cast_jni_headers',
            'cast_shell_common',
            'cast_shell_icudata',
            'cast_shell_pak',
            'cast_version_header',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_client',
            '../components/components.gyp:breakpad_host',
            '../components/components.gyp:crash_component',
            '../components/components.gyp:external_video_surface',
            '../content/content.gyp:content',
            '../skia/skia.gyp:skia',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gl/gl.gyp:gl',
          ],
          'include_dirs': [
            '../breakpad/src',
          ],
          'sources': [
            'android/cast_jni_registrar.cc',
            'android/cast_jni_registrar.h',
            'android/cast_metrics_helper_android.cc',
            'android/cast_metrics_helper_android.h',
            'android/platform_jni_loader.h',
            'app/android/cast_crash_reporter_client_android.cc',
            'app/android/cast_crash_reporter_client_android.h',
            'app/android/cast_jni_loader.cc',
            'app/android/crash_handler.cc',
            'app/android/crash_handler.h',
            'base/cast_sys_info_android.cc',
            'base/cast_sys_info_android.h',
            'base/chromecast_config_android.cc',
            'base/chromecast_config_android.h',
            'browser/android/cast_window_android.cc',
            'browser/android/cast_window_android.h',
            'browser/android/cast_window_manager.cc',
            'browser/android/cast_window_manager.h',
          ],
          'conditions': [
            ['chromecast_branding=="Chrome"', {
              'dependencies': [
                'internal/chromecast_internal.gyp:cast_shell_android_internal'
              ],
            }, {
              'sources': [
                'android/platform_jni_loader_stub.cc',
              ],
            }]
          ],
        },  # end of target 'libcast_shell_android'
        {
          'target_name': 'cast_base_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
          ],
          'variables': {
            'android_manifest_path': 'android/AndroidManifest.xml',
            'java_in_dir': 'base/java',
          },
          'includes': ['../build/java.gypi'],
        },  # end of target 'cast_base_java'
        {
          'target_name': 'cast_shell_java',
          'type': 'none',
          'dependencies': [
            '<(android_support_v13_target)',
            'cast_base_java',
            'cast_shell_manifest',
            '../base/base.gyp:base_java',
            '../components/components.gyp:external_video_surface_java',
            '../content/content.gyp:content_java',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net_java',
            '../ui/android/ui_android.gyp:ui_java',
          ],
          'variables': {
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/cast_shell_manifest/AndroidManifest.xml',
            'has_java_resources': 1,
            'java_in_dir': 'browser/android/apk',
            'resource_dir': 'browser/android/apk/res',
            'R_package': 'org.chromium.chromecast.shell',
          },
          'includes': ['../build/java.gypi'],
        },  # end of target 'cast_shell_java'
        {
          'target_name': 'cast_shell_manifest',
          'type': 'none',
          'variables': {
            'jinja_inputs': ['browser/android/apk/AndroidManifest.xml.jinja2'],
            'jinja_output': '<(SHARED_INTERMEDIATE_DIR)/cast_shell_manifest/AndroidManifest.xml',
          },
          'includes': [ '../build/android/jinja_template.gypi' ],
        },
        {
          'target_name': 'cast_shell_apk',
          'type': 'none',
          'dependencies': [
            'cast_shell_java',
            'libcast_shell_android',
          ],
          'variables': {
            'apk_name': 'CastShell',
            'manifest_package_name': 'org.chromium.chromecast.shell',
            # Note(gunsch): there are no Java files in the android/ directory.
            # Unfortunately, the java_apk.gypi target rigidly insists on having
            # a java_in_dir directory, but complains about duplicate classes
            # from the common cast_shell_java target (shared with internal APK)
            # if the actual Java path is used.
            # This will hopefully be removable after the great GN migration.
            'java_in_dir': 'android',
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/cast_shell_manifest/AndroidManifest.xml',
            'package_name': 'org.chromium.chromecast.shell',
            'native_lib_target': 'libcast_shell_android',
            'asset_location': '<(PRODUCT_DIR)/assets',
            'additional_input_paths': ['<(PRODUCT_DIR)/assets/cast_shell.pak'],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          'target_name': 'cast_jni_headers',
          'type': 'none',
          'sources': [
            'base/java/src/org/chromium/chromecast/base/ChromecastConfigAndroid.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastCrashHandler.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastMetricsHelper.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastSysInfoAndroid.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastWindowAndroid.java',
            'browser/android/apk/src/org/chromium/chromecast/shell/CastWindowManager.java',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/chromecast',
            ],
          },
          'variables': {
            'jni_gen_package': 'chromecast',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],  # end of targets
    }, {  # OS != "android"
      'targets': [
        {
          'target_name': 'cast_shell_media',
          'type': '<(component)',
          'dependencies': [
            'media/media.gyp:cast_media',
            '../content/content.gyp:content',
            '../ipc/ipc.gyp:ipc',
            '../media/media.gyp:media',
          ],
          'sources': [
            'browser/media/cast_browser_cdm_factory.cc',
            'browser/media/cast_browser_cdm_factory.h',
            'browser/media/cma_message_filter_host.cc',
            'browser/media/cma_message_filter_host.h',
            'browser/media/media_pipeline_host.cc',
            'browser/media/media_pipeline_host.h',
            'common/media/cma_ipc_common.h',
            'common/media/cma_messages.h',
            'common/media/cma_message_generator.cc',
            'common/media/cma_message_generator.h',
            'common/media/cma_param_traits.cc',
            'common/media/cma_param_traits.h',
            'common/media/shared_memory_chunk.cc',
            'common/media/shared_memory_chunk.h',
            'renderer/media/audio_pipeline_proxy.cc',
            'renderer/media/audio_pipeline_proxy.h',
            'renderer/media/chromecast_media_renderer_factory.cc',
            'renderer/media/chromecast_media_renderer_factory.h',
            'renderer/media/cma_message_filter_proxy.cc',
            'renderer/media/cma_message_filter_proxy.h',
            'renderer/media/media_channel_proxy.cc',
            'renderer/media/media_channel_proxy.h',
            'renderer/media/media_pipeline_proxy.cc',
            'renderer/media/media_pipeline_proxy.h',
            'renderer/media/video_pipeline_proxy.cc',
            'renderer/media/video_pipeline_proxy.h',
          ],
        },  # end of target 'cast_shell_media'
        # This target contains all of the primary code of |cast_shell|, except
        # for |main|. This allows end-to-end tests using |cast_shell|.
        # This also includes all targets that cannot be built on Android.
        {
          'target_name': 'cast_shell_core',
          'type': '<(component)',
          'dependencies': [
            'cast_shell_media',
            'cast_shell_common',
            'media/media.gyp:cast_media',
          ],
          'conditions': [
            ['ozone_platform_egltest==1', {
              'dependencies': [
                '../ui/ozone/ozone.gyp:eglplatform_shim_x11',
              ],
            }],
          ],
        },
        {
          'target_name': 'cast_shell',
          'type': 'executable',
          'dependencies': [
            'cast_shell_core',
          ],
          'sources': [
            'app/cast_main.cc',
          ],
          'ldflags': [
            # Allow  OEMs to override default libraries that are shipped with
            # cast receiver package by installed OEM-specific libraries in
            # /oem_cast_shlib.
            '-Wl,-rpath=/oem_cast_shlib',
            # TODO(dougsteed): remove when Chromecast moves to boringssl.
            # Allow the cast shell to find the NSS module in the same
            # directory.
            '-Wl,-rpath=\$$ORIGIN'
          ],
        },
      ],  # end of targets
    }],
    ['enable_default_cast_graphics==1', {
      'targets': [
        {
          'target_name': 'libcast_graphics_1.0',
          'type': 'shared_library',
          'dependencies': [
            'cast_public_api'
          ],
          'sources': [
            'graphics/cast_egl_platform_default.cc',
            'graphics/graphics_properties_default.cc',
            'graphics/osd_plane_default.cc'
          ],
        }
      ]
    }],
  ],  # end of conditions
}
