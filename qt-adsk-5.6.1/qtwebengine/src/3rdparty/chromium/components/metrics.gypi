# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/metrics
      'target_name': 'metrics',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:base_prefs',
        '../third_party/zlib/zlib.gyp:zlib',
        'component_metrics_proto',
        'variations',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/call_stack_profile_metrics_provider.cc',
        'metrics/call_stack_profile_metrics_provider.h',
        'metrics/clean_exit_beacon.cc',
        'metrics/clean_exit_beacon.h',
        'metrics/client_info.cc',
        'metrics/client_info.h',
        'metrics/cloned_install_detector.cc',
        'metrics/cloned_install_detector.h',
        'metrics/compression_utils.cc',
        'metrics/compression_utils.h',
        'metrics/daily_event.cc',
        'metrics/daily_event.h',
        'metrics/histogram_encoder.cc',
        'metrics/histogram_encoder.h',
        'metrics/machine_id_provider.h',
        'metrics/machine_id_provider_stub.cc',
        'metrics/machine_id_provider_win.cc',
        'metrics/metrics_hashes.cc',
        'metrics/metrics_hashes.h',
        'metrics/metrics_log.cc',
        'metrics/metrics_log.h',
        'metrics/metrics_log_manager.cc',
        'metrics/metrics_log_manager.h',
        'metrics/metrics_log_uploader.cc',
        'metrics/metrics_log_uploader.h',
        'metrics/metrics_pref_names.cc',
        'metrics/metrics_pref_names.h',
        'metrics/metrics_provider.cc',
        'metrics/metrics_provider.h',
        'metrics/metrics_reporting_scheduler.cc',
        'metrics/metrics_reporting_scheduler.h',
        'metrics/metrics_service.cc',
        'metrics/metrics_service.h',
        'metrics/metrics_service_accessor.cc',
        'metrics/metrics_service_accessor.h',
        'metrics/metrics_service_client.cc',
        'metrics/metrics_service_client.h',
        'metrics/metrics_state_manager.cc',
        'metrics/metrics_state_manager.h',
        'metrics/metrics_switches.cc',
        'metrics/metrics_switches.h',
        'metrics/persisted_logs.cc',
        'metrics/persisted_logs.h',
        'metrics/url_constants.cc',
        'metrics/url_constants.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            'metrics_serialization',
          ],
        }],
        ['OS=="win"', {
          'sources!': [
            'metrics/machine_id_provider_stub.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //components/metrics:gpu
      'target_name': 'metrics_gpu',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../ui/gfx/gfx.gyp:gfx',
        'component_metrics_proto',
        'metrics',
      ],
      'sources': [
        'metrics/gpu/gpu_metrics_provider.cc',
        'metrics/gpu/gpu_metrics_provider.h',
      ],
    },
    {
      # GN version: //components/metrics:net
      'target_name': 'metrics_net',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'component_metrics_proto',
        'metrics',
      ],
      'sources': [
        'metrics/net/net_metrics_log_uploader.cc',
        'metrics/net/net_metrics_log_uploader.h',
        'metrics/net/network_metrics_provider.cc',
        'metrics/net/network_metrics_provider.h',
        'metrics/net/wifi_access_point_info_provider.cc',
        'metrics/net/wifi_access_point_info_provider.h',
        'metrics/net/wifi_access_point_info_provider_chromeos.cc',
        'metrics/net/wifi_access_point_info_provider_chromeos.h',
      ],
    },
    {
      # GN version: //components/metrics:profiler
      'target_name': 'metrics_profiler',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../content/content.gyp:content_browser',
        '../content/content.gyp:content_common',
        'component_metrics_proto',
        'metrics',
        'variations',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/profiler/profiler_metrics_provider.cc',
        'metrics/profiler/profiler_metrics_provider.h',
        'metrics/profiler/tracking_synchronizer.cc',
        'metrics/profiler/tracking_synchronizer.h',
        'metrics/profiler/tracking_synchronizer_observer.cc',
        'metrics/profiler/tracking_synchronizer_observer.h',
      ],
    },
    {
      # Protobuf compiler / generator for UMA (User Metrics Analysis).
      #
      # GN version: //components/metrics/proto:proto
      'target_name': 'component_metrics_proto',
      'type': 'static_library',
      'sources': [
        'metrics/proto/call_stack_profile.proto',
        'metrics/proto/cast_logs.proto',
        'metrics/proto/chrome_user_metrics_extension.proto',
        'metrics/proto/histogram_event.proto',
        'metrics/proto/omnibox_event.proto',
        'metrics/proto/omnibox_input_type.proto',
        'metrics/proto/perf_data.proto',
        'metrics/proto/perf_stat.proto',
        'metrics/proto/profiler_event.proto',
        'metrics/proto/sampled_profile.proto',
        'metrics/proto/system_profile.proto',
        'metrics/proto/user_action_event.proto',
      ],
      'variables': {
        'proto_in_dir': 'metrics/proto',
        'proto_out_dir': 'components/metrics/proto',
      },
      'includes': [ '../build/protoc.gypi' ],
    },
    {
      # TODO(isherman): Remove all //chrome dependencies on this target, and
      # merge the files in this target with components_unittests.
      # GN version: //components/metrics:test_support
      'target_name': 'metrics_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'component_metrics_proto',
        'metrics',
      ],
      'export_dependent_settings': [
        'component_metrics_proto',
      ],
      'sources': [
        'metrics/test_metrics_provider.cc',
        'metrics/test_metrics_provider.h',
        'metrics/test_metrics_service_client.cc',
        'metrics/test_metrics_service_client.h',
      ],
    },
  ],
  'conditions': [
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'metrics_serialization',
          'type': 'static_library',
          'sources': [
            'metrics/serialization/metric_sample.cc',
            'metrics/serialization/metric_sample.h',
            'metrics/serialization/serialization_utils.cc',
            'metrics/serialization/serialization_utils.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
          ],
        },
      ],
    }],
  ],
}
