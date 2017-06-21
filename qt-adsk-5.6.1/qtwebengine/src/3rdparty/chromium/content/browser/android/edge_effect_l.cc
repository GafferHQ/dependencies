// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/edge_effect_l.h"

#include "cc/layers/ui_resource_layer.h"
#include "content/browser/android/animation_utils.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/system_ui_resource_type.h"

namespace content {

namespace {

// Time it will take the effect to fully recede in ms
const int kRecedeTimeMs = 600;

// Time it will take before a pulled glow begins receding in ms
const int kPullTimeMs = 167;

// Time it will take for a pulled glow to decay to partial strength before
// release
const int kPullDecayTimeMs = 2000;

const float kMaxAlpha = 0.5f;

const float kPullGlowBegin = 0.f;

// Min/max velocity that will be absorbed
const float kMinVelocity = 100.f;
const float kMaxVelocity = 10000.f;

const float kEpsilon = 0.001f;

const float kSin = 0.5f;    // sin(PI / 6)
const float kCos = 0.866f;  // cos(PI / 6);

// How much dragging should effect the height of the glow image.
// Number determined by user testing.
const float kPullDistanceAlphaGlowFactor = 0.8f;

const int kVelocityGlowFactor = 6;

const ui::SystemUIResourceType kResourceId = ui::OVERSCROLL_GLOW_L;

}  // namespace

EdgeEffectL::EdgeEffectL(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      glow_(cc::UIResourceLayer::Create(Compositor::LayerSettings())),
      glow_alpha_(0),
      glow_scale_y_(0),
      glow_alpha_start_(0),
      glow_alpha_finish_(0),
      glow_scale_y_start_(0),
      glow_scale_y_finish_(0),
      displacement_(0.5f),
      target_displacement_(0.5f),
      state_(STATE_IDLE),
      pull_distance_(0) {
  // Prevent the provided layers from drawing until the effect is activated.
  glow_->SetIsDrawable(false);
}

EdgeEffectL::~EdgeEffectL() {
  glow_->RemoveFromParent();
}

bool EdgeEffectL::IsFinished() const {
  return state_ == STATE_IDLE;
}

void EdgeEffectL::Finish() {
  glow_->SetIsDrawable(false);
  pull_distance_ = 0;
  state_ = STATE_IDLE;
}

void EdgeEffectL::Pull(base::TimeTicks current_time,
                       float delta_distance,
                       float displacement) {
  target_displacement_ = displacement;
  if (state_ == STATE_PULL_DECAY && current_time - start_time_ < duration_) {
    return;
  }
  if (state_ != STATE_PULL) {
    glow_scale_y_ = std::max(kPullGlowBegin, glow_scale_y_);
  }
  state_ = STATE_PULL;

  start_time_ = current_time;
  duration_ = base::TimeDelta::FromMilliseconds(kPullTimeMs);

  float abs_delta_distance = std::abs(delta_distance);
  pull_distance_ += delta_distance;

  glow_alpha_ = glow_alpha_start_ = std::min(
      kMaxAlpha,
      glow_alpha_ + (abs_delta_distance * kPullDistanceAlphaGlowFactor));

  if (pull_distance_ == 0) {
    glow_scale_y_ = glow_scale_y_start_ = 0;
  } else {
    float scale = 1.f -
                  1.f / std::sqrt(std::abs(pull_distance_) * bounds_.height()) -
                  0.3f;
    glow_scale_y_ = glow_scale_y_start_ = std::max(0.f, scale) / 0.7f;
  }

  glow_alpha_finish_ = glow_alpha_;
  glow_scale_y_finish_ = glow_scale_y_;
}

void EdgeEffectL::Release(base::TimeTicks current_time) {
  pull_distance_ = 0;

  if (state_ != STATE_PULL && state_ != STATE_PULL_DECAY)
    return;

  state_ = STATE_RECEDE;
  glow_alpha_start_ = glow_alpha_;
  glow_scale_y_start_ = glow_scale_y_;

  glow_alpha_finish_ = 0.f;
  glow_scale_y_finish_ = 0.f;

  start_time_ = current_time;
  duration_ = base::TimeDelta::FromMilliseconds(kRecedeTimeMs);
}

void EdgeEffectL::Absorb(base::TimeTicks current_time, float velocity) {
  state_ = STATE_ABSORB;

  velocity = Clamp(std::abs(velocity), kMinVelocity, kMaxVelocity);

  start_time_ = current_time;
  // This should never be less than 1 millisecond.
  duration_ = base::TimeDelta::FromMilliseconds(0.15f + (velocity * 0.02f));

  // The glow depends more on the velocity, and therefore starts out
  // nearly invisible.
  glow_alpha_start_ = 0.3f;
  glow_scale_y_start_ = std::max(glow_scale_y_, 0.f);

  // Growth for the size of the glow should be quadratic to properly respond
  // to a user's scrolling speed. The faster the scrolling speed, the more
  // intense the effect should be for both the size and the saturation.
  glow_scale_y_finish_ =
      std::min(0.025f + (velocity * (velocity / 100) * 0.00015f) / 2.f, 1.f);
  // Alpha should change for the glow as well as size.
  glow_alpha_finish_ = Clamp(
      glow_alpha_start_, velocity * kVelocityGlowFactor * .00001f, kMaxAlpha);
  target_displacement_ = 0.5;
}

bool EdgeEffectL::Update(base::TimeTicks current_time) {
  if (IsFinished())
    return false;

  const double dt = (current_time - start_time_).InMilliseconds();
  const double t = std::min(dt / duration_.InMilliseconds(), 1.);
  const float interp = static_cast<float>(Damp(t, 1.));

  glow_alpha_ = Lerp(glow_alpha_start_, glow_alpha_finish_, interp);
  glow_scale_y_ = Lerp(glow_scale_y_start_, glow_scale_y_finish_, interp);
  displacement_ = (displacement_ + target_displacement_) / 2.f;

  if (t >= 1.f - kEpsilon) {
    switch (state_) {
      case STATE_ABSORB:
        state_ = STATE_RECEDE;
        start_time_ = current_time;
        duration_ = base::TimeDelta::FromMilliseconds(kRecedeTimeMs);

        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        glow_alpha_finish_ = 0.f;
        glow_scale_y_finish_ = 0.f;
        break;
      case STATE_PULL:
        state_ = STATE_PULL_DECAY;
        start_time_ = current_time;
        duration_ = base::TimeDelta::FromMilliseconds(kPullDecayTimeMs);

        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        // After pull, the glow should fade to nothing.
        glow_alpha_finish_ = 0.f;
        glow_scale_y_finish_ = 0.f;
        break;
      case STATE_PULL_DECAY:
        state_ = STATE_RECEDE;
        break;
      case STATE_RECEDE:
        Finish();
        break;
      default:
        break;
    }
  }

  bool one_last_frame = false;
  if (state_ == STATE_RECEDE && glow_scale_y_ <= 0) {
    Finish();
    one_last_frame = true;
  }

  return !IsFinished() || one_last_frame;
}

float EdgeEffectL::GetAlpha() const {
  return IsFinished() ? 0.f : glow_alpha_;
}

void EdgeEffectL::ApplyToLayers(const gfx::SizeF& size,
                                const gfx::Transform& transform) {
  if (IsFinished())
    return;

  // An empty window size, while meaningless, is also relatively harmless, and
  // will simply prevent any drawing of the layers.
  if (size.IsEmpty()) {
    glow_->SetIsDrawable(false);
    return;
  }

  const float r = size.width() * 0.75f / kSin;
  const float y = kCos * r;
  const float h = r - y;
  const float o_r = size.height() * 0.75f / kSin;
  const float o_y = kCos * o_r;
  const float o_h = o_r - o_y;
  const float base_glow_scale = h > 0.f ? std::min(o_h / h, 1.f) : 1.f;
  bounds_ = gfx::Size(size.width(), (int)std::min(size.height(), h));
  gfx::Size image_bounds(
      r, std::min(1.f, glow_scale_y_) * base_glow_scale * bounds_.height());

  glow_->SetIsDrawable(true);
  glow_->SetUIResourceId(resource_manager_->GetUIResourceId(
      ui::ANDROID_RESOURCE_TYPE_SYSTEM, kResourceId));
  glow_->SetTransformOrigin(gfx::Point3F(bounds_.width() * 0.5f, 0, 0));
  glow_->SetBounds(image_bounds);
  glow_->SetContentsOpaque(false);
  glow_->SetOpacity(Clamp(glow_alpha_, 0.f, 1.f));

  const float displacement = Clamp(displacement_, 0.f, 1.f) - 0.5f;
  const float displacement_offset_x = bounds_.width() * displacement * 0.5f;
  const float image_offset_x = (bounds_.width() - image_bounds.width()) * 0.5f;
  gfx::Transform offset_transform;
  offset_transform.Translate(image_offset_x - displacement_offset_x, 0);
  offset_transform.ConcatTransform(transform);
  glow_->SetTransform(offset_transform);
}

void EdgeEffectL::SetParent(cc::Layer* parent) {
  if (glow_->parent() != parent)
    parent->AddChild(glow_);
}

// static
void EdgeEffectL::PreloadResources(ui::ResourceManager* resource_manager) {
  DCHECK(resource_manager);
  resource_manager->PreloadResource(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                    kResourceId);
}

}  // namespace content
