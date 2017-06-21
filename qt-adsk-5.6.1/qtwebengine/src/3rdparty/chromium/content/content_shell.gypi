# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'content_shell_product_name': 'Content Shell',
    # The "19" is so that sites that sniff for version think that this is
    # something reasonably current; the "77.34.5" is a hint that this isn't a
    # standard Chrome.
    'content_shell_version': '19.77.34.5',
    'conditions': [
      ['OS=="linux"', {
       'use_custom_freetype%': 1,
      }, {
       'use_custom_freetype%': 0,
      }],
    ],
  },
  'targets': [
    {
      # GN version: //content/shell:content_shell_lib
      'target_name': 'content_shell_lib',
      'type': 'static_library',
      'defines': ['CONTENT_SHELL_VERSION="<(content_shell_version)"'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'app/resources/content_resources.gyp:content_resources',
        'app/strings/content_strings.gyp:content_strings',
        'content.gyp:content_app_both',
        'content.gyp:content_browser',
        'content.gyp:content_common',
        'content.gyp:content_gpu',
        'content.gyp:content_plugin',
        'content.gyp:content_ppapi_plugin',
        'content.gyp:content_renderer',
        'content.gyp:content_resources',
        'content.gyp:content_utility',
        'content_shell_resources',
        'copy_test_netscape_plugin',
        'layouttest_support_content',
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../cc/blink/cc_blink.gyp:cc_blink',
        '../cc/cc.gyp:cc',
        '../components/components.gyp:crash_component_breakpad_mac_to_be_deleted',
        '../components/components.gyp:devtools_discovery',
        '../components/components.gyp:devtools_http_handler',
        '../components/components.gyp:web_cache_renderer',
        '../components/components.gyp:plugins_renderer',
        '../components/test_runner/test_runner.gyp:test_runner',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '../gin/gin.gyp:gin',
        '../gpu/gpu.gyp:gpu',
        '../ipc/ipc.gyp:ipc',
        '../media/blink/media_blink.gyp:media_blink',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../skia/skia.gyp:skia',
        '../storage/storage_browser.gyp:storage',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../third_party/WebKit/public/blink.gyp:blink_test_support',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/base/ime/ui_base_ime.gyp:ui_base_ime',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/events/events.gyp:events_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '../ui/gl/gl.gyp:gl',
        '../url/url.gyp:url_lib',
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'shell/android/shell_jni_registrar.cc',
        'shell/android/shell_jni_registrar.h',
        'shell/android/shell_manager.cc',
        'shell/android/shell_manager.h',
        'shell/app/blink_test_platform_support.h',
        'shell/app/blink_test_platform_support_android.cc',
        'shell/app/blink_test_platform_support_linux.cc',
        'shell/app/blink_test_platform_support_mac.mm',
        'shell/app/blink_test_platform_support_win.cc',
        'shell/app/paths_mac.h',
        'shell/app/paths_mac.mm',
        'shell/app/shell_crash_reporter_client.cc',
        'shell/app/shell_crash_reporter_client.h',
        'shell/app/shell_main_delegate.cc',
        'shell/app/shell_main_delegate.h',
        'shell/app/shell_main_delegate_mac.h',
        'shell/app/shell_main_delegate_mac.mm',
        'shell/browser/blink_test_controller.cc',
        'shell/browser/blink_test_controller.h',
        'shell/browser/ipc_echo_message_filter.cc',
        'shell/browser/ipc_echo_message_filter.h',
        'shell/browser/layout_test/layout_test_android.cc',
        'shell/browser/layout_test/layout_test_android.h',
        'shell/browser/layout_test/layout_test_bluetooth_adapter_provider.cc',
        'shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h',
        'shell/browser/layout_test/layout_test_browser_context.cc',
        'shell/browser/layout_test/layout_test_browser_context.h',
        'shell/browser/layout_test/layout_test_browser_main.cc',
        'shell/browser/layout_test/layout_test_browser_main.h',
        'shell/browser/layout_test/layout_test_browser_main_parts.cc',
        'shell/browser/layout_test/layout_test_browser_main_parts.h',
        'shell/browser/layout_test/layout_test_content_browser_client.cc',
        'shell/browser/layout_test/layout_test_content_browser_client.h',
        'shell/browser/layout_test/layout_test_devtools_frontend.cc',
        'shell/browser/layout_test/layout_test_devtools_frontend.h',
        'shell/browser/layout_test/layout_test_download_manager_delegate.cc',
        'shell/browser/layout_test/layout_test_download_manager_delegate.h',
        'shell/browser/layout_test/layout_test_javascript_dialog_manager.cc',
        'shell/browser/layout_test/layout_test_javascript_dialog_manager.h',
        'shell/browser/layout_test/layout_test_message_filter.cc',
        'shell/browser/layout_test/layout_test_message_filter.h',
        'shell/browser/layout_test/layout_test_navigator_connect_service_factory.cc',
        'shell/browser/layout_test/layout_test_navigator_connect_service_factory.h',
        'shell/browser/layout_test/layout_test_notification_manager.cc',
        'shell/browser/layout_test/layout_test_notification_manager.h',
        'shell/browser/layout_test/layout_test_permission_manager.cc',
        'shell/browser/layout_test/layout_test_permission_manager.h',
        'shell/browser/layout_test/layout_test_push_messaging_service.cc',
        'shell/browser/layout_test/layout_test_push_messaging_service.h',
        'shell/browser/layout_test/layout_test_resource_dispatcher_host_delegate.cc',
        'shell/browser/layout_test/layout_test_resource_dispatcher_host_delegate.h',
        'shell/browser/layout_test/layout_test_url_request_context_getter.cc',
        'shell/browser/layout_test/layout_test_url_request_context_getter.h',
        'shell/browser/notify_done_forwarder.cc',
        'shell/browser/notify_done_forwarder.h',
        'shell/browser/shell.cc',
        'shell/browser/shell.h',
        'shell/browser/shell_access_token_store.cc',
        'shell/browser/shell_access_token_store.h',
        'shell/browser/shell_android.cc',
        'shell/browser/shell_application_mac.h',
        'shell/browser/shell_application_mac.mm',
        'shell/browser/shell_aura.cc',
        'shell/browser/shell_browser_context.cc',
        'shell/browser/shell_browser_context.h',
        'shell/browser/shell_browser_main.cc',
        'shell/browser/shell_browser_main.h',
        'shell/browser/shell_browser_main_parts.cc',
        'shell/browser/shell_browser_main_parts.h',
        'shell/browser/shell_browser_main_parts_mac.mm',
        'shell/browser/shell_content_browser_client.cc',
        'shell/browser/shell_content_browser_client.h',
        'shell/browser/shell_devtools_frontend.cc',
        'shell/browser/shell_devtools_frontend.h',
        'shell/browser/shell_devtools_manager_delegate.cc',
        'shell/browser/shell_devtools_manager_delegate.h',
        'shell/browser/shell_download_manager_delegate.cc',
        'shell/browser/shell_download_manager_delegate.h',
        'shell/browser/shell_javascript_dialog.h',
        'shell/browser/shell_javascript_dialog_mac.mm',
        'shell/browser/shell_javascript_dialog_manager.cc',
        'shell/browser/shell_javascript_dialog_manager.h',
        'shell/browser/shell_javascript_dialog_win.cc',
        'shell/browser/shell_login_dialog.cc',
        'shell/browser/shell_login_dialog.h',
        'shell/browser/shell_login_dialog_mac.mm',
        'shell/browser/shell_mac.mm',
        'shell/browser/shell_mojo_test_utils_android.cc',
        'shell/browser/shell_mojo_test_utils_android.h',
        'shell/browser/shell_net_log.cc',
        'shell/browser/shell_net_log.h',
        'shell/browser/shell_network_delegate.cc',
        'shell/browser/shell_network_delegate.h',
        'shell/browser/shell_permission_manager.cc',
        'shell/browser/shell_permission_manager.h',
        'shell/browser/shell_platform_data_aura.cc',
        'shell/browser/shell_platform_data_aura.h',
        'shell/browser/shell_plugin_service_filter.cc',
        'shell/browser/shell_plugin_service_filter.h',
        'shell/browser/shell_quota_permission_context.cc',
        'shell/browser/shell_quota_permission_context.h',
        'shell/browser/shell_resource_dispatcher_host_delegate.cc',
        'shell/browser/shell_resource_dispatcher_host_delegate.h',
        'shell/browser/shell_speech_recognition_manager_delegate.cc',
        'shell/browser/shell_speech_recognition_manager_delegate.h',
        'shell/browser/shell_url_request_context_getter.cc',
        'shell/browser/shell_url_request_context_getter.h',
        'shell/browser/shell_views.cc',
        'shell/browser/shell_web_contents_view_delegate.h',
        'shell/browser/shell_web_contents_view_delegate_android.cc',
        'shell/browser/shell_web_contents_view_delegate_creator.h',
        'shell/browser/shell_web_contents_view_delegate_mac.mm',
        'shell/browser/shell_web_contents_view_delegate_win.cc',
        'shell/common/layout_test/layout_test_messages.cc',
        'shell/common/layout_test/layout_test_messages.h',
        'shell/common/leak_detection_result.h',
        'shell/common/shell_content_client.cc',
        'shell/common/shell_content_client.h',
        'shell/common/shell_messages.cc',
        'shell/common/shell_messages.h',
        'shell/common/shell_switches.cc',
        'shell/common/shell_switches.h',
        'shell/common/shell_test_configuration.cc',
        'shell/common/shell_test_configuration.h',
        'shell/common/v8_breakpad_support_win.cc',
        'shell/common/v8_breakpad_support_win.h',
        'shell/renderer/ipc_echo.cc',
        'shell/renderer/ipc_echo.h',
        'shell/renderer/layout_test/blink_test_helpers.cc',
        'shell/renderer/layout_test/blink_test_helpers.h',
        'shell/renderer/layout_test/blink_test_runner.cc',
        'shell/renderer/layout_test/blink_test_runner.h',
        'shell/renderer/layout_test/gc_controller.cc',
        'shell/renderer/layout_test/gc_controller.h',
        'shell/renderer/layout_test/layout_test_content_renderer_client.cc',
        'shell/renderer/layout_test/layout_test_content_renderer_client.h',
        'shell/renderer/layout_test/layout_test_render_frame_observer.cc',
        'shell/renderer/layout_test/layout_test_render_frame_observer.h',
        'shell/renderer/layout_test/layout_test_render_process_observer.cc',
        'shell/renderer/layout_test/layout_test_render_process_observer.h',
        'shell/renderer/layout_test/leak_detector.cc',
        'shell/renderer/layout_test/leak_detector.h',
        'shell/renderer/layout_test/test_media_stream_renderer_factory.cc',
        'shell/renderer/layout_test/test_media_stream_renderer_factory.h',
        'shell/renderer/layout_test/test_video_frame_provider.cc',
        'shell/renderer/layout_test/test_video_frame_provider.h',
        'shell/renderer/shell_content_renderer_client.cc',
        'shell/renderer/shell_content_renderer_client.h',
        'shell/renderer/shell_render_view_observer.cc',
        'shell/renderer/shell_render_view_observer.h',
        'shell/utility/shell_content_utility_client.cc',
        'shell/utility/shell_content_utility_client.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/content/app/strings',
          ],
          'dependencies': [
            '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],  # OS=="win"
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
          ],
        }],
        ['use_x11 == 1', {
          'dependencies': [
            '../ui/events/devices/events_devices.gyp:events_devices',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../mojo/mojo_base.gyp:libmojo_system_java',
            'content_shell_jni_headers',
          ],
          'dependencies!': [
            'copy_test_netscape_plugin',
          ],
        }],  # OS=="android"
        ['os_posix == 1 and OS != "mac"', {
          'dependencies': [
            '../components/components.gyp:breakpad_host',
          ],
        }],
        ['(OS=="linux" or OS=="android") and use_allocator!="none"', {
          'dependencies': [
            # This is needed by content/app/content_main_runner.cc
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:aura',
            '../ui/aura/aura.gyp:aura_test_support',
            '../ui/events/events.gyp:events',
            '../ui/strings/ui_strings.gyp:ui_strings',
            '../ui/wm/wm.gyp:wm',
          ],
          'conditions': [
            ['toolkit_views==1', {
              'dependencies': [
                '../ui/resources/ui_resources.gyp:ui_resources',
                '../ui/views/controls/webview/webview.gyp:webview',
                '../ui/views/views.gyp:views',
                '../ui/views/views.gyp:views_test_support',
              ],
              'sources/': [
                ['exclude', 'shell/browser/shell_aura.cc'],
              ],
            }, {
              'sources/': [
                ['exclude', 'shell/browser/shell_views.cc'],
              ],
            }],
          ],
        }, {
          'sources/': [
            ['exclude', 'shell/browser/shell_aura.cc'],
            ['exclude', 'shell/browser/shell_views.cc'],
          ],
        }],  # use_aura==1
        # The test plugin relies on X11.
        ['OS=="linux" and use_x11==0', {
          'dependencies!': [
            'copy_test_netscape_plugin',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
            '../ui/wm/wm.gyp:wm_test_support',
           ],
        }], # chromeos==1
        ['use_custom_freetype==1', {
          'dependencies': [
             '../third_party/freetype2/freetype2.gyp:freetype2',
          ],
        }],
        ['enable_plugins==0', {
          'sources!': [
            'shell/browser/shell_plugin_service_filter.cc',
            'shell/browser/shell_plugin_service_filter.h',
          ],
        }]
      ],
    },
    {
      # GN version: //content/shell:resources
      'target_name': 'content_shell_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'dependencies': [
        '../components/test_runner/test_runner.gyp:resources',
      ],
      'actions': [
        {
          'action_name': 'generate_content_shell_resources',
          'variables': {
            'grit_grd_file': 'shell/shell_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak'
          ],
        },
      ],
    },
    {
      # We build a minimal set of resources so WebKit in content_shell has
      # access to necessary resources.
      # GN version: //content/shell:pak
      'target_name': 'content_shell_pak',
      'type': 'none',
      'dependencies': [
        'app/resources/content_resources.gyp:content_resources',
        'app/strings/content_strings.gyp:content_strings',
        'browser/tracing/tracing_resources.gyp:tracing_resources',
        'content.gyp:content_resources',
        'content_shell_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        '<(DEPTH)/ui/strings/ui_strings.gyp:ui_strings',
      ],
      'conditions': [
        ['OS!="android"', {
          'dependencies': [
            'browser/devtools/devtools_resources.gyp:devtools_resources',
          ],
        }],
        ['OS=="android"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(PRODUCT_DIR)/content_shell/assets/content_shell.pak'
              ],
            },
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '<(DEPTH)/ui/views/resources/views_resources.gyp:views_resources'
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'repack_content_shell_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
            ],
            'conditions': [
              ['toolkit_views==1', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/views/resources/views_resources_100_percent.pak',
                ],
              }],
              ['OS!="android"', {
                'pak_inputs': ['<(SHARED_INTERMEDIATE_DIR)/blink/devtools_resources.pak',],
                'pak_output': '<(PRODUCT_DIR)/content_shell.pak',
              }, {
                'pak_output': '<(PRODUCT_DIR)/content_shell/assets/content_shell.pak',
              }],
            ],
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
    },
    {
      # GN version: //content/shell:content_shell
      'target_name': 'content_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_shell_lib',
        'content_shell_pak',
        '../third_party/mesa/mesa.gyp:osmesa',
        '../tools/imagediff/image_diff.gyp:image_diff',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'shell/app/shell_main.cc',
      ],
      'mac_bundle_resources': [
        'shell/app/app.icns',
        'shell/app/app-Info.plist',
      ],
      # TODO(mark): Come up with a fancier way to do this.  It should only
      # be necessary to list app-Info.plist once, not the three times it is
      # listed here.
      'mac_bundle_resources!': [
        'shell/app/app-Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'shell/app/app-Info.plist',
      },
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'shell/app/shell.exe.manifest',
          ],
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'shell/app/shell.rc',
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],  # OS=="win"
        ['OS == "win"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],  # OS=="win"
        ['OS=="win" and asan==0', {
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
            },
          },
        }],  # OS=="win" and asan==0
        ['OS=="mac"', {
          'product_name': '<(content_shell_product_name)',
          'dependencies!': [
            'content_shell_lib',
          ],
          'dependencies': [
            'content_shell_framework',
            'content_shell_helper_app',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(content_shell_product_name).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(content_shell_product_name) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(content_shell_product_name) Framework.framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(content_shell_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--scm=1',
                         '--version=<(content_shell_version)'],
            },
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # Content Shell Helper EH.app and Content Shell Helper NP.app are
              # created from Content Shell Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                '<(content_shell_product_name)',
              ],
            },
            {
              # Make sure there isn't any Objective-C in the shell's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
        }],  # OS=="mac"
        ['OS=="android"', {
          'dependencies': [
            '../tools/imagediff/image_diff.gyp:image_diff#host',
          ],
          'dependencies!': [
            '../tools/imagediff/image_diff.gyp:image_diff',
          ],
        }],  # OS=="android"
      ],
    },
    {
      'target_name': 'content_shell_builder',
      'type': 'none',
      'dependencies': [
        'content_shell',
      ],
    },

    {
      'target_name': 'test_netscape_plugin',
      'type': 'loadable_module',
      'sources': [
        'shell/tools/plugin/PluginObject.cpp',
        'shell/tools/plugin/PluginObject.h',
        'shell/tools/plugin/PluginObjectMac.mm',
        'shell/tools/plugin/PluginTest.cpp',
        'shell/tools/plugin/PluginTest.h',
        'shell/tools/plugin/TestObject.cpp',
        'shell/tools/plugin/Tests/DocumentOpenInDestroyStream.cpp',
        'shell/tools/plugin/Tests/EvaluateJSAfterRemovingPluginElement.cpp',
        'shell/tools/plugin/Tests/FormValue.cpp',
        'shell/tools/plugin/Tests/GetURLNotifyWithURLThatFailsToLoad.cpp',
        'shell/tools/plugin/Tests/GetURLWithJavaScriptURL.cpp',
        'shell/tools/plugin/Tests/GetURLWithJavaScriptURLDestroyingPlugin.cpp',
        'shell/tools/plugin/Tests/GetUserAgentWithNullNPPFromNPPNew.cpp',
        'shell/tools/plugin/Tests/LeakWindowScriptableObject.cpp',
        'shell/tools/plugin/Tests/LogNPPSetWindow.cpp',
        'shell/tools/plugin/Tests/NPDeallocateCalledBeforeNPShutdown.cpp',
        'shell/tools/plugin/Tests/NPPNewFails.cpp',
        'shell/tools/plugin/Tests/NPRuntimeCallsWithNullNPP.cpp',
        'shell/tools/plugin/Tests/NPRuntimeObjectFromDestroyedPlugin.cpp',
        'shell/tools/plugin/Tests/NPRuntimeRemoveProperty.cpp',
        'shell/tools/plugin/Tests/NullNPPGetValuePointer.cpp',
        'shell/tools/plugin/Tests/PassDifferentNPPStruct.cpp',
        'shell/tools/plugin/Tests/PluginScriptableNPObjectInvokeDefault.cpp',
        'shell/tools/plugin/Tests/PluginScriptableObjectOverridesAllProperties.cpp',
        'shell/tools/plugin/main.cpp',
        'shell/tools/plugin/test_object.h',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/content/shell/tools/plugin/',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/npapi/npapi.gyp:npapi',
      ],
      'conditions': [
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_extension': 'plugin',
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ]
          },
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
            'INFOPLIST_FILE': 'shell/tools/plugin/mac/Info.plist',
          },
        }],
        ['os_posix == 1 and OS != "mac"', {
          'cflags': [
            '-fvisibility=default',
          ],
        }],
        ['use_x11 == 1', {
          'dependencies': [ '../build/linux/system.gyp:x11' ],
        }],
        ['OS=="win"', {
          'conditions': [
            ['MSVS_VERSION < "2015"', {
              'defines': [
                # This seems like a hack, but this is what Safari Win does.
                # Luckily it is no longer needed/allowed with VS 2015.
                'snprintf=_snprintf',
              ],
            }],
          ],
          'sources': [
            'shell/tools/plugin/win/TestNetscapePlugin.def',
            'shell/tools/plugin/win/TestNetscapePlugin.rc',
          ],
          # The .rc file requires that the name of the dll is np_test_netscape_plugin.dll.
          'product_name': 'np_test_netscape_plugin',
          # Disable c4267 warnings until we fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],
      ],
    },
    {
      'target_name': 'copy_test_netscape_plugin',
      'type': 'none',
      'dependencies': [
        'test_netscape_plugin',
      ],
      'conditions': [
        ['OS=="win"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)/plugins',
            'files': ['<(PRODUCT_DIR)/np_test_netscape_plugin.dll'],
          }],
        }],
        ['OS=="mac"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)/plugins/',
            'files': ['<(PRODUCT_DIR)/test_netscape_plugin.plugin/'],
          }],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)/plugins',
            'files': ['<(PRODUCT_DIR)/libtest_netscape_plugin.so'],
          }],
        }],
      ],
    }
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'content_shell_framework',
          'type': 'shared_library',
          'product_name': '<(content_shell_product_name) Framework',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            'shell/app/English.lproj/HttpAuth.xib',
            'shell/app/English.lproj/MainMenu.xib',
            '<(PRODUCT_DIR)/content_shell.pak',
            'shell/app/framework-Info.plist',
          ],
          'mac_bundle_resources!': [
            'shell/app/framework-Info.plist',
          ],
          'xcode_settings': {
            # The framework is placed within the .app's Framework
            # directory.  DYLIB_INSTALL_NAME_BASE and
            # LD_DYLIB_INSTALL_NAME affect -install_name.
            'DYLIB_INSTALL_NAME_BASE':
                '@executable_path/../Frameworks',
            # See /build/mac/copy_framework_unversioned.sh for
            # information on LD_DYLIB_INSTALL_NAME.
            'LD_DYLIB_INSTALL_NAME':
                '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',

            'INFOPLIST_FILE': 'shell/app/framework-Info.plist',
          },
          'dependencies': [
            'content_shell_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/app/shell_content_main.cc',
            'shell/app/shell_content_main.h',
          ],
          'postbuilds': [
            {
              # Modify the Info.plist as needed.  The script explains why
              # this is needed.  This is also done in the chrome target.
              # The framework needs the Breakpad keys if this feature is
              # enabled.  It does not need the Keystone keys; these always
              # come from the outer application bundle.  The framework
              # doesn't currently use the SCM keys for anything,
              # but this seems like a really good place to store them.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=1',
                         '--keystone=0',
                         '--scm=1',
                         '--version=<(content_shell_version)',
                         '--branding=<(content_shell_product_name)'],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
              'files': [
                '<(PRODUCT_DIR)/crash_inspector',
                '<(PRODUCT_DIR)/crash_report_sender.app'
              ],
            },
          ],
          'conditions': [
            ['enable_webrtc==1', {
              'variables': {
                'libpeer_target_type%': 'static_library',
              },
              'conditions': [
                ['libpeer_target_type!="static_library"', {
                  'copies': [{
                   'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
                   'files': [
                      '<(PRODUCT_DIR)/libpeerconnection.so',
                    ],
                  }],
                }],
              ],
            }],
            ['icu_use_data_file_flag==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
            ['v8_use_external_startup_data==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        },  # target content_shell_framework
        {
          'target_name': 'content_shell_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(content_shell_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'content_shell_framework',
          ],
          'sources': [
            'shell/app/shell_main.cc',
            'shell/app/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'shell/app/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'shell/app/helper-Info.plist',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # content_shell_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Frameworks/<(content_shell_product_name) Framework.framework/<(content_shell_product_name) Framework',
                '@executable_path/../../../<(content_shell_product_name) Framework.framework/<(content_shell_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0',
                         '--version=<(content_shell_version)'],
            },
            {
              # Make sure there isn't any Objective-C in the helper app's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
          'conditions': [
            ['component=="shared_library"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [
                  # Get back from Content Shell.app/Contents/Frameworks/
                  #                                 Helper.app/Contents/MacOS
                  '@loader_path/../../../../../..',
                ],
              },
            }],
          ],
        },  # target content_shell_helper_app
      ],
    }],  # OS=="mac"
    ['OS=="android"', {
      'targets': [
        {
          # TODO(jrg): Update this action and other jni generators to only
          # require specifying the java directory and generate the rest.
          'target_name': 'content_shell_jni_headers',
          'type': 'none',
          'sources': [
            'shell/android/java/src/org/chromium/content_shell/ShellLayoutTestUtils.java',
            'shell/android/java/src/org/chromium/content_shell/ShellMojoTestUtils.java',
            'shell/android/java/src/org/chromium/content_shell/ShellManager.java',
            'shell/android/java/src/org/chromium/content_shell/Shell.java',
          ],
          'variables': {
            'jni_gen_package': 'content/shell',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libcontent_shell_content_view',
          'type': 'shared_library',
          'dependencies': [
            'content_shell_jni_headers',
            'content_shell_lib',
            'content_shell_pak',
            # Skia is necessary to ensure the dependencies needed by
            # WebContents are included.
            '../skia/skia.gyp:skia',
            '<(DEPTH)/media/media.gyp:player_android',
          ],
          'sources': [
            'shell/android/shell_library_loader.cc',
          ],
        },
        {
          'target_name': 'content_shell_java',
          'type': 'none',
          'dependencies': [
            'content.gyp:content_java',
          ],
          'variables': {
            'java_in_dir': '../content/shell/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.content_shell',
            'R_package_relpath': 'org/chromium/content_shell',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # content_shell_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # content_shell_apk_java. Dependents of content_shell_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument content_shell_apk.
          'target_name': 'content_shell_apk_java',
          'type': 'none',
          'dependencies': [
            'content_shell_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },
        {
          # GN version: //content/shell/android:content_shell_manifest
          'target_name': 'content_shell_manifest',
          'type': 'none',
          'variables': {
            'jinja_inputs': ['shell/android/shell_apk/AndroidManifest.xml.jinja2'],
            'jinja_output': '<(SHARED_INTERMEDIATE_DIR)/content_shell_manifest/AndroidManifest.xml',
          },
          'includes': [ '../build/android/jinja_template.gypi' ],
        },
        {
          # GN version: //content/shell/android:content_shell_apk
          'target_name': 'content_shell_apk',
          'type': 'none',
          'dependencies': [
            'content.gyp:content_icudata',
            'content.gyp:content_java',
            'content.gyp:content_v8_external_data',
            'content_java_test_support',
            'content_shell_java',
            'libcontent_shell_content_view',
            '../base/base.gyp:base_java',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net_java',
            '../third_party/mesa/mesa.gyp:osmesa_in_lib_dir',
            '../tools/android/forwarder/forwarder.gyp:forwarder',
            '../tools/imagediff/image_diff.gyp:image_diff#host',
            '../ui/android/ui_android.gyp:ui_java',
          ],
          'variables': {
            'apk_name': 'ContentShell',
            'manifest_package_name': 'org.chromium.content_shell_apk',
            'android_manifest_path': '<(SHARED_INTERMEDIATE_DIR)/content_shell_manifest/AndroidManifest.xml',
            'java_in_dir': 'shell/android/shell_apk',
            'resource_dir': 'shell/android/shell_apk/res',
            'native_lib_target': 'libcontent_shell_content_view',
            'additional_input_paths': ['<(PRODUCT_DIR)/content_shell/assets/content_shell.pak'],
            'asset_location': '<(PRODUCT_DIR)/content_shell/assets',
            'extra_native_libs': ['<(SHARED_LIB_DIR)/libosmesa.so'],
            'conditions': [
              ['icu_use_data_file_flag==1', {
                'additional_input_paths': [
                  '<(PRODUCT_DIR)/icudtl.dat',
                ],
              }],
              ['v8_use_external_startup_data==1', {
                'additional_input_paths': [
                  '<(PRODUCT_DIR)/natives_blob.bin',
                  '<(PRODUCT_DIR)/snapshot_blob.bin',
                ],
              }],
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],
    }],  # OS=="android"
    ['OS=="win"', {
      'targets': [
        {
          # GN version: //content/shell:crash_service
          'target_name': 'content_shell_crash_service',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../components/components.gyp:breakpad_crash_service',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/tools/content_shell_crash_service.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
        },
      ],
    }],  # OS=="win"
    ['OS=="win" and fastbuild==0 and target_arch=="ia32" and syzyasan==1', {
      'variables': {
        'dest_dir': '<(PRODUCT_DIR)/syzygy',
      },
      'targets': [
        {
          'target_name': 'content_shell_syzyasan',
          'type': 'none',
          'sources' : [],
          # Instrument content_shell with SyzyAsan.
          'actions': [
            {
              'action_name': 'Instrument content_shell with SyzyAsan',
              'inputs': [
                '<(PRODUCT_DIR)/content_shell.exe',
              ],
              'outputs': [
                '<(dest_dir)/content_shell.exe',
                '<(dest_dir)/content_shell.exe.pdb',
              ],
              'action': [
                'python',
                '<(DEPTH)/chrome/tools/build/win/syzygy/instrument.py',
                '--mode', 'asan',
                '--input_executable', '<(PRODUCT_DIR)/content_shell.exe',
                '--input_symbol', '<(PRODUCT_DIR)/content_shell.exe.pdb',
                '--destination_dir', '<(dest_dir)',
              ],
            },
          ],
        },
      ],
    }],  # OS=="win" and fastbuild==0 and target_arch=="ia32"
  ]
}
