# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'mt19937ar',
      'type': 'static_library',
      'sources': [
        'mt19937ar.cc',
        'mt19937ar.h',
      ],
      'include_dirs': [
        '../..',
      ],
    },
  ],
}
