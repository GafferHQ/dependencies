// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

#include "base/test/null_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace {

// An animation observer that confirms upon animation completion, that the
// compositor is not null.
class TestLayerAnimationObserver : public ImplicitAnimationObserver {
 public:
  TestLayerAnimationObserver(Layer* layer) : layer_(layer) {}
  ~TestLayerAnimationObserver() override {}

  // ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    EXPECT_NE(nullptr, layer_->GetCompositor());
  }

 private:
  Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationObserver);
};

// Test fixture for LayerOwner tests that require a ui::Compositor.
class LayerOwnerTestWithCompositor : public testing::Test {
 public:
  LayerOwnerTestWithCompositor();
  ~LayerOwnerTestWithCompositor() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(LayerOwnerTestWithCompositor);
};

LayerOwnerTestWithCompositor::LayerOwnerTestWithCompositor() {
}

LayerOwnerTestWithCompositor::~LayerOwnerTestWithCompositor() {
}

void LayerOwnerTestWithCompositor::SetUp() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      new base::NullTaskRunner();

  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(false);

  compositor_.reset(new ui::Compositor(gfx::kNullAcceleratedWidget,
                                       context_factory, task_runner));
}

void LayerOwnerTestWithCompositor::TearDown() {
  compositor_.reset();
  ui::TerminateContextFactoryForTests();
}

}  // namespace

TEST(LayerOwnerTest, RecreateLayerHonorsAnimationTargets) {
  LayerOwner owner;
  Layer* layer = new Layer(LAYER_SOLID_COLOR);
  layer->SetVisible(true);
  layer->SetOpacity(1.0f);
  layer->SetColor(SK_ColorRED);

  owner.SetLayer(layer);

  ScopedLayerAnimationSettings settings(layer->GetAnimator());
  layer->SetVisible(false);
  layer->SetOpacity(0.0f);
  layer->SetColor(SK_ColorGREEN);
  EXPECT_TRUE(layer->visible());
  EXPECT_EQ(1.0f, layer->opacity());
  EXPECT_EQ(SK_ColorRED, layer->background_color());

  scoped_ptr<Layer> old_layer(owner.RecreateLayer());
  EXPECT_FALSE(owner.layer()->visible());
  EXPECT_EQ(0.0f, owner.layer()->opacity());
  EXPECT_EQ(SK_ColorGREEN, owner.layer()->background_color());
}

// Tests that when a LAYER_SOLID_COLOR which is not backed by a SolidColorLayer
// that opaqueness and color targets are maintained when the
// LayerOwner::RecreateLayers is called.
TEST(LayerOwnerTest, RecreateLayerSolidColorWithChangedCCLayerHonorsTargets) {
  SkColor transparent = SK_ColorTRANSPARENT;
  LayerOwner owner;
  Layer* layer = new Layer(LAYER_SOLID_COLOR);
  owner.SetLayer(layer);
  layer->SetFillsBoundsOpaquely(false);
  layer->SetColor(transparent);
  // Changing the backing layer takes LAYER_SOLID_COLOR off of the normal layer
  // flow, need to ensure that set values are maintained.
  layer->SwitchCCLayerForTest();

  EXPECT_FALSE(layer->fills_bounds_opaquely());
  EXPECT_EQ(transparent, layer->background_color());
  EXPECT_EQ(transparent, layer->GetTargetColor());

  scoped_ptr<Layer> old_layer(owner.RecreateLayer());
  EXPECT_FALSE(owner.layer()->fills_bounds_opaquely());
  EXPECT_EQ(transparent, owner.layer()->background_color());
  EXPECT_EQ(transparent, owner.layer()->GetTargetColor());
}

TEST(LayerOwnerTest, RecreateRootLayerWithNullCompositor) {
  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();

  EXPECT_EQ(nullptr, owner.layer()->GetCompositor());
  EXPECT_EQ(nullptr, layer_copy->GetCompositor());
}

TEST_F(LayerOwnerTestWithCompositor, RecreateRootLayerWithCompositor) {
  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);

  compositor()->SetRootLayer(layer);

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();

  EXPECT_EQ(compositor(), owner.layer()->GetCompositor());
  EXPECT_EQ(owner.layer(), compositor()->root_layer());
  EXPECT_EQ(nullptr, layer_copy->GetCompositor());
}

// Tests that recreating the root layer, while one of its children is animating,
// properly updates the compositor. So that compositor is not null for observers
// of animations being cancelled.
TEST_F(LayerOwnerTestWithCompositor, RecreateRootLayerDuringAnimation) {
  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);
  compositor()->SetRootLayer(layer);

  scoped_ptr<Layer> child(new Layer);
  child->SetBounds(gfx::Rect(0, 0, 100, 100));
  layer->Add(child.get());

  // This observer checks that the compositor of |child| is not null upon
  // animation completion.
  scoped_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver(child.get()));
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> long_duration_animation(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  {
    ui::ScopedLayerAnimationSettings animation(child->GetAnimator());
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    animation.AddObserver(observer.get());
    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    child->SetTransform(transform);
  }

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();
}

// Tests that recreating a non-root layer, while one of its children is
// animating, properly updates the compositor. So that compositor is not null
// for observers of animations being cancelled.
TEST_F(LayerOwnerTestWithCompositor, RecreateNonRootLayerDuringAnimation) {
  scoped_ptr<Layer> root_layer(new Layer);
  compositor()->SetRootLayer(root_layer.get());

  LayerOwner owner;
  Layer* layer = new Layer;
  owner.SetLayer(layer);
  root_layer->Add(layer);

  scoped_ptr<Layer> child(new Layer);
  child->SetBounds(gfx::Rect(0, 0, 100, 100));
  layer->Add(child.get());

  // This observer checks that the compositor of |child| is not null upon
  // animation completion.
  scoped_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver(child.get()));
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> long_duration_animation(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  {
    ui::ScopedLayerAnimationSettings animation(child->GetAnimator());
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    animation.AddObserver(observer.get());
    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    child->SetTransform(transform);
  }

  scoped_ptr<Layer> layer_copy = owner.RecreateLayer();
}

}  // namespace ui
