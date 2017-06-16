// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_settings.h"

#include <GLES2/gl2.h>
#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace cc {

LayerSettings::LayerSettings() : use_compositor_animation_timelines(false) {
}

LayerSettings::~LayerSettings() {
}

LayerTreeSettings::LayerTreeSettings()
    : single_thread_proxy_scheduler(true),
      use_external_begin_frame_source(false),
      main_frame_before_activation_enabled(false),
      using_synchronous_renderer_compositor(false),
      report_overscroll_only_for_scrollable_axes(false),
      accelerated_animation_enabled(true),
      can_use_lcd_text(true),
      use_distance_field_text(false),
      gpu_rasterization_enabled(false),
      gpu_rasterization_forced(false),
      gpu_rasterization_msaa_sample_count(0),
      gpu_rasterization_skewport_target_time_in_seconds(0.2f),
      create_low_res_tiling(false),
      scrollbar_animator(NO_ANIMATOR),
      scrollbar_fade_delay_ms(0),
      scrollbar_fade_resize_delay_ms(0),
      scrollbar_fade_duration_ms(0),
      scrollbar_show_scale_threshold(1.0f),
      solid_color_scrollbar_color(SK_ColorWHITE),
      timeout_and_draw_when_animation_checkerboards(true),
      layer_transforms_should_scale_layer_contents(false),
      layers_always_allowed_lcd_text(false),
      minimum_contents_scale(0.0625f),
      low_res_contents_scale_factor(0.25f),
      top_controls_show_threshold(0.5f),
      top_controls_hide_threshold(0.5f),
      background_animation_rate(1.0),
      max_partial_texture_updates(std::numeric_limits<size_t>::max()),
      default_tile_size(gfx::Size(256, 256)),
      max_untiled_layer_size(gfx::Size(512, 512)),
      default_tile_grid_size(gfx::Size(256, 256)),
      minimum_occlusion_tracking_size(gfx::Size(160, 160)),
      // At 256x256 tiles, 128 tiles cover an area of 2048x4096 pixels.
      max_tiles_for_interest_area(128),
      skewport_target_time_in_seconds(1.0f),
      skewport_extrapolation_limit_in_content_pixels(2000),
      max_unused_resource_memory_percentage(100),
      max_memory_for_prepaint_percentage(100),
      strict_layer_property_change_checking(false),
      use_one_copy(true),
      use_zero_copy(false),
      use_persistent_map_for_gpu_memory_buffers(false),
      enable_elastic_overscroll(false),
      use_image_texture_target(GL_TEXTURE_2D),
      ignore_root_layer_flings(false),
      scheduled_raster_task_limit(32),
      use_occlusion_for_tile_prioritization(false),
      record_full_layer(false),
      use_display_lists(false),
      verify_property_trees(false),
      gather_pixel_refs(false),
      use_compositor_animation_timelines(false),
      invert_viewport_scroll_order(false) {
}

LayerTreeSettings::~LayerTreeSettings() {}

SchedulerSettings LayerTreeSettings::ToSchedulerSettings() const {
  SchedulerSettings scheduler_settings;
  scheduler_settings.use_external_begin_frame_source =
      use_external_begin_frame_source;
  scheduler_settings.main_frame_before_activation_enabled =
      main_frame_before_activation_enabled;
  scheduler_settings.timeout_and_draw_when_animation_checkerboards =
      timeout_and_draw_when_animation_checkerboards;
  scheduler_settings.using_synchronous_renderer_compositor =
      using_synchronous_renderer_compositor;
  scheduler_settings.throttle_frame_production =
      !renderer_settings.disable_gpu_vsync;
  scheduler_settings.background_frame_interval =
      base::TimeDelta::FromSecondsD(1.0 / background_animation_rate);
  return scheduler_settings;
}

}  // namespace cc
