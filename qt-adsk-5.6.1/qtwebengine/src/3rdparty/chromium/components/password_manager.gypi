# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/password_manager/core/browser
      'target_name': 'password_manager_core_browser',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../sync/sync.gyp:sync',
        '../third_party/protobuf/protobuf.gyp:protobuf_lite',
        '../url/url.gyp:url_lib',
        'autofill_core_common',
        'components_strings.gyp:components_strings',
        'keyed_service_core',
        'os_crypt',
        '../third_party/re2/re2.gyp:re2',
        'password_manager_core_common',
        'password_manager_core_browser_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'password_manager/core/browser/affiliated_match_helper.cc',
        'password_manager/core/browser/affiliated_match_helper.h',
        'password_manager/core/browser/affiliation_backend.cc',
        'password_manager/core/browser/affiliation_backend.h',
        'password_manager/core/browser/affiliation_database.cc',
        'password_manager/core/browser/affiliation_database.h',
        'password_manager/core/browser/affiliation_fetch_throttler.cc',
        'password_manager/core/browser/affiliation_fetch_throttler.h',
        'password_manager/core/browser/affiliation_fetch_throttler_delegate.h',
        'password_manager/core/browser/affiliation_fetcher.cc',
        'password_manager/core/browser/affiliation_fetcher.h',
        'password_manager/core/browser/affiliation_fetcher_delegate.h',
        'password_manager/core/browser/affiliation_service.cc',
        'password_manager/core/browser/affiliation_service.h',
        'password_manager/core/browser/affiliation_utils.cc',
        'password_manager/core/browser/affiliation_utils.h',
        'password_manager/core/browser/browser_save_password_progress_logger.cc',
        'password_manager/core/browser/browser_save_password_progress_logger.h',
        'password_manager/core/browser/credential_manager_password_form_manager.cc',
        'password_manager/core/browser/credential_manager_password_form_manager.h',
        'password_manager/core/browser/credential_manager_pending_request_task.cc',
        'password_manager/core/browser/credential_manager_pending_request_task.h',
        'password_manager/core/browser/credential_manager_pending_require_user_mediation_task.cc',
        'password_manager/core/browser/credential_manager_pending_require_user_mediation_task.h',
        'password_manager/core/browser/export/csv_writer.cc',
        'password_manager/core/browser/export/csv_writer.h',
        'password_manager/core/browser/facet_manager.cc',
        'password_manager/core/browser/facet_manager.h',
        'password_manager/core/browser/facet_manager_host.h',
        'password_manager/core/browser/import/csv_reader.cc',
        'password_manager/core/browser/import/csv_reader.h',
        'password_manager/core/browser/keychain_migration_status_mac.h',
        'password_manager/core/browser/log_receiver.h',
        'password_manager/core/browser/log_router.cc',
        'password_manager/core/browser/log_router.h',
        'password_manager/core/browser/login_database.cc',
        'password_manager/core/browser/login_database.h',
        'password_manager/core/browser/login_database_mac.cc',
        'password_manager/core/browser/login_database_posix.cc',
        'password_manager/core/browser/login_database_win.cc',
        'password_manager/core/browser/login_model.h',
        'password_manager/core/browser/password_autofill_manager.cc',
        'password_manager/core/browser/password_autofill_manager.h',
        'password_manager/core/browser/password_bubble_experiment.cc',
        'password_manager/core/browser/password_bubble_experiment.h',
        'password_manager/core/browser/password_form_manager.cc',
        'password_manager/core/browser/password_form_manager.h',
        'password_manager/core/browser/password_generation_manager.cc',
        'password_manager/core/browser/password_generation_manager.h',
        'password_manager/core/browser/password_manager.cc',
        'password_manager/core/browser/password_manager.h',
        'password_manager/core/browser/password_manager_client.cc',
        'password_manager/core/browser/password_manager_client.h',
        'password_manager/core/browser/password_manager_driver.h',
        'password_manager/core/browser/password_manager_internals_service.cc',
        'password_manager/core/browser/password_manager_internals_service.h',
        'password_manager/core/browser/password_manager_metrics_util.cc',
        'password_manager/core/browser/password_manager_metrics_util.h',
        'password_manager/core/browser/password_manager_util.cc',
        'password_manager/core/browser/password_manager_util.h',
        'password_manager/core/browser/password_store.cc',
        'password_manager/core/browser/password_store.h',
        'password_manager/core/browser/password_store_change.h',
        'password_manager/core/browser/password_store_consumer.cc',
        'password_manager/core/browser/password_store_consumer.h',
        'password_manager/core/browser/password_store_default.cc',
        'password_manager/core/browser/password_store_default.h',
        'password_manager/core/browser/password_store_sync.cc',
        'password_manager/core/browser/password_store_sync.h',
        'password_manager/core/browser/password_syncable_service.cc',
        'password_manager/core/browser/password_syncable_service.h',
        'password_manager/core/browser/psl_matching_helper.cc',
        'password_manager/core/browser/psl_matching_helper.h',
        'password_manager/core/browser/statistics_table.cc',
        'password_manager/core/browser/statistics_table.h',
        'password_manager/core/browser/test_affiliation_fetcher_factory.h',
        'password_manager/core/browser/webdata/logins_table.cc',
        'password_manager/core/browser/webdata/logins_table.h',
        'password_manager/core/browser/webdata/logins_table_win.cc',
        'password_manager/core/browser/webdata/password_web_data_service_win.cc',
        'password_manager/core/browser/webdata/password_web_data_service_win.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            # TODO(blundell): Provide the iOS login DB implementation and then
            # also exclude the POSIX one from iOS. http://crbug.com/341429
            'password_manager/core/browser/login_database_posix.cc',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //components/password_manager/core/browser:proto
      'target_name': 'password_manager_core_browser_proto',
      'type': 'static_library',
      'sources': [
        'password_manager/core/browser/affiliation_api.proto'
      ],
      'variables': {
        'proto_in_dir': 'password_manager/core/browser',
        'proto_out_dir': 'components/password_manager/core/browser',
      },
      'includes': ['../build/protoc.gypi'],
    },
    {
      # GN version: //components/password_manager/core/browser:test_support
      'target_name': 'password_manager_core_browser_test_support',
      'type': 'static_library',
      'dependencies': [
        'autofill_core_common',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'password_manager/core/browser/fake_affiliation_api.cc',
        'password_manager/core/browser/fake_affiliation_api.h',
        'password_manager/core/browser/fake_affiliation_fetcher.cc',
        'password_manager/core/browser/fake_affiliation_fetcher.h',
        'password_manager/core/browser/mock_affiliation_consumer.cc',
        'password_manager/core/browser/mock_affiliation_consumer.h',
        'password_manager/core/browser/mock_password_store.cc',
        'password_manager/core/browser/mock_password_store.h',
        'password_manager/core/browser/password_manager_test_utils.cc',
        'password_manager/core/browser/password_manager_test_utils.h',
        'password_manager/core/browser/stub_password_manager_client.cc',
        'password_manager/core/browser/stub_password_manager_client.h',
        'password_manager/core/browser/stub_password_manager_driver.cc',
        'password_manager/core/browser/stub_password_manager_driver.h',
        'password_manager/core/browser/test_password_store.cc',
        'password_manager/core/browser/test_password_store.h',
      ],
    },
    {
      # GN version: //components/password_manager/core/common
      'target_name': 'password_manager_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'password_manager/core/common/credential_manager_types.cc',
        'password_manager/core/common/credential_manager_types.h',
        'password_manager/core/common/experiments.cc',
        'password_manager/core/common/experiments.h',
        'password_manager/core/common/password_manager_pref_names.cc',
        'password_manager/core/common/password_manager_pref_names.h',
        'password_manager/core/common/password_manager_switches.cc',
        'password_manager/core/common/password_manager_switches.h',
        'password_manager/core/common/password_manager_ui.h',
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          # GN version: //components/password_manager/content/common
          'target_name': 'password_manager_content_common',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
            '../third_party/WebKit/public/blink.gyp:blink_minimal',
            'password_manager_core_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'password_manager/content/common/credential_manager_content_utils.cc',
            'password_manager/content/common/credential_manager_content_utils.h',
            'password_manager/content/common/credential_manager_message_generator.cc',
            'password_manager/content/common/credential_manager_message_generator.h',
            'password_manager/content/common/credential_manager_messages.h',
          ],
        },
        {
          # GN version: //components/password_manager/content/renderer
          'target_name': 'password_manager_content_renderer',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
            '../third_party/WebKit/public/blink.gyp:blink',
            'password_manager_core_common',
            'password_manager_content_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'password_manager/content/renderer/credential_manager_client.cc',
            'password_manager/content/renderer/credential_manager_client.h',
          ],
        },
        {
          # GN version: //components/password_manager/content/browser
          'target_name': 'password_manager_content_browser',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../content/content.gyp:content_common',
            '../ipc/ipc.gyp:ipc',
            '../net/net.gyp:net',
            'autofill_content_browser',
            'autofill_content_common',
            'autofill_core_common',
            'keyed_service_content',
            'password_manager_core_browser',
            'password_manager_content_common',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'password_manager/content/browser/content_password_manager_driver.cc',
            'password_manager/content/browser/content_password_manager_driver.h',
            'password_manager/content/browser/content_password_manager_driver_factory.cc',
            'password_manager/content/browser/content_password_manager_driver_factory.h',
            'password_manager/content/browser/credential_manager_dispatcher.cc',
            'password_manager/content/browser/credential_manager_dispatcher.h',
            'password_manager/content/browser/password_manager_internals_service_factory.cc',
            'password_manager/content/browser/password_manager_internals_service_factory.h',
          ],
        },
      ],
    }],
  ],
}
