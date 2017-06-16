// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_glow.h"

#include "cc/layers/layer.h"
#include "content/browser/android/edge_effect_base.h"
#include "content/public/browser/android/compositor.h"

using std::max;
using std::min;

namespace content {

namespace {

const float kEpsilon = 1e-3f;

bool IsApproxZero(float value) {
  return std::abs(value) < kEpsilon;
}

gfx::Vector2dF ZeroSmallComponents(gfx::Vector2dF vector) {
  if (IsApproxZero(vector.x()))
    vector.set_x(0);
  if (IsApproxZero(vector.y()))
    vector.set_y(0);
  return vector;
}

gfx::Transform ComputeTransform(OverscrollGlow::Edge edge,
                                const gfx::SizeF& window_size,
                                float offset) {
  // Transforms assume the edge layers are anchored to their *top center point*.
  switch (edge) {
    case OverscrollGlow::EDGE_TOP:
      return gfx::Transform(1, 0, 0, 1, 0, offset);
    case OverscrollGlow::EDGE_LEFT:
      return gfx::Transform(0,
                            1,
                            -1,
                            0,
                            -window_size.height() / 2.f + offset,
                            window_size.height() / 2.f);
    case OverscrollGlow::EDGE_BOTTOM:
      return gfx::Transform(-1, 0, 0, -1, 0, window_size.height() + offset);
    case OverscrollGlow::EDGE_RIGHT:
      return gfx::Transform(
          0,
          -1,
          1,
          0,
          -window_size.height() / 2.f + window_size.width() + offset,
          window_size.height() / 2.f);
    default:
      NOTREACHED() << "Invalid edge: " << edge;
      return gfx::Transform();
  };
}

gfx::SizeF ComputeSize(OverscrollGlow::Edge edge,
                       const gfx::SizeF& window_size) {
  switch (edge) {
    case OverscrollGlow::EDGE_TOP:
    case OverscrollGlow::EDGE_BOTTOM:
      return window_size;
    case OverscrollGlow::EDGE_LEFT:
    case OverscrollGlow::EDGE_RIGHT:
      return gfx::SizeF(window_size.height(), window_size.width());
    default:
      NOTREACHED() << "Invalid edge: " << edge;
      return gfx::SizeF();
  };
}

}  // namespace

OverscrollGlow::OverscrollGlow(OverscrollGlowClient* client)
    : client_(client), edge_offsets_(), initialized_(false) {
  DCHECK(client);
}

OverscrollGlow::~OverscrollGlow() {
  Detach();
}

void OverscrollGlow::Reset() {
  if (!initialized_)
    return;
  Detach();
  for (size_t i = 0; i < EDGE_COUNT; ++i)
    edge_effects_[i]->Finish();
}

bool OverscrollGlow::IsActive() const {
  if (!initialized_)
    return false;
  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!edge_effects_[i]->IsFinished())
      return true;
  }
  return false;
}

float OverscrollGlow::GetVisibleAlpha() const {
  float max_alpha = 0;
  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!edge_effects_[i]->IsFinished())
      max_alpha = std::max(max_alpha, edge_effects_[i]->GetAlpha());
  }
  return std::min(max_alpha, 1.f);
}

bool OverscrollGlow::OnOverscrolled(base::TimeTicks current_time,
          const gfx::Vector2dF& accumulated_overscroll,
          gfx::Vector2dF overscroll_delta,
          gfx::Vector2dF velocity,
          const gfx::Vector2dF& displacement) {
  // The size of the glow determines the relative effect of the inputs; an
  // empty-sized effect is effectively disabled.
  if (viewport_size_.IsEmpty())
    return false;

  // Ignore sufficiently small values that won't meaningfuly affect animation.
  overscroll_delta = ZeroSmallComponents(overscroll_delta);
  if (overscroll_delta.IsZero()) {
    if (initialized_)
      Release(current_time);
    return CheckNeedsAnimate();
  }

  if (!InitializeIfNecessary())
    return false;

  gfx::Vector2dF old_overscroll = accumulated_overscroll - overscroll_delta;
  bool x_overscroll_started =
      !IsApproxZero(overscroll_delta.x()) && IsApproxZero(old_overscroll.x());
  bool y_overscroll_started =
      !IsApproxZero(overscroll_delta.y()) && IsApproxZero(old_overscroll.y());

  velocity = ZeroSmallComponents(velocity);
  if (!velocity.IsZero())
    Absorb(current_time, velocity, x_overscroll_started, y_overscroll_started);
  else
    Pull(current_time, overscroll_delta, displacement);

  return CheckNeedsAnimate();
}

bool OverscrollGlow::Animate(base::TimeTicks current_time,
                             cc::Layer* parent_layer) {
  DCHECK(parent_layer);
  if (!CheckNeedsAnimate())
    return false;

  UpdateLayerAttachment(parent_layer);

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (edge_effects_[i]->Update(current_time)) {
      Edge edge = static_cast<Edge>(i);
      edge_effects_[i]->ApplyToLayers(
          ComputeSize(edge, viewport_size_),
          ComputeTransform(edge, viewport_size_, edge_offsets_[i]));
    }
  }

  return CheckNeedsAnimate();
}

void OverscrollGlow::OnFrameUpdated(
    const gfx::SizeF& viewport_size,
    const gfx::SizeF& content_size,
    const gfx::Vector2dF& content_scroll_offset) {
  viewport_size_ = viewport_size;
  edge_offsets_[OverscrollGlow::EDGE_TOP] = -content_scroll_offset.y();
  edge_offsets_[OverscrollGlow::EDGE_LEFT] = -content_scroll_offset.x();
  edge_offsets_[OverscrollGlow::EDGE_BOTTOM] = content_size.height() -
                                               content_scroll_offset.y() -
                                               viewport_size.height();
  edge_offsets_[OverscrollGlow::EDGE_RIGHT] =
      content_size.width() - content_scroll_offset.x() - viewport_size.width();
}

bool OverscrollGlow::CheckNeedsAnimate() {
  if (!initialized_) {
    Detach();
    return false;
  }

  if (IsActive())
    return true;

  Detach();
  return false;
}

void OverscrollGlow::UpdateLayerAttachment(cc::Layer* parent) {
  DCHECK(parent);
  if (!root_layer_.get())
    return;

  if (!CheckNeedsAnimate())
    return;

  if (root_layer_->parent() != parent)
    parent->AddChild(root_layer_);

  for (size_t i = 0; i < EDGE_COUNT; ++i)
    edge_effects_[i]->SetParent(root_layer_.get());
}

void OverscrollGlow::Detach() {
  if (root_layer_.get())
    root_layer_->RemoveFromParent();
}

bool OverscrollGlow::InitializeIfNecessary() {
  if (initialized_)
    return true;

  DCHECK(!root_layer_.get());
  root_layer_ = cc::Layer::Create(Compositor::LayerSettings());
  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    edge_effects_[i] = client_->CreateEdgeEffect();
    DCHECK(edge_effects_[i]);
  }

  initialized_ = true;
  return true;
}

void OverscrollGlow::Pull(base::TimeTicks current_time,
                          const gfx::Vector2dF& overscroll_delta,
                          const gfx::Vector2dF& overscroll_location) {
  DCHECK(initialized_);
  DCHECK(!overscroll_delta.IsZero());
  DCHECK(!viewport_size_.IsEmpty());
  const float inv_width = 1.f / viewport_size_.width();
  const float inv_height = 1.f / viewport_size_.height();

  gfx::Vector2dF overscroll_pull =
      gfx::ScaleVector2d(overscroll_delta, inv_width, inv_height);
  const float edge_pull[EDGE_COUNT] = {
      min(overscroll_pull.y(), 0.f),  // Top
      min(overscroll_pull.x(), 0.f),  // Left
      max(overscroll_pull.y(), 0.f),  // Bottom
      max(overscroll_pull.x(), 0.f)   // Right
  };

  gfx::Vector2dF displacement =
      gfx::ScaleVector2d(overscroll_location, inv_width, inv_height);
  displacement.set_x(max(0.f, min(1.f, displacement.x())));
  displacement.set_y(max(0.f, min(1.f, displacement.y())));
  const float edge_displacement[EDGE_COUNT] = {
      1.f - displacement.x(),  // Top
      displacement.y(),        // Left
      displacement.x(),        // Bottom
      1.f - displacement.y()   // Right
  };

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!edge_pull[i])
      continue;

    edge_effects_[i]->Pull(
        current_time, std::abs(edge_pull[i]), edge_displacement[i]);
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Absorb(base::TimeTicks current_time,
                            const gfx::Vector2dF& velocity,
                            bool x_overscroll_started,
                            bool y_overscroll_started) {
  DCHECK(initialized_);
  DCHECK(!velocity.IsZero());

  // Only trigger on initial overscroll at a non-zero velocity
  const float overscroll_velocities[EDGE_COUNT] = {
      y_overscroll_started ? min(velocity.y(), 0.f) : 0,  // Top
      x_overscroll_started ? min(velocity.x(), 0.f) : 0,  // Left
      y_overscroll_started ? max(velocity.y(), 0.f) : 0,  // Bottom
      x_overscroll_started ? max(velocity.x(), 0.f) : 0   // Right
  };

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!overscroll_velocities[i])
      continue;

    edge_effects_[i]->Absorb(current_time, std::abs(overscroll_velocities[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Release(base::TimeTicks current_time) {
  DCHECK(initialized_);
  for (size_t i = 0; i < EDGE_COUNT; ++i)
    edge_effects_[i]->Release(current_time);
}

EdgeEffectBase* OverscrollGlow::GetOppositeEdge(int edge_index) {
  DCHECK(initialized_);
  return edge_effects_[(edge_index + 2) % EDGE_COUNT].get();
}

}  // namespace content
