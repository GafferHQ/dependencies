# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'nacl_win64_target': 0,
  },
  'includes': [
    'gpu_common.gypi',
  ],
  'targets': [
    {
      # Library emulates GLES2 using command_buffers.
      # GN version: //gpu/command_buffer/client:gles2_implementation
      'target_name': 'gles2_implementation',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'gles2_cmd_helper',
      ],
      'defines': [
        'GLES2_IMPL_IMPLEMENTATION',
      ],
      'sources': [
        '<@(gles2_implementation_source_files)',
      ],
      'includes': [
        # Disable LTO due to ELF section name out of range
        # crbug.com/422251
        '../build/android/disable_lto.gypi',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //gpu/command_buffer/client:gl_in_process_context
      'target_name': 'gl_in_process_context',
      'type': '<(component)',
      'dependencies': [
        'command_buffer/command_buffer.gyp:gles2_utils',
        'gles2_implementation',
        'gpu',
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
      ],
      'defines': [
        'GL_IN_PROCESS_CONTEXT_IMPLEMENTATION',
      ],
      'sources': [
        'command_buffer/client/gl_in_process_context.cc',
        'command_buffer/client/gl_in_process_context.h',
        'command_buffer/client/gl_in_process_context_export.h',
      ],
    },
    {
      # Library emulates GLES2 using command_buffers.
      'target_name': 'gles2_implementation_no_check',
      'type': '<(component)',
      'defines': [
        'GLES2_IMPL_IMPLEMENTATION',
        'GLES2_CONFORMANCE_TESTS=1',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'gles2_cmd_helper',
      ],
      'sources': [
        '<@(gles2_implementation_source_files)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # Stub to expose gles2_implemenation in C instead of C++.
      # so GLES2 C programs can work with no changes.
      # GN version: //gpu/command_buffer/client:gles2_c_lib
      'target_name': 'gles2_c_lib',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'command_buffer_client',
      ],
      'export_dependent_settings': [
        'command_buffer_client',
      ],
      'defines': [
        'GLES2_C_LIB_IMPLEMENTATION',
      ],
      'sources': [
        '<@(gles2_c_lib_source_files)',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      # GN version: //gpu/command_buffer/client:gles2_c_lib_nocheck
      # Same as gles2_c_lib except with no parameter checking. Required for
      # OpenGL ES 2.0 conformance tests.
      'target_name': 'gles2_c_lib_nocheck',
      'type': '<(component)',
      'defines': [
        'GLES2_C_LIB_IMPLEMENTATION',
        'GLES2_CONFORMANCE_TESTS=1',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'command_buffer_client',
        'gles2_implementation_no_check',
      ],
      'export_dependent_settings': [
        'command_buffer_client',
      ],
      'sources': [
        '<@(gles2_c_lib_source_files)',
      ],
    },
    {
      # GN version: //gpu:angle_unittests
      # TODO(kbr): port this refactoring to the GN build.
      'target_name': 'angle_unittests',
      'type': '<(gtest_target_type)',
      'includes': [
        '../third_party/angle/build/common_defines.gypi',
        '../third_party/angle/src/tests/angle_unittests.gypi',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
      ],
      'include_dirs': [
        '..',
        '../third_party/angle/include',
      ],
      'sources': [
        'angle_unittest_main.cc',
      ],
    },
    {
      # GN version: //gpu:gpu_unittests
      'target_name': 'gpu_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '<(angle_path)/src/angle.gyp:translator',
        '../ui/gl/gl.gyp:gl',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'command_buffer_client',
        'command_buffer_common',
        'command_buffer_service',
        'gpu',
        'gpu_unittest_utils',
        'gles2_implementation',
        'gles2_cmd_helper',
        'gles2_c_lib',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'command_buffer/client/buffer_tracker_unittest.cc',
        'command_buffer/client/client_test_helper.cc',
        'command_buffer/client/client_test_helper.h',
        'command_buffer/client/cmd_buffer_helper_test.cc',
        'command_buffer/client/fenced_allocator_test.cc',
        'command_buffer/client/gles2_implementation_unittest.cc',
        'command_buffer/client/mapped_memory_unittest.cc',
        'command_buffer/client/program_info_manager_unittest.cc',
        'command_buffer/client/query_tracker_unittest.cc',
        'command_buffer/client/ring_buffer_test.cc',
        'command_buffer/client/transfer_buffer_unittest.cc',
        'command_buffer/client/vertex_array_object_manager_unittest.cc',
        'command_buffer/common/bitfield_helpers_test.cc',
        'command_buffer/common/command_buffer_mock.cc',
        'command_buffer/common/command_buffer_mock.h',
        'command_buffer/common/command_buffer_shared_test.cc',
        'command_buffer/common/debug_marker_manager_unittest.cc',
        'command_buffer/common/gles2_cmd_format_test.cc',
        'command_buffer/common/gles2_cmd_format_test_autogen.h',
        'command_buffer/common/gles2_cmd_utils_unittest.cc',
        'command_buffer/common/id_allocator_test.cc',
        'command_buffer/common/trace_event.h',
        'command_buffer/common/unittest_main.cc',
        'command_buffer/service/async_pixel_transfer_delegate_mock.cc',
        'command_buffer/service/async_pixel_transfer_delegate_mock.h',
        'command_buffer/service/async_pixel_transfer_manager_mock.cc',
        'command_buffer/service/async_pixel_transfer_manager_mock.h',
        'command_buffer/service/buffer_manager_unittest.cc',
        'command_buffer/service/cmd_parser_test.cc',
        'command_buffer/service/command_buffer_service_unittest.cc',
        'command_buffer/service/common_decoder_unittest.cc',
        'command_buffer/service/context_group_unittest.cc',
        'command_buffer/service/context_state_unittest.cc',
        'command_buffer/service/feature_info_unittest.cc',
        'command_buffer/service/framebuffer_manager_unittest.cc',
        'command_buffer/service/gl_context_mock.cc',
        'command_buffer/service/gl_context_mock.h',
        'command_buffer/service/gl_surface_mock.cc',
        'command_buffer/service/gl_surface_mock.h',
        'command_buffer/service/gles2_cmd_decoder_unittest.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_0_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_1.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_1_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_2.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_3.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_3_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_async_pixel.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_attribs.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_base.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_base.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_buffers.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_context_lost.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_context_state.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_drawing.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_extensions.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_extensions_autogen.h',
        'command_buffer/service/gles2_cmd_decoder_unittest_framebuffers.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_programs.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_textures.cc',
        'command_buffer/service/gles2_cmd_decoder_unittest_valuebuffer.cc',
        'command_buffer/service/gpu_scheduler_unittest.cc',
        'command_buffer/service/gpu_service_test.cc',
        'command_buffer/service/gpu_service_test.h',
        'command_buffer/service/gpu_tracer_unittest.cc',
        'command_buffer/service/id_manager_unittest.cc',
        'command_buffer/service/mailbox_manager_unittest.cc',
        'command_buffer/service/memory_program_cache_unittest.cc',
        'command_buffer/service/mocks.cc',
        'command_buffer/service/mocks.h',
        'command_buffer/service/program_cache_unittest.cc',
        'command_buffer/service/program_manager_unittest.cc',
        'command_buffer/service/query_manager_unittest.cc',
        'command_buffer/service/renderbuffer_manager_unittest.cc',
        'command_buffer/service/shader_manager_unittest.cc',
        'command_buffer/service/shader_translator_cache_unittest.cc',
        'command_buffer/service/shader_translator_unittest.cc',
        'command_buffer/service/test_helper.cc',
        'command_buffer/service/test_helper.h',
        'command_buffer/service/texture_manager_unittest.cc',
        'command_buffer/service/transfer_buffer_manager_unittest.cc',
        'command_buffer/service/valuebuffer_manager_unittest.cc',
        'command_buffer/service/vertex_array_manager_unittest.cc',
        'command_buffer/service/vertex_attrib_manager_unittest.cc',
        'config/gpu_blacklist_unittest.cc',
        'config/gpu_control_list_entry_unittest.cc',
        'config/gpu_control_list_number_info_unittest.cc',
        'config/gpu_control_list_os_info_unittest.cc',
        'config/gpu_control_list_unittest.cc',
        'config/gpu_control_list_version_info_unittest.cc',
        'config/gpu_driver_bug_list_unittest.cc',
        'config/gpu_info_collector_unittest.cc',
        'config/gpu_info_unittest.cc',
        'config/gpu_test_config_unittest.cc',
        'config/gpu_test_expectations_parser_unittest.cc',
        'config/gpu_util_unittest.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and use_allocator!="none"', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //gpu/gpu_perftests
      'target_name': 'gpu_perftests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../testing/perf/perf_test.gyp:perf_test',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'command_buffer_service',
      ],
      'sources': [
        'perftests/measurements.cc',
        'perftests/run_all_tests.cc',
        'perftests/texture_upload_perftest.cc',
      ],
      'conditions': [
        ['OS == "android"',
          {
            'dependencies': [
              '../testing/android/native_test.gyp:native_test_native_code',
            ],
          }
        ],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and use_allocator!="none"',
          {
            'dependencies': [
              '../base/allocator/allocator.gyp:allocator',
            ],
          }
        ],
      ],
    },
    {
      # GN version: //gpu:gl_tests
      'target_name': 'gl_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '<(angle_path)/src/angle.gyp:translator',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'command_buffer/command_buffer.gyp:gles2_utils',
        'command_buffer_client',
        'command_buffer_common',
        'command_buffer_service',
        'gpu',
        'gpu_unittest_utils',
        'gles2_implementation',
        'gles2_cmd_helper',
        'gles2_c_lib',
        #'gl_unittests',
      ],
      'defines': [
        'GL_GLEXT_PROTOTYPES',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'command_buffer/tests/compressed_texture_test.cc',
        'command_buffer/tests/gl_bind_uniform_location_unittest.cc',
        'command_buffer/tests/gl_chromium_framebuffer_multisample_unittest.cc',
        'command_buffer/tests/gl_chromium_path_rendering_unittest.cc',
        'command_buffer/tests/gl_clear_framebuffer_unittest.cc',
        'command_buffer/tests/gl_compressed_copy_texture_CHROMIUM_unittest.cc',
        'command_buffer/tests/gl_copy_texture_CHROMIUM_unittest.cc',
        'command_buffer/tests/gl_depth_texture_unittest.cc',
        'command_buffer/tests/gl_gpu_memory_buffer_unittest.cc',
        'command_buffer/tests/gl_lose_context_chromium_unittest.cc',
        'command_buffer/tests/gl_manager.cc',
        'command_buffer/tests/gl_manager.h',
        'command_buffer/tests/gl_pointcoord_unittest.cc',
        'command_buffer/tests/gl_program_unittest.cc',
        'command_buffer/tests/gl_query_unittest.cc',
        'command_buffer/tests/gl_readback_unittest.cc',
        'command_buffer/tests/gl_shared_resources_unittest.cc',
        'command_buffer/tests/gl_stream_draw_unittest.cc',
        'command_buffer/tests/gl_test_utils.cc',
        'command_buffer/tests/gl_test_utils.h',
        'command_buffer/tests/gl_tests_main.cc',
        'command_buffer/tests/gl_texture_mailbox_unittest.cc',
        'command_buffer/tests/gl_texture_storage_unittest.cc',
        'command_buffer/tests/gl_unittest.cc',
        'command_buffer/tests/gl_unittests_android.cc',
        'command_buffer/tests/gl_virtual_contexts_unittest.cc',
        'command_buffer/tests/occlusion_query_unittest.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        ['OS == "win" and use_qt==0', {
          'dependencies': [
            '../third_party/angle/src/angle.gyp:libEGL',
            '../third_party/angle/src/angle.gyp:libGLESv2',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //gpu:test_support
      'target_name': 'gpu_unittest_utils',
      'type': 'static_library',
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        '../ui/gl/gl.gyp:gl_unittest_utils',
        'gpu',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'command_buffer/client/gles2_interface_stub.cc',
        'command_buffer/client/gles2_interface_stub.h',
        'command_buffer/service/error_state_mock.cc',
        'command_buffer/service/gles2_cmd_decoder_mock.cc',
      ],
    },
  ],
  'conditions': [
    ['component=="static_library"', {
      'targets': [
         {
          # GN version: //gpu/command_buffer/service:disk_cache_proto
          'target_name': 'disk_cache_proto',
          'type': 'static_library',
          'sources': [ 'command_buffer/service/disk_cache_proto.proto' ],
          'variables': {
            'proto_in_dir': 'command_buffer/service',
            'proto_out_dir': 'gpu/command_buffer/service',
          },
          'includes': [ '../build/protoc.gypi' ],
        },
        {
          # GN version: //gpu
          'target_name': 'gpu',
          'type': 'none',
          'dependencies': [
            'command_buffer_client',
            'command_buffer_common',
            'command_buffer_service',
            'gles2_cmd_helper',
            'gpu_config',
            'gpu_ipc',
          ],
          'export_dependent_settings': [
            'command_buffer_common',
          ],
          'sources': [
            'gpu_export.h',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/command_buffer/common
          'target_name': 'command_buffer_common',
          'type': 'static_library',
          'includes': [
            'command_buffer_common.gypi',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'command_buffer/command_buffer.gyp:gles2_utils',
          ],
          'export_dependent_settings': [
            '../base/base.gyp:base',
          ],
        },
        {
          # Library helps make GLES2 command buffers.
          # GN version: //gpu/command_buffer/client:gles2_cmd_helper
          'target_name': 'gles2_cmd_helper',
          'type': 'static_library',
          'includes': [
            'gles2_cmd_helper.gypi',
          ],
          'dependencies': [
            'command_buffer_client',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/command_buffer/client
          'target_name': 'command_buffer_client',
          'type': 'static_library',
          'includes': [
            'command_buffer_client.gypi',
          ],
          'dependencies': [
            'command_buffer_common',
          ],
          'export_dependent_settings': [
            'command_buffer_common',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/command_buffer/service
          'target_name': 'command_buffer_service',
          'type': 'static_library',
          'includes': [
            'command_buffer_service.gypi',
            '../build/android/increase_size_for_speed.gypi',
            # Disable LTO due to ELF section name out of range
            # crbug.com/422251
            '../build/android/disable_lto.gypi',
          ],
          'dependencies': [
            'command_buffer_common',
            'disk_cache_proto',
            'gpu_config',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/ipc
          'target_name': 'gpu_ipc',
          'type': 'static_library',
          'includes': [
            'gpu_ipc.gypi',
          ],
          'dependencies': [
            'command_buffer_common',
          ],
          'export_dependent_settings': [
            'command_buffer_common',
          ],
        },
        {
          'target_name': 'gpu_config',
          'type': 'static_library',
          'dependencies': [
            '../third_party/khronos/khronos.gyp:khronos_headers',
          ],
          'includes': [
            'gpu_config.gypi',
          ],
        },
      ],
    },
    { # component != static_library
      'targets': [
         {
          # GN version: //gpu/command_buffer/service:disk_cache_proto
          'target_name': 'disk_cache_proto',
          'type': 'static_library',
          'sources': [ 'command_buffer/service/disk_cache_proto.proto' ],
          'variables': {
            'proto_in_dir': 'command_buffer/service',
            'proto_out_dir': 'gpu/command_buffer/service',
          },
          'includes': [ '../build/protoc.gypi' ],
        },
        {
          # GN version: //gpu
          'target_name': 'gpu',
          'type': 'shared_library',
          'includes': [
            'command_buffer_client.gypi',
            'command_buffer_common.gypi',
            'command_buffer_service.gypi',
            'gles2_cmd_helper.gypi',
            'gpu_config.gypi',
            'gpu_ipc.gypi',
            '../build/android/increase_size_for_speed.gypi',
          ],
          'defines': [
            'GPU_IMPLEMENTATION',
          ],
          'sources': [
            'gpu_export.h',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'command_buffer/command_buffer.gyp:gles2_utils',
            'disk_cache_proto',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/command_buffer/common
          'target_name': 'command_buffer_common',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          # Library helps make GLES2 command buffers.
          # GN version: //gpu/command_buffer/client:gles2_cmd_helper
          'target_name': 'gles2_cmd_helper',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          # GN version: //gpu/command_buffer/client
          'target_name': 'command_buffer_client',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          # GN version: //gpu/command_buffer/service
          'target_name': 'command_buffer_service',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
        {
          # GN version: //gpu/ipc
          'target_name': 'gpu_ipc',
          'type': 'none',
          'dependencies': [
            'gpu',
          ],
        },
      ],
    }],
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'command_buffer_common_win64',
          'type': 'static_library',
          'variables': {
            'nacl_win64_target': 1,
          },
          'includes': [
            'command_buffer_common.gypi',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
            'GPU_IMPLEMENTATION',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'gpu_ipc_win64',
          'type': 'static_library',
          'variables': {
            'nacl_win64_target': 1,
          },
          'includes': [
            'gpu_ipc.gypi',
          ],
          'dependencies': [
            '../base/base.gyp:base_win64',
            '../ipc/ipc.gyp:ipc_win64',
            'command_buffer_common_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
            'GPU_IMPLEMENTATION',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'gl_tests_apk',
          'type': 'none',
          'dependencies': [
            'gl_tests',
          ],
          'variables': {
            'test_suite_name': 'gl_tests',
          },
          'includes': [
            '../build/apk_test.gypi',
          ],
        },
        {
          'target_name': 'gpu_unittests_apk',
          'type': 'none',
          'dependencies': [
            'gpu_unittests',
          ],
          'variables': {
            'test_suite_name': 'gpu_unittests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
        {
          'target_name': 'gpu_perftests_apk',
          'type': 'none',
          'dependencies': [
            'gpu_perftests',
          ],
          'variables': {
            'test_suite_name': 'gpu_perftests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
    ['OS == "win" or (OS == "linux" and use_x11==1)', {
      'targets': [
        {
          # TODO(kbr): port this target to the GN build.
          'target_name': 'angle_end2end_tests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'includes': [
            '../third_party/angle/build/common_defines.gypi',
            '../third_party/angle/src/tests/angle_end2end_tests.gypi',
          ],
          'sources': [
            'angle_end2end_tests_main.cc',
          ],
        },
      ],
    }],
    ['OS == "win"', {
      'targets': [
        {
          # TODO(jmadill): port this target to the GN build.
          'target_name': 'angle_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'includes': [
            '../third_party/angle/build/common_defines.gypi',
            '../third_party/angle/src/tests/angle_perftests.gypi',
          ],
          'sources': [
            'angle_perftests_main.cc',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'gpu_unittests_run',
          'type': 'none',
          'dependencies': [
            'gpu_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'gpu_unittests.isolate',
          ],
          'conditions': [
            ['use_x11==1',
              {
                'dependencies': [
                  '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
                ],
              }
            ],
          ],
        },
      ],
    }],
    ['OS == "win" and archive_gpu_tests==1', {
      'targets': [
        {
          # Only build dEQP on test configs. Note that dEQP is test-only code,
          # and is only a part of the Chromium build to allow easy integration
          # with the GPU bot waterfall. (Note that dEQP uses exceptions, and
          # currently can't build with Clang on Windows)
          'target_name': 'angle_deqp_tests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../third_party/angle/src/tests/tests.gyp:angle_deqp_gtest_support',
          ],
          'includes': [
            '../third_party/angle/build/common_defines.gypi',
          ],
          'sources': [
            'angle_deqp_tests_main.cc',
          ],
        },
      ],
    }]
  ],
}
