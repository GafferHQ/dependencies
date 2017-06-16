{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
   '../../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'latency_info_nacl',
          'type': 'none',
          'defines': [
            'EVENTS_BASE_IMPLEMENTATION',
            'EVENTS_IMPLEMENTATION',
          ],
          'include_dirs': [
            '../..',
          ],
          'dependencies': [
            '../../base/base_nacl.gyp:base_nacl',
            '../../base/base_nacl.gyp:base_nacl_nonsfi',
            '../../ipc/ipc_nacl.gyp:ipc_nacl',
            '../../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
          ],
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'liblatency_info_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'sources': [
            'ipc/latency_info_param_traits.cc',
            'ipc/latency_info_param_traits.h',
            'latency_info.cc',
            'latency_info.h',
          ],
        },
      ],
    }],
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'latency_info_nacl_win64',
          'type' : '<(component)',
          'variables': {
            'nacl_win64_target': 1,
          },
          'dependencies': [
            '../../base/base.gyp:base_win64',
            '../../ipc/ipc.gyp:ipc_win64',
          ],
          'defines': [
            'EVENTS_BASE_IMPLEMENTATION',
            'EVENTS_IMPLEMENTATION',
            '<@(nacl_win64_defines)',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'ipc/latency_info_param_traits.cc',
            'ipc/latency_info_param_traits.h',
            'latency_info.cc',
            'latency_info.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}

