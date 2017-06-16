# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_core',
      'type': 'static_library',
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'device_client.cc',
        'device_client.h',
        'device_monitor_win.cc',
        'device_monitor_win.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
