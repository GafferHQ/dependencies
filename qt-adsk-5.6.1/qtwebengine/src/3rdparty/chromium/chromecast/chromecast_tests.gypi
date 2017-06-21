# Copyright (c) 2014 Google Inc. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1
  },
  'targets': [
    {
      'target_name': 'cast_base_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'chromecast.gyp:cast_base',
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'base/error_codes_unittest.cc',
        'base/path_utils_unittest.cc',
        'base/process_utils_unittest.cc',
        'base/serializers_unittest.cc',
      ],
    },  # end of cast_base_unittests
    {
      'target_name': 'cast_crash_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'chromecast.gyp:cast_crash',
        '../base/base.gyp:run_all_unittests',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../breakpad/src',
      ],
      'sources': [
        'crash/cast_crashdump_uploader_unittest.cc',
        'crash/linux/dummy_minidump_generator_unittest.cc',
        'crash/linux/dump_info_unittest.cc',
        'crash/linux/synchronized_minidump_manager_unittest.cc',
        'crash/linux/minidump_writer_unittest.cc',
      ],
    },  # end of cast_crash_unittests
    {
      'target_name': 'cast_tests',
      'type': 'none',
      'dependencies': [
        'cast_test_generator',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:cast_tests_internal',
          ],
        }],
      ],
    },
    # This target only depends on targets that generate test binaries.
    {
      'target_name': 'cast_test_generator',
      'type': 'none',
      'dependencies': [
        'cast_base_unittests',
        'cast_crash_unittests',
        '../base/base.gyp:base_unittests',
        '../content/content_shell_and_tests.gyp:content_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../ipc/ipc.gyp:ipc_tests',
        '../jingle/jingle.gyp:jingle_unittests',
        '../media/media.gyp:media_unittests',
        '../media/midi/midi.gyp:midi_unittests',
        '../net/net.gyp:net_unittests',
        '../sandbox/sandbox.gyp:sandbox_linux_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:sync_unit_tests',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
        '../ui/base/ui_base_tests.gyp:ui_base_unittests',
        '../url/url.gyp:url_unittests',
      ],
      'conditions': [
        ['target_arch=="arm" and OS!="android"', {
          'variables': {
            'filters': [
              # Run net_unittests first to avoid random failures due to slow python startup
              # KeygenHandlerTest.SmokeTest and KeygenHandlerTest.ConcurrencyTest fail due to
              # readonly certdb (b/8153161)
              # URLRequestTestHTTP.GetTest_ManyCookies takes roughly 55s to run. Increase
              # timeout to 75s from 45s to allow it to pass (b/19821476)
              # ProxyScriptFetcherImplTest.HttpMimeType is flaking (b/19848784)
             'net_unittests --gtest_filter=-KeygenHandlerTest.SmokeTest:KeygenHandlerTest.ConcurrencyTest:ProxyScriptFetcherImplTest.HttpMimeType --test-launcher-timeout=75000',
              # Disable ProcessMetricsTest.GetNumberOfThreads (b/15610509)
              # Disable ProcessUtilTest.* (need to define OS_ANDROID)
              # Disable StackContainer.BufferAlignment (don't support 16-byte alignment)
              # Disable SystemMetrics2Test.GetSystemMemoryInfo (buffers>0 can't be guaranteed)
              'base_unittests --gtest_filter=-ProcessMetricsTest.GetNumberOfThreads:ProcessUtilTest.*:StackContainer.BufferAlignment:SystemMetrics2Test.GetSystemMemoryInfo',
              # DesktopCaptureDeviceTest.*: No capture device on Eureka
              # Disable PepperGamepadHostTest.WaitForReply (pepper not supported on Eureka)
              # Disable GpuDataManagerImplPrivateTest.SetGLStrings and
              # RenderWidgetHostTest.Background because we disable the blacklist to enable WebGL (b/16142554)
              'content_unittests --gtest_filter=-DOMStorageDatabaseTest.TestCanOpenAndReadWebCoreDatabase:DesktopCaptureDeviceTest.Capture:GamepadProviderTest.PollingAccess:GpuDataManagerImplPrivateTest.SetGLStrings:PepperGamepadHostTest.WaitForReply:RenderWidgetHostTest.Background',
              # Disable VP9 related tests (b/18593324)
              #   PipelineIntegrationTest.BasicPlayback_MediaSource_VP9_WebM
              #   PipelineIntegrationTest.BasicPlayback_VideoOnly_VP9_WebM
              #   PipelineIntegrationTest.BasicPlayback_VP9*
              #   PipelineIntegrationTest.P444_VP9_WebM
              # Disable VP8A tests (b/18593324)
              #   PipelineIntegrationTest.BasicPlayback_VP8A*
              # Disable OpusAudioDecoderTest/AudioDecoderTest.ProduceAudioSamples/0 (unit
              # test fails when Opus decoder uses fixed-point)
              # Due to b/16456550, disable the following four test cases:
              #   AudioOutputControllerTest.PlayDivertSwitchDeviceRevertClose
              #   AudioOutputControllerTest.PlaySwitchDeviceClose
              #   AudioStreamHandlerTest.Play
              #   SoundsManagerTest.Play
              # Disable AudioStreamHandlerTest.ConsecutivePlayRequests (b/16539293)
              'media_unittests --gtest_filter=-AudioOutputControllerTest.PlayDivertSwitchDeviceRevertClose:AudioOutputControllerTest.PlaySwitchDeviceClose:AudioStreamHandlerTest.Play:AudioStreamHandlerTest.ConsecutivePlayRequests:PipelineIntegrationTest.BasicPlayback_MediaSource_VP9_WebM:PipelineIntegrationTest.BasicPlayback_VideoOnly_VP9_WebM:PipelineIntegrationTest.BasicPlayback_VP9*:PipelineIntegrationTest.P444_VP9_WebM:PipelineIntegrationTest.BasicPlayback_VP8A*:OpusAudioDecoderTest/AudioDecoderTest.ProduceAudioSamples/0:SoundsManagerTest.Play',
              'sync_unit_tests --gtest_filter=-SyncHttpBridgeTest.*',
              # DoAppendUTF8Invalid fails because of dcheck_always_on flag in Eng builds
              'url_unittests --gtest_filter=-URLCanonTest.DoAppendUTF8Invalid',
            ],
          },
        }, { # else "x86" or "android"
          'variables': {
            'filters': [
              # Disable PipelineIntegrationTest.BasicPlayback_MediaSource_VP9_WebM (not supported)
              'media_unittests --gtest_filter=-PipelineIntegrationTest.BasicPlayback_MediaSource_VP9_WebM',
            ],
          }
        }],
        ['disable_display==0', {
          'dependencies': [
            '../gpu/gpu.gyp:gpu_unittests',
          ],
        }],
        ['OS!="android"', {
          'dependencies': [
            'cast_shell_unittests',
            'cast_shell_browser_test',
            'media/media.gyp:cast_media_unittests',
          ],
          'variables': {
            'filters': [
              'cast_shell_browser_test --no-sandbox --disable-gpu',
            ],
          },
        }],
        ['disable_display==1', {
          'variables': {
            'filters': [
              # These are not supported by the backend right now. b/21737919
              'cast_media_unittests --gtest_filter=-AudioVideoPipelineDeviceTest.VorbisPlayback:AudioVideoPipelineDeviceTest.WebmPlayback',
            ],
          }
        }],
        ['enable_plugins==1', {
          'dependencies': [
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
          ],
        }],
      ],
      'includes': ['build/tests/test_list.gypi'],
    },
    # Builds all tests and the output lists of build/run targets for those tests.
    # Note: producing a predetermined list of dependent inputs on which to
    # regenerate this output is difficult with GYP. This file is not
    # guaranteed to be regenerated outside of a clean build.
    {
      'target_name': 'cast_test_lists',
      'type': 'none',
      'dependencies': [
        'cast_tests',
      ],
      'variables': {
        'test_generator_py': '<(DEPTH)/chromecast/tools/build/generate_test_lists.py',
        'test_inputs_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast/tests',
        'test_additional_options': '--ozone-platform=test'
      },
      'actions': [
        {
          'action_name': 'generate_combined_test_build_list',
          'message': 'Generating combined test build list',
          'inputs': ['<(test_generator_py)'],
          'outputs': ['<(PRODUCT_DIR)/tests/build_test_list.txt'],
          'action': [
            'python', '<(test_generator_py)',
            '-t', '<(test_inputs_dir)',
            '-o', '<@(_outputs)',
            'pack_build',
          ],
        },
        {
          'action_name': 'generate_combined_test_run_list',
          'message': 'Generating combined test run list',
          'inputs': ['<(test_generator_py)'],
          'outputs': ['<(PRODUCT_DIR)/tests/run_test_list.txt'],
          'action': [
            'python', '<(test_generator_py)',
            '-t', '<(test_inputs_dir)',
            '-o', '<@(_outputs)',
            '-a', '<(test_additional_options)',
            'pack_run',
          ],
        }
      ],
    },
    {
      'target_name': 'cast_metrics_test_support',
      'type': '<(component)',
      'dependencies': [
        'cast_base',
      ],
      'sources': [
        'base/metrics/cast_metrics_test_helper.cc',
        'base/metrics/cast_metrics_test_helper.h',
      ],
    },  # end of target 'cast_metrics_test_support'
  ],  # end of targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'cast_android_tests',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests_apk',
            '../cc/cc_tests.gyp:cc_unittests_apk',
            '../ipc/ipc.gyp:ipc_tests_apk',
            '../media/media.gyp:media_unittests_apk',
            '../media/midi/midi.gyp:midi_unittests_apk',
            '../net/net.gyp:net_unittests_apk',
            '../sandbox/sandbox.gyp:sandbox_linux_jni_unittests_apk',
            '../sql/sql.gyp:sql_unittests_apk',
            '../sync/sync.gyp:sync_unit_tests_apk',
            '../ui/events/events.gyp:events_unittests_apk',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests_apk',
          ],
          'includes': ['build/tests/test_list.gypi'],
        },
        {
          'target_name': 'cast_android_test_lists',
          'type': 'none',
          'dependencies': [
            'cast_android_tests',
          ],
          'variables': {
            'test_generator_py': '<(DEPTH)/chromecast/tools/build/generate_test_lists.py',
            'test_inputs_dir': '<(SHARED_INTERMEDIATE_DIR)/chromecast/tests',
          },
          'actions': [
            {
              'action_name': 'generate_combined_test_build_list',
              'message': 'Generating combined test build list',
              'inputs': ['<(test_generator_py)'],
              'outputs': ['<(PRODUCT_DIR)/tests/build_test_list_android.txt'],
              'action': [
                'python', '<(test_generator_py)',
                '-t', '<(test_inputs_dir)',
                '-o', '<@(_outputs)',
                'pack_build',
              ],
            },
          ],
        },
      ],  # end of targets
    }, {  # OS!="android"
      'targets': [
        {
          'target_name': 'cast_shell_test_support',
          'type': '<(component)',
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'dependencies': [
            'cast_shell_core',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../testing/gtest.gyp:gtest',
            '../third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
          ],
          'sources': [
            'browser/test/chromecast_browser_test.cc',
            'browser/test/chromecast_browser_test.h',
            'browser/test/chromecast_browser_test_runner.cc',
          ],
        },  # end of target 'cast_shell_test_support'
        {
          'target_name': 'cast_shell_browser_test',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_shell_test_support',
            '../testing/gtest.gyp:gtest',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'browser/test/chromecast_shell_browser_test.cc',
          ],
        },
        {
          'target_name': 'cast_shell_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'chromecast.gyp:cast_crash_client',
            '../base/base.gyp:run_all_unittests',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'app/linux/cast_crash_reporter_client_unittest.cc',
          ],
        },  # end of cast_shell_unittests
      ],  # end of targets
    }],
  ],  # end of conditions
}
