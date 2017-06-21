# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# There's a couple key GYP variables that control how FFmpeg is built:
#   ffmpeg_branding
#     Controls whether we build the Chromium or Google Chrome version of
#     FFmpeg.  The Google Chrome version contains additional codecs.
#     Typical values are Chromium, Chrome, ChromiumOS, and ChromeOS.
#   ffmpeg_component
#     Set true to build ffmpeg as a shared library. NOTE: this means we should
#     always consult the value of 'ffmpeg_component' instead of 'component' for
#     this file. This helps linux chromium packagers that swap out our
#     ffmpeg.so with their own. See discussion here
#     https://groups.google.com/a/chromium.org/forum/#!msg/chromium-packagers/R5rcZXWxBEQ/B6k0zzmJbvcJ
#

{
  'variables': {
    'use_system_ffmpeg%': 0,
  },
  'conditions' : [
    ['use_system_ffmpeg == 0', {
  'target_defaults': {
    'variables': {
      # Since we are not often debugging FFmpeg, and performance is
      # unacceptable without optimization, freeze the optimizations to -O2.
      # If someone really wants -O1 , they can change these in their checkout.
      # If you want -O0, see the Gotchas in README.Chromium for why that
      # won't work.
      'release_optimize': '2',
      'debug_optimize': '2',
      'mac_debug_optimization': '2',
      # In addition to the above reasons, /Od optimization won't remove symbols
      # that are under "if (0)" style sections.  Which lead to link time errors
      # when for example it tries to link an ARM symbol on X86.
      'win_debug_Optimization': '2',
      # Run time checks are incompatible with any level of optimizations.
      'win_debug_RuntimeChecks': '0',
      'conditions': [
        ['OS == "win"', {
          # Setting the optimizations to 'speed' or to 'max' results in a lot of
          # unresolved symbols. The only supported mode is 'size_no_ltcg' (see
          # crbug.com/264459).
          'optimize' :'size_no_ltcg',
        }],
      ],
    },
  },
  'variables': {
    # Make sure asm_sources is always defined even if an arch doesn't have any
    # asm sources (e.g. mips or x86 with forcefully disabled asm).
    'asm_sources': [
    ],

    # Allow overriding the selection of which FFmpeg binaries to copy via an
    # environment variable.  Affects the ffmpeg_binaries target.
    'conditions': [
      ['target_arch == "arm" and arm_version == 7 and arm_neon == 1', {
        # Need a separate config for arm+neon vs arm
        'ffmpeg_config%': 'arm-neon',
      }, {
        'ffmpeg_config%': '<(target_arch)',
      }],
      ['OS == "mac" or OS == "win" or OS == "openbsd" or OS == "android"', {
        'os_config%': '<(OS)',
      }, {  # all other Unix OS's use the linux config
        'conditions': [
          ['msan==1', {
            # MemorySanitizer doesn't like assembly code.
            'os_config%': 'linux-noasm',
          }, {
            'os_config%': 'linux',
          }]
        ],
      }],
      ['chromeos == 1', {
        'ffmpeg_branding%': '<(branding)OS',
      }, {  # otherwise, assume Chrome/Chromium.
        'ffmpeg_branding%': '<(branding)',
      }],
    ],

    'ffmpeg_component%': '<(component)',

    # Locations for generated artifacts.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/ffmpeg',

    # Stub generator script and signatures of all functions used by Chrome.
    'generate_stubs_script': '../../tools/generate_stubs/generate_stubs.py',
    'sig_files': ['chromium/ffmpeg.sigs'],
  },
  'conditions': [
    ['chromeos == 1', {
      # This short-lived hack allows chromium changes to statically link ffmpeg to be independent
      # from chrome-os changes to ebuild files that explicitly mention libffmpegsumo.so as a target.
      # TODO(chcunningham): Remove this once ebuilds are patched.
      'targets': [
        {
          'target_name': 'ffmpegsumo',
          'type': 'loadable_module',
          'sources': [
            # Reusing an existing dummy file.
            'xcode_hack.c',
          ],
        },
      ], # targets
    }], # (chromeos == 1)
    ['(target_arch == "ia32" or target_arch == "x64") and os_config != "linux-noasm"', {
      'targets': [
        {
          'target_name': 'ffmpeg_yasm',
          'type': 'static_library',
          'includes': [
            'ffmpeg_generated.gypi',
            '../yasm/yasm_compile.gypi',
          ],
          'sources': [
            '<@(asm_sources)',
            # XCode doesn't want to link a pure assembly target and will fail
            # to link when it creates an empty file list.  So add a dummy file
            # keep the linker happy.  See http://crbug.com/157073
            'xcode_hack.c',
          ],
          'variables': {
            # Path to platform configuration files.
            'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',

            'conditions': [
              ['target_arch == "ia32"', {
                'more_yasm_flags': [
                  '-DARCH_X86_32',
                 ],
              }, {
                'more_yasm_flags': [
                  '-DARCH_X86_64',
                ],
              }],
              ['OS == "mac"', {
                'more_yasm_flags': [
                  # Necessary to ensure symbols end up with a _ prefix; added by
                  # yasm_compile.gypi for Windows, but not Mac.
                  '-DPREFIX',
                ]
              }],
            ],
            'yasm_flags': [
              '-DPIC',
              '>@(more_yasm_flags)',
              '-I', '<(platform_config_root)',
              '-I', 'libavcodec/x86/',
              '-I', 'libavutil/x86/',
              '-I', '.',
              # Disable warnings, prevents log spam for things we won't fix.
              '-w',
              '-P', 'config.asm',
            ],
            'yasm_includes': [
              # Sets visibility hidden for cglobal functions. Explicitly included
              # to avoid overlooking changes to this file in incremental builds.
              'libavutil/x86/x86inc.asm',
            ],
            'yasm_output_path': '<(shared_generated_dir)/yasm'
          },
        },
      ] # targets
    }], # (target_arch == "ia32" or target_arch == "x64")
  ],

  'targets': [{
    'target_name': 'ffmpeg',
    'type': '<(ffmpeg_component)',
    'variables': {
      # Path to platform configuration files.
      'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
    },
    'conditions': [
      # This looks crazy, but it seems to be the only way to make this work. Without this
      # conditions block, platform_config_root will be undefined when expanded below.
      ['1 == 1',
        {
          'direct_dependent_settings': {
            'include_dirs': [
              '../..',  # The chromium 'src' directory.
              '<(platform_config_root)',
              '.',
            ],
          },
          'includes': [
            'ffmpeg_generated.gypi',
            '../../build/util/version.gypi',
          ],
          'sources': [
            '<@(c_sources)',
            '<(platform_config_root)/config.h',
            '<(platform_config_root)/libavutil/avconfig.h',
          ],
          'include_dirs': [
            '<(platform_config_root)',
            '.',
          ],
          'defines': [
            'HAVE_AV_CONFIG_H',
            '_POSIX_C_SOURCE=200112',
            '_XOPEN_SOURCE=600',
            'PIC',
            # Disable deprecated features which generate spammy warnings.
            'FF_API_PIX_FMT_DESC=0',
            'FF_API_OLD_DECODE_AUDIO=0',
            'FF_API_DESTRUCT_PACKET=0',
            'FF_API_GET_BUFFER=0',
          ],
          'variables': {
            'clang_warning_flags': [
              '-Wno-absolute-value',
              # ffmpeg uses its own deprecated functions.
              '-Wno-deprecated-declarations',
              # ffmpeg doesn't care about pointer constness.
              '-Wno-incompatible-pointer-types',
              # ffmpeg doesn't follow usual parentheses conventions.
              '-Wno-parentheses',
              # ffmpeg doesn't care about pointer signedness.
              '-Wno-pointer-sign',
              # ffmpeg doesn't believe in exhaustive switch statements.
              '-Wno-switch',
              # matroskadec.c has a "failed:" label that's only used if some
              # CONFIG_ flags we don't set are set.
              '-Wno-unused-label',
              # ffmpeg has a lot of unused variables.
              '-Wno-unused-variable',
              # This fires on `av_assert0(!"valid element size")` in utils.c
              '-Wno-string-conversion',
              # This fires on `pos_min` and `pos_max` in
              # autorename_libavformat_utils.c
              '-Wno-sometimes-uninitialized',
            ],
          },
          'cflags': [
            '-fPIC',
            '-fomit-frame-pointer',
            # ffmpeg uses its own deprecated functions.
            '-Wno-deprecated-declarations',
          ],
          # Silence a warning in libc++ builds (C code doesn't need this flag).
          'ldflags!': [ '-stdlib=libc++', ],
          'conditions': [
            ['(target_arch == "ia32" or target_arch == "x64") and os_config != "linux-noasm"', {
              'dependencies': [
                'ffmpeg_yasm',
              ],
            }],
            ['clang != 1', {
              'cflags': [
                # gcc doesn't have flags for specific warnings, so disable them
                # all.
                '-w',
              ],
            }],
            ['target_arch == "ia32"', {
              # Turn off valgrind build option that breaks ffmpeg builds.
              'cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'debug_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'release_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
            }],  # target_arch == "ia32"
            ['target_arch == "arm" or target_arch == "arm64"', {
              # On arm we use gcc to compile the assembly.
              'sources': [
                '<@(asm_sources)',
              ],
              'conditions': [
                ['arm_float_abi == "hard"', {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=1'
                  ],
                }, {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=0'
                  ],
                }],
              ],
            }],
            ['os_posix == 1 and OS != "mac"', {
              'defines': [
                '_ISOC99_SOURCE',
                '_LARGEFILE_SOURCE',
                # BUG(ihf): ffmpeg compiles with this define. But according to
                # ajwong: I wouldn't change _FILE_OFFSET_BITS.  That's a scary change
                # because it affects the default length of off_t, and fpos_t,
                # which can cause strange problems if the loading code doesn't
                # have it set and you start passing FILE*s or file descriptors
                # between symbol contexts.
                # '_FILE_OFFSET_BITS=64',
              ],
              'cflags': [
                '-std=c99',
                '-pthread',
                '-fno-math-errno',
                '-fno-signed-zeros',
                '-fno-tree-vectorize',
              ],
              'link_settings': {
                'libraries': [
                  '-lm',
                  '-lz',
                ],
              },
              'conditions': [
                ['OS != "android"', {
                  'link_settings': {
                    'libraries': [
                      '-lrt',
                    ],
                  },
                }],
                ['ffmpeg_component == "shared_library"', {
                  # Export all symbols when building as component.
                  'cflags!': [
                    '-fvisibility=hidden',
                  ],
                  # Fixes warnings PIC relocation when building as component.
                  # *WARNING* -- DO NOT put this inside of a link_settings
                  # section or these flags will be propagated outside of the
                  # ffmpeg target and cause debug allocator crashes.
                  'ldflags': [
                    '-Wl,-Bsymbolic',
                    '-L<(shared_generated_dir)',
                  ],
                }],
              ],
            }],  # os_posix == 1 and OS != "mac"
            ['OS == "openbsd"', {
              # OpenBSD's gcc (4.2.1) does not support this flag
              'cflags!': [
                '-fno-signed-zeros',
              ],
            }],
            ['OS == "mac"', {
              'defines': [
                '_DARWIN_C_SOURCE',
              ],
              'conditions': [
                ['mac_breakpad == 1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                }],
                ['target_arch != "x64"', {
                  # -read_only_relocs cannot be used with x86_64
                  'xcode_settings': {
                    'OTHER_LDFLAGS': [
                      # This is needed because even though FFmpeg now builds
                      # with -fPIC, it's not quite a complete PIC build, only
                      # partial :( Thus we need to instruct the linker to allow
                      # relocations for read-only segments for this target to be
                      # able to generated the shared library on Mac.
                      #
                      # This makes Mark sad, but he's okay with it since it is
                      # isolated to this module. When Mark finds this in the
                      # future, and has forgotten this conversation, this
                      # comment should remind him that the world is still nice
                      # and butterflies still exist...as do rainbows, sunshine,
                      # tulips, etc., etc...but not kittens. Those went away
                      # with this flag.
                      '-Wl,-read_only_relocs,suppress',
                    ],
                  },
                }],
                ['ffmpeg_component == "shared_library"', {
                  'xcode_settings': {
                    # GCC version of no -fvisiliity=hidden. Ensures that all
                    # symbols are exported for component builds.
                    'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
                  },
                }],
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libz.dylib',
                ],
              },
              'xcode_settings': {
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
                'LIBRARY_SEARCH_PATHS': [
                  '<(shared_generated_dir)'
                ],
              },
            }],  # OS == "mac"
            ['OS == "win"', {
              # TODO(dalecurtis): We should fix these.  http://crbug.com/154421
              'msvs_disabled_warnings': [
                4996, 4018, 4090, 4305, 4133, 4146, 4554, 4028, 4334, 4101, 4102,
                4116, 4307, 4273, 4005, 4056, 4756,
              ],
              'conditions': [
                ['clang == 1', {
                  'msvs_settings': {
                    'VCCLCompilerTool': {
                      # This corresponds to msvs_disabled_warnings 4273 above.
                      'AdditionalOptions': [ '-Wno-inconsistent-dllimport' ],
                    },
                  },
                }],
                ['clang == 1 or (MSVS_VERSION == "2013" or MSVS_VERSION == "2013e")', {
                  'defines': [
                    'inline=__inline',
                    'strtoll=_strtoi64',
                    '_ISOC99_SOURCE',
                    '_LARGEFILE_SOURCE',
                    'HAVE_AV_CONFIG_H',
                    'strtod=avpriv_strtod',
                    'snprintf=avpriv_snprintf',
                    '_snprintf=avpriv_snprintf',
                    'vsnprintf=avpriv_vsnprintf',
                  ],
                  # Fix warnings about a local symbol being inefficiently imported.
                  'msvs_settings': {
                    'VCCLCompilerTool': {
                      'AdditionalOptions': [
                        '/FIcompat/msvcrt/snprintf.h',
                        '/FIcompat/msvcrt/strtod.h',
                      ],
                    },
                  },
                }],
                ['target_arch == "x64"', {
                  # TODO(wolenetz): We should fix this.  http://crbug.com/171009
                  'msvs_disabled_warnings' : [
                    4267
                  ],
                }],
                ['ffmpeg_component == "shared_library"', {
                  'sources': [
                    '<(shared_generated_dir)/ffmpeg.def',
                  ],
                  'actions': [
                    {
                      'action_name': 'generate_def',
                      'inputs': [
                        '<(generate_stubs_script)',
                        '<@(sig_files)',
                      ],
                      'outputs': [
                        '<(shared_generated_dir)/ffmpeg.def',
                      ],
                      'action': ['python',
                                 '<(generate_stubs_script)',
                                 '-i', '<(INTERMEDIATE_DIR)',
                                 '-o', '<(shared_generated_dir)',
                                 '-t', 'windows_def',
                                 '-m', 'ffmpeg.dll',
                                 '<@(_inputs)',
                      ],
                      'message': 'Generating FFmpeg export definitions',
                    },
                  ],
                }],
              ],
            }],
          ],
        },
      ],
    ],
  }],
  }, #  'use_system_ffmpeg == 0'
  {
  'targets': [
    {
      'target_name': 'ffmpeg',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags libavcodec libavformat libavutil)',

          '<!(python <(DEPTH)/tools/compile_test/compile_test.py '
              '--code "#define __STDC_CONSTANT_MACROS\n'
              '#include <libavcodec/avcodec.h>\n'
              'int test() { return AV_CODEC_ID_OPUS; }" '
              '--on-failure -DCHROMIUM_OMIT_AV_CODEC_ID_OPUS=1)',

          '<!(python <(DEPTH)/tools/compile_test/compile_test.py '
              '--code "#define __STDC_CONSTANT_MACROS\n'
              '#include <libavcodec/avcodec.h>\n'
              'int test() { return AV_CODEC_ID_VP9; }" '
              '--on-failure -DCHROMIUM_OMIT_AV_CODEC_ID_VP9=1)',

          '<!(python <(DEPTH)/tools/compile_test/compile_test.py '
              '--code "#define __STDC_CONSTANT_MACROS\n'
              '#include <libavcodec/avcodec.h>\n'
              'int test() { return AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL; }" '
              '--on-failure -DCHROMIUM_OMIT_AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL=1)',

          '<!(python <(DEPTH)/tools/compile_test/compile_test.py '
              '--code "#define __STDC_CONSTANT_MACROS\n'
              '#include <libavutil/frame.h>\n'
              'int test() { struct AVFrame frame;\n'
              'return av_frame_get_channels(&frame); }" '
              '--on-failure -DCHROMIUM_NO_AVFRAME_CHANNELS=1)',
        ],
        'defines': [
          '__STDC_CONSTANT_MACROS',
          'USE_SYSTEM_FFMPEG',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libavcodec libavformat libavutil)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libavcodec libavformat libavutil)',
        ],
      },
    },
  ],
  }
  ]]
}
