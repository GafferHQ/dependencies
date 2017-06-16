# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'ipc.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ipc_nacl',
          'type': 'none',
          'variables': {
            'ipc_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libipc_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl',
            '../crypto/crypto_nacl.gyp:crypto_nacl',
          ],
        },
        {
          'target_name': 'ipc_nacl_nonsfi',
          'type': 'none',
          'variables': {
            'ipc_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libipc_nacl_nonsfi.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            # Use linux IPC channel implementation for nacl_helper_nonsfi,
            # instead of NaCl's IPC implementation (ipc_channel_nacl.cc),
            # because nacl_helper_nonsfi will be running directly on Linux.
            # ipc_channel_nacl.cc is excluded below.
            'sources': [
              'ipc_channel.cc',
              'ipc_channel_posix.cc',
            ],
          },
          'sources!': [
            'ipc_channel_nacl.cc',
          ],
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../crypto/crypto_nacl.gyp:crypto_nacl',
          ],
        },
      ],
    }],
  ],
}
