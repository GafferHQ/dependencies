# Copyright 2014 PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Definitions to be used when building stand-alone PDFium binaries.

{
  'variables': {
    'component%': 'static_library',
    'clang%': 0,
    'msvs_multi_core_compile%': '1',
    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              # This handles the Unix platforms we generally deal with.
              # Anything else gets passed through, which probably won't work
              # very well; such hosts should pass an explicit target_arch
              # to gyp.
              'host_arch%':
                '<!(uname -m | sed -e "s/i.86/ia32/;\
                                       s/x86_64/x64/;\
                                       s/amd64/x64/;\
                                       s/arm.*/arm/;\
                                       s/aarch64/arm64/;\
                                       s/mips.*/mipsel/")',
            }, {
              # OS!="linux" and OS!="mac"
              'host_arch%': 'ia32',
            }],
          ],
        },
        'host_arch%': '<(host_arch)',
        'target_arch%': '<(host_arch)',
      },
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
    },
    # These two are needed by V8.
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(target_arch)',
    'werror%': '-Werror',
    'v8_optimized_debug%': 0,
    'v8_use_external_startup_data%': 0,
    'icu_gyp_path': '../v8/third_party/icu/icu.gyp',
    'conditions': [
      ['OS == "win"', {
        'os_posix%': 0,
      }, {
        'os_posix%': 1,
      }],
    ],
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'cflags': [
          '-g',
          '-O0',
          '-fdata-sections',
          '-ffunction-sections',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '0',  # -O0
        },
      },
      'Release': {
        'cflags': [
          '-fno-strict-aliasing',
        ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
          'GCC_STRICT_ALIASING': 'NO',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'InlineFunctionExpansion': '2',
            'EnableIntrinsicFunctions': 'true',
            'FavorSizeOrSpeed': '0',
            'StringPooling': 'true',
            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '2',  #/MD
              }, {
                'RuntimeLibrary': '0',  #/MT
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
            'OptimizeReferences': '2',
            'EnableCOMDATFolding': '2',
          },
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-O3',
              '-O2',
            ],
          }],
          ['OS=="android"', {
            'cflags!': [
              '-O3',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-O2',
            ],
          }],
        ],  # conditions
      },
      'Debug_x64': {
        'inherit_from': ['Debug'],
        'msvs_configuration_platform': 'x64',
      },
      'Release_x64': {
        'inherit_from': ['Release'],
        'msvs_configuration_platform': 'x64',
      },
    },
    'cflags': [
      '-Wall',
      '-W',
      '-Wno-missing-field-initializers',
      # Code might someday be made clean for -Wsign-compare, but for now
      # this produces too much noise to be useful.
      '-Wno-sign-compare',
      '-Wno-unused-parameter',
      '-pthread',
      '-fno-exceptions',
      '-fvisibility=hidden',
    ],
    'cflags_cc': [
      '-std=gnu++0x',
      '-Wnon-virtual-dtor',
      '-fno-rtti',
    ],
    'ldflags': [
      '-pthread',
    ],
    'defines': [
      # Don't use deprecated V8 APIs anywhere.
      'V8_DEPRECATION_WARNINGS',
    ],
    'msvs_cygwin_dirs': ['<(DEPTH)/v8/third_party/cygwin'],
    'msvs_configuration_attributes': {
      'OutputDirectory': '<(DEPTH)\\build\\$(ConfigurationName)',
      'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
      'CharacterSet': '1',
    },
    'msvs_disabled_warnings': [4800, 4996, 4456, 4457, 4458, 4459, 4091],
    # 4456, 4457, 4458, 4459 are variable shadowing warnings that are new in
    # VS2015.
    # C4091: 'typedef ': ignored on left of 'X' when no variable is
    #                    declared.
    # This happens in a number of Windows headers with VS 2015.
    'msvs_settings': {
      'VCCLCompilerTool': {
        'MinimalRebuild': 'false',
        'BufferSecurityCheck': 'true',
        'EnableFunctionLevelLinking': 'true',
        'RuntimeTypeInfo': 'false',
        'WarningLevel': '3',
        'DebugInformationFormat': '3',
        'Detect64BitPortabilityProblems': 'false',
        'conditions': [
          [ 'msvs_multi_core_compile', {
            'AdditionalOptions': ['/MP'],
          }],
          ['component=="shared_library"', {
            'ExceptionHandling': '1',  # /EHsc
          }, {
            'ExceptionHandling': '0',
          }],
          ['target_arch=="x64"', {
            # 64-bit warnings need to be resolved.
            # https://code.google.com/p/pdfium/issues/detail?id=101
            'WarnAsError': 'false',
          }, {
            'WarnAsError': 'true',
          }],
        ],
      },
      'VCLibrarianTool': {
        'AdditionalOptions': ['/ignore:4221'],
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
        'LinkIncremental': '1',
        # SubSystem values:
        #   0 == not set
        #   1 == /SUBSYSTEM:CONSOLE
        #   2 == /SUBSYSTEM:WINDOWS
        'SubSystem': '1',
      },
    },
    'xcode_settings': {
      'ALWAYS_SEARCH_USER_PATHS': 'NO',
      'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++11',
      'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
      'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                # (Equivalent to -fPIC)
      'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
      'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
      'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
      # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
      'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
      'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
      'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',     # -Werror
      'GCC_WARN_NON_VIRTUAL_DESTRUCTOR': 'YES', # -Wnon-virtual-dtor
      'SYMROOT': '<(DEPTH)/xcodebuild',
      'USE_HEADERMAP': 'NO',
      'OTHER_CFLAGS': [
        '-fno-strict-aliasing',
      ],
      'WARNING_CFLAGS': [
        '-Wall',
        '-Wendif-labels',
        '-W',
        '-Wno-unused-parameter',
      ],
    },
  },
  'conditions': [
    ['component=="shared_library"', {
      'cflags': [
        '-fPIC',
      ],
    }],
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          'NOMINMAX',
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
        ],
      },
    }],  # OS=="win"
    ['OS=="mac"', {
      'target_defaults': {
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
    ['v8_use_external_startup_data==1', {
      'target_defaults': {
        'defines': [
          'V8_USE_EXTERNAL_STARTUP_DATA',
        ],
      },
    }],  # v8_use_external_startup_data==1
  ],
  'xcode_settings': {
    # See comment in Chromium's common.gypi for why this is needed.
    'SYMROOT': '<(DEPTH)/xcodebuild',
  }
}
