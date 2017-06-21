// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <algorithm>
#include <set>

#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/math_util.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_host_common_test.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class LayerWithForcedDrawsContent : public Layer {
 public:
  explicit LayerWithForcedDrawsContent(const LayerSettings& settings)
      : Layer(settings) {}

  bool DrawsContent() const override;

 private:
  ~LayerWithForcedDrawsContent() override {}
};

bool LayerWithForcedDrawsContent::DrawsContent() const { return true; }

class MockContentLayerClient : public ContentLayerClient {
 public:
  MockContentLayerClient() {}
  ~MockContentLayerClient() override {}
  void PaintContents(SkCanvas* canvas,
                     const gfx::Rect& clip,
                     PaintingControlSetting picture_control) override {}
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      const gfx::Rect& clip,
      PaintingControlSetting picture_control) override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  bool FillsBoundsCompletely() const override { return false; }
};

scoped_refptr<FakePictureLayer> CreateDrawablePictureLayer(
    const LayerSettings& settings,
    ContentLayerClient* delegate) {
  scoped_refptr<FakePictureLayer> to_return =
      FakePictureLayer::Create(settings, delegate);
  to_return->SetIsDrawable(true);
  return to_return;
}

#define EXPECT_CONTENTS_SCALE_EQ(expected, layer)         \
  do {                                                    \
    EXPECT_FLOAT_EQ(expected, layer->contents_scale_x()); \
    EXPECT_FLOAT_EQ(expected, layer->contents_scale_y()); \
  } while (false)

#define EXPECT_IDEAL_SCALE_EQ(expected, layer)                 \
  do {                                                         \
    EXPECT_FLOAT_EQ(expected, layer->GetIdealContentsScale()); \
  } while (false)

class LayerTreeSettingsScaleContent : public LayerTreeSettings {
 public:
  LayerTreeSettingsScaleContent() {
    layer_transforms_should_scale_layer_contents = true;
  }
};

class LayerTreeHostCommonScalingTest : public LayerTreeHostCommonTest {
 public:
  LayerTreeHostCommonScalingTest()
      : LayerTreeHostCommonTest(LayerTreeSettingsScaleContent()) {}
};

TEST_F(LayerTreeHostCommonTest, TransformsForNoOpLayer) {
  // Sanity check: For layers positioned at zero, with zero size,
  // and with identity transforms, then the draw transform,
  // screen space transform, and the hierarchy passed on to children
  // layers should also be identity transforms.

  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  parent->AddChild(child);
  child->AddChild(grand_child);

  host()->SetRootLayer(parent);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);

  ExecuteCalculateDrawProperties(parent.get());

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, DoNotSkipLayersWithHandlers) {
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  parent->AddChild(child);
  child->AddChild(grand_child);

  host()->SetRootLayer(parent);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(10, 10),
                               gfx::Size(100, 100),
                               true,
                               false);
  // This would have previously caused us to skip our subtree, but this would be
  // wrong; we need up-to-date draw properties to do hit testing on the layers
  // with handlers.
  child->SetOpacity(0.f);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(10, 10),
                               gfx::Size(100, 100),
                               true,
                               false);
  grand_child->SetTouchEventHandlerRegion(gfx::Rect(0, 0, 100, 100));

  ExecuteCalculateDrawProperties(parent.get());

  // Check that we've computed draw properties for the subtree rooted at
  // |child|.
  EXPECT_FALSE(child->draw_transform().IsIdentity());
  EXPECT_FALSE(grand_child->draw_transform().IsIdentity());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleLayer) {
  gfx::Transform identity_matrix;
  scoped_refptr<Layer> layer = Layer::Create(layer_settings());

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               true,
                               false);
  root->AddChild(layer);

  host()->SetRootLayer(root);

  // Case 2: Setting the bounds of the layer should not affect either the draw
  // transform or the screenspace transform.
  gfx::Transform translation_to_center;
  translation_to_center.Translate(5.0, 6.0);
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  layer->screen_space_transform());

  // Case 3: The anchor point by itself (without a layer transform) should have
  // no effect on the transforms.
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  layer->screen_space_transform());

  // Case 4: A change in actual position affects both the draw transform and
  // screen space transform.
  gfx::Transform position_transform;
  position_transform.Translate(0.f, 1.2f);
  SetLayerPropertiesForTesting(layer.get(),
                               identity_matrix,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(position_transform, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(position_transform,
                                  layer->screen_space_transform());

  // Case 5: In the correct sequence of transforms, the layer transform should
  // pre-multiply the translation_to_center. This is easily tested by using a
  // scale transform, because scale and translation are not commutative.
  gfx::Transform layer_transform;
  layer_transform.Scale3d(2.0, 2.0, 1.0);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(layer_transform, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(layer_transform,
                                  layer->screen_space_transform());

  // Case 6: The layer transform should occur with respect to the anchor point.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(5.0, 0.0);
  gfx::Transform expected_result =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               gfx::Point3F(5.0f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result,
                                  layer->screen_space_transform());

  // Case 7: Verify that position pre-multiplies the layer transform.  The
  // current implementation of CalculateDrawProperties does this implicitly, but
  // it is still worth testing to detect accidental regressions.
  expected_result = position_transform * translation_to_anchor *
                    layer_transform * Inverse(translation_to_anchor);
  SetLayerPropertiesForTesting(layer.get(),
                               layer_transform,
                               gfx::Point3F(5.0f, 0.f, 0.f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result, layer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_result,
                                  layer->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsAboutScrollOffset) {
  const gfx::ScrollOffset kScrollOffset(50, 100);
  const gfx::Vector2dF kScrollDelta(2.34f, 5.67f);
  const gfx::Vector2d kMaxScrollOffset(200, 200);
  const gfx::PointF kScrollLayerPosition(-kScrollOffset.x(),
                                         -kScrollOffset.y());
  const float kPageScale = 0.888f;
  const float kDeviceScale = 1.666f;

  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);

  gfx::Transform identity_matrix;
  scoped_ptr<LayerImpl> sublayer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 1));
  LayerImpl* sublayer = sublayer_scoped_ptr.get();
  SetLayerPropertiesForTesting(sublayer, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(500, 500), true, false,
                               false);

  scoped_ptr<LayerImpl> scroll_layer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 2));
  LayerImpl* scroll_layer = scroll_layer_scoped_ptr.get();
  SetLayerPropertiesForTesting(scroll_layer, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 20), true, false,
                               false);
  scoped_ptr<LayerImpl> clip_layer_scoped_ptr(
      LayerImpl::Create(host_impl.active_tree(), 4));
  LayerImpl* clip_layer = clip_layer_scoped_ptr.get();

  scroll_layer->SetScrollClipLayer(clip_layer->id());
  clip_layer->SetBounds(
      gfx::Size(scroll_layer->bounds().width() + kMaxScrollOffset.x(),
                scroll_layer->bounds().height() + kMaxScrollOffset.y()));
  scroll_layer->SetScrollClipLayer(clip_layer->id());
  scroll_layer->SetScrollDelta(kScrollDelta);
  gfx::Transform impl_transform;
  scroll_layer->AddChild(sublayer_scoped_ptr.Pass());
  LayerImpl* scroll_layer_raw_ptr = scroll_layer_scoped_ptr.get();
  clip_layer->AddChild(scroll_layer_scoped_ptr.Pass());
  scroll_layer_raw_ptr->PushScrollOffsetFromMainThread(kScrollOffset);

  scoped_ptr<LayerImpl> root(LayerImpl::Create(host_impl.active_tree(), 3));
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(3, 4), true, false,
                               false);
  root->AddChild(clip_layer_scoped_ptr.Pass());
  root->SetHasRenderSurface(true);

  ExecuteCalculateDrawProperties(
      root.get(), kDeviceScale, kPageScale, scroll_layer->parent());
  gfx::Transform expected_transform = identity_matrix;
  gfx::PointF sub_layer_screen_position = kScrollLayerPosition - kScrollDelta;
  expected_transform.Translate(MathUtil::Round(sub_layer_screen_position.x() *
                                               kPageScale * kDeviceScale),
                               MathUtil::Round(sub_layer_screen_position.y() *
                                               kPageScale * kDeviceScale));
  expected_transform.Scale(kPageScale * kDeviceScale,
                           kPageScale * kDeviceScale);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->screen_space_transform());

  gfx::Transform arbitrary_translate;
  const float kTranslateX = 10.6f;
  const float kTranslateY = 20.6f;
  arbitrary_translate.Translate(kTranslateX, kTranslateY);
  SetLayerPropertiesForTesting(scroll_layer, arbitrary_translate,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(10, 20),
                               true, false, false);
  ExecuteCalculateDrawProperties(
      root.get(), kDeviceScale, kPageScale, scroll_layer->parent());
  expected_transform.MakeIdentity();
  expected_transform.Translate(
      MathUtil::Round(kTranslateX * kPageScale * kDeviceScale +
                      sub_layer_screen_position.x() * kPageScale *
                          kDeviceScale),
      MathUtil::Round(kTranslateY * kPageScale * kDeviceScale +
                      sub_layer_screen_position.y() * kPageScale *
                          kDeviceScale));
  expected_transform.Scale(kPageScale * kDeviceScale,
                           kPageScale * kDeviceScale);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_transform,
                                  sublayer->draw_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSimpleHierarchy) {
  gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);

  host()->SetRootLayer(root);

  // One-time setup of root layer
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               true,
                               false);

  // Case 1: parent's anchor point should not affect child or grand_child.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->screen_space_transform());

  // Case 2: parent's position affects child and grand_child.
  gfx::Transform parent_position_transform;
  parent_position_transform.Translate(0.f, 1.2f);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(0.f, 1.2f),
                               gfx::Size(10, 12),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_position_transform,
                                  grand_child->screen_space_transform());

  // Case 3: parent's local transform affects child and grandchild
  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(76, 78),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForSingleRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);

  host()->SetRootLayer(root);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               true,
                               false);

  // Child is set up so that a new render surface should be created.
  child->SetOpacity(0.5f);
  child->SetForceRenderSurface(true);

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(1.f, 0.9f, 1.f);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(25.0, 30.0);

  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  gfx::Vector2dF parent_composite_scale =
      MathUtil::ComputeTransform2dScaleComponents(parent_composite_transform,
                                                  1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform surface_sublayer_composite_transform =
      parent_composite_transform * Inverse(surface_sublayer_transform);

  // Child's render surface should not exist yet.
  ASSERT_FALSE(child->render_surface());

  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               gfx::Point3F(25.0f, 30.0f, 0.f),
                               gfx::PointF(),
                               gfx::Size(100, 120),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(8, 10),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());

  // Render surface should have been created now.
  ASSERT_TRUE(child->render_surface());
  ASSERT_EQ(child.get(), child->render_target());

  // The child layer's draw transform should refer to its new render surface.
  // The screen-space transform, however, should still refer to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(surface_sublayer_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(parent_composite_transform,
                                  child->screen_space_transform());

  // Because the grand_child is the only drawable content, the child's render
  // surface will tighten its bounds to the grand_child.  The scale at which the
  // surface's subtree is drawn must be removed from the composite transform.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_sublayer_composite_transform,
      child->render_target()->render_surface()->draw_transform());

  // The screen space is the same as the target since the child surface draws
  // into the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      surface_sublayer_composite_transform,
      child->render_target()->render_surface()->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForReplica) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> child_replica = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);
  child->SetReplicaLayer(child_replica.get());

  host()->SetRootLayer(root);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               true,
                               false);

  // Child is set up so that a new render surface should be created.
  child->SetOpacity(0.5f);

  gfx::Transform parent_layer_transform;
  parent_layer_transform.Scale3d(2.0, 2.0, 1.0);
  gfx::Transform parent_translation_to_anchor;
  parent_translation_to_anchor.Translate(2.5, 3.0);
  gfx::Transform parent_composite_transform =
      parent_translation_to_anchor * parent_layer_transform *
      Inverse(parent_translation_to_anchor);
  gfx::Transform replica_layer_transform;
  replica_layer_transform.Scale3d(3.0, 3.0, 1.0);
  gfx::Vector2dF parent_composite_scale =
      MathUtil::ComputeTransform2dScaleComponents(parent_composite_transform,
                                                  1.f);
  gfx::Transform surface_sublayer_transform;
  surface_sublayer_transform.Scale(parent_composite_scale.x(),
                                   parent_composite_scale.y());
  gfx::Transform replica_composite_transform =
      parent_composite_transform * replica_layer_transform *
      Inverse(surface_sublayer_transform);
  child_replica->SetIsDrawable(true);
  // Child's render surface should not exist yet.
  ASSERT_FALSE(child->render_surface());

  SetLayerPropertiesForTesting(parent.get(),
                               parent_layer_transform,
                               gfx::Point3F(2.5f, 3.0f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 12),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(16, 18),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(-0.5f, -0.5f),
                               gfx::Size(1, 1),
                               true,
                               false);
  SetLayerPropertiesForTesting(child_replica.get(),
                               replica_layer_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);
  ExecuteCalculateDrawProperties(root.get());

  // Render surface should have been created now.
  ASSERT_TRUE(child->render_surface());
  ASSERT_EQ(child.get(), child->render_target());

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      replica_composite_transform,
      child->render_target()->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(replica_composite_transform,
                                  child->render_target()
                                      ->render_surface()
                                      ->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForRenderSurfaceHierarchy) {
  // This test creates a more complex tree and verifies it all at once. This
  // covers the following cases:
  //   - layers that are described w.r.t. a render surface: should have draw
  //   transforms described w.r.t. that surface
  //   - A render surface described w.r.t. an ancestor render surface: should
  //   have a draw transform described w.r.t. that ancestor surface
  //   - Replicas of a render surface are described w.r.t. the replica's
  //   transform around its anchor, along with the surface itself.
  //   - Sanity check on recursion: verify transforms of layers described w.r.t.
  //   a render surface that is described w.r.t. an ancestor render surface.
  //   - verifying that each layer has a reference to the correct render surface
  //   and render target values.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> child_of_root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child_of_rs1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> child_of_rs2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> replica_of_rs1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> replica_of_rs2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child_of_root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> grand_child_of_rs2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(parent);
  parent->AddChild(render_surface1);
  parent->AddChild(child_of_root);
  render_surface1->AddChild(child_of_rs1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child_of_rs2);
  child_of_root->AddChild(grand_child_of_root);
  child_of_rs1->AddChild(grand_child_of_rs1);
  child_of_rs2->AddChild(grand_child_of_rs2);
  render_surface1->SetReplicaLayer(replica_of_rs1.get());
  render_surface2->SetReplicaLayer(replica_of_rs2.get());

  host()->SetRootLayer(root);

  // In combination with descendant draws content, opacity != 1 forces the layer
  // to have a new render surface.
  render_surface1->SetOpacity(0.5f);
  render_surface2->SetOpacity(0.33f);

  // One-time setup of root layer
  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(1, 2),
                               true,
                               false);

  // All layers in the tree are initialized with an anchor at .25 and a size of
  // (10,10).  matrix "A" is the composite layer transform used in all layers,
  // Matrix "R" is the composite replica transform used in all replica layers.
  gfx::Transform translation_to_anchor;
  translation_to_anchor.Translate(2.5, 0.0);
  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);
  gfx::Transform replica_layer_transform;
  replica_layer_transform.Scale3d(-2.0, 5.0, 1.0);

  gfx::Transform A =
      translation_to_anchor * layer_transform * Inverse(translation_to_anchor);
  gfx::Transform R = A * translation_to_anchor * replica_layer_transform *
                     Inverse(translation_to_anchor);

  gfx::Vector2dF surface1_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(A, 1.f);
  gfx::Transform surface1_sublayer_transform;
  surface1_sublayer_transform.Scale(surface1_parent_transform_scale.x(),
                                    surface1_parent_transform_scale.y());

  // SS1 = transform given to the subtree of render_surface1
  gfx::Transform SS1 = surface1_sublayer_transform;
  // S1 = transform to move from render_surface1 pixels to the layer space of
  // the owning layer
  gfx::Transform S1 = Inverse(surface1_sublayer_transform);

  gfx::Vector2dF surface2_parent_transform_scale =
      MathUtil::ComputeTransform2dScaleComponents(SS1 * A, 1.f);
  gfx::Transform surface2_sublayer_transform;
  surface2_sublayer_transform.Scale(surface2_parent_transform_scale.x(),
                                    surface2_parent_transform_scale.y());

  // SS2 = transform given to the subtree of render_surface2
  gfx::Transform SS2 = surface2_sublayer_transform;
  // S2 = transform to move from render_surface2 pixels to the layer space of
  // the owning layer
  gfx::Transform S2 = Inverse(surface2_sublayer_transform);

  SetLayerPropertiesForTesting(parent.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(child_of_root.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(child_of_rs1.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(child_of_rs2.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child_of_root.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs1.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child_of_rs2.get(),
                               layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(replica_of_rs1.get(),
                               replica_layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);
  SetLayerPropertiesForTesting(replica_of_rs2.get(),
                               replica_layer_transform,
                               gfx::Point3F(2.5f, 0.f, 0.f),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);

  ExecuteCalculateDrawProperties(root.get());

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(root->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(root.get(), parent->render_target());
  EXPECT_EQ(root.get(), child_of_root->render_target());
  EXPECT_EQ(root.get(), grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1.get(), render_surface1->render_target());
  EXPECT_EQ(render_surface1.get(), child_of_rs1->render_target());
  EXPECT_EQ(render_surface1.get(), grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2.get(), render_surface2->render_target());
  EXPECT_EQ(render_surface2.get(), child_of_rs2->render_target());
  EXPECT_EQ(render_surface2.get(), grand_child_of_rs2->render_target());

  // Verify layer draw transforms note that draw transforms are described with
  // respect to the nearest ancestor render surface but screen space transforms
  // are described with respect to the root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A, child_of_root->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  grand_child_of_root->draw_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1, render_surface1->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * A, child_of_rs1->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS1 * A * A,
                                  grand_child_of_rs1->draw_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2, render_surface2->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * A, child_of_rs2->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(SS2 * A * A,
                                  grand_child_of_rs2->draw_transform());

  // Verify layer screen-space transforms
  //
  EXPECT_TRANSFORMATION_MATRIX_EQ(A, parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A,
                                  child_of_root->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * A, grand_child_of_root->screen_space_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A,
                                  render_surface1->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  child_of_rs1->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A,
                                  grand_child_of_rs1->screen_space_transform());

  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A,
                                  render_surface2->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A,
                                  child_of_rs2->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(A * A * A * A * A,
                                  grand_child_of_rs2->screen_space_transform());

  // Verify render surface transforms.
  //
  // Draw transform of render surface 1 is described with respect to root.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * S1, render_surface1->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * R * S1, render_surface1->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * S1, render_surface1->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * R * S1,
      render_surface1->render_surface()->replica_screen_space_transform());
  // Draw transform of render surface 2 is described with respect to render
  // surface 1.
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      SS1 * A * S2, render_surface2->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      SS1 * R * S2,
      render_surface2->render_surface()->replica_draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * A * S2,
      render_surface2->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      A * A * R * S2,
      render_surface2->render_surface()->replica_screen_space_transform());

  // Sanity check. If these fail there is probably a bug in the test itself.  It
  // is expected that we correctly set up transforms so that the y-component of
  // the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0,
                  child_of_rs1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0,
                  child_of_rs2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->screen_space_transform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, TransformsForFlatteningLayer) {
  // For layers that flatten their subtree, there should be an orthographic
  // projection (for x and y values) in the middle of the transform sequence.
  // Note that the way the code is currently implemented, it is not expected to
  // use a canonical orthographic projection.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> great_grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  gfx::Transform rotation_about_y_axis;
  rotation_about_y_axis.RotateAboutYAxis(30.0);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               rotation_about_y_axis,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               rotation_about_y_axis,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(great_grand_child.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(10, 10),
                               true, false);

  root->AddChild(child);
  child->AddChild(grand_child);
  grand_child->AddChild(great_grand_child);
  child->SetForceRenderSurface(true);

  host()->SetRootLayer(root);

  // No layers in this test should preserve 3d.
  ASSERT_TRUE(root->should_flatten_transform());
  ASSERT_TRUE(child->should_flatten_transform());
  ASSERT_TRUE(grand_child->should_flatten_transform());
  ASSERT_TRUE(great_grand_child->should_flatten_transform());

  gfx::Transform expected_child_draw_transform = rotation_about_y_axis;
  gfx::Transform expected_child_screen_space_transform = rotation_about_y_axis;
  gfx::Transform expected_grand_child_draw_transform =
      rotation_about_y_axis;  // draws onto child's render surface
  gfx::Transform flattened_rotation_about_y = rotation_about_y_axis;
  flattened_rotation_about_y.FlattenTo2d();
  gfx::Transform expected_grand_child_screen_space_transform =
      flattened_rotation_about_y * rotation_about_y_axis;
  gfx::Transform expected_great_grand_child_draw_transform =
      flattened_rotation_about_y;
  gfx::Transform expected_great_grand_child_screen_space_transform =
      flattened_rotation_about_y * flattened_rotation_about_y;

  ExecuteCalculateDrawProperties(root.get());

  // The child's draw transform should have been taken by its surface.
  ASSERT_TRUE(child->render_surface());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_draw_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_child_screen_space_transform,
      child->render_surface()->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_screen_space_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_draw_transform,
                                  grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_grand_child_screen_space_transform,
                                  grand_child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_great_grand_child_draw_transform,
                                  great_grand_child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_great_grand_child_screen_space_transform,
      great_grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformsForDegenerateIntermediateLayer) {
  // A layer that is empty in one axis, but not the other, was accidentally
  // skipping a necessary translation.  Without that translation, the coordinate
  // space of the layer's draw transform is incorrect.
  //
  // Normally this isn't a problem, because the layer wouldn't be drawn anyway,
  // but if that layer becomes a render surface, then its draw transform is
  // implicitly inherited by the rest of the subtree, which then is positioned
  // incorrectly as a result.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  // The child height is zero, but has non-zero width that should be accounted
  // for while computing draw transforms.
  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 0),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);

  root->AddChild(child);
  child->AddChild(grand_child);
  child->SetForceRenderSurface(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(child->render_surface());
  // This is the real test, the rest are sanity checks.
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());
}

TEST_F(LayerTreeHostCommonTest, TransformAboveRootLayer) {
  // Transformations applied at the root of the tree should be forwarded
  // to child layers instead of applied to the root RenderSurface.
  const gfx::Transform identity_matrix;
  scoped_refptr<LayerWithForcedDrawsContent> root =
      new LayerWithForcedDrawsContent(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      new LayerWithForcedDrawsContent(layer_settings());
  child->SetScrollClipLayerId(root->id());
  root->AddChild(child);

  host()->SetRootLayer(root);

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);

  gfx::Transform translate;
  translate.Translate(50, 50);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(translate, root->draw_properties().target_space_transform);
    EXPECT_EQ(translate, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform scale;
  scale.Scale(2, 2);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), scale, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(scale, root->draw_properties().target_space_transform);
    EXPECT_EQ(scale, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform rotate;
  rotate.Rotate(2);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), rotate, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(rotate, root->draw_properties().target_space_transform);
    EXPECT_EQ(rotate, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  gfx::Transform composite;
  composite.ConcatTransform(translate);
  composite.ConcatTransform(scale);
  composite.ConcatTransform(rotate);
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), composite, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_EQ(composite, root->draw_properties().target_space_transform);
    EXPECT_EQ(composite, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify it composes correctly with device scale.
  float device_scale_factor = 1.5f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.device_scale_factor = device_scale_factor;
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform device_scaled_translate = translate;
    device_scaled_translate.Scale(device_scale_factor, device_scale_factor);
    EXPECT_EQ(device_scaled_translate,
              root->draw_properties().target_space_transform);
    EXPECT_EQ(device_scaled_translate,
              child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify it composes correctly with page scale.
  float page_scale_factor = 2.f;

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), translate, &render_surface_layer_list);
    inputs.page_scale_factor = page_scale_factor;
    inputs.page_scale_layer = root.get();
    inputs.can_adjust_raster_scales = true;
    inputs.property_trees->needs_rebuild = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform page_scaled_translate = translate;
    page_scaled_translate.Scale(page_scale_factor, page_scale_factor);
    EXPECT_EQ(page_scaled_translate,
              root->draw_properties().target_space_transform);
    EXPECT_EQ(page_scaled_translate,
              child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }

  // Verify that it composes correctly with transforms directly on root layer.
  root->SetTransform(composite);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), root->bounds(), composite, &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    gfx::Transform compositeSquared = composite;
    compositeSquared.ConcatTransform(composite);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeSquared, root->draw_properties().target_space_transform);
    EXPECT_TRANSFORMATION_MATRIX_EQ(
        compositeSquared, child->draw_properties().target_space_transform);
    EXPECT_EQ(identity_matrix, root->render_surface()->draw_transform());
  }
}

TEST_F(LayerTreeHostCommonTest,
       RenderSurfaceListForRenderSurfaceWithClippedLayer) {
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  host()->SetRootLayer(parent);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(30.f, 30.f),
                               gfx::Size(10, 10),
                               true,
                               false);

  parent->AddChild(render_surface1);
  parent->SetMasksToBounds(true);
  render_surface1->AddChild(child);
  render_surface1->SetForceRenderSurface(true);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(),
      parent->bounds(),
      gfx::Transform(),
      &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // The child layer's content is entirely outside the parent's clip rect, so
  // the intermediate render surface should not be listed here, even if it was
  // forced to be created. Render surfaces without children or visible content
  // are unexpected at draw time (e.g. we might try to create a content texture
  // of size 0).
  ASSERT_TRUE(parent->render_surface());
  EXPECT_EQ(1U, render_surface_layer_list.size());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceListForTransparentChild) {
  LayerImpl* parent = root_layer();
  LayerImpl* render_surface1 = AddChild<LayerImpl>(parent);
  LayerImpl* child = AddChild<LayerImpl>(render_surface1);
  child->SetDrawsContent(true);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(render_surface1, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false,
                               true);
  SetLayerPropertiesForTesting(child, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false,
                               false);
  render_surface1->SetOpacity(0.f);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      parent, parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Since the layer is transparent, render_surface1->render_surface() should
  // not have gotten added anywhere.  Also, the drawable content rect should not
  // have been extended by the children.
  ASSERT_TRUE(parent->render_surface());
  EXPECT_EQ(0U, parent->render_surface()->layer_list().size());
  EXPECT_EQ(1U, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(gfx::Rect(), parent->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceForBlendMode) {
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  host()->SetRootLayer(parent);

  const gfx::Transform identity_matrix;
  const SkXfermode::Mode blend_mode = SkXfermode::kMultiply_Mode;
  SetLayerPropertiesForTesting(child.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);

  parent->AddChild(child);
  child->SetBlendMode(blend_mode);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Since the child layer has a blend mode other than normal, it should get
  // its own render surface. Also, layer's draw_properties should contain the
  // default blend mode, since the render surface becomes responsible for
  // applying the blend mode.
  ASSERT_TRUE(child->render_surface());
  EXPECT_EQ(1U, child->render_surface()->layer_list().size());
  EXPECT_EQ(SkXfermode::kSrcOver_Mode, child->draw_properties().blend_mode);
}

TEST_F(LayerTreeHostCommonTest, ForceRenderSurface) {
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  render_surface1->SetForceRenderSurface(true);

  host()->SetRootLayer(parent);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);

  parent->AddChild(render_surface1);
  render_surface1->AddChild(child);

  // Sanity check before the actual test
  EXPECT_FALSE(parent->render_surface());
  EXPECT_FALSE(render_surface1->render_surface());

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // The root layer always creates a render surface
    EXPECT_TRUE(parent->render_surface());
    EXPECT_TRUE(render_surface1->render_surface());
    EXPECT_EQ(2U, render_surface_layer_list.size());
  }

  {
    RenderSurfaceLayerList render_surface_layer_list;
    render_surface1->SetForceRenderSurface(false);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
    EXPECT_TRUE(parent->render_surface());
    EXPECT_FALSE(render_surface1->render_surface());
    EXPECT_EQ(1U, render_surface_layer_list.size());
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfacesFlattenScreenSpaceTransform) {
  // Render surfaces act as a flattening point for their subtree, so should
  // always flatten the target-to-screen space transform seen by descendants.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  gfx::Transform rotation_about_y_axis;
  rotation_about_y_axis.RotateAboutYAxis(30.0);
  // Make |parent| have a render surface.
  parent->SetOpacity(0.9f);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(parent.get(), rotation_about_y_axis,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(10, 10),
                               true, false);
  SetLayerPropertiesForTesting(child.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  SetLayerPropertiesForTesting(grand_child.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(10, 10),
                               true, false);

  root->AddChild(parent);
  parent->AddChild(child);
  child->AddChild(grand_child);

  grand_child->SetShouldFlattenTransform(false);

  host()->SetRootLayer(root);

  // Only grand_child should preserve 3d.
  EXPECT_TRUE(root->should_flatten_transform());
  EXPECT_TRUE(parent->should_flatten_transform());
  EXPECT_TRUE(child->should_flatten_transform());
  EXPECT_FALSE(grand_child->should_flatten_transform());

  gfx::Transform expected_child_draw_transform = identity_matrix;
  gfx::Transform expected_grand_child_draw_transform = identity_matrix;

  gfx::Transform flattened_rotation_about_y = rotation_about_y_axis;
  flattened_rotation_about_y.FlattenTo2d();

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(parent->render_surface());
  EXPECT_FALSE(child->render_surface());
  EXPECT_FALSE(grand_child->render_surface());

  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix, child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_matrix,
                                  grand_child->draw_transform());

  // The screen-space transform inherited by |child| and |grand_child| should
  // have been flattened at their render target. In particular, the fact that
  // |grand_child| happens to preserve 3d shouldn't affect this flattening.
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_y,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(flattened_rotation_about_y,
                                  grand_child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsRenderSurfaces) {
  // The entire subtree of layers that are outside the clip rect should be
  // culled away, and should not affect the render_surface_layer_list.
  //
  // The test tree is set up as follows:
  //  - all layers except the leaf_nodes are forced to be a new render surface
  //  that have something to draw.
  //  - parent is a large container layer.
  //  - child has masksToBounds=true to cause clipping.
  //  - grand_child is positioned outside of the child's bounds
  //  - great_grand_child is also kept outside child's bounds.
  //
  // In this configuration, grand_child and great_grand_child are completely
  // outside the clip rect, and they should never get scheduled on the list of
  // render surfaces.
  //

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  scoped_refptr<Layer> great_grand_child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  parent->AddChild(child);
  child->AddChild(grand_child);
  grand_child->AddChild(great_grand_child);

  host()->SetRootLayer(parent);

  // leaf_node1 ensures that parent and child are kept on the
  // render_surface_layer_list, even though grand_child and great_grand_child
  // should be clipped.
  child->AddChild(leaf_node1);
  great_grand_child->AddChild(leaf_node2);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(great_grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);

  child->SetMasksToBounds(true);
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child->SetOpacity(0.5f);
  great_grand_child->SetOpacity(0.4f);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  ASSERT_EQ(2U, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(child->id(), render_surface_layer_list.at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, ClipRectCullsSurfaceWithoutVisibleContent) {
  // When a render surface has a clip rect, it is used to clip the content rect
  // of the surface. When the render surface is animating its transforms, then
  // the content rect's position in the clip rect is not defined on the main
  // thread, and its content rect should not be clipped.

  // The test tree is set up as follows:
  //  - parent is a container layer that masksToBounds=true to cause clipping.
  //  - child is a render surface, which has a clip rect set to the bounds of
  //  the parent.
  //  - grand_child is a render surface, and the only visible content in child.
  //  It is positioned outside of the clip rect from parent.

  // In this configuration, grand_child should be outside the clipped
  // content rect of the child, making grand_child not appear in the
  // render_surface_layer_list. However, when we place an animation on the
  // child, this clipping should be avoided and we should keep the grand_child
  // in the render_surface_layer_list.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  parent->AddChild(child);
  child->AddChild(grand_child);
  grand_child->AddChild(leaf_node);

  host()->SetRootLayer(parent);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(200.f, 200.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);

  parent->SetMasksToBounds(true);
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child->SetOpacity(0.4f);
  grand_child->SetForceRenderSurface(true);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // Without an animation, we should cull child and grand_child from the
    // render_surface_layer_list.
    ASSERT_EQ(1U, render_surface_layer_list.size());
    EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  }

  // Now put an animating transform on child.
  AddAnimatedTransformToController(
      child->layer_animation_controller(), 10.0, 30, 0);

  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        parent.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    // With an animating transform, we should keep child and grand_child in the
    // render_surface_layer_list.
    ASSERT_EQ(3U, render_surface_layer_list.size());
    EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
    EXPECT_EQ(child->id(), render_surface_layer_list.at(1)->id());
    EXPECT_EQ(grand_child->id(), render_surface_layer_list.at(2)->id());
  }
}

TEST_F(LayerTreeHostCommonTest, IsClippedIsSetCorrectly) {
  // Layer's IsClipped() property is set to true when:
  //  - the layer clips its subtree, e.g. masks to bounds,
  //  - the layer is clipped by an ancestor that contributes to the same
  //    render target,
  //  - a surface is clipped by an ancestor that contributes to the same
  //    render target.
  //
  // In particular, for a layer that owns a render surface:
  //  - the render surface inherits any clip from ancestors, and does NOT
  //    pass that clipped status to the layer itself.
  //  - but if the layer itself masks to bounds, it is considered clipped
  //    and propagates the clip to the subtree.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> child2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(parent);
  parent->AddChild(child1);
  parent->AddChild(child2);
  child1->AddChild(grand_child);
  child2->AddChild(leaf_node2);
  grand_child->AddChild(leaf_node1);

  host()->SetRootLayer(root);

  child2->SetForceRenderSurface(true);

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);

  // Case 1: nothing is clipped except the root render surface.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_FALSE(parent->is_clipped());
    EXPECT_FALSE(child1->is_clipped());
    EXPECT_FALSE(child2->is_clipped());
    EXPECT_FALSE(child2->render_surface()->is_clipped());
    EXPECT_FALSE(grand_child->is_clipped());
    EXPECT_FALSE(leaf_node1->is_clipped());
    EXPECT_FALSE(leaf_node2->is_clipped());
  }

  // Case 2: parent masksToBounds, so the parent, child1, and child2's
  // surface are clipped. But layers that contribute to child2's surface are
  // not clipped explicitly because child2's surface already accounts for
  // that clip.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    parent->SetMasksToBounds(true);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_TRUE(parent->is_clipped());
    EXPECT_TRUE(child1->is_clipped());
    EXPECT_FALSE(child2->is_clipped());
    EXPECT_TRUE(child2->render_surface()->is_clipped());
    EXPECT_TRUE(grand_child->is_clipped());
    EXPECT_TRUE(leaf_node1->is_clipped());
    EXPECT_FALSE(leaf_node2->is_clipped());
  }

  // Case 3: child2 masksToBounds. The layer and subtree are clipped, and
  // child2's render surface is not clipped.
  {
    RenderSurfaceLayerList render_surface_layer_list;
    parent->SetMasksToBounds(false);
    child2->SetMasksToBounds(true);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root.get(), parent->bounds(), &render_surface_layer_list);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    ASSERT_TRUE(root->render_surface());
    ASSERT_TRUE(child2->render_surface());

    EXPECT_FALSE(root->is_clipped());
    EXPECT_TRUE(root->render_surface()->is_clipped());
    EXPECT_FALSE(parent->is_clipped());
    EXPECT_FALSE(child1->is_clipped());
    EXPECT_TRUE(child2->is_clipped());
    EXPECT_FALSE(child2->render_surface()->is_clipped());
    EXPECT_FALSE(grand_child->is_clipped());
    EXPECT_FALSE(leaf_node1->is_clipped());
    EXPECT_TRUE(leaf_node2->is_clipped());
  }
}

TEST_F(LayerTreeHostCommonTest, DrawableContentRectForLayers) {
  // Verify that layers get the appropriate DrawableContentRect when their
  // parent masksToBounds is true.
  //
  //   grand_child1 - completely inside the region; DrawableContentRect should
  //   be the layer rect expressed in target space.
  //   grand_child2 - partially clipped but NOT masksToBounds; the clip rect
  //   will be the intersection of layer bounds and the mask region.
  //   grand_child3 - partially clipped and masksToBounds; the
  //   DrawableContentRect will still be the intersection of layer bounds and
  //   the mask region.
  //   grand_child4 - outside parent's clip rect; the DrawableContentRect should
  //   be empty.
  //

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child3 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child4 = Layer::Create(layer_settings());

  parent->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);
  child->AddChild(grand_child4);

  host()->SetRootLayer(parent);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child4.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               true,
                               false);

  child->SetMasksToBounds(true);
  grand_child3->SetMasksToBounds(true);

  // Force everyone to be a render surface.
  child->SetOpacity(0.4f);
  grand_child1->SetOpacity(0.5f);
  grand_child2->SetOpacity(0.5f);
  grand_child3->SetOpacity(0.5f);
  grand_child4->SetOpacity(0.5f);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5), grand_child3->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(15, 15, 5, 5), grand_child3->drawable_content_rect());
  EXPECT_TRUE(grand_child4->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest, ClipRectIsPropagatedCorrectlyToSurfaces) {
  // Verify that render surfaces (and their layers) get the appropriate
  // clip rects when their parent masksToBounds is true.
  //
  // Layers that own render surfaces (at least for now) do not inherit any
  // clipping; instead the surface will enforce the clip for the entire subtree.
  // They may still have a clip rect of their own layer bounds, however, if
  // masksToBounds was true.
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child2 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child3 = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child4 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> leaf_node4 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  parent->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);
  child->AddChild(grand_child4);

  host()->SetRootLayer(parent);

  // the leaf nodes ensure that these grand_children become render surfaces for
  // this test.
  grand_child1->AddChild(leaf_node1);
  grand_child2->AddChild(leaf_node2);
  grand_child3->AddChild(leaf_node3);
  grand_child4->AddChild(leaf_node4);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(500, 500),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(15.f, 15.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child4.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(45.f, 45.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(leaf_node4.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);

  child->SetMasksToBounds(true);
  grand_child3->SetMasksToBounds(true);
  grand_child4->SetMasksToBounds(true);

  // Force everyone to be a render surface.
  child->SetOpacity(0.4f);
  child->SetForceRenderSurface(true);
  grand_child1->SetOpacity(0.5f);
  grand_child1->SetForceRenderSurface(true);
  grand_child2->SetOpacity(0.5f);
  grand_child2->SetForceRenderSurface(true);
  grand_child3->SetOpacity(0.5f);
  grand_child3->SetForceRenderSurface(true);
  grand_child4->SetOpacity(0.5f);
  grand_child4->SetForceRenderSurface(true);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  ASSERT_TRUE(grand_child1->render_surface());
  ASSERT_TRUE(grand_child2->render_surface());
  ASSERT_TRUE(grand_child3->render_surface());

  // Surfaces are clipped by their parent, but un-affected by the owning layer's
  // masksToBounds.
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child1->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child2->render_surface()->clip_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20),
            grand_child3->render_surface()->clip_rect());
}

TEST_F(LayerTreeHostCommonTest, AnimationsForRenderSurfaceHierarchy) {
  LayerImpl* parent = root_layer();
  LayerImpl* render_surface1 = AddChildToRoot<LayerImpl>();
  LayerImpl* child_of_rs1 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* grand_child_of_rs1 = AddChild<LayerImpl>(child_of_rs1);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(render_surface1);
  LayerImpl* child_of_rs2 = AddChild<LayerImpl>(render_surface2);
  LayerImpl* grand_child_of_rs2 = AddChild<LayerImpl>(child_of_rs2);
  LayerImpl* child_of_root = AddChildToRoot<LayerImpl>();
  LayerImpl* grand_child_of_root = AddChild<LayerImpl>(child_of_root);

  grand_child_of_rs1->SetDrawsContent(true);
  grand_child_of_rs2->SetDrawsContent(true);

  gfx::Transform layer_transform;
  layer_transform.Translate(1.0, 1.0);

  SetLayerPropertiesForTesting(
      parent, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, true);
  SetLayerPropertiesForTesting(
      render_surface1, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, true);
  SetLayerPropertiesForTesting(
      render_surface2, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, true);
  SetLayerPropertiesForTesting(
      child_of_root, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);
  SetLayerPropertiesForTesting(
      child_of_rs1, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);
  SetLayerPropertiesForTesting(
      child_of_rs2, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);
  SetLayerPropertiesForTesting(
      grand_child_of_root, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);
  SetLayerPropertiesForTesting(
      grand_child_of_rs1, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);
  SetLayerPropertiesForTesting(
      grand_child_of_rs2, layer_transform, gfx::Point3F(0.25f, 0.f, 0.f),
      gfx::PointF(2.5f, 0.f), gfx::Size(10, 10), true, false, false);

  // Put an animated opacity on the render surface.
  AddOpacityTransitionToController(
      render_surface1->layer_animation_controller(), 10.0, 1.f, 0.f, false);

  // Also put an animated opacity on a layer without descendants.
  AddOpacityTransitionToController(
      grand_child_of_root->layer_animation_controller(), 10.0, 1.f, 0.f, false);

  // Put a transform animation on the render surface.
  AddAnimatedTransformToController(
      render_surface2->layer_animation_controller(), 10.0, 30, 0);

  // Also put transform animations on grand_child_of_root, and
  // grand_child_of_rs2
  AddAnimatedTransformToController(
      grand_child_of_root->layer_animation_controller(), 10.0, 30, 0);
  AddAnimatedTransformToController(
      grand_child_of_rs2->layer_animation_controller(), 10.0, 30, 0);

  ExecuteCalculateDrawProperties(parent);

  // Only layers that are associated with render surfaces should have an actual
  // RenderSurface() value.
  ASSERT_TRUE(parent->render_surface());
  ASSERT_FALSE(child_of_root->render_surface());
  ASSERT_FALSE(grand_child_of_root->render_surface());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_FALSE(child_of_rs1->render_surface());
  ASSERT_FALSE(grand_child_of_rs1->render_surface());

  ASSERT_TRUE(render_surface2->render_surface());
  ASSERT_FALSE(child_of_rs2->render_surface());
  ASSERT_FALSE(grand_child_of_rs2->render_surface());

  // Verify all render target accessors
  EXPECT_EQ(parent, parent->render_target());
  EXPECT_EQ(parent, child_of_root->render_target());
  EXPECT_EQ(parent, grand_child_of_root->render_target());

  EXPECT_EQ(render_surface1, render_surface1->render_target());
  EXPECT_EQ(render_surface1, child_of_rs1->render_target());
  EXPECT_EQ(render_surface1, grand_child_of_rs1->render_target());

  EXPECT_EQ(render_surface2, render_surface2->render_target());
  EXPECT_EQ(render_surface2, child_of_rs2->render_target());
  EXPECT_EQ(render_surface2, grand_child_of_rs2->render_target());

  // Verify draw_opacity_is_animating values
  EXPECT_FALSE(parent->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_root->draw_opacity_is_animating());
  EXPECT_TRUE(grand_child_of_root->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface1->draw_opacity_is_animating());
  EXPECT_TRUE(render_surface1->render_surface()->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_rs1->draw_opacity_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface2->draw_opacity_is_animating());
  EXPECT_FALSE(render_surface2->render_surface()->draw_opacity_is_animating());
  EXPECT_FALSE(child_of_rs2->draw_opacity_is_animating());
  EXPECT_FALSE(grand_child_of_rs2->draw_opacity_is_animating());

  // Verify draw_transform_is_animating values
  EXPECT_FALSE(parent->draw_transform_is_animating());
  EXPECT_FALSE(child_of_root->draw_transform_is_animating());
  EXPECT_TRUE(grand_child_of_root->draw_transform_is_animating());
  EXPECT_FALSE(render_surface1->draw_transform_is_animating());
  EXPECT_FALSE(render_surface1->render_surface()
                   ->target_surface_transforms_are_animating());
  EXPECT_FALSE(child_of_rs1->draw_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->draw_transform_is_animating());
  EXPECT_FALSE(render_surface2->draw_transform_is_animating());
  EXPECT_TRUE(render_surface2->render_surface()
                  ->target_surface_transforms_are_animating());
  EXPECT_FALSE(child_of_rs2->draw_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->draw_transform_is_animating());

  // Verify screen_space_transform_is_animating values
  EXPECT_FALSE(parent->screen_space_transform_is_animating());
  EXPECT_FALSE(child_of_root->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_root->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->screen_space_transform_is_animating());
  EXPECT_FALSE(render_surface1->render_surface()
                   ->screen_space_transforms_are_animating());
  EXPECT_FALSE(child_of_rs1->screen_space_transform_is_animating());
  EXPECT_FALSE(grand_child_of_rs1->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->screen_space_transform_is_animating());
  EXPECT_TRUE(render_surface2->render_surface()
                  ->screen_space_transforms_are_animating());
  EXPECT_TRUE(child_of_rs2->screen_space_transform_is_animating());
  EXPECT_TRUE(grand_child_of_rs2->screen_space_transform_is_animating());

  // Sanity check. If these fail there is probably a bug in the test itself.
  // It is expected that we correctly set up transforms so that the y-component
  // of the screen-space transform encodes the "depth" of the layer in the tree.
  EXPECT_FLOAT_EQ(1.0, parent->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(2.0,
                  child_of_root->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      3.0, grand_child_of_root->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(2.0,
                  render_surface1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(3.0,
                  child_of_rs1->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      4.0, grand_child_of_rs1->screen_space_transform().matrix().get(1, 3));

  EXPECT_FLOAT_EQ(3.0,
                  render_surface2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(4.0,
                  child_of_rs2->screen_space_transform().matrix().get(1, 3));
  EXPECT_FLOAT_EQ(
      5.0, grand_child_of_rs2->screen_space_transform().matrix().get(1, 3));
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForIdentityTransform) {
  // Test the calculateVisibleRect() function works correctly for identity
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  gfx::Rect layer_content_rect = gfx::Rect(10, 10, 30, 30);
  gfx::Rect expected = gfx::Rect(10, 10, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_content_rect = gfx::Rect(120, 120, 30, 30);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_content_rect = gfx::Rect(80, 80, 30, 30);
  expected = gfx::Rect(80, 80, 20, 20);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForTranslations) {
  // Test the calculateVisibleRect() function works correctly for scaling
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(10.0, 10.0);
  gfx::Rect expected = gfx::Rect(0, 0, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(120.0, 120.0);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: Layer is partially overlapping the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(80.0, 80.0);
  expected = gfx::Rect(0, 0, 20, 20);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor2DRotations) {
  // Test the calculateVisibleRect() function works correctly for rotations
  // about z-axis (i.e. 2D rotations).  Remember that calculateVisibleRect()
  // should return the g in the layer's space.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 30, 30);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Layer is contained within the surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.Rotate(45.0);
  gfx::Rect expected = gfx::Rect(0, 0, 30, 30);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 2: Layer is outside the surface rect.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-50.0, 0.0);
  layer_to_surface_transform.Rotate(45.0);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_TRUE(actual.IsEmpty());

  // Case 3: The layer is rotated about its top-left corner. In surface space,
  // the layer is oriented diagonally, with the left half outside of the render
  // surface. In this case, the g should still be the entire layer
  // (remember the g is computed in layer space); both the top-left
  // and bottom-right corners of the layer are still visible.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Rotate(45.0);
  expected = gfx::Rect(0, 0, 30, 30);
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 4: The layer is rotated about its top-left corner, and translated
  // upwards. In surface space, the layer is oriented diagonally, with only the
  // top corner of the surface overlapping the layer. In layer space, the render
  // surface overlaps the right side of the layer. The g should be
  // the layer's right half.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(0.0, -sqrt(2.0) * 15.0);
  layer_to_surface_transform.Rotate(45.0);
  expected = gfx::Rect(15, 0, 15, 30);  // Right half of layer bounds.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dOrthographicTransform) {
  // Test that the calculateVisibleRect() function works correctly for 3d
  // transforms.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, should be fully contained in the render surface.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  gfx::Rect expected = gfx::Rect(0, 0, 100, 100);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 2: Orthographic projection of a layer rotated about y-axis by 45
  // degrees, but shifted to the side so only the right-half the layer would be
  // visible on the surface.
  // 100 is the un-rotated layer width; divided by sqrt(2) is the rotated width.
  SkMScalar half_width_of_rotated_layer =
      SkDoubleToMScalar((100.0 / sqrt(2.0)) * 0.5);
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(-half_width_of_rotated_layer, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);  // Rotates about the left
                                                      // edge of the layer.
  expected = gfx::Rect(50, 0, 50, 100);  // Tight half of the layer.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dPerspectiveTransform) {
  // Test the calculateVisibleRect() function works correctly when the layer has
  // a perspective projection onto the target surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-50, -50, 200, 200);
  gfx::Transform layer_to_surface_transform;

  // Case 1: Even though the layer is twice as large as the surface, due to
  // perspective foreshortening, the layer will fit fully in the surface when
  // its translated more than the perspective amount.
  layer_to_surface_transform.MakeIdentity();

  // The following sequence of transforms applies the perspective about the
  // center of the surface.
  layer_to_surface_transform.Translate(50.0, 50.0);
  layer_to_surface_transform.ApplyPerspectiveDepth(9.0);
  layer_to_surface_transform.Translate(-50.0, -50.0);

  // This translate places the layer in front of the surface's projection plane.
  layer_to_surface_transform.Translate3d(0.0, 0.0, -27.0);

  gfx::Rect expected = gfx::Rect(-50, -50, 200, 200);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);

  // Case 2: same projection as before, except that the layer is also translated
  // to the side, so that only the right half of the layer should be visible.
  //
  // Explanation of expected result: The perspective ratio is (z distance
  // between layer and camera origin) / (z distance between projection plane and
  // camera origin) == ((-27 - 9) / 9) Then, by similar triangles, if we want to
  // move a layer by translating -50 units in projected surface units (so that
  // only half of it is visible), then we would need to translate by (-36 / 9) *
  // -50 == -200 in the layer's units.
  layer_to_surface_transform.Translate3d(-200.0, 0.0, 0.0);
  expected = gfx::Rect(gfx::Point(50, -50),
                       gfx::Size(100, 200));  // The right half of the layer's
                                              // bounding rect.
  actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectFor3dOrthographicIsNotClippedBehindSurface) {
  // There is currently no explicit concept of an orthographic projection plane
  // in our code (nor in the CSS spec to my knowledge). Therefore, layers that
  // are technically behind the surface in an orthographic world should not be
  // clipped when they are flattened to the surface.

  gfx::Rect target_surface_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Transform layer_to_surface_transform;

  // This sequence of transforms effectively rotates the layer about the y-axis
  // at the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.Translate(50.0, 0.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.Translate(-50.0, 0.0);

  gfx::Rect expected = gfx::Rect(0, 0, 100, 100);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, VisibleRectFor3dPerspectiveWhenClippedByW) {
  // Test the calculateVisibleRect() function works correctly when projecting a
  // surface onto a layer, but the layer is partially behind the camera (not
  // just behind the projection plane). In this case, the cartesian coordinates
  // may seem to be valid, but actually they are not. The visible rect needs to
  // be properly clipped by the w = 0 plane in homogeneous coordinates before
  // converting to cartesian coordinates.

  gfx::Rect target_surface_rect = gfx::Rect(-50, -50, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-10, -1, 20, 2);
  gfx::Transform layer_to_surface_transform;

  // The layer is positioned so that the right half of the layer should be in
  // front of the camera, while the other half is behind the surface's
  // projection plane. The following sequence of transforms applies the
  // perspective and rotation about the center of the layer.
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(-2.0, 0.0, 1.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);

  // Sanity check that this transform does indeed cause w < 0 when applying the
  // transform, otherwise this code is not testing the intended scenario.
  bool clipped;
  MathUtil::MapQuad(layer_to_surface_transform,
                    gfx::QuadF(gfx::RectF(layer_content_rect)),
                    &clipped);
  ASSERT_TRUE(clipped);

  int expected_x_position = 0;
  int expected_width = 10;
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected_x_position, actual.x());
  EXPECT_EQ(expected_width, actual.width());
}

TEST_F(LayerTreeHostCommonTest, VisibleRectForPerspectiveUnprojection) {
  // To determine visible rect in layer space, there needs to be an
  // un-projection from surface space to layer space. When the original
  // transform was a perspective projection that was clipped, it returns a rect
  // that encloses the clipped bounds.  Un-projecting this new rect may require
  // clipping again.

  // This sequence of transforms causes one corner of the layer to protrude
  // across the w = 0 plane, and should be clipped.
  gfx::Rect target_surface_rect = gfx::Rect(-50, -50, 100, 100);
  gfx::Rect layer_content_rect = gfx::Rect(-10, -10, 20, 20);
  gfx::Transform layer_to_surface_transform;
  layer_to_surface_transform.MakeIdentity();
  layer_to_surface_transform.ApplyPerspectiveDepth(1.0);
  layer_to_surface_transform.Translate3d(0.0, 0.0, -5.0);
  layer_to_surface_transform.RotateAboutYAxis(45.0);
  layer_to_surface_transform.RotateAboutXAxis(80.0);

  // Sanity check that un-projection does indeed cause w < 0, otherwise this
  // code is not testing the intended scenario.
  bool clipped;
  gfx::RectF clipped_rect =
      MathUtil::MapClippedRect(layer_to_surface_transform, layer_content_rect);
  MathUtil::ProjectQuad(
      Inverse(layer_to_surface_transform), gfx::QuadF(clipped_rect), &clipped);
  ASSERT_TRUE(clipped);

  // Only the corner of the layer is not visible on the surface because of being
  // clipped. But, the net result of rounding visible region to an axis-aligned
  // rect is that the entire layer should still be considered visible.
  gfx::Rect expected = gfx::Rect(-10, -10, 20, 20);
  gfx::Rect actual = LayerTreeHostCommon::CalculateVisibleRect(
      target_surface_rect, layer_content_rect, layer_to_surface_transform);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest,
       VisibleRectsForPositionedRootLayerClippedByViewport) {
  scoped_refptr<Layer> root =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  // Root layer is positioned at (60, 70). The default device viewport size
  // is (0, 0, 100x100) in target space. So the root layer's visible rect
  // will be clipped by the viewport to be (0, 0, 40x30) in layer's space.
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(60, 70), gfx::Size(100, 100), true,
                               false);
  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  // In target space, not clipped.
  EXPECT_EQ(gfx::Rect(60, 70, 100, 100), root->drawable_content_rect());
  // In layer space, clipped.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 30), root->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsForSimpleLayers) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child1);
  root->AddChild(child2);
  root->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible_layer_rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // layer visible_layer_rects are clipped by their target surface.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_TRUE(child3->visible_layer_rect().IsEmpty());

  // layer drawable_content_rects are not clipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersClippedByLayer) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> grand_child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> grand_child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child);
  child->AddChild(grand_child1);
  child->AddChild(grand_child2);
  child->AddChild(grand_child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  child->SetMasksToBounds(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_FALSE(child->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), child->visible_layer_rect());

  // All grandchild visible content rects should be clipped by child.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), grand_child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), grand_child2->visible_layer_rect());
  EXPECT_TRUE(grand_child3->visible_layer_rect().IsEmpty());

  // All grandchild DrawableContentRects should also be clipped by child.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), grand_child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 25, 25), grand_child2->drawable_content_rect());
  EXPECT_TRUE(grand_child3->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectWithClippingAndScaling) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child);
  child->AddChild(grand_child);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform child_scale_matrix;
  child_scale_matrix.Scale(0.25f, 0.25f);
  gfx::Transform grand_child_scale_matrix;
  grand_child_scale_matrix.Scale(0.246f, 0.246f);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), child_scale_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  SetLayerPropertiesForTesting(grand_child.get(), grand_child_scale_matrix,
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(100, 100), true, false);

  child->SetMasksToBounds(true);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  // The visible rect is expanded to integer coordinates in target space before
  // being projected back to layer space, where it is once again expanded to
  // integer coordinates.
  EXPECT_EQ(gfx::Rect(49, 49), grand_child->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInUnclippedRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);
  render_surface1->AddChild(child2);
  render_surface1->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());

  // An unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  EXPECT_EQ(gfx::Rect(5, 5, 170, 170),
            render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child3->visible_layer_rect());

  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleContentRectsForClippedSurfaceWithEmptyClip) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child1);
  root->AddChild(child2);
  root->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child1.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(5.f, 5.f), gfx::Size(50, 50), true,
                               false);
  SetLayerPropertiesForTesting(child2.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(75.f, 75.f), gfx::Size(50, 50), true,
                               false);
  SetLayerPropertiesForTesting(child3.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(125.f, 125.f), gfx::Size(50, 50),
                               true, false);

  RenderSurfaceLayerList render_surface_layer_list;
  // Now set the root render surface an empty clip.
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), gfx::Size(), &render_surface_layer_list);

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  ASSERT_TRUE(root->render_surface());
  EXPECT_FALSE(root->is_clipped());

  gfx::Rect empty;
  EXPECT_EQ(empty, root->render_surface()->clip_rect());
  EXPECT_TRUE(root->render_surface()->is_clipped());

  // Visible content rect calculation will check if the target surface is
  // clipped or not. An empty clip rect does not indicate the render surface
  // is unclipped.
  EXPECT_EQ(empty, child1->visible_layer_rect());
  EXPECT_EQ(empty, child2->visible_layer_rect());
  EXPECT_EQ(empty, child3->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersWithUninvertibleTransform) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child);

  host()->SetRootLayer(root);

  // Case 1: a truly degenerate matrix
  gfx::Transform identity_matrix;
  gfx::Transform uninvertible_matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());

  // Case 2: a matrix with flattened z, uninvertible and not visible according
  // to the CSS spec.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());

  // Case 3: a matrix with flattened z, also uninvertible and not visible.
  uninvertible_matrix.MakeIdentity();
  uninvertible_matrix.Translate(500.0, 0.0);
  uninvertible_matrix.matrix().set(2, 2, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(child.get(),
                               uninvertible_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(child->visible_layer_rect().IsEmpty());
  EXPECT_TRUE(child->drawable_content_rect().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest,
       SingularTransformDoesNotPreventClearingDrawProperties) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform uninvertible_matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  child->set_sorted_for_recursion(true);

  TransformOperations start_transform_operations;
  start_transform_operations.AppendScale(1.f, 0.f, 0.f);

  TransformOperations end_transform_operations;
  end_transform_operations.AppendScale(1.f, 1.f, 0.f);

  AddAnimatedTransformToLayer(
      root.get(), 10.0, start_transform_operations, end_transform_operations);

  EXPECT_TRUE(root->TransformIsAnimating());

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_FALSE(child->sorted_for_recursion());
}

TEST_F(LayerTreeHostCommonTest,
       SingularNonAnimatingTransformDoesNotPreventClearingDrawProperties) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform uninvertible_matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  ASSERT_FALSE(uninvertible_matrix.IsInvertible());

  SetLayerPropertiesForTesting(root.get(),
                               uninvertible_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);

  root->set_sorted_for_recursion(true);

  EXPECT_FALSE(root->TransformIsAnimating());

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_FALSE(root->sorted_for_recursion());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForLayersInClippedRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);
  render_surface1->AddChild(child2);
  render_surface1->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::Rect(5, 5, 95, 95),
            render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the surface have their visible content
  // rect clipped by the surface clip rect.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 25, 25), child2->visible_layer_rect());
  EXPECT_TRUE(child3->visible_layer_rect().IsEmpty());

  // But the DrawableContentRects are unclipped.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsForSurfaceHierarchy) {
  // Check that clipping does not propagate down surfaces.
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface2 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child3 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(render_surface1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child1);
  render_surface2->AddChild(child2);
  render_surface2->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(7, 13),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface2->visible_layer_rect());

  // A clipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree, but also gets clamped by the ancestor's clip.
  EXPECT_EQ(gfx::Rect(5, 5, 95, 95),
            render_surface1->render_surface()->DrawableContentRect());

  // render_surface1 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1's content rect. So,
  // render_surface2 grows to enclose all drawable content of its subtree.
  EXPECT_EQ(gfx::Rect(5, 5, 170, 170),
            render_surface2->render_surface()->DrawableContentRect());

  // All layers that draw content into render_surface2 think they are unclipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child3->visible_layer_rect());

  // DrawableContentRects are also unclipped.
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(75, 75, 50, 50), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(125, 125, 50, 50), child3->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnUnclippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               child_rotation,
                               gfx::Point3F(25, 25, 0.f),
                               gfx::PointF(25.f, 25.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), root->drawable_content_rect());

  // Layers that do not draw content should have empty visible content rects.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), render_surface1->visible_layer_rect());

  // The unclipped surface grows its DrawableContentRect to include all drawable
  // regions of the subtree.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect expected_surface_drawable_content =
      gfx::Rect(50 - diagonal_radius,
                50 - diagonal_radius,
                diagonal_radius * 2,
                diagonal_radius * 2);
  EXPECT_EQ(expected_surface_drawable_content,
            render_surface1->render_surface()->DrawableContentRect());

  // All layers that draw content into the unclipped surface are also unclipped.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(expected_surface_drawable_content, child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest,
       DrawableAndVisibleContentRectsWithTransformOnClippedSurface) {
  // Layers that have non-axis aligned bounds (due to transforms) have an
  // expanded, axis-aligned DrawableContentRect and visible content rect.

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(render_surface1);
  render_surface1->AddChild(child1);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform child_rotation;
  child_rotation.Rotate(45.0);
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(3, 4),
                               true,
                               false);

  SetLayerPropertiesForTesting(child1.get(),
                               child_rotation,
                               gfx::Point3F(25, 25, 0.f),
                               gfx::PointF(25.f, 25.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(render_surface1->render_surface());

  // The clipped surface clamps the DrawableContentRect that encloses the
  // rotated layer.
  int diagonal_radius = ceil(sqrt(2.0) * 25.0);
  gfx::Rect unclipped_surface_content = gfx::Rect(50 - diagonal_radius,
                                                  50 - diagonal_radius,
                                                  diagonal_radius * 2,
                                                  diagonal_radius * 2);
  gfx::Rect expected_surface_drawable_content =
      gfx::IntersectRects(unclipped_surface_content, gfx::Rect(0, 0, 50, 50));
  EXPECT_EQ(expected_surface_drawable_content,
            render_surface1->render_surface()->DrawableContentRect());

  // On the clipped surface, only a quarter  of the child1 is visible, but when
  // rotating it back to  child1's content space, the actual enclosing rect ends
  // up covering the full left half of child1.
  //
  // Given the floating point math, this number is a little bit fuzzy.
  EXPECT_EQ(gfx::Rect(0, 0, 26, 50), child1->visible_layer_rect());

  // The child's DrawableContentRect is unclipped.
  EXPECT_EQ(unclipped_surface_content, child1->drawable_content_rect());
}

TEST_F(LayerTreeHostCommonTest, DrawableAndVisibleContentRectsInHighDPI) {
  MockContentLayerClient client;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<FakePictureLayer> render_surface1 =
      CreateDrawablePictureLayer(layer_settings(), &client);
  scoped_refptr<FakePictureLayer> render_surface2 =
      CreateDrawablePictureLayer(layer_settings(), &client);
  scoped_refptr<FakePictureLayer> child1 =
      CreateDrawablePictureLayer(layer_settings(), &client);
  scoped_refptr<FakePictureLayer> child2 =
      CreateDrawablePictureLayer(layer_settings(), &client);
  scoped_refptr<FakePictureLayer> child3 =
      CreateDrawablePictureLayer(layer_settings(), &client);
  root->AddChild(render_surface1);
  render_surface1->AddChild(render_surface2);
  render_surface2->AddChild(child1);
  render_surface2->AddChild(child2);
  render_surface2->AddChild(child3);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(3, 4),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(7, 13),
                               true,
                               false);
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(5.f, 5.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(75.f, 75.f),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(child3.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(125.f, 125.f),
                               gfx::Size(50, 50),
                               true,
                               false);

  float device_scale_factor = 2.f;

  root->SetMasksToBounds(true);
  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);
  ExecuteCalculateDrawProperties(root.get(), device_scale_factor);

  ASSERT_TRUE(render_surface1->render_surface());
  ASSERT_TRUE(render_surface2->render_surface());

  // drawable_content_rects for all layers and surfaces are scaled by
  // device_scale_factor.
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            root->render_surface()->DrawableContentRect());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), root->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(10, 10, 190, 190),
            render_surface1->render_surface()->DrawableContentRect());

  // render_surface2 lives in the "unclipped universe" of render_surface1, and
  // is only implicitly clipped by render_surface1.
  EXPECT_EQ(gfx::Rect(10, 10, 350, 350),
            render_surface2->render_surface()->DrawableContentRect());

  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), child1->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(150, 150, 100, 100), child2->drawable_content_rect());
  EXPECT_EQ(gfx::Rect(250, 250, 100, 100), child3->drawable_content_rect());

  // The root layer does not actually draw content of its own.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), root->visible_layer_rect());

  // All layer visible content rects are not expressed in content space of each
  // layer, so they are not scaled by the device_scale_factor.
  EXPECT_EQ(gfx::Rect(0, 0, 3, 4), render_surface1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 13), render_surface2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child1->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child2->visible_layer_rect());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child3->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithoutPreserves3d) {
  // Verify the behavior of back-face culling when there are no preserve-3d
  // layers. Note that 3d transforms still apply in this case, but they are
  // "flattened" to each parent layer according to current W3C spec.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  parent->AddChild(front_facing_child);
  parent->AddChild(back_facing_child);
  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(front_facing_child_of_front_facing_surface);
  front_facing_surface->AddChild(back_facing_child_of_front_facing_surface);
  back_facing_surface->AddChild(front_facing_child_of_back_facing_surface);
  back_facing_surface->AddChild(back_facing_child_of_back_facing_surface);

  host()->SetRootLayer(parent);

  // Nothing is double-sided
  front_facing_child->SetDoubleSided(false);
  back_facing_child->SetDoubleSided(false);
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);
  front_facing_child_of_front_facing_surface->SetDoubleSided(false);
  back_facing_child_of_front_facing_surface->SetDoubleSided(false);
  front_facing_child_of_back_facing_surface->SetDoubleSided(false);
  back_facing_child_of_back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Having a descendant and opacity will force these to have render surfaces.
  front_facing_surface->SetOpacity(0.5f);
  back_facing_surface->SetOpacity(0.5f);

  // Nothing preserves 3d. According to current W3C CSS gfx::Transforms spec,
  // these layers should blindly use their own local transforms to determine
  // back-face culling.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(front_facing_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(back_facing_child.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(front_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(back_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(front_facing_child_of_front_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_front_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(front_facing_child_of_back_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(back_facing_child_of_back_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created.
  EXPECT_FALSE(front_facing_child->render_surface());
  EXPECT_FALSE(back_facing_child->render_surface());
  EXPECT_TRUE(front_facing_surface->render_surface());
  EXPECT_TRUE(back_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->render_surface());

  // Verify the render_surface_layer_list.
  ASSERT_EQ(3u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());
  // Even though the back facing surface LAYER gets culled, the other
  // descendants should still be added, so the SURFACE should not be culled.
  EXPECT_EQ(back_facing_surface->id(), render_surface_layer_list.at(2)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      3u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()
                ->layer_list()
                .at(0)
                ->id());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()
                ->layer_list()
                .at(1)
                ->id());
  EXPECT_EQ(back_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()
                ->layer_list()
                .at(2)
                ->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()
                ->layer_list()
                .at(0)
                ->id());
  EXPECT_EQ(front_facing_child_of_front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()
                ->layer_list()
                .at(1)
                ->id());

  // Verify back_facing_surface's layer list; its own layer should be culled
  // from the surface list.
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(2)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child_of_back_facing_surface->id(),
            render_surface_layer_list.at(2)
                ->render_surface()
                ->layer_list()
                .at(0)
                ->id());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithPreserves3d) {
  // Verify the behavior of back-face culling when preserves-3d transform style
  // is used.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_front_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      front_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent>
      back_facing_child_of_back_facing_surface =
          make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> dummy_replica_layer1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> dummy_replica_layer2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  parent->AddChild(front_facing_child);
  parent->AddChild(back_facing_child);
  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(front_facing_child_of_front_facing_surface);
  front_facing_surface->AddChild(back_facing_child_of_front_facing_surface);
  back_facing_surface->AddChild(front_facing_child_of_back_facing_surface);
  back_facing_surface->AddChild(back_facing_child_of_back_facing_surface);

  host()->SetRootLayer(parent);

  // Nothing is double-sided
  front_facing_child->SetDoubleSided(false);
  back_facing_child->SetDoubleSided(false);
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);
  front_facing_child_of_front_facing_surface->SetDoubleSided(false);
  back_facing_child_of_front_facing_surface->SetDoubleSided(false);
  front_facing_child_of_back_facing_surface->SetDoubleSided(false);
  back_facing_child_of_back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Opacity will not force creation of render surfaces in this case because of
  // the preserve-3d transform style. Instead, an example of when a surface
  // would be created with preserve-3d is when there is a replica layer.
  front_facing_surface->SetReplicaLayer(dummy_replica_layer1.get());
  back_facing_surface->SetReplicaLayer(dummy_replica_layer2.get());

  // Each surface creates its own new 3d rendering context (as defined by W3C
  // spec).  According to current W3C CSS gfx::Transforms spec, layers in a 3d
  // rendering context should use the transform with respect to that context.
  // This 3d rendering context occurs when (a) parent's transform style is flat
  // and (b) the layer's transform style is preserve-3d.
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);  // parent transform style is flat.
  SetLayerPropertiesForTesting(front_facing_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(back_facing_child.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  // surface transform style is preserve-3d.
  SetLayerPropertiesForTesting(front_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false,
                               true);
  // surface transform style is preserve-3d.
  SetLayerPropertiesForTesting(back_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false,
                               true);
  SetLayerPropertiesForTesting(front_facing_child_of_front_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);
  SetLayerPropertiesForTesting(back_facing_child_of_front_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);
  SetLayerPropertiesForTesting(front_facing_child_of_back_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);
  SetLayerPropertiesForTesting(back_facing_child_of_back_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created and used.
  EXPECT_FALSE(front_facing_child->render_surface());
  EXPECT_FALSE(back_facing_child->render_surface());
  EXPECT_TRUE(front_facing_surface->render_surface());
  EXPECT_NE(back_facing_surface->render_target(), back_facing_surface);
  // We expect that a render_surface was created but not used.
  EXPECT_TRUE(back_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_front_facing_surface->render_surface());
  EXPECT_FALSE(front_facing_child_of_back_facing_surface->render_surface());
  EXPECT_FALSE(back_facing_child_of_back_facing_surface->render_surface());

  // Verify the render_surface_layer_list. The back-facing surface should be
  // culled.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(1)->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(front_facing_child_of_front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, BackFaceCullingWithAnimatingTransforms) {
  // Verify that layers are appropriately culled when their back face is showing
  // and they are not double sided, while animations are going on.
  //
  // Layers that are animating do not get culled on the main thread, as their
  // transforms should be treated as "unknown" so we can not be sure that their
  // back face is really showing.
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> animating_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child_of_animating_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> animating_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  parent->AddChild(child);
  parent->AddChild(animating_surface);
  animating_surface->AddChild(child_of_animating_surface);
  parent->AddChild(animating_child);
  parent->AddChild(child2);

  host()->SetRootLayer(parent);

  // Nothing is double-sided
  child->SetDoubleSided(false);
  child2->SetDoubleSided(false);
  animating_surface->SetDoubleSided(false);
  child_of_animating_surface->SetDoubleSided(false);
  animating_child->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  // Make our render surface.
  animating_surface->SetForceRenderSurface(true);

  // Animate the transform on the render surface.
  AddAnimatedTransformToController(
      animating_surface->layer_animation_controller(), 10.0, 30, 0);
  // This is just an animating layer, not a surface.
  AddAnimatedTransformToController(
      animating_child->layer_animation_controller(), 10.0, 30, 0);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(animating_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child_of_animating_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(animating_child.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_FALSE(child->render_surface());
  EXPECT_TRUE(animating_surface->render_surface());
  EXPECT_FALSE(child_of_animating_surface->render_surface());
  EXPECT_FALSE(animating_child->render_surface());
  EXPECT_FALSE(child2->render_surface());

  // Verify that the animating_child and child_of_animating_surface were not
  // culled, but that child was.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(animating_surface->id(), render_surface_layer_list.at(1)->id());

  // The non-animating child be culled from the layer list for the parent render
  // surface.
  ASSERT_EQ(
      3u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(animating_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(animating_child->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(1)->id());
  EXPECT_EQ(child2->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(2)->id());

  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(animating_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child_of_animating_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());

  EXPECT_FALSE(child2->visible_layer_rect().IsEmpty());

  // The animating layers should have a visible content rect that represents the
  // area of the front face that is within the viewport.
  EXPECT_EQ(animating_child->visible_layer_rect(),
            gfx::Rect(animating_child->bounds()));
  EXPECT_EQ(animating_surface->visible_layer_rect(),
            gfx::Rect(animating_surface->bounds()));
  // And layers in the subtree of the animating layer should have valid visible
  // content rects also.
  EXPECT_EQ(child_of_animating_surface->visible_layer_rect(),
            gfx::Rect(child_of_animating_surface->bounds()));
}

TEST_F(LayerTreeHostCommonTest,
     BackFaceCullingWithPreserves3dForFlatteningSurface) {
  // Verify the behavior of back-face culling for a render surface that is
  // created when it flattens its subtree, and its parent has preserves-3d.

  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> front_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> back_facing_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child1 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child2 =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  parent->AddChild(front_facing_surface);
  parent->AddChild(back_facing_surface);
  front_facing_surface->AddChild(child1);
  back_facing_surface->AddChild(child2);

  host()->SetRootLayer(parent);

  // RenderSurfaces are not double-sided
  front_facing_surface->SetDoubleSided(false);
  back_facing_surface->SetDoubleSided(false);

  gfx::Transform backface_matrix;
  backface_matrix.Translate(50.0, 50.0);
  backface_matrix.RotateAboutYAxis(180.0);
  backface_matrix.Translate(-50.0, -50.0);

  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               false,
                               true);  // parent transform style is preserve3d.
  SetLayerPropertiesForTesting(front_facing_surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);  // surface transform style is flat.
  SetLayerPropertiesForTesting(back_facing_surface.get(),
                               backface_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               true);  // surface transform style is flat.
  SetLayerPropertiesForTesting(child1.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child2.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);

  front_facing_surface->Set3dSortingContextId(1);
  back_facing_surface->Set3dSortingContextId(1);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Verify which render surfaces were created and used.
  EXPECT_TRUE(front_facing_surface->render_surface());

  // We expect the render surface to have been created, but remain unused.
  EXPECT_TRUE(back_facing_surface->render_surface());
  EXPECT_NE(back_facing_surface->render_target(),
            back_facing_surface);  // because it should be culled
  EXPECT_FALSE(child1->render_surface());
  EXPECT_FALSE(child2->render_surface());

  // Verify the render_surface_layer_list. The back-facing surface should be
  // culled.
  ASSERT_EQ(2u, render_surface_layer_list.size());
  EXPECT_EQ(parent->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(front_facing_surface->id(), render_surface_layer_list.at(1)->id());

  // Verify root surface's layer list.
  ASSERT_EQ(
      1u,
      render_surface_layer_list.at(0)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(0)
                ->render_surface()->layer_list().at(0)->id());

  // Verify front_facing_surface's layer list.
  ASSERT_EQ(
      2u,
      render_surface_layer_list.at(1)->render_surface()->layer_list().size());
  EXPECT_EQ(front_facing_surface->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child1->id(),
            render_surface_layer_list.at(1)
                ->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonScalingTest, LayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers not in a surface.
  gfx::Transform identity_matrix;

  LayerImpl* parent = root_layer();
  SetLayerPropertiesForTesting(parent, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), false, true,
                               true);

  LayerImpl* child = AddChildToRoot<LayerImpl>();
  SetLayerPropertiesForTesting(child, identity_matrix, gfx::Point3F(),
                               gfx::PointF(2.f, 2.f), gfx::Size(10, 10), false,
                               true, false);

  LayerImpl* child_empty = AddChildToRoot<LayerImpl>();
  SetLayerPropertiesForTesting(child_empty, identity_matrix, gfx::Point3F(),
                               gfx::PointF(2.f, 2.f), gfx::Size(), false, true,
                               false);

  float device_scale_factor = 2.5f;
  gfx::Size viewport_size(100, 100);
  ExecuteCalculateDrawProperties(parent, device_scale_factor);

  EXPECT_IDEAL_SCALE_EQ(device_scale_factor, parent);
  EXPECT_IDEAL_SCALE_EQ(device_scale_factor, child);
  EXPECT_IDEAL_SCALE_EQ(device_scale_factor, child_empty);

  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());

  // Verify parent transforms
  gfx::Transform expected_parent_transform;
  expected_parent_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->draw_transform());

  // Verify results of transformed parent rects
  gfx::RectF parent_bounds(parent->bounds());

  gfx::RectF parent_draw_rect =
      MathUtil::MapClippedRect(parent->draw_transform(), parent_bounds);
  gfx::RectF parent_screen_space_rect =
      MathUtil::MapClippedRect(parent->screen_space_transform(), parent_bounds);

  gfx::RectF expected_parent_draw_rect(parent->bounds());
  expected_parent_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_parent_draw_rect, parent_screen_space_rect);

  // Verify child and child_empty transforms. They should match.
  gfx::Transform expected_child_transform;
  expected_child_transform.Scale(device_scale_factor, device_scale_factor);
  expected_child_transform.Translate(child->position().x(),
                                     child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_empty->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_child_transform,
                                  child_empty->screen_space_transform());

  // Verify results of transformed child and child_empty rects. They should
  // match.
  gfx::RectF child_bounds(child->bounds());

  gfx::RectF child_draw_rect =
      MathUtil::MapClippedRect(child->draw_transform(), child_bounds);
  gfx::RectF child_screen_space_rect =
      MathUtil::MapClippedRect(child->screen_space_transform(), child_bounds);

  gfx::RectF child_empty_draw_rect =
      MathUtil::MapClippedRect(child_empty->draw_transform(), child_bounds);
  gfx::RectF child_empty_screen_space_rect = MathUtil::MapClippedRect(
      child_empty->screen_space_transform(), child_bounds);

  gfx::RectF expected_child_draw_rect(child->position(), child->bounds());
  expected_child_draw_rect.Scale(device_scale_factor);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_screen_space_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_empty_draw_rect);
  EXPECT_FLOAT_RECT_EQ(expected_child_draw_rect, child_empty_screen_space_rect);
}

TEST_F(LayerTreeHostCommonScalingTest, SurfaceLayerTransformsInHighDPI) {
  // Verify draw and screen space transforms of layers in a surface.
  gfx::Transform identity_matrix;
  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.0);

  gfx::Transform scale_small_matrix;
  scale_small_matrix.Scale(SK_MScalar1 / 10.f, SK_MScalar1 / 12.f);

  LayerImpl* root = root_layer();
  SetLayerPropertiesForTesting(root, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), false, true,
                               false);
  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  SetLayerPropertiesForTesting(parent, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), false, true,
                               false);

  LayerImpl* perspective_surface = AddChild<LayerImpl>(parent);
  SetLayerPropertiesForTesting(perspective_surface,
                               perspective_matrix * scale_small_matrix,
                               gfx::Point3F(), gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10), false, true, true);
  perspective_surface->SetDrawsContent(true);

  LayerImpl* scale_surface = AddChild<LayerImpl>(parent);
  SetLayerPropertiesForTesting(scale_surface, scale_small_matrix,
                               gfx::Point3F(), gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10), false, true, true);
  scale_surface->SetDrawsContent(true);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 3.f;
  ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                 root);

  EXPECT_IDEAL_SCALE_EQ(device_scale_factor * page_scale_factor, parent);
  EXPECT_IDEAL_SCALE_EQ(device_scale_factor * page_scale_factor,
                        perspective_surface);
  // Ideal scale is the max 2d scale component of the combined transform up to
  // the nearest render target. Here this includes the layer transform as well
  // as the device and page scale factors.
  gfx::Transform transform = scale_small_matrix;
  transform.Scale(device_scale_factor * page_scale_factor,
                  device_scale_factor * page_scale_factor);
  gfx::Vector2dF scales =
      MathUtil::ComputeTransform2dScaleComponents(transform, 0.f);
  float max_2d_scale = std::max(scales.x(), scales.y());
  EXPECT_IDEAL_SCALE_EQ(max_2d_scale, scale_surface);

  // The ideal scale will draw 1:1 with its render target space along
  // the larger-scale axis.
  gfx::Vector2dF target_space_transform_scales =
      MathUtil::ComputeTransform2dScaleComponents(
          scale_surface->draw_properties().target_space_transform, 0.f);
  EXPECT_FLOAT_EQ(max_2d_scale,
                  std::max(target_space_transform_scales.x(),
                           target_space_transform_scales.y()));

  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());

  gfx::Transform expected_parent_draw_transform;
  expected_parent_draw_transform.Scale(device_scale_factor * page_scale_factor,
                                       device_scale_factor * page_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_draw_transform,
                                  parent->draw_transform());

  // The scale for the perspective surface is not known, so it is rendered 1:1
  // with the screen, and then scaled during drawing.
  gfx::Transform expected_perspective_surface_draw_transform;
  expected_perspective_surface_draw_transform.Translate(
      device_scale_factor * page_scale_factor *
      perspective_surface->position().x(),
      device_scale_factor * page_scale_factor *
      perspective_surface->position().y());
  expected_perspective_surface_draw_transform.PreconcatTransform(
      perspective_matrix);
  expected_perspective_surface_draw_transform.PreconcatTransform(
      scale_small_matrix);
  gfx::Transform expected_perspective_surface_layer_draw_transform;
  expected_perspective_surface_layer_draw_transform.Scale(
      device_scale_factor * page_scale_factor,
      device_scale_factor * page_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_draw_transform,
      perspective_surface->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_perspective_surface_layer_draw_transform,
      perspective_surface->draw_transform());
}

TEST_F(LayerTreeHostCommonScalingTest, SmallIdealScale) {
  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 0.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  SetLayerPropertiesForTesting(parent, parent_scale_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), false, true,
                               false);

  LayerImpl* child_scale = AddChild<LayerImpl>(parent);
  SetLayerPropertiesForTesting(child_scale, child_scale_matrix, gfx::Point3F(),
                               gfx::PointF(2.f, 2.f), gfx::Size(10, 10), false,
                               true, false);

  float device_scale_factor = 2.5f;
  float page_scale_factor = 0.01f;

  {
    ExecuteCalculateDrawProperties(root, device_scale_factor, page_scale_factor,
                                   root);

    // The ideal scale is able to go below 1.
    float expected_ideal_scale =
        device_scale_factor * page_scale_factor * initial_parent_scale;
    EXPECT_LT(expected_ideal_scale, 1.f);
    EXPECT_IDEAL_SCALE_EQ(expected_ideal_scale, parent);

    expected_ideal_scale = device_scale_factor * page_scale_factor *
                           initial_parent_scale * initial_child_scale;
    EXPECT_LT(expected_ideal_scale, 1.f);
    EXPECT_IDEAL_SCALE_EQ(expected_ideal_scale, child_scale);
  }
}

TEST_F(LayerTreeHostCommonScalingTest, IdealScaleForAnimatingLayer) {
  gfx::Transform parent_scale_matrix;
  SkMScalar initial_parent_scale = 1.75;
  parent_scale_matrix.Scale(initial_parent_scale, initial_parent_scale);

  gfx::Transform child_scale_matrix;
  SkMScalar initial_child_scale = 1.25;
  child_scale_matrix.Scale(initial_child_scale, initial_child_scale);

  LayerImpl* root = root_layer();
  root->SetBounds(gfx::Size(100, 100));

  LayerImpl* parent = AddChildToRoot<LayerImpl>();
  SetLayerPropertiesForTesting(parent, parent_scale_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), false, true,
                               false);

  LayerImpl* child_scale = AddChild<LayerImpl>(parent);
  SetLayerPropertiesForTesting(child_scale, child_scale_matrix, gfx::Point3F(),
                               gfx::PointF(2.f, 2.f), gfx::Size(10, 10), false,
                               true, false);

  {
    ExecuteCalculateDrawProperties(root);

    EXPECT_IDEAL_SCALE_EQ(initial_parent_scale, parent);
    // Animating layers compute ideal scale in the same way as when
    // they are static.
    EXPECT_IDEAL_SCALE_EQ(initial_child_scale * initial_parent_scale,
                          child_scale);
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceTransformsInHighDPI) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<FakePictureLayer> parent =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               false,
                               true);

  scoped_refptr<FakePictureLayer> child =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               false,
                               true);

  gfx::Transform replica_transform;
  replica_transform.Scale(1.0, -1.0);
  scoped_refptr<FakePictureLayer> replica =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(replica.get(),
                               replica_transform,
                               gfx::Point3F(),
                               gfx::PointF(2.f, 2.f),
                               gfx::Size(10, 10),
                               false,
                               true);

  // This layer should end up in the same surface as child, with the same draw
  // and screen space transforms.
  scoped_refptr<FakePictureLayer> duplicate_child_non_owner =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(duplicate_child_non_owner.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               false,
                               true);

  parent->AddChild(child);
  child->AddChild(duplicate_child_non_owner);
  child->SetReplicaLayer(replica.get());

  host()->SetRootLayer(parent);

  RenderSurfaceLayerList render_surface_layer_list;

  float device_scale_factor = 1.5f;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because it has a replica layer).
  EXPECT_EQ(2u, render_surface_layer_list.size());

  gfx::Transform expected_parent_transform;
  expected_parent_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_parent_transform,
                                  parent->draw_transform());

  gfx::Transform expected_draw_transform;
  expected_draw_transform.Scale(device_scale_factor, device_scale_factor);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_draw_transform,
                                  child->draw_transform());

  gfx::Transform expected_screen_space_transform;
  expected_screen_space_transform.Scale(device_scale_factor,
                                        device_scale_factor);
  expected_screen_space_transform.Translate(child->position().x(),
                                            child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_screen_space_transform,
                                  child->screen_space_transform());

  gfx::Transform expected_duplicate_child_draw_transform =
      child->draw_transform();
  EXPECT_TRANSFORMATION_MATRIX_EQ(child->draw_transform(),
                                  duplicate_child_non_owner->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      child->screen_space_transform(),
      duplicate_child_non_owner->screen_space_transform());
  EXPECT_EQ(child->drawable_content_rect(),
            duplicate_child_non_owner->drawable_content_rect());
  EXPECT_EQ(child->bounds(), duplicate_child_non_owner->bounds());

  gfx::Transform expected_render_surface_draw_transform;
  expected_render_surface_draw_transform.Translate(
      device_scale_factor * child->position().x(),
      device_scale_factor * child->position().y());
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_render_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_draw_transform;
  expected_surface_draw_transform.Translate(device_scale_factor * 2.f,
                                            device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_surface_draw_transform,
                                  child->render_surface()->draw_transform());

  gfx::Transform expected_surface_screen_space_transform;
  expected_surface_screen_space_transform.Translate(device_scale_factor * 2.f,
                                                    device_scale_factor * 2.f);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_surface_screen_space_transform,
      child->render_surface()->screen_space_transform());

  gfx::Transform expected_replica_draw_transform;
  expected_replica_draw_transform.matrix().set(1, 1, -1.0);
  expected_replica_draw_transform.matrix().set(0, 3, 6.0);
  expected_replica_draw_transform.matrix().set(1, 3, 6.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_draw_transform,
      child->render_surface()->replica_draw_transform());

  gfx::Transform expected_replica_screen_space_transform;
  expected_replica_screen_space_transform.matrix().set(1, 1, -1.0);
  expected_replica_screen_space_transform.matrix().set(0, 3, 6.0);
  expected_replica_screen_space_transform.matrix().set(1, 3, 6.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest,
     RenderSurfaceTransformsInHighDPIAccurateScaleZeroPosition) {
  MockContentLayerClient delegate;
  gfx::Transform identity_matrix;

  scoped_refptr<FakePictureLayer> parent =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(33, 31),
                               false,
                               true);

  scoped_refptr<FakePictureLayer> child =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(13, 11),
                               false,
                               true);

  gfx::Transform replica_transform;
  replica_transform.Scale(1.0, -1.0);
  scoped_refptr<FakePictureLayer> replica =
      CreateDrawablePictureLayer(layer_settings(), &delegate);
  SetLayerPropertiesForTesting(replica.get(),
                               replica_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(13, 11),
                               false,
                               true);

  parent->AddChild(child);
  child->SetReplicaLayer(replica.get());

  host()->SetRootLayer(parent);

  float device_scale_factor = 1.7f;

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      parent.get(), parent->bounds(), &render_surface_layer_list);
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have two render surfaces. The root's render surface and child's
  // render surface (it needs one because it has a replica layer).
  EXPECT_EQ(2u, render_surface_layer_list.size());

  gfx::Transform identity_transform;
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(identity_transform,
                                  child->render_surface()->draw_transform());
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      identity_transform, child->render_surface()->screen_space_transform());

  gfx::Transform expected_replica_draw_transform;
  expected_replica_draw_transform.matrix().set(1, 1, -1.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_draw_transform,
      child->render_surface()->replica_draw_transform());

  gfx::Transform expected_replica_screen_space_transform;
  expected_replica_screen_space_transform.matrix().set(1, 1, -1.0);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_replica_screen_space_transform,
      child->render_surface()->replica_screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, SubtreeSearch) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  scoped_refptr<Layer> mask_layer = Layer::Create(layer_settings());
  scoped_refptr<Layer> replica_layer = Layer::Create(layer_settings());

  grand_child->SetReplicaLayer(replica_layer.get());
  child->AddChild(grand_child.get());
  child->SetMaskLayer(mask_layer.get());
  root->AddChild(child.get());

  host()->SetRootLayer(root);

  int nonexistent_id = -1;
  EXPECT_EQ(root.get(),
            LayerTreeHostCommon::FindLayerInSubtree(root.get(), root->id()));
  EXPECT_EQ(child.get(),
            LayerTreeHostCommon::FindLayerInSubtree(root.get(), child->id()));
  EXPECT_EQ(
      grand_child.get(),
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), grand_child->id()));
  EXPECT_EQ(
      mask_layer.get(),
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), mask_layer->id()));
  EXPECT_EQ(
      replica_layer.get(),
      LayerTreeHostCommon::FindLayerInSubtree(root.get(), replica_layer->id()));
  EXPECT_EQ(
      0, LayerTreeHostCommon::FindLayerInSubtree(root.get(), nonexistent_id));
}

TEST_F(LayerTreeHostCommonTest, TransparentChildRenderSurfaceCreation) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grand_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(100, 100),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);

  root->AddChild(child);
  child->AddChild(grand_child);
  child->SetOpacity(0.5f);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_FALSE(child->render_surface());
}

TEST_F(LayerTreeHostCommonTest, OpacityAnimatingOnPendingTree) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);

  const gfx::Transform identity_matrix;
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               false);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);
  child->SetDrawsContent(true);
  child->SetOpacity(0.0f);

  // Add opacity animation.
  AddOpacityTransitionToController(
      child->layer_animation_controller(), 10.0, 0.0f, 1.0f, false);

  root->AddChild(child.Pass());
  root->SetHasRenderSurface(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The child
  // layer should be included even though it is transparent.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
}

using LCDTextTestParam = std::tr1::tuple<bool, bool, bool>;
class LCDTextTest : public LayerTreeHostCommonTestBase,
                    public testing::TestWithParam<LCDTextTestParam> {
 public:
  LCDTextTest()
      : LayerTreeHostCommonTestBase(LayerTreeSettings()),
        host_impl_(&proxy_, &shared_bitmap_manager_, &task_graph_runner_),
        root_(nullptr),
        child_(nullptr),
        grand_child_(nullptr) {}

 protected:
  void SetUp() override {
    can_use_lcd_text_ = std::tr1::get<0>(GetParam());
    layers_always_allowed_lcd_text_ = std::tr1::get<1>(GetParam());

    scoped_ptr<LayerImpl> root_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerImpl> child_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 2);
    scoped_ptr<LayerImpl> grand_child_ptr =
        LayerImpl::Create(host_impl_.active_tree(), 3);

    // Stash raw pointers to look at later.
    root_ = root_ptr.get();
    child_ = child_ptr.get();
    grand_child_ = grand_child_ptr.get();

    child_->AddChild(grand_child_ptr.Pass());
    root_->AddChild(child_ptr.Pass());
    host_impl_.active_tree()->SetRootLayer(root_ptr.Pass());

    root_->SetContentsOpaque(true);
    child_->SetContentsOpaque(true);
    grand_child_->SetContentsOpaque(true);

    root_->SetDrawsContent(true);
    child_->SetDrawsContent(true);
    grand_child_->SetDrawsContent(true);

    gfx::Transform identity_matrix;
    SetLayerPropertiesForTesting(root_, identity_matrix, gfx::Point3F(),
                                 gfx::PointF(), gfx::Size(1, 1), true, false,
                                 true);
    SetLayerPropertiesForTesting(child_, identity_matrix, gfx::Point3F(),
                                 gfx::PointF(), gfx::Size(1, 1), true, false,
                                 std::tr1::get<2>(GetParam()));
    SetLayerPropertiesForTesting(grand_child_, identity_matrix, gfx::Point3F(),
                                 gfx::PointF(), gfx::Size(1, 1), true, false,
                                 false);
  }

  bool can_use_lcd_text_;
  bool layers_always_allowed_lcd_text_;

  FakeImplProxy proxy_;
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostImpl host_impl_;

  LayerImpl* root_;
  LayerImpl* child_;
  LayerImpl* grand_child_;
};

TEST_P(LCDTextTest, CanUseLCDText) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Case 1: Identity transform.
  gfx::Transform identity_matrix;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Case 2: Integral translation.
  gfx::Transform integral_translation;
  integral_translation.Translate(1.0, 2.0);
  child_->SetTransform(integral_translation);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Case 3: Non-integral translation.
  gfx::Transform non_integral_translation;
  non_integral_translation.Translate(1.5, 2.5);
  child_->SetTransform(non_integral_translation);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());

  // Case 4: Rotation.
  gfx::Transform rotation;
  rotation.Rotate(10.0);
  child_->SetTransform(rotation);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());

  // Case 5: Scale.
  gfx::Transform scale;
  scale.Scale(2.0, 2.0);
  child_->SetTransform(scale);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());

  // Case 6: Skew.
  gfx::Transform skew;
  skew.SkewX(10.0);
  child_->SetTransform(skew);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());

  // Case 7: Translucent.
  child_->SetTransform(identity_matrix);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  child_->SetOpacity(0.5f);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());

  // Case 8: Sanity check: restore transform and opacity.
  child_->SetTransform(identity_matrix);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  child_->SetOpacity(1.f);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Case 9: Non-opaque content.
  child_->SetContentsOpaque(false);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Case 10: Sanity check: restore content opaqueness.
  child_->SetContentsOpaque(true);
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());
}

TEST_P(LCDTextTest, CanUseLCDTextWithAnimation) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Sanity check: Make sure can_use_lcd_text_ is set on each node.
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Add opacity animation.
  child_->SetOpacity(0.9f);
  child_->layer_tree_impl()->property_trees()->needs_rebuild = true;
  AddOpacityTransitionToController(
      child_->layer_animation_controller(), 10.0, 0.9f, 0.1f, false);

  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  // Text LCD should be adjusted while animation is active.
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, grand_child_->can_use_lcd_text());
}

TEST_P(LCDTextTest, CanUseLCDTextWithAnimationContentsOpaque) {
  bool expect_lcd_text = can_use_lcd_text_ || layers_always_allowed_lcd_text_;
  bool expect_not_lcd_text = layers_always_allowed_lcd_text_;

  // Sanity check: Make sure can_use_lcd_text_ is set on each node.
  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());

  // Mark contents non-opaque within the first animation frame.
  child_->SetContentsOpaque(false);
  AddOpacityTransitionToController(child_->layer_animation_controller(), 10.0,
                                   0.9f, 0.1f, false);

  ExecuteCalculateDrawProperties(root_, 1.f, 1.f, NULL, can_use_lcd_text_,
                                 layers_always_allowed_lcd_text_);
  // LCD text should be disabled for non-opaque layers even during animations.
  EXPECT_EQ(expect_lcd_text, root_->can_use_lcd_text());
  EXPECT_EQ(expect_not_lcd_text, child_->can_use_lcd_text());
  EXPECT_EQ(expect_lcd_text, grand_child_->can_use_lcd_text());
}

INSTANTIATE_TEST_CASE_P(LayerTreeHostCommonTest,
                        LCDTextTest,
                        testing::Combine(testing::Bool(),
                                         testing::Bool(),
                                         testing::Bool()));

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_SingleLayer) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  child->SetIsDrawable(true);

  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               false);
  grand_child->SetIsDrawable(true);
  grand_child->SetHideLayerAndSubtree(true);

  child->AddChild(grand_child);
  root->AddChild(child);

  host()->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(child->id(), root->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_SingleLayerImpl) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(40, 40), true, false,
                               false);
  child->SetDrawsContent(true);

  scoped_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  SetLayerPropertiesForTesting(grand_child.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  grand_child->SetDrawsContent(true);
  grand_child->SetHideLayerAndSubtree(true);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.Pass());
  root->SetHasRenderSurface(true);

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and two layers. The grand child has
  // hidden itself.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(2, root->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_TwoLayers) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  child->SetIsDrawable(true);
  child->SetHideLayerAndSubtree(true);

  scoped_refptr<Layer> grand_child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(grand_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               false);
  grand_child->SetIsDrawable(true);

  child->AddChild(grand_child);
  root->AddChild(child);

  host()->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and one layers. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, SubtreeHidden_TwoLayersImpl) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.pending_tree(), 1);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  root->SetDrawsContent(true);

  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.pending_tree(), 2);
  SetLayerPropertiesForTesting(child.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(40, 40), true, false,
                               false);
  child->SetDrawsContent(true);
  child->SetHideLayerAndSubtree(true);

  scoped_ptr<LayerImpl> grand_child =
      LayerImpl::Create(host_impl.pending_tree(), 3);
  SetLayerPropertiesForTesting(grand_child.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  grand_child->SetDrawsContent(true);

  child->AddChild(grand_child.Pass());
  root->AddChild(child.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface and one layers. The child has
  // hidden itself and the grand child.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(1, root->render_surface()->layer_list().at(0)->id());
}

void EmptyCopyOutputCallback(scoped_ptr<CopyOutputResult> result) {}

TEST_F(LayerTreeHostCommonTest, SubtreeHiddenWithCopyRequest) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_grand_parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  copy_grand_parent->SetIsDrawable(true);

  scoped_refptr<Layer> copy_parent = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               false);
  copy_parent->SetIsDrawable(true);
  copy_parent->SetForceRenderSurface(true);

  scoped_refptr<Layer> copy_layer = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_layer.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  copy_layer->SetIsDrawable(true);

  scoped_refptr<Layer> copy_child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  copy_child->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent_sibling_before =
      Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_grand_parent_sibling_before.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  copy_grand_parent_sibling_before->SetIsDrawable(true);

  scoped_refptr<Layer> copy_grand_parent_sibling_after =
      Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_grand_parent_sibling_after.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  copy_grand_parent_sibling_after->SetIsDrawable(true);

  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  copy_grand_parent->AddChild(copy_parent);
  root->AddChild(copy_grand_parent_sibling_before);
  root->AddChild(copy_grand_parent);
  root->AddChild(copy_grand_parent_sibling_after);

  host()->SetRootLayer(root);

  // Hide the copy_grand_parent and its subtree. But make a copy request in that
  // hidden subtree on copy_layer.
  copy_grand_parent->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_before->SetHideLayerAndSubtree(true);
  copy_grand_parent_sibling_after->SetHideLayerAndSubtree(true);
  copy_layer->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
      base::Bind(&EmptyCopyOutputCallback)));
  EXPECT_TRUE(copy_layer->HasCopyRequest());

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_grand_parent->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_parent->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_TRUE(copy_layer->draw_properties().
              layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_child->draw_properties().
               layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_grand_parent_sibling_before->draw_properties().
               layer_or_descendant_has_copy_request);
  EXPECT_FALSE(copy_grand_parent_sibling_after->draw_properties().
               layer_or_descendant_has_copy_request);

  // We should have three render surfaces, one for the root, one for the parent
  // since it owns a surface, and one for the copy_layer.
  ASSERT_EQ(3u, render_surface_layer_list.size());
  EXPECT_EQ(root->id(), render_surface_layer_list.at(0)->id());
  EXPECT_EQ(copy_parent->id(), render_surface_layer_list.at(1)->id());
  EXPECT_EQ(copy_layer->id(), render_surface_layer_list.at(2)->id());

  // The root render surface should have 2 contributing layers. The
  // copy_grand_parent is hidden along with its siblings, but the copy_parent
  // will appear since something in its subtree needs to be drawn for a copy
  // request.
  ASSERT_EQ(2u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_parent->id(),
            root->render_surface()->layer_list().at(1)->id());

  // Nothing actually draws into the copy parent, so only the copy_layer will
  // appear in its list, since it needs to be drawn for the copy request.
  ASSERT_EQ(1u, copy_parent->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_parent->render_surface()->layer_list().at(0)->id());

  // The copy_layer's render surface should have two contributing layers.
  ASSERT_EQ(2u, copy_layer->render_surface()->layer_list().size());
  EXPECT_EQ(copy_layer->id(),
            copy_layer->render_surface()->layer_list().at(0)->id());
  EXPECT_EQ(copy_child->id(),
            copy_layer->render_surface()->layer_list().at(1)->id());
}

TEST_F(LayerTreeHostCommonTest, ClippedOutCopyRequest) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> copy_parent = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_parent.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(),
                               true,
                               false);
  copy_parent->SetIsDrawable(true);
  copy_parent->SetMasksToBounds(true);

  scoped_refptr<Layer> copy_layer = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_layer.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               false);
  copy_layer->SetIsDrawable(true);

  scoped_refptr<Layer> copy_child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(copy_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               false);
  copy_child->SetIsDrawable(true);

  copy_layer->AddChild(copy_child);
  copy_parent->AddChild(copy_layer);
  root->AddChild(copy_parent);

  host()->SetRootLayer(root);

  copy_layer->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
      base::Bind(&EmptyCopyOutputCallback)));
  EXPECT_TRUE(copy_layer->HasCopyRequest());

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // We should have one render surface, as the others are clipped out.
  ASSERT_EQ(1u, render_surface_layer_list.size());
  EXPECT_EQ(root->id(), render_surface_layer_list.at(0)->id());

  // The root render surface should only have 1 contributing layer, since the
  // other layers are empty/clipped away.
  ASSERT_EQ(1u, root->render_surface()->layer_list().size());
  EXPECT_EQ(root->id(), root->render_surface()->layer_list().at(0)->id());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectInsideSurface) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  const gfx::Transform identity_matrix;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  root->SetIsDrawable(true);

  // The surface is moved slightly outside of the viewport.
  scoped_refptr<Layer> surface = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(surface.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(-10, -20),
                               gfx::Size(),
                               true,
                               false);
  surface->SetForceRenderSurface(true);

  scoped_refptr<Layer> surface_child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(surface_child.get(),
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  surface_child->SetIsDrawable(true);

  surface->AddChild(surface_child);
  root->AddChild(surface);

  host()->SetRootLayer(root);

  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // The visible_layer_rect for the |surface_child| should not be clipped by
  // the viewport.
  EXPECT_EQ(gfx::Rect(50, 50).ToString(),
            surface_child->visible_layer_rect().ToString());
}

TEST_F(LayerTreeHostCommonTest, TransformedClipParent) {
  // Ensure that a transform between the layer and its render surface is not a
  // problem. Constructs the following layer tree.
  //
  //   root (a render surface)
  //     + render_surface
  //       + clip_parent (scaled)
  //         + intervening_clipping_layer
  //           + clip_child
  //
  // The render surface should be resized correctly and the clip child should
  // inherit the right clip rect.
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> intervening = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(render_surface);
  render_surface->AddChild(clip_parent);
  clip_parent->AddChild(intervening);
  intervening->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  render_surface->SetForceRenderSurface(true);

  gfx::Transform scale_transform;
  scale_transform.Scale(2, 2);

  gfx::Transform identity_transform;

  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               scale_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(10, 10),
                               true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  ASSERT_TRUE(root->render_surface());
  ASSERT_TRUE(render_surface->render_surface());

  // Ensure that we've inherited our clip parent's clip and weren't affected
  // by the intervening clip layer.
  ASSERT_EQ(gfx::Rect(1, 1, 20, 20).ToString(),
            clip_parent->clip_rect().ToString());
  ASSERT_EQ(clip_parent->clip_rect().ToString(),
            clip_child->clip_rect().ToString());
  ASSERT_EQ(gfx::Rect(3, 3, 10, 10).ToString(),
            intervening->clip_rect().ToString());

  // Ensure that the render surface reports a content rect that has been grown
  // to accomodate for the clip child.
  ASSERT_EQ(gfx::Rect(5, 5, 16, 16).ToString(),
            render_surface->render_surface()->content_rect().ToString());

  // The above check implies the two below, but they nicely demonstrate that
  // we've grown, despite the intervening layer's clip.
  ASSERT_TRUE(clip_parent->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
  ASSERT_FALSE(intervening->clip_rect().Contains(
      render_surface->render_surface()->content_rect()));
}

TEST_F(LayerTreeHostCommonTest, ClipParentWithInterveningRenderSurface) {
  // Ensure that intervening render surfaces are not a problem in the basic
  // case. In the following tree, both render surfaces should be resized to
  // accomodate for the clip child, despite an intervening clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> intervening = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface2 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(intervening);
  intervening->AddChild(render_surface2);
  render_surface2->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               translation_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(40, 40),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(-10.f, -10.f),
                               gfx::Size(60, 60),
                               true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface2->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->render_surface()->content_rect().ToString());
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(-1, -1, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_EQ(gfx::Rect(9, 9, 40, 40).ToString(),
            clip_child->visible_layer_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClipParentScrolledInterveningLayer) {
  // Ensure that intervening render surfaces are not a problem, even if there
  // is a scroll involved. Note, we do _not_ have to consider any other sort
  // of transform.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1 (sets opacity)
  //        + intervening (masks to bounds AND scrolls)
  //          + render_surface2 (also sets opacity)
  //            + clip_child
  //
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<Layer> intervening = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface2 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(intervening);
  intervening->AddChild(render_surface2);
  render_surface2->AddChild(clip_child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);
  intervening->SetScrollClipLayerId(clip_parent->id());
  intervening->SetScrollOffset(gfx::ScrollOffset(3, 3));

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  gfx::Transform translation_transform;
  translation_transform.Translate(2, 2);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               translation_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(40, 40),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(1.f, 1.f),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(-10.f, -10.f),
                               gfx::Size(60, 60),
                               true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  // Since the render surfaces could have expanded, they should not clip (their
  // bounds would no longer be reliable). We should resort to layer clipping
  // in this case.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface2->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface2->render_surface()->is_clipped());

  // NB: clip rects are in target space.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // This value is inherited from the clipping ancestor layer, 'intervening'.
  EXPECT_EQ(gfx::Rect(2, 2, 3, 3).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->is_clipped());

  // The content rects of both render surfaces should both have expanded to
  // contain the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            render_surface1->render_surface()->content_rect().ToString());
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // The clip child should have inherited the clip parent's clip (projected to
  // the right space, of course), and should have the correctly sized visible
  // content rect.
  EXPECT_EQ(gfx::Rect(2, 2, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_EQ(gfx::Rect(12, 12, 40, 40).ToString(),
            clip_child->visible_layer_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, DescendantsOfClipChildren) {
  // Ensures that descendants of the clip child inherit the correct clip.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + intervening (masks to bounds)
  //        + clip_child
  //          + child
  //
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> intervening = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(clip_parent);
  clip_parent->AddChild(intervening);
  intervening->AddChild(clip_child);
  clip_child->AddChild(child);

  clip_child->SetClipParent(clip_parent.get());

  intervening->SetMasksToBounds(true);
  clip_parent->SetMasksToBounds(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  SetLayerPropertiesForTesting(intervening.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(60, 60),
                               true,
                               false);
  SetLayerPropertiesForTesting(child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(60, 60),
                               true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  // Neither the clip child nor its descendant should have inherited the clip
  // from |intervening|.
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            clip_child->clip_rect().ToString());
  EXPECT_TRUE(clip_child->is_clipped());
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40).ToString(),
            child->visible_layer_rect().ToString());
  EXPECT_TRUE(child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest,
       SurfacesShouldBeUnaffectedByNonDescendantClipChildren) {
  // Ensures that non-descendant clip children in the tree do not affect
  // render surfaces.
  //
  //   root (a render surface)
  //    + clip_parent (masks to bounds)
  //      + render_surface1
  //        + clip_child
  //      + render_surface2
  //        + non_clip_child
  //
  // In this example render_surface2 should be unaffected by clip_child.
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip_parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> render_surface1 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<Layer> render_surface2 = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> non_clip_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(clip_parent);
  clip_parent->AddChild(render_surface1);
  render_surface1->AddChild(clip_child);
  clip_parent->AddChild(render_surface2);
  render_surface2->AddChild(non_clip_child);

  clip_child->SetClipParent(clip_parent.get());

  clip_parent->SetMasksToBounds(true);
  render_surface1->SetMasksToBounds(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(15, 15),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_parent.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface1.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(5, 5),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(render_surface2.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               true,
                               false);
  SetLayerPropertiesForTesting(clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(-1, 1),
                               gfx::Size(10, 10),
                               true,
                               false);
  SetLayerPropertiesForTesting(non_clip_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(5, 5),
                               true,
                               false);

  render_surface1->SetForceRenderSurface(true);
  render_surface2->SetForceRenderSurface(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(render_surface1->render_surface());
  EXPECT_TRUE(render_surface2->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface1->clip_rect().ToString());
  EXPECT_TRUE(render_surface1->is_clipped());

  // The render surface should not clip (it has unclipped descendants), instead
  // it should rely on layer clipping.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            render_surface1->render_surface()->clip_rect().ToString());
  EXPECT_FALSE(render_surface1->render_surface()->is_clipped());

  // That said, it should have grown to accomodate the unclipped descendant.
  EXPECT_EQ(gfx::Rect(-1, 1, 6, 4).ToString(),
            render_surface1->render_surface()->content_rect().ToString());

  // This render surface should clip. It has no unclipped descendants.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->clip_rect().ToString());
  EXPECT_TRUE(render_surface2->render_surface()->is_clipped());

  // It also shouldn't have grown to accomodate the clip child.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 5).ToString(),
            render_surface2->render_surface()->content_rect().ToString());

  // Sanity check our num_unclipped_descendants values.
  EXPECT_EQ(1u, render_surface1->num_unclipped_descendants());
  EXPECT_EQ(0u, render_surface2->num_unclipped_descendants());
}

TEST_F(LayerTreeHostCommonTest, CanRenderToSeparateSurface) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  scoped_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl.active_tree(), 12345);
  scoped_ptr<LayerImpl> child1 =
      LayerImpl::Create(host_impl.active_tree(), 123456);
  scoped_ptr<LayerImpl> child2 =
      LayerImpl::Create(host_impl.active_tree(), 1234567);
  scoped_ptr<LayerImpl> child3 =
      LayerImpl::Create(host_impl.active_tree(), 12345678);

  gfx::Transform identity_matrix;
  gfx::Point3F transform_origin;
  gfx::PointF position;
  gfx::Size bounds(100, 100);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, true);
  root->SetDrawsContent(true);

  // This layer structure normally forces render surface due to preserves3d
  // behavior.
  SetLayerPropertiesForTesting(child1.get(), identity_matrix, transform_origin,
                               position, bounds, false, true, true);
  child1->SetDrawsContent(true);
  SetLayerPropertiesForTesting(child2.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, false);
  child2->SetDrawsContent(true);
  SetLayerPropertiesForTesting(child3.get(), identity_matrix, transform_origin,
                               position, bounds, true, false, false);
  child3->SetDrawsContent(true);

  child2->Set3dSortingContextId(1);
  child3->Set3dSortingContextId(1);

  child2->AddChild(child3.Pass());
  child1->AddChild(child2.Pass());
  root->AddChild(child1.Pass());

  {
    LayerImplList render_surface_layer_list;
    FakeLayerTreeHostImpl::RecursiveUpdateNumChildren(root.get());
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_EQ(2u, render_surface_layer_list.size());

    int count_represents_target_render_surface = 0;
    int count_represents_contributing_render_surface = 0;
    int count_represents_itself = 0;
    LayerIterator end = LayerIterator::End(&render_surface_layer_list);
    for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list);
         it != end; ++it) {
      if (it.represents_target_render_surface())
        count_represents_target_render_surface++;
      if (it.represents_contributing_render_surface())
        count_represents_contributing_render_surface++;
      if (it.represents_itself())
        count_represents_itself++;
    }

    // Two render surfaces.
    EXPECT_EQ(2, count_represents_target_render_surface);
    // Second render surface contributes to root render surface.
    EXPECT_EQ(1, count_represents_contributing_render_surface);
    // All 4 layers represent itself.
    EXPECT_EQ(4, count_represents_itself);
  }

  {
    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    inputs.can_render_to_separate_surface = false;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_EQ(1u, render_surface_layer_list.size());

    int count_represents_target_render_surface = 0;
    int count_represents_contributing_render_surface = 0;
    int count_represents_itself = 0;
    LayerIterator end = LayerIterator::End(&render_surface_layer_list);
    for (LayerIterator it = LayerIterator::Begin(&render_surface_layer_list);
         it != end; ++it) {
      if (it.represents_target_render_surface())
        count_represents_target_render_surface++;
      if (it.represents_contributing_render_surface())
        count_represents_contributing_render_surface++;
      if (it.represents_itself())
        count_represents_itself++;
    }

    // Only root layer has a render surface.
    EXPECT_EQ(1, count_represents_target_render_surface);
    // No layer contributes a render surface to root render surface.
    EXPECT_EQ(0, count_represents_contributing_render_surface);
    // All 4 layers represent itself.
    EXPECT_EQ(4, count_represents_itself);
  }
}

TEST_F(LayerTreeHostCommonTest, DoNotIncludeBackfaceInvisibleSurfaces) {
  LayerImpl* root = root_layer();
  LayerImpl* render_surface = AddChild<LayerImpl>(root);
  LayerImpl* child = AddChild<LayerImpl>(render_surface);
  child->SetDrawsContent(true);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(render_surface, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               false, true, true);
  SetLayerPropertiesForTesting(child, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(20, 20), true, false,
                               false);

  root->SetShouldFlattenTransform(false);
  root->Set3dSortingContextId(1);
  render_surface->SetDoubleSided(false);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(2u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(1u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());
  EXPECT_EQ(1u, render_surface_layer_list_impl()
                    ->at(1)
                    ->render_surface()
                    ->layer_list()
                    .size());

  gfx::Transform rotation_transform = identity_transform;
  rotation_transform.RotateAboutXAxis(180.0);

  render_surface->SetTransform(rotation_transform);

  ExecuteCalculateDrawProperties(root);

  EXPECT_EQ(1u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(0u, render_surface_layer_list_impl()
                    ->at(0)
                    ->render_surface()
                    ->layer_list()
                    .size());
}

TEST_F(LayerTreeHostCommonTest, ClippedByScrollParent) {
  // Checks that the simple case (being clipped by a scroll parent that would
  // have been processed before you anyhow) results in the right clips.
  //
  // + root
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_child
  //
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> scroll_parent_border = Layer::Create(layer_settings());
  scoped_refptr<Layer> scroll_parent_clip = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(scroll_child);

  root->AddChild(scroll_parent_border);
  scroll_parent_border->AddChild(scroll_parent_clip);
  scroll_parent_clip->AddChild(scroll_parent);

  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent.get());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(scroll_parent_border.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(40, 40),
                               true,
                               false);
  SetLayerPropertiesForTesting(scroll_parent_clip.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               false);
  SetLayerPropertiesForTesting(scroll_parent.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);
  SetLayerPropertiesForTesting(scroll_child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, SingularTransformSubtreesDoNotDraw) {
  scoped_refptr<LayerWithForcedDrawsContent> root =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(parent);
  parent->AddChild(child);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(50, 50),
                               true,
                               true);
  root->SetForceRenderSurface(true);
  SetLayerPropertiesForTesting(parent.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(30, 30),
                               true,
                               true);
  parent->SetForceRenderSurface(true);
  SetLayerPropertiesForTesting(child.get(),
                               identity_transform,
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(20, 20),
                               true,
                               true);
  child->SetForceRenderSurface(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(3u, render_surface_layer_list()->size());

  gfx::Transform singular_transform;
  singular_transform.Scale3d(
      SkDoubleToMScalar(1.0), SkDoubleToMScalar(1.0), SkDoubleToMScalar(0.0));

  child->SetTransform(singular_transform);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(2u, render_surface_layer_list()->size());

  // Ensure that the entire subtree under a layer with singular transform does
  // not get rendered.
  parent->SetTransform(singular_transform);
  child->SetTransform(identity_transform);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_EQ(1u, render_surface_layer_list()->size());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollParent) {
  // Checks that clipping by a scroll parent that follows you in paint order
  // still results in correct clipping.
  //
  // + root
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //   + scroll_child
  //
  LayerImpl* root = root_layer();
  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* scroll_child = AddChild<LayerImpl>(root);

  scroll_parent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);

  scroll_parent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent);
  scoped_ptr<std::set<LayerImpl*>> scroll_children(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_child);
  scroll_parent->SetScrollChildren(scroll_children.release());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(scroll_parent_border, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent_clip, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_child, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 30, 30).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());
}

TEST_F(LayerTreeHostCommonTest, ClippedByOutOfOrderScrollGrandparent) {
  // Checks that clipping by a scroll parent and scroll grandparent that follow
  // you in paint order still results in correct clipping.
  //
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //   | + scroll_parent_clip
  //   |   + scroll_parent
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //
  LayerImpl* root = root_layer();
  LayerImpl* scroll_child = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* scroll_grandparent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_grandparent_clip =
      AddChild<LayerImpl>(scroll_grandparent_border);
  LayerImpl* scroll_grandparent = AddChild<LayerImpl>(scroll_grandparent_clip);

  scroll_parent->SetDrawsContent(true);
  scroll_grandparent->SetDrawsContent(true);
  scroll_child->SetDrawsContent(true);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent);
  scoped_ptr<std::set<LayerImpl*>> scroll_children(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_child);
  scroll_parent->SetScrollChildren(scroll_children.release());

  scroll_parent_border->SetScrollParent(scroll_grandparent);
  scroll_children.reset(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_parent_border);
  scroll_grandparent->SetScrollChildren(scroll_children.release());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(scroll_grandparent_border, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_grandparent_clip, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(20, 20),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_grandparent, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent_border, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent_clip, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_child, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we visited the above layers out of order to get the
  // correct clip, the layer lists should be unaffected.
  EXPECT_EQ(3u, root->render_surface()->layer_list().size());
  EXPECT_EQ(scroll_child, root->render_surface()->layer_list().at(0));
  EXPECT_EQ(scroll_parent, root->render_surface()->layer_list().at(1));
  EXPECT_EQ(scroll_grandparent, root->render_surface()->layer_list().at(2));
}

TEST_F(LayerTreeHostCommonTest, OutOfOrderClippingRequiresRSLLSorting) {
  // Ensures that even if we visit layers out of order, we still produce a
  // correctly ordered render surface layer list.
  // + root
  //   + scroll_child
  //   + scroll_parent_border
  //     + scroll_parent_clip
  //       + scroll_parent
  //         + render_surface2
  //   + scroll_grandparent_border
  //     + scroll_grandparent_clip
  //       + scroll_grandparent
  //         + render_surface1
  //
  LayerImpl* root = root_layer();
  root->SetDrawsContent(true);

  LayerImpl* scroll_child = AddChild<LayerImpl>(root);
  scroll_child->SetDrawsContent(true);

  LayerImpl* scroll_parent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_parent_clip = AddChild<LayerImpl>(scroll_parent_border);
  LayerImpl* scroll_parent = AddChild<LayerImpl>(scroll_parent_clip);
  LayerImpl* render_surface2 = AddChild<LayerImpl>(scroll_parent);
  LayerImpl* scroll_grandparent_border = AddChild<LayerImpl>(root);
  LayerImpl* scroll_grandparent_clip =
      AddChild<LayerImpl>(scroll_grandparent_border);
  LayerImpl* scroll_grandparent = AddChild<LayerImpl>(scroll_grandparent_clip);
  LayerImpl* render_surface1 = AddChild<LayerImpl>(scroll_grandparent);

  scroll_parent->SetDrawsContent(true);
  render_surface1->SetDrawsContent(true);
  scroll_grandparent->SetDrawsContent(true);
  render_surface2->SetDrawsContent(true);

  scroll_parent_clip->SetMasksToBounds(true);
  scroll_grandparent_clip->SetMasksToBounds(true);

  scroll_child->SetScrollParent(scroll_parent);
  scoped_ptr<std::set<LayerImpl*>> scroll_children(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_child);
  scroll_parent->SetScrollChildren(scroll_children.release());

  scroll_parent_border->SetScrollParent(scroll_grandparent);
  scroll_children.reset(new std::set<LayerImpl*>);
  scroll_children->insert(scroll_parent_border);
  scroll_grandparent->SetScrollChildren(scroll_children.release());

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(scroll_grandparent_border, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_grandparent_clip, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(20, 20),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_grandparent, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, false);
  SetLayerPropertiesForTesting(render_surface1, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, true);
  SetLayerPropertiesForTesting(scroll_parent_border, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent_clip, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  SetLayerPropertiesForTesting(scroll_parent, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, false);
  SetLayerPropertiesForTesting(render_surface2, identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false, true);
  SetLayerPropertiesForTesting(scroll_child, identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);

  ExecuteCalculateDrawProperties(root);

  EXPECT_TRUE(root->render_surface());

  EXPECT_EQ(gfx::Rect(0, 0, 20, 20).ToString(),
            scroll_child->clip_rect().ToString());
  EXPECT_TRUE(scroll_child->is_clipped());

  // Despite the fact that we had to process the layers out of order to get the
  // right clip, our render_surface_layer_list's order should be unaffected.
  EXPECT_EQ(3u, render_surface_layer_list_impl()->size());
  EXPECT_EQ(root, render_surface_layer_list_impl()->at(0));
  EXPECT_EQ(render_surface2, render_surface_layer_list_impl()->at(1));
  EXPECT_EQ(render_surface1, render_surface_layer_list_impl()->at(2));
  EXPECT_TRUE(render_surface_layer_list_impl()->at(0)->render_surface());
  EXPECT_TRUE(render_surface_layer_list_impl()->at(1)->render_surface());
  EXPECT_TRUE(render_surface_layer_list_impl()->at(2)->render_surface());
}

TEST_F(LayerTreeHostCommonTest, FixedPositionWithInterveningRenderSurface) {
  // Ensures that when we have a render surface between a fixed position layer
  // and its container, we compute the fixed position layer's draw transform
  // with respect to that intervening render surface, not with respect to its
  // container's render target.
  //
  // + root
  //   + render_surface
  //     + fixed
  //       + child
  //
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> render_surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> fixed =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(render_surface);
  render_surface->AddChild(fixed);
  fixed->AddChild(child);

  root->SetIsContainerForFixedPositionLayers(true);
  render_surface->SetForceRenderSurface(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  SetLayerPropertiesForTesting(root.get(), gfx::Transform(), gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(render_surface.get(), gfx::Transform(),
                               gfx::Point3F(), gfx::PointF(7.f, 9.f),
                               gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(fixed.get(), gfx::Transform(), gfx::Point3F(),
                               gfx::PointF(10.f, 15.f), gfx::Size(50, 50), true,
                               false);
  SetLayerPropertiesForTesting(child.get(), gfx::Transform(), gfx::Point3F(),
                               gfx::PointF(1.f, 2.f), gfx::Size(50, 50), true,
                               false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());

  gfx::Transform expected_fixed_draw_transform;
  expected_fixed_draw_transform.Translate(10.f, 15.f);
  EXPECT_EQ(expected_fixed_draw_transform, fixed->draw_transform());

  gfx::Transform expected_fixed_screen_space_transform;
  expected_fixed_screen_space_transform.Translate(17.f, 24.f);
  EXPECT_EQ(expected_fixed_screen_space_transform,
            fixed->screen_space_transform());

  gfx::Transform expected_child_draw_transform;
  expected_child_draw_transform.Translate(11.f, 17.f);
  EXPECT_EQ(expected_child_draw_transform, child->draw_transform());

  gfx::Transform expected_child_screen_space_transform;
  expected_child_screen_space_transform.Translate(18.f, 26.f);
  EXPECT_EQ(expected_child_screen_space_transform,
            child->screen_space_transform());
}

TEST_F(LayerTreeHostCommonTest, ScrollCompensationWithRounding) {
  // This test verifies that a scrolling layer that gets snapped to
  // integer coordinates doesn't move a fixed position child.
  //
  // + root
  //   + container
  //     + scroller
  //       + fixed
  //
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> container =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* container_layer = container.get();
  scoped_ptr<LayerImpl> scroller =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* scroll_layer = scroller.get();
  scoped_ptr<LayerImpl> fixed = LayerImpl::Create(host_impl.active_tree(), 4);
  LayerImpl* fixed_layer = fixed.get();

  container->SetIsContainerForFixedPositionLayers(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  scroller->SetScrollClipLayer(container->id());

  gfx::Transform identity_transform;
  gfx::Transform container_transform;
  container_transform.Translate3d(10.0, 20.0, 0.0);
  gfx::Vector2dF container_offset = container_transform.To2dTranslation();

  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(container.get(), container_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroller.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false, false);
  SetLayerPropertiesForTesting(fixed.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               false);

  scroller->AddChild(fixed.Pass());
  container->AddChild(scroller.Pass());
  root->AddChild(container.Pass());

  // Rounded to integers already.
  {
    gfx::Vector2dF scroll_delta(3.0, 5.0);
    scroll_layer->SetScrollDelta(scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - scroll_delta);
  }

  // Scroll delta requiring rounding.
  {
    gfx::Vector2dF scroll_delta(4.1f, 8.1f);
    scroll_layer->SetScrollDelta(scroll_delta);

    gfx::Vector2dF rounded_scroll_delta(4.f, 8.f);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);
    EXPECT_VECTOR_EQ(scroll_layer->draw_properties()
                         .screen_space_transform.To2dTranslation(),
                     container_offset - rounded_scroll_delta);
  }

  // Scale is applied earlier in the tree.
  {
    gfx::Transform scaled_container_transform = container_transform;
    scaled_container_transform.Scale3d(3.0, 3.0, 1.0);
    container_layer->SetTransform(scaled_container_transform);

    gfx::Vector2dF scroll_delta(4.5f, 8.5f);
    scroll_layer->SetScrollDelta(scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        container_layer->draw_properties().screen_space_transform,
        fixed_layer->draw_properties().screen_space_transform);
    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);

    container_layer->SetTransform(container_transform);
  }

  // Scale is applied on the scroll layer itself.
  {
    gfx::Transform scale_transform;
    scale_transform.Scale3d(3.0, 3.0, 1.0);
    scroll_layer->SetTransform(scale_transform);

    gfx::Vector2dF scroll_delta(4.5f, 8.5f);
    scroll_layer->SetScrollDelta(scroll_delta);

    LayerImplList render_surface_layer_list;
    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root.get(), root->bounds(), &render_surface_layer_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    EXPECT_VECTOR_EQ(
        fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
        container_offset);

    scroll_layer->SetTransform(identity_transform);
  }
}

TEST_F(LayerTreeHostCommonTest,
       ScrollCompensationMainScrollOffsetFractionalPart) {
  // This test verifies that a scrolling layer that has fractional scroll offset
  // from main doesn't move a fixed position child.
  //
  // + root
  //   + container
  //     + scroller
  //       + fixed
  //
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  host_impl.CreatePendingTree();
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> container =
      LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* container_layer = container.get();
  scoped_ptr<LayerImpl> scroller =
      LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* scroll_layer = scroller.get();
  scoped_ptr<LayerImpl> fixed = LayerImpl::Create(host_impl.active_tree(), 4);
  LayerImpl* fixed_layer = fixed.get();

  container->SetIsContainerForFixedPositionLayers(true);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  scroller->SetScrollClipLayer(container->id());

  gfx::Transform identity_transform;
  gfx::Transform container_transform;
  container_transform.Translate3d(10.0, 20.0, 0.0);
  gfx::Vector2dF container_offset = container_transform.To2dTranslation();

  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false,
                               true);
  SetLayerPropertiesForTesting(container.get(), container_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(40, 40),
                               true, false, false);
  SetLayerPropertiesForTesting(scroller.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(0.0, 0.0),
                               gfx::Size(30, 30), true, false, false);

  gfx::ScrollOffset scroll_offset(3.3, 4.2);
  gfx::Vector2dF main_scroll_fractional_part(0.3f, 0.2f);
  gfx::Vector2dF scroll_delta(0.1f, 0.4f);
  // Blink only uses the integer part of the scroll_offset for fixed
  // position layer.
  SetLayerPropertiesForTesting(fixed.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(3.0f, 4.0f), gfx::Size(50, 50), true,
                               false, false);
  scroll_layer->PushScrollOffsetFromMainThread(scroll_offset);
  scroll_layer->SetScrollDelta(scroll_delta);
  scroll_layer->SetScrollCompensationAdjustment(main_scroll_fractional_part);

  scroller->AddChild(fixed.Pass());
  container->AddChild(scroller.Pass());
  root->AddChild(container.Pass());

  LayerImplList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root.get(), root->bounds(), &render_surface_layer_list);
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      container_layer->draw_properties().screen_space_transform,
      fixed_layer->draw_properties().screen_space_transform);
  EXPECT_VECTOR_EQ(
      fixed_layer->draw_properties().screen_space_transform.To2dTranslation(),
      container_offset);

  gfx::ScrollOffset effective_scroll_offset =
      ScrollOffsetWithDelta(scroll_offset, scroll_delta);
  gfx::Vector2d rounded_effective_scroll_offset =
      ToRoundedVector2d(ScrollOffsetToVector2dF(effective_scroll_offset));
  EXPECT_VECTOR_EQ(
      scroll_layer->draw_properties().screen_space_transform.To2dTranslation(),
      container_offset - rounded_effective_scroll_offset);
}

class AnimationScaleFactorTrackingLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<AnimationScaleFactorTrackingLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return make_scoped_ptr(
        new AnimationScaleFactorTrackingLayerImpl(tree_impl, id));
  }

  ~AnimationScaleFactorTrackingLayerImpl() override {}

 private:
  explicit AnimationScaleFactorTrackingLayerImpl(LayerTreeImpl* tree_impl,
                                                 int id)
      : LayerImpl(tree_impl, id) {
    SetDrawsContent(true);
  }
};

TEST_F(LayerTreeHostCommonTest, MaximumAnimationScaleFactor) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  gfx::Transform identity_matrix;
  scoped_ptr<AnimationScaleFactorTrackingLayerImpl> grand_parent =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<AnimationScaleFactorTrackingLayerImpl> parent =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<AnimationScaleFactorTrackingLayerImpl> child =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<AnimationScaleFactorTrackingLayerImpl> grand_child =
      AnimationScaleFactorTrackingLayerImpl::Create(host_impl.active_tree(), 4);

  AnimationScaleFactorTrackingLayerImpl* parent_raw = parent.get();
  AnimationScaleFactorTrackingLayerImpl* child_raw = child.get();
  AnimationScaleFactorTrackingLayerImpl* grand_child_raw = grand_child.get();

  child->AddChild(grand_child.Pass());
  parent->AddChild(child.Pass());
  grand_parent->AddChild(parent.Pass());

  SetLayerPropertiesForTesting(grand_parent.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(1, 2),
                               true, false, true);
  SetLayerPropertiesForTesting(parent_raw, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 2), true, false,
                               false);
  SetLayerPropertiesForTesting(child_raw, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 2), true, false,
                               false);

  SetLayerPropertiesForTesting(grand_child_raw, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 2), true, false,
                               false);

  ExecuteCalculateDrawProperties(grand_parent.get());

  // No layers have animations.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  TransformOperations translation;
  translation.AppendTranslate(1.f, 2.f, 3.f);

  AddAnimatedTransformToLayer(
      parent_raw, 1.0, TransformOperations(), translation);

  // No layers have scale-affecting animations.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  TransformOperations scale;
  scale.AppendScale(5.f, 4.f, 3.f);

  AddAnimatedTransformToLayer(child_raw, 1.0, TransformOperations(), scale);
  ExecuteCalculateDrawProperties(grand_parent.get());

  // Only |child| has a scale-affecting animation.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(5.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      5.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  AddAnimatedTransformToLayer(
      grand_parent.get(), 1.0, TransformOperations(), scale);
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |grand_parent| and |child| have scale-affecting animations.
  EXPECT_EQ(5.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(5.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  // We don't support combining animated scales from two nodes; 0.f means
  // that the maximum scale could not be computed.
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  AddAnimatedTransformToLayer(parent_raw, 1.0, TransformOperations(), scale);
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |grand_parent|, |parent|, and |child| have scale-affecting animations.
  EXPECT_EQ(5.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  grand_parent->layer_animation_controller()->AbortAnimations(
      Animation::TRANSFORM);
  parent_raw->layer_animation_controller()->AbortAnimations(
      Animation::TRANSFORM);
  child_raw->layer_animation_controller()->AbortAnimations(
      Animation::TRANSFORM);

  TransformOperations perspective;
  perspective.AppendPerspective(10.f);

  AddAnimatedTransformToLayer(
      child_raw, 1.0, TransformOperations(), perspective);
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |child| has a scale-affecting animation but computing the maximum of this
  // animation is not supported.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  child_raw->layer_animation_controller()->AbortAnimations(
      Animation::TRANSFORM);

  gfx::Transform scale_matrix;
  scale_matrix.Scale(1.f, 2.f);
  grand_parent->SetTransform(scale_matrix);
  parent_raw->SetTransform(scale_matrix);
  grand_parent->layer_tree_impl()->property_trees()->needs_rebuild = true;
  AddAnimatedTransformToLayer(parent_raw, 1.0, TransformOperations(), scale);
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |grand_parent| and |parent| each have scale 2.f. |parent| has a  scale
  // animation with maximum scale 5.f.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(10.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(10.f,
            child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      10.f,
      grand_child_raw->draw_properties().maximum_animation_contents_scale);

  gfx::Transform perspective_matrix;
  perspective_matrix.ApplyPerspectiveDepth(2.f);
  child_raw->SetTransform(perspective_matrix);
  grand_parent->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |child| has a transform that's neither a translation nor a scale.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(10.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  parent_raw->SetTransform(perspective_matrix);
  grand_parent->layer_tree_impl()->property_trees()->needs_rebuild = true;
  ExecuteCalculateDrawProperties(grand_parent.get());

  // |parent| and |child| have transforms that are neither translations nor
  // scales.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);

  parent_raw->SetTransform(identity_matrix);
  child_raw->SetTransform(identity_matrix);
  grand_parent->SetTransform(perspective_matrix);
  grand_parent->layer_tree_impl()->property_trees()->needs_rebuild = true;

  ExecuteCalculateDrawProperties(grand_parent.get());

  // |grand_parent| has a transform that's neither a translation nor a scale.
  EXPECT_EQ(0.f,
            grand_parent->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f,
            parent_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(0.f, child_raw->draw_properties().maximum_animation_contents_scale);
  EXPECT_EQ(
      0.f, grand_child_raw->draw_properties().maximum_animation_contents_scale);
}

static int membership_id(LayerImpl* layer) {
  return layer->draw_properties().last_drawn_render_surface_layer_list_id;
}

static void GatherDrawnLayers(LayerImplList* rsll,
                              std::set<LayerImpl*>* drawn_layers) {
  for (LayerIterator it = LayerIterator::Begin(rsll),
                     end = LayerIterator::End(rsll);
       it != end; ++it) {
    LayerImpl* layer = *it;
    if (it.represents_itself())
      drawn_layers->insert(layer);

    if (!it.represents_contributing_render_surface())
      continue;

    if (layer->mask_layer())
      drawn_layers->insert(layer->mask_layer());
    if (layer->replica_layer() && layer->replica_layer()->mask_layer())
      drawn_layers->insert(layer->replica_layer()->mask_layer());
  }
}

TEST_F(LayerTreeHostCommonTest, RenderSurfaceLayerListMembership) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);
  gfx::Transform identity_matrix;

  scoped_ptr<LayerImpl> grand_parent =
      LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> parent = LayerImpl::Create(host_impl.active_tree(), 3);
  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.active_tree(), 5);
  scoped_ptr<LayerImpl> grand_child1 =
      LayerImpl::Create(host_impl.active_tree(), 7);
  scoped_ptr<LayerImpl> grand_child2 =
      LayerImpl::Create(host_impl.active_tree(), 9);

  LayerImpl* grand_parent_raw = grand_parent.get();
  LayerImpl* parent_raw = parent.get();
  LayerImpl* child_raw = child.get();
  LayerImpl* grand_child1_raw = grand_child1.get();
  LayerImpl* grand_child2_raw = grand_child2.get();

  child->AddChild(grand_child1.Pass());
  child->AddChild(grand_child2.Pass());
  parent->AddChild(child.Pass());
  grand_parent->AddChild(parent.Pass());

  SetLayerPropertiesForTesting(grand_parent_raw, identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(1, 2),
                               true, false, true);
  SetLayerPropertiesForTesting(parent_raw, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 2), true, false,
                               false);

  SetLayerPropertiesForTesting(child_raw, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 2), true, false,
                               false);

  SetLayerPropertiesForTesting(grand_child1_raw, identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(1, 2),
                               true, false, false);

  SetLayerPropertiesForTesting(grand_child2_raw, identity_matrix,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(1, 2),
                               true, false, false);

  // Start with nothing being drawn.
  ExecuteCalculateDrawProperties(grand_parent_raw);
  int member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_NE(member_id, membership_id(grand_child2_raw));

  std::set<LayerImpl*> expected;
  std::set<LayerImpl*> actual;
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // If we force render surface, but none of the layers are in the layer list,
  // then this layer should not appear in RSLL.
  grand_child1_raw->SetHasRenderSurface(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_NE(member_id, membership_id(grand_child2_raw));

  expected.clear();
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // However, if we say that this layer also draws content, it will appear in
  // RSLL.
  grand_child1_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_EQ(member_id, membership_id(grand_child1_raw));
  EXPECT_NE(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(grand_child1_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Now child is forced to have a render surface, and one if its children draws
  // content.
  grand_child1_raw->SetDrawsContent(false);
  grand_child1_raw->SetHasRenderSurface(false);
  child_raw->SetHasRenderSurface(true);
  grand_child2_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_EQ(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(grand_child2_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Add a mask layer to child.
  child_raw->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 6).Pass());

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_EQ(member_id, membership_id(child_raw->mask_layer()));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_EQ(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->mask_layer());

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->mask_layer());

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Add replica mask layer.
  scoped_ptr<LayerImpl> replica_layer =
      LayerImpl::Create(host_impl.active_tree(), 20);
  replica_layer->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 21));
  child_raw->SetReplicaLayer(replica_layer.Pass());

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_EQ(member_id, membership_id(child_raw->mask_layer()));
  EXPECT_EQ(member_id, membership_id(child_raw->replica_layer()->mask_layer()));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_EQ(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(grand_child2_raw);
  expected.insert(child_raw->mask_layer());
  expected.insert(child_raw->replica_layer()->mask_layer());

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  child_raw->TakeReplicaLayer();

  // With nothing drawing, we should have no layers.
  grand_child2_raw->SetDrawsContent(false);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_NE(member_id, membership_id(child_raw));
  EXPECT_NE(member_id, membership_id(child_raw->mask_layer()));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_NE(member_id, membership_id(grand_child2_raw));

  expected.clear();
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  // Child itself draws means that we should have the child and the mask in the
  // list.
  child_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_NE(member_id, membership_id(grand_parent_raw));
  EXPECT_NE(member_id, membership_id(parent_raw));
  EXPECT_EQ(member_id, membership_id(child_raw));
  EXPECT_EQ(member_id, membership_id(child_raw->mask_layer()));
  EXPECT_NE(member_id, membership_id(grand_child1_raw));
  EXPECT_NE(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(child_raw);
  expected.insert(child_raw->mask_layer());
  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);

  child_raw->TakeMaskLayer();

  // Now everyone's a member!
  grand_parent_raw->SetDrawsContent(true);
  parent_raw->SetDrawsContent(true);
  child_raw->SetDrawsContent(true);
  grand_child1_raw->SetDrawsContent(true);
  grand_child2_raw->SetDrawsContent(true);

  ExecuteCalculateDrawProperties(grand_parent_raw);
  member_id = render_surface_layer_list_count();

  EXPECT_EQ(member_id, membership_id(grand_parent_raw));
  EXPECT_EQ(member_id, membership_id(parent_raw));
  EXPECT_EQ(member_id, membership_id(child_raw));
  EXPECT_EQ(member_id, membership_id(grand_child1_raw));
  EXPECT_EQ(member_id, membership_id(grand_child2_raw));

  expected.clear();
  expected.insert(grand_parent_raw);
  expected.insert(parent_raw);
  expected.insert(child_raw);
  expected.insert(grand_child1_raw);
  expected.insert(grand_child2_raw);

  actual.clear();
  GatherDrawnLayers(render_surface_layer_list_impl(), &actual);
  EXPECT_EQ(expected, actual);
}

TEST_F(LayerTreeHostCommonTest, DrawPropertyScales) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  settings.layer_transforms_should_scale_layer_contents = true;
  FakeLayerTreeHostImpl host_impl(settings, &proxy, &shared_bitmap_manager,
                                  &task_graph_runner);

  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  LayerImpl* root_layer = root.get();
  scoped_ptr<LayerImpl> child1 = LayerImpl::Create(host_impl.active_tree(), 2);
  LayerImpl* child1_layer = child1.get();
  scoped_ptr<LayerImpl> child2 = LayerImpl::Create(host_impl.active_tree(), 3);
  LayerImpl* child2_layer = child2.get();

  root->AddChild(child1.Pass());
  root->AddChild(child2.Pass());
  root->SetHasRenderSurface(true);

  gfx::Transform identity_matrix, scale_transform_child1,
      scale_transform_child2;
  scale_transform_child1.Scale(2, 3);
  scale_transform_child2.Scale(4, 5);

  SetLayerPropertiesForTesting(root_layer, identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1, 1), true, false,
                               true);
  SetLayerPropertiesForTesting(child1_layer, scale_transform_child1,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(), true,
                               false, false);

  child1_layer->SetMaskLayer(
      LayerImpl::Create(host_impl.active_tree(), 4).Pass());

  scoped_ptr<LayerImpl> replica_layer =
      LayerImpl::Create(host_impl.active_tree(), 5);
  replica_layer->SetHasRenderSurface(true);
  replica_layer->SetMaskLayer(LayerImpl::Create(host_impl.active_tree(), 6));
  child1_layer->SetReplicaLayer(replica_layer.Pass());
  child1_layer->SetHasRenderSurface(true);

  ExecuteCalculateDrawProperties(root_layer);

  TransformOperations scale;
  scale.AppendScale(5.f, 8.f, 3.f);

  AddAnimatedTransformToLayer(child2_layer, 1.0, TransformOperations(), scale);
  SetLayerPropertiesForTesting(child2_layer, scale_transform_child2,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(), true,
                               false, false);

  ExecuteCalculateDrawProperties(root_layer);

  EXPECT_FLOAT_EQ(1.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(3.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(3.f, child1_layer->mask_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(5.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(
      0.f, root_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      0.f, child1_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->replica_layer()
                      ->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      8.f, child2_layer->draw_properties().maximum_animation_contents_scale);

  // Changing page-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  float page_scale_factor = 3.f;
  float device_scale_factor = 1.0f;
  std::vector<LayerImpl*> render_surface_layer_list;
  gfx::Size device_viewport_size =
      gfx::Size(root_layer->bounds().width() * device_scale_factor,
                root_layer->bounds().height() * device_scale_factor);
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root_layer, device_viewport_size, &render_surface_layer_list);

  inputs.page_scale_factor = page_scale_factor;
  inputs.can_adjust_raster_scales = true;
  inputs.page_scale_layer = root_layer;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_FLOAT_EQ(3.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(9.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(9.f, child1_layer->mask_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(
      9.f,
      child1_layer->replica_layer()->mask_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(15.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(
      0.f, root_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      0.f, child1_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->replica_layer()
                      ->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      24.f, child2_layer->draw_properties().maximum_animation_contents_scale);

  // Changing device-scale would affect ideal_contents_scale and
  // maximum_animation_contents_scale.

  device_scale_factor = 4.0f;
  inputs.device_scale_factor = device_scale_factor;
  inputs.can_adjust_raster_scales = true;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_FLOAT_EQ(12.f, root_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(36.f, child1_layer->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(36.f, child1_layer->mask_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(
      36.f,
      child1_layer->replica_layer()->mask_layer()->GetIdealContentsScale());
  EXPECT_FLOAT_EQ(60.f, child2_layer->GetIdealContentsScale());

  EXPECT_FLOAT_EQ(
      0.f, root_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      0.f, child1_layer->draw_properties().maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(0.f,
                  child1_layer->replica_layer()
                      ->mask_layer()
                      ->draw_properties()
                      .maximum_animation_contents_scale);
  EXPECT_FLOAT_EQ(
      96.f, child2_layer->draw_properties().maximum_animation_contents_scale);
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectInChildRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(),
                               gfx::Transform(),
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(768 / 2, 3000),
                               true,
                               false);
  root->SetIsDrawable(true);

  scoped_refptr<Layer> clip = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(clip.get(),
                               gfx::Transform(),
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(768 / 2, 10000),
                               true,
                               false);
  clip->SetMasksToBounds(true);

  scoped_refptr<Layer> content = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(content.get(),
                               gfx::Transform(),
                               gfx::Point3F(),
                               gfx::PointF(),
                               gfx::Size(768 / 2, 10000),
                               true,
                               false);
  content->SetIsDrawable(true);
  content->SetForceRenderSurface(true);

  root->AddChild(clip);
  clip->AddChild(content);

  host()->SetRootLayer(root);

  gfx::Size device_viewport_size(768, 582);
  RenderSurfaceLayerList render_surface_layer_list;
  LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
      host()->root_layer(), device_viewport_size, &render_surface_layer_list);
  inputs.device_scale_factor = 2.f;
  inputs.page_scale_factor = 1.f;
  inputs.page_scale_layer = NULL;
  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  // Layers in the root render surface have their visible content rect clipped
  // by the viewport.
  EXPECT_EQ(gfx::Rect(768 / 2, 582 / 2), root->visible_layer_rect());

  // Layers drawing to a child render surface should still have their visible
  // content rect clipped by the viewport.
  EXPECT_EQ(gfx::Rect(768 / 2, 582 / 2), content->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, BoundsDeltaAffectVisibleContentRect) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);

  // Set two layers: the root layer clips it's child,
  // the child draws its content.

  gfx::Size root_size = gfx::Size(300, 500);

  // Sublayer should be bigger than the root enlarged by bounds_delta.
  gfx::Size sublayer_size = gfx::Size(300, 1000);

  // Device viewport accomidated the root and the top controls.
  gfx::Size device_viewport_size = gfx::Size(300, 600);
  gfx::Transform identity_matrix;

  host_impl.SetViewportSize(device_viewport_size);
  host_impl.active_tree()->SetRootLayer(
      LayerImpl::Create(host_impl.active_tree(), 1));

  LayerImpl* root = host_impl.active_tree()->root_layer();
  SetLayerPropertiesForTesting(root,
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               root_size,
                               false,
                               false,
                               true);
  root->SetMasksToBounds(true);

  root->AddChild(LayerImpl::Create(host_impl.active_tree(), 2));

  LayerImpl* sublayer = root->child_at(0);
  SetLayerPropertiesForTesting(sublayer,
                               identity_matrix,
                               gfx::Point3F(),
                               gfx::PointF(),
                               sublayer_size,
                               false,
                               false,
                               false);
  sublayer->SetDrawsContent(true);

  LayerImplList layer_impl_list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, device_viewport_size, &layer_impl_list);

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  EXPECT_EQ(gfx::Rect(root_size), sublayer->visible_layer_rect());

  root->SetBoundsDelta(gfx::Vector2dF(0.0, 50.0));

  LayerTreeHostCommon::CalculateDrawProperties(&inputs);

  gfx::Rect affected_by_delta(0, 0, root_size.width(),
                              root_size.height() + 50);
  EXPECT_EQ(affected_by_delta, sublayer->visible_layer_rect());
}

TEST_F(LayerTreeHostCommonTest, NodesAffectedByBoundsDeltaGetUpdated) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> inner_viewport_container_layer =
      Layer::Create(layer_settings());
  scoped_refptr<Layer> inner_viewport_scroll_layer =
      Layer::Create(layer_settings());
  scoped_refptr<Layer> outer_viewport_container_layer =
      Layer::Create(layer_settings());
  scoped_refptr<Layer> outer_viewport_scroll_layer =
      Layer::Create(layer_settings());

  root->AddChild(inner_viewport_container_layer);
  inner_viewport_container_layer->AddChild(inner_viewport_scroll_layer);
  inner_viewport_scroll_layer->AddChild(outer_viewport_container_layer);
  outer_viewport_container_layer->AddChild(outer_viewport_scroll_layer);

  inner_viewport_scroll_layer->SetScrollClipLayerId(
      inner_viewport_container_layer->id());
  outer_viewport_scroll_layer->SetScrollClipLayerId(
      outer_viewport_container_layer->id());

  inner_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  outer_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);
  host()->RegisterViewportLayers(nullptr, root, inner_viewport_scroll_layer,
                                 outer_viewport_scroll_layer);

  scoped_refptr<Layer> fixed_to_inner = Layer::Create(layer_settings());
  scoped_refptr<Layer> fixed_to_outer = Layer::Create(layer_settings());

  inner_viewport_scroll_layer->AddChild(fixed_to_inner);
  outer_viewport_scroll_layer->AddChild(fixed_to_outer);

  LayerPositionConstraint fixed_to_right;
  fixed_to_right.set_is_fixed_position(true);
  fixed_to_right.set_is_fixed_to_right_edge(true);

  fixed_to_inner->SetPositionConstraint(fixed_to_right);
  fixed_to_outer->SetPositionConstraint(fixed_to_right);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  TransformTree& transform_tree = host()->property_trees()->transform_tree;
  EXPECT_TRUE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_TRUE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());

  LayerPositionConstraint fixed_to_left;
  fixed_to_left.set_is_fixed_position(true);
  fixed_to_inner->SetPositionConstraint(fixed_to_left);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_TRUE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());

  fixed_to_outer->SetPositionConstraint(fixed_to_left);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByInnerViewportBoundsDelta());
  EXPECT_FALSE(transform_tree.HasNodesAffectedByOuterViewportBoundsDelta());
}

TEST_F(LayerTreeHostCommonTest, VisibleContentRectForAnimatedLayer) {
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> animated =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(animated);

  host()->SetRootLayer(root);

  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(animated.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(20, 20), true, false);

  root->SetMasksToBounds(true);
  root->SetForceRenderSurface(true);
  animated->SetOpacity(0.f);

  AddOpacityTransitionToController(animated->layer_animation_controller(), 10.0,
                                   0.f, 1.f, false);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  EXPECT_FALSE(animated->visible_rect_from_property_trees().IsEmpty());
}

TEST_F(LayerTreeHostCommonTest,
       VisibleContentRectForAnimatedLayerWithSingularTransform) {
  const gfx::Transform identity_matrix;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> clip = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> animated =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> surface =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> descendant_of_animation =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(clip);
  clip->AddChild(animated);
  animated->AddChild(surface);
  surface->AddChild(descendant_of_animation);

  clip->SetMasksToBounds(true);
  surface->SetForceRenderSurface(true);

  host()->SetRootLayer(root);

  gfx::Transform uninvertible_matrix;
  uninvertible_matrix.Scale3d(6.f, 6.f, 0.f);

  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(clip.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  SetLayerPropertiesForTesting(animated.get(), uninvertible_matrix,
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(120, 120), true, false);
  SetLayerPropertiesForTesting(surface.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(descendant_of_animation.get(), identity_matrix,
                               gfx::Point3F(), gfx::PointF(),
                               gfx::Size(200, 200), true, false);

  TransformOperations start_transform_operations;
  start_transform_operations.AppendMatrix(uninvertible_matrix);
  TransformOperations end_transform_operations;

  AddAnimatedTransformToLayer(animated.get(), 10.0, start_transform_operations,
                              end_transform_operations);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  // The animated layer has a singular transform and maps to a non-empty rect in
  // clipped target space, so is treated as fully visible.
  EXPECT_EQ(gfx::Rect(120, 120), animated->visible_rect_from_property_trees());

  // The singular transform on |animated| is flattened when inherited by
  // |surface|, and this happens to make it invertible.
  EXPECT_EQ(gfx::Rect(2, 2), surface->visible_rect_from_property_trees());
  EXPECT_EQ(gfx::Rect(2, 2),
            descendant_of_animation->visible_rect_from_property_trees());

  gfx::Transform zero_matrix;
  zero_matrix.Scale3d(0.f, 0.f, 0.f);
  SetLayerPropertiesForTesting(animated.get(), zero_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(120, 120), true, false);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  // The animated layer maps to the empty rect in clipped target space, so is
  // treated as having an empty visible rect.
  EXPECT_EQ(gfx::Rect(), animated->visible_rect_from_property_trees());

  // This time, flattening does not make |animated|'s transform invertible. This
  // means the clip cannot be projected into |surface|'s space, so we treat
  // |surface| and layers that draw into it as fully visible.
  EXPECT_EQ(gfx::Rect(100, 100), surface->visible_rect_from_property_trees());
  EXPECT_EQ(gfx::Rect(200, 200),
            descendant_of_animation->visible_rect_from_property_trees());
}

// Verify that having an animated filter (but no current filter, as these
// are mutually exclusive) correctly creates a render surface.
TEST_F(LayerTreeHostCommonTest, AnimatedFilterCreatesRenderSurface) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<Layer> grandchild = Layer::Create(layer_settings());
  root->AddChild(child);
  child->AddChild(grandchild);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(child.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(grandchild.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(50, 50),
                               true, false);
  host()->SetRootLayer(root);

  AddAnimatedFilterToLayer(child.get(), 10.0, 0.1f, 0.2f);

  ExecuteCalculateDrawProperties(root.get());

  EXPECT_TRUE(root->render_surface());
  EXPECT_TRUE(child->render_surface());
  EXPECT_FALSE(grandchild->render_surface());

  EXPECT_TRUE(root->filters().IsEmpty());
  EXPECT_TRUE(child->filters().IsEmpty());
  EXPECT_TRUE(grandchild->filters().IsEmpty());

  EXPECT_FALSE(root->FilterIsAnimating());
  EXPECT_TRUE(child->FilterIsAnimating());
  EXPECT_FALSE(grandchild->FilterIsAnimating());
}

// Ensures that the property tree code accounts for offsets between fixed
// position layers and their respective containers.
TEST_F(LayerTreeHostCommonTest, PropertyTreesAccountForFixedParentOffset) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grandchild =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(child);
  child->AddChild(grandchild);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(child.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(1000, 1000), gfx::Size(50, 50), true,
                               false);
  SetLayerPropertiesForTesting(grandchild.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(-1000, -1000),
                               gfx::Size(50, 50), true, false);

  root->SetMasksToBounds(true);
  root->SetIsContainerForFixedPositionLayers(true);
  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  grandchild->SetPositionConstraint(constraint);

  root->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50),
            grandchild->visible_rect_from_property_trees());
}

// Ensures that the property tree code accounts for offsets between fixed
// position containers and their transform tree parents, when a fixed position
// layer's container is its layer tree parent, but this parent doesn't have its
// own transform tree node.
TEST_F(LayerTreeHostCommonTest,
       PropertyTreesAccountForFixedParentOffsetWhenContainerIsParent) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> grandchild =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(child);
  child->AddChild(grandchild);

  gfx::Transform identity_transform;
  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(child.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(1000, 1000), gfx::Size(50, 50), true,
                               false);
  SetLayerPropertiesForTesting(grandchild.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(-1000, -1000),
                               gfx::Size(50, 50), true, false);

  root->SetMasksToBounds(true);
  child->SetIsContainerForFixedPositionLayers(true);
  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  grandchild->SetPositionConstraint(constraint);

  root->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  EXPECT_EQ(gfx::Rect(0, 0, 50, 50),
            grandchild->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, CombineClipsUsingContentTarget) {
  // In the following layer tree, the layer |box|'s render target is |surface|.
  // |surface| also creates a transform node. We want to combine clips for |box|
  // in the space of its target (i.e., |surface|), not its target's target. This
  // test ensures that happens.

  gfx::Transform rotate;
  rotate.Rotate(5);
  gfx::Transform identity;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(2500, 1500), true,
                               false);

  scoped_refptr<Layer> frame_clip = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(frame_clip.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(2500, 1500), true,
                               false);
  frame_clip->SetMasksToBounds(true);

  scoped_refptr<Layer> rotated = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(rotated.get(), rotate,
                               gfx::Point3F(1250, 250, 0), gfx::PointF(),
                               gfx::Size(2500, 500), true, false);

  scoped_refptr<Layer> surface = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(surface.get(), rotate, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(2500, 500), true,
                               false);
  surface->SetOpacity(0.5);

  scoped_refptr<LayerWithForcedDrawsContent> container =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(container.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(300, 300), true, false);

  scoped_refptr<LayerWithForcedDrawsContent> box =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(box.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);

  root->AddChild(frame_clip);
  frame_clip->AddChild(rotated);
  rotated->AddChild(surface);
  surface->AddChild(container);
  surface->AddChild(box);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());
}

TEST_F(LayerTreeHostCommonTest, OnlyApplyFixedPositioningOnce) {
  gfx::Transform identity;
  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(800, 800), true, false);
  root->SetIsContainerForFixedPositionLayers(true);

  scoped_refptr<Layer> frame_clip = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(frame_clip.get(), translate_z, gfx::Point3F(),
                               gfx::PointF(500, 100), gfx::Size(100, 100), true,
                               false);
  frame_clip->SetMasksToBounds(true);

  scoped_refptr<LayerWithForcedDrawsContent> fixed =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(fixed.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1000, 1000), true,
                               false);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  root->AddChild(frame_clip);
  frame_clip->AddChild(fixed);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  gfx::Rect expected(0, 0, 100, 100);
  EXPECT_EQ(expected, fixed->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest,
       PropertyTreesAccountForScrollCompensationAdjustment) {
  gfx::Transform identity;
  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(800, 800), true, false);
  root->SetIsContainerForFixedPositionLayers(true);

  scoped_refptr<Layer> frame_clip = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(frame_clip.get(), translate_z, gfx::Point3F(),
                               gfx::PointF(500, 100), gfx::Size(100, 100), true,
                               false);
  frame_clip->SetMasksToBounds(true);

  scoped_refptr<LayerWithForcedDrawsContent> scroller =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(scroller.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1000, 1000), true,
                               false);

  scroller->SetScrollCompensationAdjustment(gfx::Vector2dF(0.3f, 0.7f));
  scroller->SetScrollOffset(gfx::ScrollOffset(0.3, 0.7));
  scroller->SetScrollClipLayerId(frame_clip->id());

  scoped_refptr<LayerWithForcedDrawsContent> fixed =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(fixed.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);

  scoped_refptr<LayerWithForcedDrawsContent> fixed_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(fixed_child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);

  fixed_child->SetPositionConstraint(constraint);

  root->AddChild(frame_clip);
  frame_clip->AddChild(scroller);
  scroller->AddChild(fixed);
  fixed->AddChild(fixed_child);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  gfx::Rect expected(0, 0, 50, 50);
  EXPECT_EQ(expected, fixed->visible_rect_from_property_trees());

  expected = gfx::Rect(0, 0, 10, 10);
  EXPECT_EQ(expected, fixed_child->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, FixedClipsShouldBeAssociatedWithTheRightNode) {
  gfx::Transform identity;

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(800, 800), true, false);
  root->SetIsContainerForFixedPositionLayers(true);

  scoped_refptr<Layer> frame_clip = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(frame_clip.get(), identity, gfx::Point3F(),
                               gfx::PointF(500, 100), gfx::Size(100, 100), true,
                               false);
  frame_clip->SetMasksToBounds(true);

  scoped_refptr<LayerWithForcedDrawsContent> scroller =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(scroller.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(1000, 1000), true,
                               false);

  scroller->SetScrollOffset(gfx::ScrollOffset(100, 100));
  scroller->SetScrollClipLayerId(frame_clip->id());

  scoped_refptr<LayerWithForcedDrawsContent> fixed =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(fixed.get(), identity, gfx::Point3F(),
                               gfx::PointF(100, 100), gfx::Size(50, 50), true,
                               false);

  LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(true);
  fixed->SetPositionConstraint(constraint);
  fixed->SetForceRenderSurface(true);
  fixed->SetMasksToBounds(true);

  root->AddChild(frame_clip);
  frame_clip->AddChild(scroller);
  scroller->AddChild(fixed);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());

  gfx::Rect expected(0, 0, 50, 50);
  EXPECT_EQ(expected, fixed->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, ChangingAxisAlignmentTriggersRebuild) {
  gfx::Transform identity;
  gfx::Transform translate;
  gfx::Transform rotate;

  translate.Translate(10, 10);
  rotate.Rotate(45);

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(800, 800), true, false);
  root->SetIsContainerForFixedPositionLayers(true);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(translate);
  EXPECT_FALSE(host()->property_trees()->needs_rebuild);

  root->SetTransform(rotate);
  EXPECT_TRUE(host()->property_trees()->needs_rebuild);
}

TEST_F(LayerTreeHostCommonTest, ChangeTransformOrigin) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  root->AddChild(child);

  host()->SetRootLayer(root);

  gfx::Transform identity_matrix;
  gfx::Transform scale_matrix;
  scale_matrix.Scale(2.f, 2.f);
  SetLayerPropertiesForTesting(root.get(), identity_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), scale_matrix, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_rect_from_property_trees());

  child->SetTransformOrigin(gfx::Point3F(10.f, 10.f, 10.f));

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(5, 5, 5, 5), child->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, UpdateScrollChildPosition) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> scroll_parent =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> scroll_child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));

  root->AddChild(scroll_child);
  root->AddChild(scroll_parent);
  scroll_child->SetScrollParent(scroll_parent.get());
  scroll_parent->SetScrollClipLayerId(root->id());

  host()->SetRootLayer(root);

  gfx::Transform identity_transform;
  gfx::Transform scale;
  scale.Scale(2.f, 2.f);
  SetLayerPropertiesForTesting(root.get(), identity_transform, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(50, 50), true, false);
  SetLayerPropertiesForTesting(scroll_child.get(), scale, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(40, 40), true, false);
  SetLayerPropertiesForTesting(scroll_parent.get(), identity_transform,
                               gfx::Point3F(), gfx::PointF(), gfx::Size(30, 30),
                               true, false);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(25, 25),
            scroll_child->visible_rect_from_property_trees());

  scroll_child->SetPosition(gfx::PointF(0, -10.f));
  scroll_parent->SetScrollOffset(gfx::ScrollOffset(0.f, 10.f));
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 5, 25, 25),
            scroll_child->visible_rect_from_property_trees());
}

static void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
}

TEST_F(LayerTreeHostCommonTest, SkippingSubtreeMain) {
  gfx::Transform identity;
  FakeContentLayerClient client;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<LayerWithForcedDrawsContent> grandchild =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  scoped_refptr<FakePictureLayer> greatgrandchild(
      FakePictureLayer::Create(layer_settings(), &client));
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  SetLayerPropertiesForTesting(grandchild.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  SetLayerPropertiesForTesting(greatgrandchild.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);

  root->AddChild(child);
  child->AddChild(grandchild);
  grandchild->AddChild(greatgrandchild);

  host()->SetRootLayer(root);

  // Check the non-skipped case.
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10), grandchild->visible_rect_from_property_trees());

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  grandchild->set_visible_rect_from_property_trees(gfx::Rect());
  gfx::Transform singular;
  singular.matrix().set(0, 0, 0);

  child->SetTransform(singular);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), grandchild->visible_rect_from_property_trees());
  child->SetTransform(identity);

  child->SetHideLayerAndSubtree(true);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), grandchild->visible_rect_from_property_trees());
  child->SetHideLayerAndSubtree(false);

  child->SetOpacity(0.f);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), grandchild->visible_rect_from_property_trees());

  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild->SetTouchEventHandlerRegion(Region(gfx::Rect(0, 0, 10, 10)));
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10), grandchild->visible_rect_from_property_trees());
  grandchild->set_visible_rect_from_property_trees(gfx::Rect());
  grandchild->SetTouchEventHandlerRegion(Region());
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), grandchild->visible_rect_from_property_trees());
  grandchild->set_visible_rect_from_property_trees(gfx::Rect());

  greatgrandchild->RequestCopyOfOutput(
      CopyOutputRequest::CreateBitmapRequest(base::Bind(&CopyOutputCallback)));
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10), grandchild->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, SkippingSubtreeImpl) {
  FakeImplProxy proxy;
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;
  FakeLayerTreeHostImpl host_impl(&proxy, &shared_bitmap_manager,
                                  &task_graph_runner);

  gfx::Transform identity;
  scoped_ptr<LayerImpl> root = LayerImpl::Create(host_impl.active_tree(), 1);
  scoped_ptr<LayerImpl> child = LayerImpl::Create(host_impl.active_tree(), 2);
  scoped_ptr<LayerImpl> grandchild =
      LayerImpl::Create(host_impl.active_tree(), 3);

  scoped_ptr<FakePictureLayerImpl> greatgrandchild(
      FakePictureLayerImpl::Create(host_impl.active_tree(), 4));

  child->SetDrawsContent(true);
  grandchild->SetDrawsContent(true);
  greatgrandchild->SetDrawsContent(true);

  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false,
                               true);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false,
                               false);
  SetLayerPropertiesForTesting(grandchild.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false,
                               false);
  SetLayerPropertiesForTesting(greatgrandchild.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false,
                               true);

  LayerImpl* child_ptr = child.get();
  LayerImpl* grandchild_ptr = grandchild.get();
  LayerImpl* greatgrandchild_ptr = greatgrandchild.get();

  grandchild->AddChild(greatgrandchild.Pass());
  child->AddChild(grandchild.Pass());
  root->AddChild(child.Pass());

  // Check the non-skipped case.
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10),
            grandchild_ptr->visible_rect_from_property_trees());

  // Now we will reset the visible rect from property trees for the grandchild,
  // and we will configure |child| in several ways that should force the subtree
  // to be skipped. The visible content rect for |grandchild| should, therefore,
  // remain empty.
  grandchild_ptr->set_visible_rect_from_property_trees(gfx::Rect());
  gfx::Transform singular;
  singular.matrix().set(0, 0, 0);

  child_ptr->SetTransform(singular);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0),
            grandchild_ptr->visible_rect_from_property_trees());
  child_ptr->SetTransform(identity);

  child_ptr->SetHideLayerAndSubtree(true);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0),
            grandchild_ptr->visible_rect_from_property_trees());
  child_ptr->SetHideLayerAndSubtree(false);

  child_ptr->SetOpacity(0.f);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0),
            grandchild_ptr->visible_rect_from_property_trees());

  // Now, even though child has zero opacity, we will configure |grandchild| and
  // |greatgrandchild| in several ways that should force the subtree to be
  // processed anyhow.
  grandchild_ptr->SetTouchEventHandlerRegion(Region(gfx::Rect(0, 0, 10, 10)));
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10),
            grandchild_ptr->visible_rect_from_property_trees());
  grandchild_ptr->set_visible_rect_from_property_trees(gfx::Rect());
  grandchild_ptr->SetTouchEventHandlerRegion(Region());
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0),
            grandchild_ptr->visible_rect_from_property_trees());
  grandchild_ptr->set_visible_rect_from_property_trees(gfx::Rect());

  ScopedPtrVector<CopyOutputRequest> requests;
  requests.push_back(CopyOutputRequest::CreateEmptyRequest());

  greatgrandchild_ptr->PassCopyRequests(&requests);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10),
            grandchild_ptr->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, SkippingLayer) {
  gfx::Transform identity;
  FakeContentLayerClient client;
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<LayerWithForcedDrawsContent> child =
      make_scoped_refptr(new LayerWithForcedDrawsContent(layer_settings()));
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(10, 10), true, false);
  root->AddChild(child);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(10, 10), child->visible_rect_from_property_trees());
  child->set_visible_rect_from_property_trees(gfx::Rect());

  child->SetHideLayerAndSubtree(true);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_rect_from_property_trees());
  child->SetHideLayerAndSubtree(false);

  child->SetBounds(gfx::Size());
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_rect_from_property_trees());
  child->SetBounds(gfx::Size(10, 10));

  gfx::Transform rotate;
  child->SetDoubleSided(false);
  rotate.RotateAboutXAxis(180.f);
  child->SetTransform(rotate);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_rect_from_property_trees());
  child->SetDoubleSided(true);
  child->SetTransform(identity);

  child->SetOpacity(0.f);
  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(gfx::Rect(0, 0), child->visible_rect_from_property_trees());
}

TEST_F(LayerTreeHostCommonTest, LayerTreeRebuildTest) {
  // Ensure that the treewalk in LayerTreeHostCommom::
  // PreCalculateMetaInformation happens when its required.
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> parent = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());

  root->AddChild(parent);
  parent->AddChild(child);

  child->SetClipParent(root.get());

  gfx::Transform identity;

  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(parent.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawProperties(root.get());
  EXPECT_EQ(parent->draw_properties().num_unclipped_descendants, 1u);

  // Ensure the dynamic update to input handlers happens.
  child->SetHaveWheelEventHandlers(true);
  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_input_handler);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_input_handler);

  child->SetHaveWheelEventHandlers(false);
  EXPECT_FALSE(root->draw_properties().layer_or_descendant_has_input_handler);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_FALSE(root->draw_properties().layer_or_descendant_has_input_handler);

  child->RequestCopyOfOutput(
      CopyOutputRequest::CreateRequest(base::Bind(&EmptyCopyOutputCallback)));
  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_copy_request);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_TRUE(root->draw_properties().layer_or_descendant_has_copy_request);
}

TEST_F(LayerTreeHostCommonTest, InputHandlersRecursiveUpdateTest) {
  // Ensure that the treewalk in LayertreeHostCommon::
  // PreCalculateMetaInformation updates input handlers correctly.
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());

  root->AddChild(child);

  child->SetHaveWheelEventHandlers(true);

  gfx::Transform identity;

  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);

  host()->SetRootLayer(root);

  EXPECT_EQ(root->num_layer_or_descendants_with_input_handler(), 0);
  ExecuteCalculateDrawProperties(root.get());
  EXPECT_EQ(root->num_layer_or_descendants_with_input_handler(), 1);
  child->SetHaveWheelEventHandlers(false);
  EXPECT_EQ(root->num_layer_or_descendants_with_input_handler(), 0);
}

TEST_F(LayerTreeHostCommonTest, ResetPropertyTreeIndices) {
  gfx::Transform identity;
  gfx::Transform translate_z;
  translate_z.Translate3d(0, 0, 10);

  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(800, 800), true, false);

  scoped_refptr<Layer> child = Layer::Create(layer_settings());
  SetLayerPropertiesForTesting(child.get(), translate_z, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);

  root->AddChild(child);

  host()->SetRootLayer(root);

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_NE(-1, child->transform_tree_index());

  child->RemoveFromParent();

  ExecuteCalculateDrawPropertiesWithPropertyTrees(root.get());
  EXPECT_EQ(-1, child->transform_tree_index());
}

TEST_F(LayerTreeHostCommonTest, ResetLayerDrawPropertiestest) {
  scoped_refptr<Layer> root = Layer::Create(layer_settings());
  scoped_refptr<Layer> child = Layer::Create(layer_settings());

  root->AddChild(child);
  gfx::Transform identity;

  SetLayerPropertiesForTesting(root.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);
  SetLayerPropertiesForTesting(child.get(), identity, gfx::Point3F(),
                               gfx::PointF(), gfx::Size(100, 100), true, false);

  host()->SetRootLayer(root);

  EXPECT_FALSE(root->layer_or_descendant_is_drawn());
  EXPECT_FALSE(root->visited());
  EXPECT_FALSE(root->sorted_for_recursion());
  EXPECT_FALSE(child->layer_or_descendant_is_drawn());
  EXPECT_FALSE(child->visited());
  EXPECT_FALSE(child->sorted_for_recursion());

  root->set_layer_or_descendant_is_drawn(true);
  root->set_visited(true);
  root->set_sorted_for_recursion(true);
  child->set_layer_or_descendant_is_drawn(true);
  child->set_visited(true);
  child->set_sorted_for_recursion(true);

  LayerTreeHostCommon::PreCalculateMetaInformationForTesting(root.get());

  EXPECT_FALSE(root->layer_or_descendant_is_drawn());
  EXPECT_FALSE(root->visited());
  EXPECT_FALSE(root->sorted_for_recursion());
  EXPECT_FALSE(child->layer_or_descendant_is_drawn());
  EXPECT_FALSE(child->visited());
  EXPECT_FALSE(child->sorted_for_recursion());
}

}  // namespace
}  // namespace cc
