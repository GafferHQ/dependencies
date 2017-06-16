// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/mojo/init/ui_init.h"

#include "base/memory/singleton.h"
#include "ui/mojo/init/screen_mojo.h"

#if defined(OS_ANDROID)
#include "ui/events/gesture_detection/gesture_configuration.h"
#endif

namespace ui {
namespace mojo {

#if defined(OS_ANDROID)
// TODO(sky): this needs to come from system.
class GestureConfigurationMojo : public ui::GestureConfiguration {
 public:
  GestureConfigurationMojo() : GestureConfiguration() {
    set_double_tap_enabled(false);
    set_double_tap_timeout_in_ms(semi_long_press_time_in_ms());
    set_gesture_begin_end_types_enabled(true);
    set_min_gesture_bounds_length(default_radius());
    set_min_pinch_update_span_delta(0);
    set_min_scaling_touch_major(default_radius() * 2);
    set_velocity_tracker_strategy(
        ui::VelocityTracker::Strategy::LSQ2_RESTRICTED);
    set_span_slop(max_touch_move_in_pixels_for_click() * 2);
    set_swipe_enabled(true);
    set_two_finger_tap_enabled(true);
  }

  ~GestureConfigurationMojo() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureConfigurationMojo);
};
#endif

UIInit::UIInit(const gfx::Size& screen_size_in_pixels, float device_pixel_ratio)
    : screen_(new ScreenMojo(screen_size_in_pixels, device_pixel_ratio)) {
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
#if defined(OS_ANDROID)
  gesture_configuration_.reset(new GestureConfigurationMojo);
  ui::GestureConfiguration::SetInstance(gesture_configuration_.get());
#endif
}

UIInit::~UIInit() {
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, nullptr);
#if defined(OS_ANDROID)
  ui::GestureConfiguration::SetInstance(nullptr);
#endif
}

}  // namespace mojo
}  // namespace ui
