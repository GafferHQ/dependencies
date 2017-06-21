# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'network_ui',

      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/cr_elements/v1_0/cr_onc/cr_onc_types.js',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:util',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
        ],
        'externs': [
          '../../../../../ui/webui/resources/cr_elements/v1_0/cr_network_icon/cr_network_icon_externs.js',
          '../../../../../third_party/closure_compiler/externs/chrome_extensions.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
