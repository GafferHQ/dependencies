# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # .gyp files should set chromium_code to 1 if they build Chromium-specific
    # code, as opposed to external code.  This variable is used to control
    # such things as the set of warnings to enable, and whether warnings are
    # treated as errors.
    'chromium_code%': 0,

    # Variables expected to be overriden on the GYP command line (-D) or by
    # ~/.gyp/include.gypi.

    # Override chromium_mac_pch and set it to 0 to suppress the use of
    # precompiled headers on the Mac.  Prefix header injection may still be
    # used, but prefix headers will not be precompiled.  This is useful when
    # using distcc to distribute a build to compile slaves that don't
    # share the same compiler executable as the system driving the compilation,
    # because precompiled headers rely on pointers into a specific compiler
    # executable's image.  Setting this to 0 is needed to use an experimental
    # Linux-Mac cross compiler distcc farm.
    'chromium_mac_pch%': 1,

    # Enable building with ASAN (Clang's -fsanitize=address option).
    # -fsanitize=address only works with clang, but asan=1 implies clang=1
    # See https://sites.google.com/a/chromium.org/dev/developers/testing/addresssanitizer
    'asan%': 0,

    # Set this to true when building with Clang.
    'clang%': 0,

    'conditions': [
      ['OS=="mac"', {
        # On Mac only clang is new enough to build the trusted code.
        'clang%': 1,
        # The Mac SDK is set for iOS builds and passed through to Mac
        # sub-builds. This allows the Mac sub-build SDK in an iOS build to be
        # overridden from the command line the same way it is for a Mac build.
        'mac_sdk%': '<!(python <(DEPTH)/native_client/build/mac/find_sdk.py 10.6)',

      }],
      ['OS=="linux" and target_arch!="mipsel" and target_arch!="arm"', {
        'clang%': 1,
      }],
      ['target_arch=="mipsel"', {
        'mips_arch_variant%': 'r2',
      }],
      # Set ARM float abi compilation flag.
      ['OS=="android"', {
        'arm_float_abi%': 'softfp',
      }, {
        'arm_float_abi%': 'hard',
      }],
      ['nacl_standalone==1 and host_clang==1', {
        'host_cc': '<(make_clang_dir)/bin/clang',
        'host_cxx': '<(make_clang_dir)/bin/clang++',
      }],
      ['nacl_standalone==1 and host_clang==0', {
        'host_cc': '<!(which gcc)',
        'host_cxx': '<!(which g++)',
      }],
    ],

    # Set to 1 to enable code coverage.  In addition to build changes
    # (e.g. extra CFLAGS), also creates a new target in the src/chrome
    # project file called "coverage".
    # Currently ignored on Windows.
    'coverage%': 0,

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    # NOTE: adapted from them chrome common.gypi file for arm
    'arm_version%': 7,

    # Set Neon compilation flags (only meaningful if arm_version==7).
    'arm_neon%': 1,

    # Set Thumb compilation flags.
    'arm_thumb%': 0,

    # Set ARM fpu compilation flags (only meaningful if arm_version==7 and
    # arm_neon==0).
    'arm_fpu%': 'vfpv3',

    'mac_deployment_target%': '10.6',

    # NOTE: end adapted from them chrome common.gypi file for arm

    # Doing this in a sub-dict so that it can be referred to below.
    'variables': {
      # By default we assume that we are building as part of Chrome
      'variables': {
        'nacl_standalone%': 0,

        # Override branding to select the desired branding flavor.
        'branding%': 'Chromium',

        # Override buildtype to select the desired build flavor.
        # Dev - everyday build for development/testing
        # Official - release build (generally implies additional processing)
        # TODO(mmoss) Once 'buildtype' is fully supported (e.g. Windows gyp
        # conversion is done), some of the things which are now controlled by
        # 'branding', such as symbol generation, will need to be refactored
        # based on 'buildtype' (i.e. we don't care about saving symbols for
        # non-Official builds).
        'buildtype%': 'Dev',


        'make_clang_dir%': 'third_party/llvm-build/Release+Asserts',

        'conditions': [
          # Compute the architecture that we're building on.
          # This logic needs to be kept in sync with Chrome's common.gypi or
          # subtle build errors will result when integrating into Chrome.
          # Generally, a desync will result in NaCl libraries will be a
          # different bittage than Chrome.  The kicker is that builds that
          # explicitly specify target_arch will not notice this desync, (this
          # logic is overridden) but builds that do not specify target_arch will
          # break.  This can cause confusion because only some trybots break.
          ['OS=="win" or OS=="ios"', {
            'host_arch%': 'ia32',
          }, {
            # This handles the Linux platforms we generally deal with. Anything
            # else gets passed through, which probably won't work very well;
            # such hosts should pass an explicit target_arch to gyp.
            #
            # NOTE: currently only nacl is generating gyp files on an arm board.
            #    The arm.* -> arm substitution in chrome's common.gypi isn't
            #    appropriate in that context as we actually use target_arch==arm
            #    to mean x86 -> arm cross compile. When actually running on an
            #    arm board, we'll generate ia32 for now, so that the generation
            #    succeeds.
            'host_arch%':
                '<!(echo "<!pymod_do_main(detect_nacl_host_arch)" | sed -e "s/arm.*/ia32/")',
          }],
        ]
      },
      # These come from the above variable scope.
      'nacl_standalone%': '<(nacl_standalone)',
      'host_arch%': '<(host_arch)',
      'branding%': '<(branding)',
      'buildtype%': '<(buildtype)',
      'make_clang_dir%': '<(make_clang_dir)',

      'conditions': [
        ['nacl_standalone==1', {
          # The system root for cross-compiles. Default: none.
          # If we are building in chrome we want to rely on chrome's default,
          # which means we can't set a default here.
          'sysroot%': '',
          # If this is set clang is used as host compiler, but not as target
          # compiler. Always do this by default.
          'host_clang%': 1,
        }],
        # A flag for POSIX platforms
        ['OS=="win"', {
          'os_posix%': 0,
         }, {
          'os_posix%': 1,
        }],
        ['OS=="android"', { # Android target_arch defaults to ARM.
          'target_arch%': 'arm',
        }, {
          # By default, build for the architecture that we're building on.
          'target_arch%': '<(host_arch)',
        }],
      ],

      # This variable is to allow us to build components as either static
      # libraries or dynamic shared libraries.
      'component%': 'static_library',
    },
    # These come from the above variable scope.
    'target_arch%': '<(target_arch)',
    'host_arch%': '<(host_arch)',
    'sysroot%': '<(sysroot)',
    'nacl_standalone%': '<(nacl_standalone)',
    'branding%': '<(branding)',
    'buildtype%': '<(buildtype)',
    'component%': '<(component)',
    'make_clang_dir%': '<(make_clang_dir)',

    'nacl_strict_warnings%': 1,

    'linux2%': 0,

    'os_posix%': '<(os_posix)',
  },

  'target_defaults': {
    'include_dirs': [
      # Putting this first so that people who include "ppapi/..." get the
      # third_party version instead of any other version (the chrome location
      # for instance).
      '../src/third_party',
      '../..',
    ],
    'variables': {
      'win_target': 'x32',
    },
    'target_conditions': [
      ['win_target=="x64"', {
        'target_arch': 'x64',
        'configurations': {
          'Common_Base': {
            'msvs_target_platform': 'x64',
          },
        },
      }],
    ],
    'conditions': [
      ['linux2==1', {
        'defines': ['LINUX2=1'],
      }],
      ['OS=="win"', {
        'defines': [ 'NOMINMAX' ]
      }],
      ['coverage!=0', {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_INSTRUMENT_PROGRAM_FLOW_ARCS': 'YES',
              'GCC_GENERATE_TEST_COVERAGE_FILES': 'YES',
            },
            # Add -lgcov for executables, not for static_libraries.
            # This is a delayed conditional.
            'target_conditions': [
              ['_type=="executable"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [ '-lgcov' ] },
              }],
            ],
          }],
          # Linux gyp (into scons) doesn't like target_conditions?
          # TODO(???): track down why 'target_conditions' doesn't work
          # on Linux gyp into scons like it does on Mac gyp into xcodeproj.
          ['OS=="linux"', {
            'cflags': [ '-ftest-coverage',
                        '-fprofile-arcs'],
            'link_settings': { 'libraries': [ '-lgcov' ] },
          }],
        ],
      # TODO(jrg): options for code coverage on Windows
      }],
      ['clang==1', {
        'cflags': [
          '-Wheader-hygiene',
          # Clang spots more unused functions.
          '-Wno-unused-function',
          # Don't die on dtoa code that uses a char as an array index.
          '-Wno-char-subscripts',
          # Especially needed for gtest macros using enum values from Mac
          # system headers.
          # TODO(pkasting): In C++11 this is legal, so this should be
          # removed when we change to that.  (This is also why we don't
          # bother fixing all these cases today.)
          '-Wno-unnamed-type-template-args',
          # Allow 'default' label in switch even when all enumeration
          # cases have been covered.
          '-Wno-covered-switch-default',
          # Allow C++11 extensions (for "override")
          '-Wno-c++11-extensions',
        ],
        'cflags!': [
          # Clang doesn't seem to know know this flag.
          '-mfpmath=sse',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="linux" or OS=="android"', {
      'target_defaults': {
        # Enable -Werror by default, but put it in a variable so it can
        # be disabled in ~/.gyp/include.gypi on the valgrind builders.
        'variables': {
          'werror%': '-Werror',
        },
        'cflags': [
          '<(werror)',  # See note above about the werror variable.
          '-pthread',
          '-fno-exceptions',
          '-Wall', # TODO(bradnelson): why does this disappear?!?
        ],
        'conditions': [
          ['nacl_standalone==1 and OS=="linux"', {
            'cflags': ['-fPIC'],
          }],
          ['OS=="android"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'defines': ['ANDROID'],
              }],
            ],
           }, {
            'link_settings': {
              'libraries': [
                '-lrt',
                '-lpthread',
              ],
            }
          }],
          ['nacl_standalone==1 and nacl_strict_warnings==1', {
            # TODO(gregoryd): remove the condition when the issues in
            # Chrome code are fixed.
            'cflags': [
              # TODO(bradnelson): This was being masked by key naming
              # collisions. Track down the items blocking enabling this.
              #'-pedantic',
              '-Wextra',
              '-Wno-long-long',
              '-Wswitch-enum',
              '-Wsign-compare',
              '-Wundef',
              '-fdiagnostics-show-option',
            ],
            'cflags_c': [
              # Require defining functions as "foo(void)" rather than
              # "foo()" because, in C (but not C++), the latter
              # defines a function with unspecified arguments rather
              # than no arguments.
              '-Wstrict-prototypes',
            ],
          }],
          ['target_arch=="arm"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '-Wno-abi',
                  '-fno-exceptions',
                  '-Wall',
                  '-fPIC',
                ],
                # TODO(mcgrathr): This is copied from the arm section of
                # chromium/src/build/common.gypi, but these details really
                # should be more fully harmonized and shared.
                'conditions': [
                  ['sysroot!=""', {
                    'cflags': [
                      '--sysroot=<(sysroot)',
                    ],
                    'ldflags': [
                      '--sysroot=<(sysroot)',
                    ],
                  }],
                  ['arm_thumb==1', {
                    'cflags': [
                      '-mthumb',
                    ]
                  }],
                  ['arm_version==7', {
                    'cflags': [
                    '-march=armv7-a',
                    '-mtune=cortex-a9',
                    '-mfloat-abi=<(arm_float_abi)',
                    ],
                    'conditions': [
                      ['arm_neon==1', {
                        'cflags': [ '-mfpu=neon', ],
                      }, {
                        'cflags': [ '-mfpu=<(arm_fpu)', ],
                      }]
                    ],
                  }],
                ],
              }],
            ],
          }],
          ['target_arch=="mipsel"', {
            'target_conditions': [
              ['_toolset=="target"', {
                # Copied from chromium build/common.gypi
                'conditions': [
                  ['mips_arch_variant=="r2"', {
                    'cflags': ['-mips32r2'],
                  }, {
                    'cflags': ['-mips32'],
                  }],
                ],
              }],
            ],
          }],
          ['target_arch=="ia32" or target_arch=="x64"', {
            'conditions': [
              ['target_arch=="x64"', {
                'variables': {
                  'mbits_flag': '-m64',
                },
              }, {
                'variables': {
                  'mbits_flag': '-m32',
                }
              },],
            ],
            'target_conditions': [
              ['_toolset=="target"', {
                'asflags': [ '<(mbits_flag)', ],
                'cflags': [ '<(mbits_flag)', ],
                'ldflags': [ '<(mbits_flag)', ],
              }],
            ],
            'cflags': [
              '-fno-exceptions',
              '-Wall',
            ],
          }],
          ['host_arch=="ia32" or host_arch=="x64"', {
            'conditions': [
              ['host_arch=="x64"', {
                'variables': { 'host_mbits_flag': '-m64', },
              }, {
                'variables': { 'host_mbits_flag': '-m32', }
              }],
            ],
            'target_conditions': [
              ['_toolset=="host"', {
                'asflags': [ '<(host_mbits_flag)', ],
                'cflags': [ '<(host_mbits_flag)', ],
                'ldflags': [ '<(host_mbits_flag)', ],
              }],
            ],
          }],
        ],
        'cflags_cc': [
          '-fno-rtti',
          '-fno-threadsafe-statics',
        ],
        'ldflags': [
          '-pthread',
          '-Wl,-z,noexecstack',
        ],
        'defines': [
          '_DEFAULT_SOURCE=1',
          '_BSD_SOURCE=1',
          '_POSIX_C_SOURCE=199506',
          '_XOPEN_SOURCE=600',
          '_GNU_SOURCE=1',
          '__STDC_LIMIT_MACROS=1',
        ],
        'configurations': {
          'Debug': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O<(debug_optimize)',
              '-g',
              # One can use '-gstabs' to enable building the debugging
              # information in STABS format for breakpad's dumpsyms.
            ],
            'ldflags': [
              '-rdynamic',  # Allows backtrace to resolve symbols.
            ],
          },
          'Release': {
            'variables': {
              'release_optimize%': '2',
            },
            'cflags': [
              '-O<(release_optimize)',
              # Don't emit the GCC version ident directives, they just end up
              # in the .comment section taking up binary size.
              '-fno-ident',
              # Put data and code in their own sections, so that unused symbols
              # can be removed at link time with --gc-sections.
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'conditions': [
              ['clang==1', {
                'cflags!': [
                  '-fno-ident',
                ],
              }],
            ],
          },
        },
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'variables': {
          # This should be 'mac_real_dsym%', but there seems to be a bug
          # with % in variables that are intended to be set to different
          # values in different targets, like this one.
          'mac_real_dsym': 0,  # Fake .dSYMs are fine in most cases.
        },
        'mac_bundle': 0,
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'gnu99',
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          'GCC_INLINES_ARE_PRIVATE_EXTERN':
            'YES',  # -fvisibility-inlines-hidden
          'GCC_OBJC_CALL_CXX_CDTORS': 'YES',        # -fobjc-call-cxx-cdtors
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          # Don't set GCC_VERSION so that we default to /usr/bin/gcc
          #'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'USE_HEADERMAP': 'NO',
          # TODO(bradnelson): -Werror ?!?
          'WARNING_CFLAGS': ['-Wall', '-Wendif-labels', '-Wno-long-long'],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}],
            ['clang==1', {
              'CC': '$(SOURCE_ROOT)/<(clang_dir)/clang',
              'LDPLUSPLUS': '$(SOURCE_ROOT)/<(clang_dir)/clang++',
              'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
              'WARNING_CFLAGS': [
                '-Wheader-hygiene',
                # Don't die on dtoa code that uses a char as an array index.
                # This is required solely for base/third_party/dmg_fp/dtoa.cc.
                '-Wno-char-subscripts',
                # Clang spots more unused functions.
                '-Wno-unused-function',
                # See comments on this flag higher up in this file.
                '-Wno-unnamed-type-template-args',
                # Allow 'default' label in switch even when all enumeration
                # cases have been covered.
                '-Wno-covered-switch-default',
                # Allow C++11 extensions (for "override")
                '-Wno-c++11-extensions',
                # TODO(thakis): Reenable once the one instance this warns on
                # is fixed.
                '-Wno-parentheses',
              ],
            }],
            ['nacl_standalone==1', {
              # If part of the Chromium build, use the Chromium default.
              # Otherwise, when building standalone, use this.
              'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)', # -mmacosx-version-min
              'SDKROOT': 'macosx<(mac_sdk)',            # -isysroot
            }],
          ],
        },
        'conditions': [
          ['clang==1', {
            'variables': {
              'clang_dir':
                '<(DEPTH)/third_party/llvm-build/Release+Asserts/bin',
            },
          }],
          ['nacl_standalone==1 and nacl_strict_warnings==1', {
            'xcode_settings': {
              # TODO(gregoryd): remove the condition when the issues in
              # Chrome code are fixed.
              'WARNING_CFLAGS': [
                '-pedantic',
                '-Wextra',
                '-Wno-long-long',
                '-Wswitch-enum',
                '-Wsign-compare',
                '-Wundef',
                '-fdiagnostics-show-option',
              ],
            },
          }],
          ['nacl_standalone==1 and target_arch=="x64"', {
            'xcode_settings': {'ARCHS': ['x86_64']},
          }],
          ['nacl_standalone==1 and target_arch=="ia32"', {
            'xcode_settings': {'ARCHS': ['i386']},
          }],
          ['nacl_standalone==1', {
            'target_conditions': [
              ['_type!="static_library"', {
                'xcode_settings': {
                  'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
              }],
              ['_mac_bundle', {
                'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
              }],
              ['_type=="executable"', {
                'target_conditions': [
                  ['mac_real_dsym == 1', {
                    # To get a real .dSYM bundle produced by dsymutil, set the
                    # debug information format to dwarf-with-dsym.  Since
                    # strip_from_xcode will not be used, set Xcode to do the
                    # stripping as well.
                    'configurations': {
                      'Release': {
                        'xcode_settings': {
                          'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                          'DEPLOYMENT_POSTPROCESSING': 'YES',
                          'STRIP_INSTALLED_PRODUCT': 'YES',
                        },
                      },
                    },
                  }, {  # mac_real_dsym != 1
                    # To get a fast fake .dSYM bundle, use a post-build step to
                    # produce the .dSYM and strip the executable.  strip_from_xcode
                    # only operates in the Release configuration.
                    'postbuilds': [
                      {
                        'variables': {
                          # Define strip_from_xcode in a variable ending in _path
                          # so that gyp understands it's a path and performs proper
                          # relativization during dict merging.
                          'strip_from_xcode_path':
                              'mac/strip_from_xcode',
                        },
                        'postbuild_name': 'Strip If Needed',
                        'action': ['<(strip_from_xcode_path)'],
                      },
                    ],
                  }],
                ],
              }],
            ],
          }],
        ],
      },
    }],
    ['OS=="win"', {
      'target_defaults': {
        'variables': {
          'windows_asm_rule%': 1,
        },
        'target_conditions': [
          ['windows_asm_rule==1', {
            'rules': [
              {
                'rule_name': 'assembler (gnu-compatible)',
                'msvs_cygwin_shell': 0,
                'msvs_quote_cmd': 0,
                'extension': 'S',
                'inputs': [
                  '<(DEPTH)/native_client/tools/win_as.py',
                ],
                'outputs': [
                  '$(IntDir)/$(InputName).obj',
                ],
                'action': [
                   'python',
                   '<(DEPTH)/native_client/tools/win_as.py',
                   # target architecture: Win32 or x64
                   '-a', '$(PlatformName)',
                   # output path
                   '-o', '$(IntDir)/$(InputName).obj',
                   # path to top of tree, e.g. svn/nacl
                   '-p', '<(DEPTH)',
                   # .S file
                  '$(InputPath)'
                ],
                'message': 'Building assembly language file $(InputPath)',
                'process_outputs_as_sources': 1,
              },
            ],
          }],
        ],
        'defines': [
          '_WIN32_WINNT=0x0603',
          'WINVER=0x0603',
          # WIN32 is used by ppapi
          'WIN32',
          'NOMINMAX',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          # WIN32_LEAN_AND_MEAN tells windows.h to omit obsolete and rarely
          # used #include files. This allows use of Winsock 2.0 which otherwise
          # would conflict with Winsock 1.x included by windows.h.
          'WIN32_LEAN_AND_MEAN',
          '_SECURE_ATL',
          '__STDC_LIMIT_MACROS=1',
          '_HAS_EXCEPTIONS=0',
        ],
        'conditions': [
          ['MSVS_VERSION=="2008"', {
            'defines': [
              '_HAS_TR1=0',
            ],
          }],
        ],
        'msvs_cygwin_dirs': ['../third_party/cygwin'],
        # TODO(bsy) remove 4355 once cross-repo
        # NACL_ALLOW_THIS_IN_INITIALIZER_LIST changes go in.
        'msvs_disabled_warnings': [4355, 4396, 4503, 4800, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'ExceptionHandling': '0',
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
          },
          'VCLinkerTool': {
            'AdditionalOptions': [
              '/safeseh:NO',
              '/dynamicbase:NO',
              '/ignore:4199',
              '/ignore:4221',
              '/nxcompat',
            ],
            'AdditionalDependencies': [
              'advapi32.lib',
              'wininet.lib',
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
              'dbghelp.lib',
            ],
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'uxtheme.dll',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'TargetMachine': '1',
            'FixedBaseAddress': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            # Most of the executables we'll ever create are tests
            # and utilities with console output.
            'SubSystem': '1',
          },
          'VCMIDLTool': {
            'GenerateStublessProxies': 'true',
            'TypeLibraryName': '$(InputName).tlb',
            'OutputDirectory': '$(IntDir)',
            'HeaderFileName': '$(InputName).h',
            'DLLDataFileName': 'dlldata.c',
            'InterfaceIdentifierFileName': '$(InputName)_i.c',
            'ProxyFileName': '$(InputName)_p.c',
          },
          'VCResourceCompilerTool': {
            'Culture' : '1033',
            'AdditionalIncludeDirectories': ['<(DEPTH)'],
          },
        },
      },
    }],
    ['chromium_code==0 and nacl_standalone==0', {
      # This section must follow the other conditon sections above because
      # external_code.gypi expects to be merged into those settings.
      'includes': [
        'external_code.gypi',
      ],
    }, {
      'target_defaults': {
        # In Chromium code, we define __STDC_FORMAT_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_FORMAT_MACROS',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Detect64BitPortabilityProblems': 'false',
            # TODO(new_hire): above line should go away
          },
        },
      },
    }],
    ['clang==1 and nacl_standalone==1', {
      'make_global_settings': [
        ['CC', '<(make_clang_dir)/bin/clang'],
        ['CXX', '<(make_clang_dir)/bin/clang++'],
      ],
    }],
    ['OS=="linux" and target_arch=="mipsel" and clang==0 and nacl_standalone==1', {
      'make_global_settings': [
        ['CC', '<!(which mipsel-linux-gnu-gcc)'],
        ['CXX', '<!(which mipsel-linux-gnu-g++)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    ['OS=="linux" and target_arch=="arm" and host_arch!="arm" and clang==0 and nacl_standalone==1', {
      # Set default ARM cross compiling on linux.  These can be overridden
      # using CC/CXX/etc environment variables.
      'make_global_settings': [
        ['CC', '<!(which arm-linux-gnueabihf-gcc)'],
        ['CXX', '<!(which arm-linux-gnueabihf-g++)'],
        ['CC.host', '<(host_cc)'],
        ['CXX.host', '<(host_cxx)'],
      ],
    }],
    ['OS=="android" and nacl_standalone==1', {
      'includes': [
        'android_settings.gypi',
      ],
    }],  # OS=="android" and nacl_standalone==1
  ],
  'xcode_settings': {
    # The Xcode generator will look for an xcode_settings section at the root
    # of each dict and use it to apply settings on a file-wide basis.  Most
    # settings should not be here, they should be in target-specific
    # xcode_settings sections, or better yet, should use non-Xcode-specific
    # settings in target dicts.  SYMROOT is a special case, because many other
    # Xcode variables depend on it, including variables such as
    # PROJECT_DERIVED_FILE_DIR.  When a source group corresponding to something
    # like PROJECT_DERIVED_FILE_DIR is added to a project, in order for the
    # files to appear (when present) in the UI as actual files and not red
    # red "missing file" proxies, the correct path to PROJECT_DERIVED_FILE_DIR,
    # and therefore SYMROOT, needs to be set at the project level.
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
  'includes': [
    'untrusted.gypi',
  ],
}
