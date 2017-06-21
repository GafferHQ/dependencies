{
    # This asks gyp to generate a .pri file with linking
    # information so that qmake can take care of the deployment.
    'let_qmake_do_the_linking': 1,
    'variables': {
      'version_script_location%': '<(chromium_src_dir)/build/util/version.py',
    },
    'dependencies': [
      '<(chromium_src_dir)/base/base.gyp:base',
      '<(chromium_src_dir)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      '<(chromium_src_dir)/components/components.gyp:devtools_discovery',
      '<(chromium_src_dir)/components/components.gyp:devtools_http_handler',
      '<(chromium_src_dir)/components/components.gyp:error_page_renderer',
      '<(chromium_src_dir)/components/components.gyp:visitedlink_browser',
      '<(chromium_src_dir)/components/components.gyp:visitedlink_renderer',
      '<(chromium_src_dir)/components/components.gyp:web_cache_browser',
      '<(chromium_src_dir)/components/components.gyp:web_cache_renderer',
      '<(chromium_src_dir)/content/content.gyp:content',
      '<(chromium_src_dir)/content/content.gyp:content_app_browser',
      '<(chromium_src_dir)/content/content.gyp:content_browser',
      '<(chromium_src_dir)/content/content.gyp:content_common',
      '<(chromium_src_dir)/content/content.gyp:content_gpu',
      '<(chromium_src_dir)/content/content.gyp:content_ppapi_plugin',
      '<(chromium_src_dir)/content/content.gyp:content_renderer',
      '<(chromium_src_dir)/content/content.gyp:content_utility',
      '<(chromium_src_dir)/content/app/resources/content_resources.gyp:content_resources',
      '<(chromium_src_dir)/ipc/ipc.gyp:ipc',
      '<(chromium_src_dir)/media/media.gyp:media',
      '<(chromium_src_dir)/net/net.gyp:net',
      '<(chromium_src_dir)/net/net.gyp:net_resources',
      '<(chromium_src_dir)/net/net.gyp:net_with_v8',
      '<(chromium_src_dir)/skia/skia.gyp:skia',
      '<(chromium_src_dir)/third_party/WebKit/Source/web/web.gyp:blink_web',
      '<(chromium_src_dir)/ui/base/ui_base.gyp:ui_base',
      '<(chromium_src_dir)/ui/gl/gl.gyp:gl',
      '<(chromium_src_dir)/url/url.gyp:url_lib',
      '<(chromium_src_dir)/v8/tools/gyp/v8.gyp:v8',

      'chrome_qt.gyp:chrome_qt',
    ],
    'include_dirs': [
      '<(chromium_src_dir)',
      '<(SHARED_INTERMEDIATE_DIR)/net', # Needed to include grit/net_resources.h
      '<(SHARED_INTERMEDIATE_DIR)/chrome', # Needed to include grit/generated_resources.h
    ],
    # Chromium code defines those in common.gypi, do the same for our code that include Chromium headers.
    'defines': [
      '__STDC_CONSTANT_MACROS',
      '__STDC_FORMAT_MACROS',
      'CHROMIUM_VERSION=\"<!(python <(version_script_location) -f <(chromium_src_dir)/chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")\"',
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'RuntimeTypeInfo': 'true',
      },
      'VCLinkerTool': {
        'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
      },
    },
    'conditions': [
      ['OS=="win" and win_use_allocator_shim==1', {
        'dependencies': [
          '<(chromium_src_dir)/base/allocator/allocator.gyp:allocator',
        ],
      }],
      # embedded_linux need some additional options.
      ['qt_os=="embedded_linux"', {
        'configurations': {
          'Debug_Base': {
            # Reduce the binary size.
            'variables': {
              'debug_optimize%': 's',
            },
            'ldflags': [
              # Only link with needed input sections.
              '-Wl,--gc-sections',
              # Warn in case of text relocations.
              '-Wl,--warn-shared-textrel',
              '-Wl,-O1',
              '-Wl,--as-needed',
            ],
            'cflags': [
              '-fomit-frame-pointer',
              '-fdata-sections',
              '-ffunction-sections',
            ],
          },
        },
        'dependencies': [
          '<(chromium_src_dir)/ui/events/ozone/events_ozone.gyp:events_ozone_evdev'
        ]
      }],
      ['qt_os=="win32" and qt_gl=="opengl"', {
        'include_dirs': [
          '<(chromium_src_dir)/third_party/khronos',
        ],
      }],
      ['OS=="win"', {
        'resource_include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
        'configurations': {
          'Debug_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
              },
            },
          },
        },
        # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
        'msvs_disabled_warnings': [ 4267, 4996, ],
      }],  # OS=="win"
      ['OS=="linux"', {
        'dependencies': [
          '<(chromium_src_dir)/build/linux/system.gyp:fontconfig',
        ],
      }],
      ['OS=="mac"', {
        'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
      }],
      ['icu_use_data_file_flag==1', {
        'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE'],
      }, { # else icu_use_data_file_flag !=1
        'conditions': [
          ['OS=="win"', {
            'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_SHARED'],
          }, {
            'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC'],
          }],
        ],
      }],
    ],
}
