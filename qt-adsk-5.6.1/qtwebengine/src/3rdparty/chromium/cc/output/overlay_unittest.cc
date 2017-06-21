// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/overlay_processor.h"
#include "cc/output/overlay_strategy_single_on_top.h"
#include "cc/output/overlay_strategy_underlay.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace cc {
namespace {

const gfx::Rect kOverlayRect(0, 0, 128, 128);
const gfx::Rect kOverlayTopLeftRect(0, 0, 64, 64);
const gfx::Rect kOverlayBottomRightRect(64, 64, 64, 64);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const gfx::Transform kNormalTransform =
    gfx::Transform(0.9f, 0, 0, 0.8f, 0.1f, 0.2f);  // x,y -> x,y.
const gfx::Transform kXMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, 0.8f, 1.0f, 0.2f);  // x,y -> 1-x,y.
const gfx::Transform kYMirrorTransform =
    gfx::Transform(0.9f, 0, 0, -0.8f, 0.1f, 1.0f);  // x,y -> x,1-y.
const gfx::Transform kBothMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, -0.8f, 1.0f, 1.0f);  // x,y -> 1-x,1-y.
const gfx::Transform kSwapTransform =
    gfx::Transform(0, 1, 1, 0, 0, 0);  // x,y -> y,x.

void MailboxReleased(unsigned sync_point,
                     bool lost_resource,
                     BlockingTaskRunner* main_thread_task_runner) {
}

class SingleOverlayValidator : public OverlayCandidateValidator {
 public:
  void CheckOverlaySupport(OverlayCandidateList* surfaces) override;
};

void SingleOverlayValidator::CheckOverlaySupport(
    OverlayCandidateList* surfaces) {
  ASSERT_EQ(2U, surfaces->size());

  OverlayCandidate& candidate = surfaces->back();
  if (candidate.display_rect.width() == 64) {
    EXPECT_EQ(kOverlayBottomRightRect, candidate.display_rect);
  } else {
    EXPECT_NEAR(kOverlayRect.x(), candidate.display_rect.x(), 0.01f);
    EXPECT_NEAR(kOverlayRect.y(), candidate.display_rect.y(), 0.01f);
    EXPECT_NEAR(kOverlayRect.width(), candidate.display_rect.width(), 0.01f);
    EXPECT_NEAR(kOverlayRect.height(), candidate.display_rect.height(), 0.01f);
  }
  EXPECT_EQ(BoundingRect(kUVTopLeft, kUVBottomRight).ToString(),
            candidate.uv_rect.ToString());
  candidate.overlay_handled = true;
}

template <typename OverlayStrategyType>
class SingleOverlayProcessor : public OverlayProcessor {
 public:
  explicit SingleOverlayProcessor(OutputSurface* surface)
      : OverlayProcessor(surface) {
    EXPECT_EQ(surface, surface_);
  }

  // Virtual to allow testing different strategies.
  void Initialize() override {
    OverlayCandidateValidator* candidates =
        surface_->GetOverlayCandidateValidator();
    ASSERT_TRUE(candidates != NULL);
    strategies_.push_back(
        scoped_ptr<Strategy>(new OverlayStrategyType(candidates)));
  }
};

class DefaultOverlayProcessor : public OverlayProcessor {
 public:
  explicit DefaultOverlayProcessor(OutputSurface* surface);
  size_t GetStrategyCount();
};

DefaultOverlayProcessor::DefaultOverlayProcessor(OutputSurface* surface)
    : OverlayProcessor(surface) {
}

size_t DefaultOverlayProcessor::GetStrategyCount() {
  return strategies_.size();
}

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(scoped_refptr<ContextProvider> context_provider)
      : OutputSurface(context_provider) {}

  // OutputSurface implementation
  void SwapBuffers(CompositorFrame* frame) override;

  void InitWithSingleOverlayValidator() {
    overlay_candidate_validator_.reset(new SingleOverlayValidator);
  }

  OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return overlay_candidate_validator_.get();
  }

 private:
  scoped_ptr<OverlayCandidateValidator> overlay_candidate_validator_;
};

void OverlayOutputSurface::SwapBuffers(CompositorFrame* frame) {
  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

scoped_ptr<RenderPass> CreateRenderPass() {
  RenderPassId id(1, 0);
  gfx::Rect output_rect(0, 0, 256, 256);
  bool has_transparent_background = true;

  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id,
               output_rect,
               output_rect,
               gfx::Transform(),
               has_transparent_background);

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  return pass.Pass();
}

ResourceId CreateResource(ResourceProvider* resource_provider) {
  unsigned sync_point = 0;
  TextureMailbox mailbox =
      TextureMailbox(gpu::Mailbox::Generate(), GL_TEXTURE_2D, sync_point);
  scoped_ptr<SingleReleaseCallbackImpl> release_callback =
      SingleReleaseCallbackImpl::Create(base::Bind(&MailboxReleased));

  return resource_provider->CreateResourceFromTextureMailbox(
      mailbox, release_callback.Pass());
}

SolidColorDrawQuad* CreateSolidColorQuadAt(
    const SharedQuadState* shared_quad_state,
    SkColor color,
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  SolidColorDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, rect, rect, color, false);
  return quad;
}

TextureDrawQuad* CreateCandidateQuadAt(ResourceProvider* resource_provider,
                                       const SharedQuadState* shared_quad_state,
                                       RenderPass* render_pass,
                                       const gfx::Rect& rect) {
  ResourceId resource_id = CreateResource(resource_provider);
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::Size resource_size_in_pixels = gfx::Size(64, 64);
  bool allow_overlay = true;

  TextureDrawQuad* overlay_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state,
                       rect,
                       rect,
                       rect,
                       resource_id,
                       premultiplied_alpha,
                       kUVTopLeft,
                       kUVBottomRight,
                       SK_ColorTRANSPARENT,
                       vertex_opacity,
                       flipped,
                       nearest_neighbor);
  overlay_quad->set_allow_overlay(allow_overlay);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

  return overlay_quad;
}

StreamVideoDrawQuad* CreateCandidateVideoQuadAt(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Rect& rect,
    const gfx::Transform& transform) {
  ResourceId resource_id = CreateResource(resource_provider);
  gfx::Size resource_size_in_pixels = gfx::Size(64, 64);
  bool allow_overlay = true;

  StreamVideoDrawQuad* overlay_quad =
      render_pass->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, rect, resource_id,
                       resource_size_in_pixels, allow_overlay, transform);

  return overlay_quad;
}

TextureDrawQuad* CreateFullscreenCandidateQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass) {
  return CreateCandidateQuadAt(
      resource_provider, shared_quad_state, render_pass, kOverlayRect);
}

StreamVideoDrawQuad* CreateFullscreenCandidateVideoQuad(
    ResourceProvider* resource_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Transform& transform) {
  return CreateCandidateVideoQuadAt(resource_provider, shared_quad_state,
                                    render_pass, kOverlayRect, transform);
}

void CreateCheckeredQuadAt(ResourceProvider* resource_provider,
                           const SharedQuadState* shared_quad_state,
                           RenderPass* render_pass,
                           const gfx::Rect& rect) {
  CheckerboardDrawQuad* checkerboard_quad =
      render_pass->CreateAndAppendDrawQuad<CheckerboardDrawQuad>();
  checkerboard_quad->SetNew(shared_quad_state, rect, rect, SkColor(), 1.f);
}

void CreateFullscreenCheckeredQuad(ResourceProvider* resource_provider,
                                   const SharedQuadState* shared_quad_state,
                                   RenderPass* render_pass) {
  CreateCheckeredQuadAt(
      resource_provider, shared_quad_state, render_pass, kOverlayRect);
}

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i];
    RenderPass* actual = actual_list[i];

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (auto exp_iter = expected->quad_list.cbegin(),
              act_iter = actual->quad_list.cbegin();
         exp_iter != expected->quad_list.cend();
         ++exp_iter, ++act_iter) {
      EXPECT_EQ(exp_iter->rect.ToString(), act_iter->rect.ToString());
      EXPECT_EQ(exp_iter->shared_quad_state->quad_layer_bounds.ToString(),
                act_iter->shared_quad_state->quad_layer_bounds.ToString());
    }
  }
}

TEST(OverlayTest, NoOverlaysByDefault) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  EXPECT_EQ(NULL, output_surface.GetOverlayCandidateValidator());

  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.GetOverlayCandidateValidator() != NULL);
}

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  OverlayOutputSurface output_surface(provider);
  FakeOutputSurfaceClient client;
  EXPECT_TRUE(output_surface.BindToClient(&client));
  output_surface.InitWithSingleOverlayValidator();
  EXPECT_TRUE(output_surface.GetOverlayCandidateValidator() != NULL);

  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());
  scoped_ptr<ResourceProvider> resource_provider = FakeResourceProvider::Create(
      &output_surface, shared_bitmap_manager.get());

  scoped_ptr<DefaultOverlayProcessor> overlay_processor(
      new DefaultOverlayProcessor(&output_surface));
  overlay_processor->Initialize();
  EXPECT_GE(2U, overlay_processor->GetStrategyCount());
}

template <typename OverlayStrategyType>
class OverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    EXPECT_TRUE(output_surface_->BindToClient(&client_));
    output_surface_->InitWithSingleOverlayValidator();
    EXPECT_TRUE(output_surface_->GetOverlayCandidateValidator() != NULL);

    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get());

    overlay_processor_.reset(
        new SingleOverlayProcessor<OverlayStrategyType>(output_surface_.get()));
    overlay_processor_->Initialize();
  }

  scoped_refptr<TestContextProvider> provider_;
  scoped_ptr<OverlayOutputSurface> output_surface_;
  FakeOutputSurfaceClient client_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SingleOverlayProcessor<OverlayStrategyType>> overlay_processor_;
};

typedef OverlayTest<OverlayStrategySingleOnTop> SingleOverlayOnTopTest;
typedef OverlayTest<OverlayStrategyUnderlay> UnderlayTest;

TEST_F(SingleOverlayOnTopTest, SuccessfullOverlay) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* original_quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  unsigned original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);

  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());

  RenderPass* main_pass = pass_list.back();
  // Check that the quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  const QuadList& quad_list = main_pass->quad_list;
  for (QuadList::ConstBackToFrontIterator it = quad_list.BackToFrontBegin();
       it != quad_list.BackToFrontEnd();
       ++it) {
    EXPECT_NE(DrawQuad::TEXTURE_CONTENT, it->material);
  }

  // Check that the right resource id got extracted.
  EXPECT_EQ(original_resource_id, candidate_list.back().resource_id);
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  RenderPassList pass_list;
  pass_list.push_back(CreateRenderPass());

  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  // Add something behind it.
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(2U, candidate_list.size());

  // This should be the same.
  ASSERT_EQ(2U, pass_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectPremultipliedAlpha) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->premultiplied_alpha = true;

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlending) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->needs_blending = true;

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad =
      CreateFullscreenCandidateQuad(resource_provider_.get(),
                                    pass->shared_quad_state_list.back(),
                                    pass.get());
  quad->background_color = SK_ColorBLACK;

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->blend_mode = SkXfermode::kScreen_Mode;

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectNonAxisAlignedTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVerticalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  rect.Offset(0, -rect.height());
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      -1.0f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowHorizontalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_height(rect.height() / 2);
  rect.Offset(-rect.width(), 0);
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(-1.0f,
                                                                      2.0f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowPositiveScaleTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      1.0f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, Allow90DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(90.f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_90, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, Allow180DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), -rect.height());
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(180.f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_180, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, Allow270DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), 0);
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(270.f);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_270, candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowNotTopIfNotOccluded) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateCheckeredQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(),
                        pass.get(),
                        kOverlayTopLeftRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(),
                        pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentOnTop) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentColorOnTop) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                         SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpaqueColorOnTop) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.5f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectTransparentColorOnTopWithoutBlending) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state, SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect)->opaque_rect =
      kOverlayBottomRightRect;
  CreateCandidateQuadAt(resource_provider_.get(), shared_state, pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectVideoSwapTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kSwapTransform);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoXMirrorTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kXMirrorTransform);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoBothMirrorTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kBothMirrorTransform);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoNormalTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kNormalTransform);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowVideoYMirrorTransform) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateVideoQuad(resource_provider_.get(),
                                     pass->shared_quad_state_list.back(),
                                     pass.get(), kYMirrorTransform);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());
  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  ASSERT_EQ(1U, pass_list.size());
  EXPECT_EQ(2U, candidate_list.size());
}

TEST_F(UnderlayTest, OverlayLayerUnderMainLayer) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateCandidateQuadAt(resource_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(0, candidate_list[0].plane_z_order);
  EXPECT_EQ(-1, candidate_list[1].plane_z_order);
  EXPECT_EQ(2U, pass_list[0]->quad_list.size());
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(pass_list[0]->quad_list.back()->material, DrawQuad::SOLID_COLOR);
}

TEST_F(UnderlayTest, AllowOnTop) {
  scoped_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  pass->CreateAndAppendSharedQuadState()->opacity = 0.5f;
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  OverlayCandidateList candidate_list;
  overlay_processor_->ProcessForOverlays(&pass_list, &candidate_list);
  EXPECT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, candidate_list.size());
  EXPECT_EQ(0, candidate_list[0].plane_z_order);
  EXPECT_EQ(-1, candidate_list[1].plane_z_order);
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(pass_list[0]->quad_list.front()->material, DrawQuad::SOLID_COLOR);
}

class OverlayInfoRendererGL : public GLRenderer {
 public:
  OverlayInfoRendererGL(RendererClient* client,
                        const RendererSettings* settings,
                        OutputSurface* output_surface,
                        ResourceProvider* resource_provider)
      : GLRenderer(client,
                   settings,
                   output_surface,
                   resource_provider,
                   NULL,
                   0),
        expect_overlays_(false) {}

  MOCK_METHOD3(DoDrawQuad,
               void(DrawingFrame* frame,
                    const DrawQuad* quad,
                    const gfx::QuadF* draw_region));

  using GLRenderer::BeginDrawingFrame;

  void FinishDrawingFrame(DrawingFrame* frame) override {
    GLRenderer::FinishDrawingFrame(frame);

    if (!expect_overlays_) {
      EXPECT_EQ(0U, frame->overlay_list.size());
      return;
    }

    ASSERT_EQ(2U, frame->overlay_list.size());
    EXPECT_NE(0U, frame->overlay_list.back().resource_id);
  }

  void set_expect_overlays(bool expect_overlays) {
    expect_overlays_ = expect_overlays;
  }

 private:
  bool expect_overlays_;
};

class FakeRendererClient : public RendererClient {
 public:
  // RendererClient methods.
  void SetFullRootLayerDamage() override {}
};

class MockOverlayScheduler {
 public:
  MOCK_METHOD5(Schedule,
               void(int plane_z_order,
                    gfx::OverlayTransform plane_transform,
                    unsigned overlay_texture_id,
                    const gfx::Rect& display_bounds,
                    const gfx::RectF& uv_rect));
};

class GLRendererWithOverlaysTest : public testing::Test {
 protected:
  GLRendererWithOverlaysTest() {
    provider_ = TestContextProvider::Create();
    output_surface_.reset(new OverlayOutputSurface(provider_));
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ =
        FakeResourceProvider::Create(output_surface_.get(), nullptr);

    provider_->support()->SetScheduleOverlayPlaneCallback(base::Bind(
        &MockOverlayScheduler::Schedule, base::Unretained(&scheduler_)));
  }

  void Init(bool use_validator) {
    if (use_validator)
      output_surface_->InitWithSingleOverlayValidator();

    renderer_ =
        make_scoped_ptr(new OverlayInfoRendererGL(&renderer_client_,
                                                  &settings_,
                                                  output_surface_.get(),
                                                  resource_provider_.get()));
  }

  void SwapBuffers() { renderer_->SwapBuffers(CompositorFrameMetadata()); }

  RendererSettings settings_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<OverlayOutputSurface> output_surface_;
  FakeRendererClient renderer_client_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<OverlayInfoRendererGL> renderer_;
  scoped_refptr<TestContextProvider> provider_;
  MockOverlayScheduler scheduler_;
};

TEST_F(GLRendererWithOverlaysTest, OverlayQuadNotDrawn) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Candidate pass was taken out and extra skipped pass added,
  // so only draw 2 quads.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(2);
  EXPECT_CALL(scheduler_,
              Schedule(1,
                       gfx::OVERLAY_TRANSFORM_NONE,
                       _,
                       kOverlayRect,
                       BoundingRect(kUVTopLeft, kUVBottomRight))).Times(1);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, OccludedQuadInUnderlay) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Candidate quad should fail to be overlaid on top because of occlusion.
  // Expect to be replaced with transparent hole quad and placed in underlay.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(3);
  EXPECT_CALL(scheduler_,
              Schedule(-1, gfx::OVERLAY_TRANSFORM_NONE, _, kOverlayRect,
                       BoundingRect(kUVTopLeft, kUVBottomRight))).Times(1);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, NoValidatorNoOverlay) {
  bool use_validator = false;
  Init(use_validator);
  renderer_->set_expect_overlays(false);
  gfx::Rect viewport_rect(16, 16);

  scoped_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());
  CreateFullscreenCheckeredQuad(resource_provider_.get(),
                                pass->shared_quad_state_list.back(),
                                pass.get());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // Should see no overlays.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _, _)).Times(3);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->DrawFrame(&pass_list, 1.f, viewport_rect, viewport_rect, false);

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturned) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 = CreateResource(resource_provider_.get());
  ResourceId resource2 = CreateResource(resource_provider_.get());

  scoped_ptr<RenderPass> pass = CreateRenderPass();
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(2);
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(2);
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = resource2;
  overlay2.plane_z_order = 1;

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->BeginDrawingFrame(&frame2);
  renderer_->FinishDrawingFrame(&frame2);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // No overlays, release the resource.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  renderer_->set_expect_overlays(false);
  renderer_->BeginDrawingFrame(&frame3);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource2));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Use the same buffer twice.
  renderer_->set_expect_overlays(true);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(1);
  renderer_->BeginDrawingFrame(&frame1);
  renderer_->FinishDrawingFrame(&frame1);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _)).Times(0);
  renderer_->set_expect_overlays(false);
  renderer_->BeginDrawingFrame(&frame3);
  renderer_->FinishDrawingFrame(&frame3);
  EXPECT_TRUE(resource_provider_->InUseByConsumer(resource1));
  SwapBuffers();
  EXPECT_FALSE(resource_provider_->InUseByConsumer(resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);
}

}  // namespace
}  // namespace cc
