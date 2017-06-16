# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'run_multi_machine_tests%': '0',
  },

  'conditions': [
    ['archive_chromoting_tests==1', {
      'conditions': [
        ['OS=="linux"', {
          'targets': [
            {
              'target_name': 'app_remoting_integration_tests_run',
              'includes': [
                './dependencies.gypi',
              ],
              'sources': [
                'app_remoting_integration_tests.isolate',
              ],
            },  # target_name: 'app_remoting_integration_tests_run'
          ],
        }],
        ['run_multi_machine_tests==1', {
          'targets': [
            {
              'target_name': 'chromoting_multi_machine_example_test',
              'includes': [
                './dependencies.gypi',
              ],
              'sources': [
                'multi_machine_example/example_test_controller.isolate',
                'multi_machine_example/example_task.isolate',
              ],
            },  # target_name: 'chromoting_multi_machine_example_test'
          ],
        }],
      ],
      'targets': [
        {
          'target_name': 'chromoting_integration_tests_run',
          'includes': [
            './dependencies.gypi',
          ],
          'sources': [
            'chromoting_integration_tests.isolate',
          ],
        },  # target_name: 'chromoting_integration_tests_run'
      ],
    }],
  ],
}
