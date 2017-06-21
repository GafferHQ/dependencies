// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/animation/animation_curve.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/base/time_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

class LayerTreeHostAnimationTest : public LayerTreeTest {
 public:
  void SetupTree() override {
    LayerTreeTest::SetupTree();
    layer_tree_host()->root_layer()->set_layer_animation_delegate(this);
  }
};

// Makes sure that SetNeedsAnimate does not cause the CommitRequested() state to
// be set.
class LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested()
      : num_commits_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    // We skip the first commit because its the commit that populates the
    // impl thread with a tree. After the second commit, the test is done.
    if (num_commits_ != 1)
      return;

    layer_tree_host()->SetNeedsAnimate();
    // Right now, CommitRequested is going to be true, because during
    // BeginFrame, we force CommitRequested to true to prevent requests from
    // hitting the impl thread. But, when the next DidCommit happens, we should
    // verify that CommitRequested has gone back to false.
  }

  void DidCommit() override {
    if (!num_commits_) {
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
      layer_tree_host()->SetNeedsAnimate();
      EXPECT_FALSE(layer_tree_host()->CommitRequested());
    }

    // Verifies that the SetNeedsAnimate we made in ::Animate did not
    // trigger CommitRequested.
    EXPECT_FALSE(layer_tree_host()->CommitRequested());
    EndTest();
    num_commits_++;
  }

  void AfterTest() override {}

 private:
  int num_commits_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateShouldNotSetCommitRequested);

// Trigger a frame with SetNeedsCommit. Then, inside the resulting animate
// callback, request another frame using SetNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where SetNeedsAnimate() is called inside the BeginFrame
// flow.
class LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback()
      : num_begin_frames_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (!num_begin_frames_) {
      layer_tree_host()->SetNeedsAnimate();
      num_begin_frames_++;
      return;
    }
    EndTest();
  }

  void AfterTest() override {}

 private:
  int num_begin_frames_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSetNeedsAnimateInsideAnimationCallback);

// Add a layer animation and confirm that
// LayerTreeHostImpl::UpdateAnimationState does get called.
class LayerTreeHostAnimationTestAddAnimation
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimation()
      : update_animation_state_was_called_(false) {}

  void BeginTest() override {
    PostAddInstantAnimationToMainThread(layer_tree_host()->root_layer());
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    EXPECT_FALSE(has_unfinished_animation);
    update_animation_state_was_called_ = true;
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {
    EXPECT_LT(base::TimeTicks(), monotonic_time);

    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->layer_animation_controller();
    Animation* animation = controller->GetAnimation(Animation::OPACITY);
    if (animation)
      controller->RemoveAnimation(animation->id());

    EndTest();
  }

  void AfterTest() override { EXPECT_TRUE(update_animation_state_was_called_); }

 private:
  bool update_animation_state_was_called_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAddAnimation);

// Add a layer animation to a layer, but continually fail to draw. Confirm that
// after a while, we do eventually force a draw.
class LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws()
      : started_animating_(false) {}

  void BeginTest() override {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    started_animating_ = true;
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (started_animating_)
      EndTest();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame,
                                   DrawResult draw_result) override {
    return DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
  }

  void AfterTest() override {}

 private:
  bool started_animating_;
};

// Starvation can only be an issue with the MT compositor.
MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCheckerboardDoesNotStarveDraws);

// Ensures that animations eventually get deleted.
class LayerTreeHostAnimationTestAnimationsGetDeleted
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsGetDeleted()
      : started_animating_(false) {}

  void BeginTest() override {
    PostAddAnimationToMainThread(layer_tree_host()->root_layer());
  }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    bool have_animations = !host_impl->animation_registrar()
                                ->active_animation_controllers_for_testing()
                                .empty();
    if (!started_animating_ && have_animations) {
      started_animating_ = true;
      return;
    }

    if (started_animating_ && !have_animations)
      EndTest();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               Animation::TargetProperty target_property,
                               int group) override {
    // Animations on the impl-side controller only get deleted during a commit,
    // so we need to schedule a commit.
    layer_tree_host()->SetNeedsCommit();
  }

  void AfterTest() override {}

 private:
  bool started_animating_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimationsGetDeleted);

// Ensure that an animation's timing function is respected.
class LayerTreeHostAnimationTestAddAnimationWithTimingFunction
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAddAnimationWithTimingFunction() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(layer_settings(), &client_);
    picture_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(picture_);
  }

  void BeginTest() override { PostAddAnimationToMainThread(picture_.get()); }

  void AnimateLayers(LayerTreeHostImpl* host_impl,
                     base::TimeTicks monotonic_time) override {
    LayerAnimationController* controller_impl =
        host_impl->active_tree()->root_layer()->children()[0]->
        layer_animation_controller();
    Animation* animation = controller_impl->GetAnimation(Animation::OPACITY);
    if (!animation)
      return;

    const FloatAnimationCurve* curve =
        animation->curve()->ToFloatAnimationCurve();
    float start_opacity = curve->GetValue(base::TimeDelta());
    float end_opacity = curve->GetValue(curve->Duration());
    float linearly_interpolated_opacity =
        0.25f * end_opacity + 0.75f * start_opacity;
    base::TimeDelta time = TimeUtil::Scale(curve->Duration(), 0.25f);
    // If the linear timing function associated with this animation was not
    // picked up, then the linearly interpolated opacity would be different
    // because of the default ease timing function.
    EXPECT_FLOAT_EQ(linearly_interpolated_opacity, curve->GetValue(time));

    EndTest();
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationWithTimingFunction);

// Ensures that main thread animations have their start times synchronized with
// impl thread animations.
class LayerTreeHostAnimationTestSynchronizeAnimationStartTimes
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestSynchronizeAnimationStartTimes() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(layer_settings(), &client_);
    picture_->SetBounds(gfx::Size(4, 4));
    picture_->set_layer_animation_delegate(this);
    layer_tree_host()->root_layer()->AddChild(picture_);
  }

  void BeginTest() override { PostAddAnimationToMainThread(picture_.get()); }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {
    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->children()[0]->
        layer_animation_controller();
    Animation* animation = controller->GetAnimation(Animation::OPACITY);
    main_start_time_ = animation->start_time();
    controller->RemoveAnimation(animation->id());
    EndTest();
  }

  void UpdateAnimationState(LayerTreeHostImpl* impl_host,
                            bool has_unfinished_animation) override {
    LayerAnimationController* controller =
        impl_host->active_tree()->root_layer()->children()[0]->
        layer_animation_controller();
    Animation* animation = controller->GetAnimation(Animation::OPACITY);
    if (!animation)
      return;

    impl_start_time_ = animation->start_time();
  }

  void AfterTest() override {
    EXPECT_EQ(impl_start_time_, main_start_time_);
    EXPECT_LT(base::TimeTicks(), impl_start_time_);
  }

 private:
  base::TimeTicks main_start_time_;
  base::TimeTicks impl_start_time_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestSynchronizeAnimationStartTimes);

// Ensures that notify animation finished is called.
class LayerTreeHostAnimationTestAnimationFinishedEvents
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationFinishedEvents() {}

  void BeginTest() override {
    PostAddInstantAnimationToMainThread(layer_tree_host()->root_layer());
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               Animation::TargetProperty target_property,
                               int group) override {
    LayerAnimationController* controller =
        layer_tree_host()->root_layer()->layer_animation_controller();
    Animation* animation = controller->GetAnimation(Animation::OPACITY);
    if (animation)
      controller->RemoveAnimation(animation->id());
    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimationFinishedEvents);

// Ensures that when opacity is being animated, this value does not cause the
// subtree to be skipped.
class LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity()
      : update_check_layer_(
            FakePictureLayer::Create(layer_settings(), &client_)) {}

  void SetupTree() override {
    update_check_layer_->SetOpacity(0.f);
    layer_tree_host()->SetRootLayer(update_check_layer_);
    LayerTreeHostAnimationTest::SetupTree();
  }

  void BeginTest() override {
    PostAddAnimationToMainThread(update_check_layer_.get());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    LayerAnimationController* controller_impl =
        host_impl->active_tree()->root_layer()->layer_animation_controller();
    Animation* animation_impl =
        controller_impl->GetAnimation(Animation::OPACITY);
    controller_impl->RemoveAnimation(animation_impl->id());
    EndTest();
  }

  void AfterTest() override {
    // Update() should have been called once, proving that the layer was not
    // skipped.
    EXPECT_EQ(1, update_check_layer_->update_count());

    // clear update_check_layer_ so LayerTreeHost dies.
    update_check_layer_ = NULL;
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> update_check_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestDoNotSkipLayersWithAnimatedOpacity);

// Layers added to tree with existing active animations should have the
// animation correctly recognized.
class LayerTreeHostAnimationTestLayerAddedWithAnimation
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestLayerAddedWithAnimation() {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      scoped_refptr<Layer> layer = Layer::Create(layer_settings());
      layer->set_layer_animation_delegate(this);

      // Any valid AnimationCurve will do here.
      scoped_ptr<AnimationCurve> curve(new FakeFloatAnimationCurve());
      scoped_ptr<Animation> animation(
          Animation::Create(curve.Pass(), 1, 1, Animation::OPACITY));
      layer->layer_animation_controller()->AddAnimation(animation.Pass());

      // We add the animation *before* attaching the layer to the tree.
      layer_tree_host()->root_layer()->AddChild(layer);
    }
  }

  void AnimateLayers(LayerTreeHostImpl* impl_host,
                     base::TimeTicks monotonic_time) override {
    EndTest();
  }

  void AfterTest() override {}
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestLayerAddedWithAnimation);

class LayerTreeHostAnimationTestCancelAnimateCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestCancelAnimateCommit()
      : num_begin_frames_(0), num_commit_calls_(0), num_draw_calls_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    num_begin_frames_++;
    // No-op animate will cancel the commit.
    if (layer_tree_host()->source_frame_number() == 1) {
      EndTest();
      return;
    }
    layer_tree_host()->SetNeedsAnimate();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* impl) override {
    num_commit_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Commit should have been canceled.";
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    num_draw_calls_++;
    if (impl->active_tree()->source_frame_number() > 1)
      FAIL() << "Draw should have been canceled.";
  }

  void AfterTest() override {
    EXPECT_EQ(2, num_begin_frames_);
    EXPECT_EQ(1, num_commit_calls_);
    EXPECT_EQ(1, num_draw_calls_);
  }

 private:
  int num_begin_frames_;
  int num_commit_calls_;
  int num_draw_calls_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestCancelAnimateCommit);

class LayerTreeHostAnimationTestForceRedraw
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestForceRedraw()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (++num_animate_ < 2)
      layer_tree_host()->SetNeedsAnimate();
  }

  void Layout() override { layer_tree_host()->SetNextCommitForcesRedraw(); }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the animation was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_EQ(2, num_animate_);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestForceRedraw);

class LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit()
      : num_animate_(0), num_draw_layers_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (++num_animate_ <= 2) {
      layer_tree_host()->SetNeedsCommit();
      layer_tree_host()->SetNeedsAnimate();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* impl) override {
    if (++num_draw_layers_ == 2)
      EndTest();
  }

  void AfterTest() override {
    // The first commit will always draw; make sure the second draw triggered
    // by the SetNeedsCommit was not cancelled.
    EXPECT_EQ(2, num_draw_layers_);
    EXPECT_GE(num_animate_, 2);
  }

 private:
  int num_animate_;
  int num_draw_layers_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestAnimateAfterSetNeedsCommit);

// Make sure the main thread can still execute animations when CanDraw() is not
// true.
class LayerTreeHostAnimationTestRunAnimationWhenNotCanDraw
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestRunAnimationWhenNotCanDraw() : started_times_(0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(layer_settings(), &client_);
    picture_->SetBounds(gfx::Size(4, 4));
    picture_->set_layer_animation_delegate(this);
    layer_tree_host()->root_layer()->AddChild(picture_);
  }

  void BeginTest() override {
    layer_tree_host()->SetViewportSize(gfx::Size());
    PostAddAnimationToMainThread(picture_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {
    started_times_++;
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               Animation::TargetProperty target_property,
                               int group) override {
    EndTest();
  }

  void AfterTest() override { EXPECT_EQ(1, started_times_); }

 private:
  int started_times_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestRunAnimationWhenNotCanDraw);

// Animations should not be started when frames are being skipped due to
// checkerboard.
class LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations
    : public LayerTreeHostAnimationTest {
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(layer_settings(), &client_);
    picture_->SetBounds(gfx::Size(4, 4));
    picture_->set_layer_animation_delegate(this);
    layer_tree_host()->root_layer()->AddChild(picture_);
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    // Make sure that drawing many times doesn't cause a checkerboarded
    // animation to start so we avoid flake in this test.
    settings->timeout_and_draw_when_animation_checkerboards = false;
  }

  void BeginTest() override {
    prevented_draw_ = 0;
    added_animations_ = 0;
    started_times_ = 0;

    PostSetNeedsCommitToMainThread();
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    if (added_animations_ < 2)
      return draw_result;
    if (TestEnded())
      return draw_result;
    // Act like there is checkerboard when the second animation wants to draw.
    ++prevented_draw_;
    if (prevented_draw_ > 2)
      EndTest();
    return DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The animation is longer than 1 BeginFrame interval.
        AddOpacityTransitionToLayer(picture_.get(), 0.1, 0.2f, 0.8f, false);
        added_animations_++;
        break;
      case 2:
        // This second animation will not be drawn so it should not start.
        AddAnimatedTransformToLayer(picture_.get(), 0.1, 5, 5);
        added_animations_++;
        break;
    }
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {
    if (TestEnded())
      return;
    started_times_++;
  }

  void AfterTest() override {
    // Make sure we tried to draw the second animation but failed.
    EXPECT_LT(0, prevented_draw_);
    // The first animation should be started, but the second should not because
    // of checkerboard.
    EXPECT_EQ(1, started_times_);
  }

  int prevented_draw_;
  int added_animations_;
  int started_times_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestCheckerboardDoesntStartAnimations);

// Verifies that scroll offset animations are only accepted when impl-scrolling
// is supported, and that when scroll offset animations are accepted,
// scroll offset updates are sent back to the main thread.
class LayerTreeHostAnimationTestScrollOffsetChangesArePropagated
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetChangesArePropagated() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(layer_settings(), &client_);
    scroll_layer_->SetScrollClipLayerId(layer_tree_host()->root_layer()->id());
    scroll_layer_->SetBounds(gfx::Size(1000, 1000));
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(10, 20));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1: {
        scoped_ptr<ScrollOffsetAnimationCurve> curve(
            ScrollOffsetAnimationCurve::Create(
                gfx::ScrollOffset(500.f, 550.f),
                EaseInOutTimingFunction::Create()));
        scoped_ptr<Animation> animation(
            Animation::Create(curve.Pass(), 1, 0, Animation::SCROLL_OFFSET));
        animation->set_needs_synchronized_start_time(true);
        bool animation_added = scroll_layer_->AddAnimation(animation.Pass());
        bool impl_scrolling_supported =
            layer_tree_host()->proxy()->SupportsImplScrolling();
        EXPECT_EQ(impl_scrolling_supported, animation_added);
        if (!impl_scrolling_supported)
          EndTest();
        break;
      }
      default:
        if (scroll_layer_->scroll_offset().x() > 10 &&
            scroll_layer_->scroll_offset().y() > 20)
          EndTest();
    }
  }

  void AfterTest() override {}

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestScrollOffsetChangesArePropagated);

// Verifies that when the main thread removes a scroll animation and sets a new
// scroll position, the active tree takes on exactly this new scroll position
// after activation, and the main thread doesn't receive a spurious scroll
// delta.
class LayerTreeHostAnimationTestScrollOffsetAnimationRemoval
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestScrollOffsetAnimationRemoval()
      : final_postion_(50.0, 100.0) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();

    scroll_layer_ = FakePictureLayer::Create(layer_settings(), &client_);
    scroll_layer_->SetScrollClipLayerId(layer_tree_host()->root_layer()->id());
    scroll_layer_->SetBounds(gfx::Size(10000, 10000));
    scroll_layer_->SetScrollOffset(gfx::ScrollOffset(100.0, 200.0));
    layer_tree_host()->root_layer()->AddChild(scroll_layer_);

    scoped_ptr<ScrollOffsetAnimationCurve> curve(
        ScrollOffsetAnimationCurve::Create(gfx::ScrollOffset(6500.f, 7500.f),
                                           EaseInOutTimingFunction::Create()));
    scoped_ptr<Animation> animation(
        Animation::Create(curve.Pass(), 1, 0, Animation::SCROLL_OFFSET));
    animation->set_needs_synchronized_start_time(true);
    scroll_layer_->AddAnimation(animation.Pass());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    switch (layer_tree_host()->source_frame_number()) {
      case 0:
        break;
      case 1: {
        Animation* animation =
            scroll_layer_->layer_animation_controller()->GetAnimation(
                Animation::SCROLL_OFFSET);
        scroll_layer_->layer_animation_controller()->RemoveAnimation(
            animation->id());
        scroll_layer_->SetScrollOffset(final_postion_);
        break;
      }
      default:
        EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(true);
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const BeginFrameArgs& args) override {
    if (!host_impl->pending_tree())
      return;

    if (!host_impl->active_tree()->root_layer()) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
      return;
    }

    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->root_layer()->children()[0];
    Animation* animation =
        scroll_layer_impl->layer_animation_controller()->GetAnimation(
            Animation::SCROLL_OFFSET);

    if (!animation || animation->run_state() != Animation::RUNNING) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
      return;
    }

    // Block activation until the running animation has a chance to produce a
    // scroll delta.
    gfx::Vector2dF scroll_delta = scroll_layer_impl->ScrollDelta();
    if (scroll_delta.x() < 1.f || scroll_delta.y() < 1.f)
      return;

    host_impl->BlockNotifyReadyToActivateForTesting(false);
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->pending_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->pending_tree()->root_layer()->children()[0];
    EXPECT_EQ(final_postion_, scroll_layer_impl->CurrentScrollOffset());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    if (host_impl->active_tree()->source_frame_number() != 1)
      return;
    LayerImpl* scroll_layer_impl =
        host_impl->active_tree()->root_layer()->children()[0];
    EXPECT_EQ(final_postion_, scroll_layer_impl->CurrentScrollOffset());
    EndTest();
  }

  void AfterTest() override {
    EXPECT_EQ(final_postion_, scroll_layer_->scroll_offset());
  }

 private:
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;
  const gfx::ScrollOffset final_postion_;
};

MULTI_THREAD_TEST_F(LayerTreeHostAnimationTestScrollOffsetAnimationRemoval);

// When animations are simultaneously added to an existing layer and to a new
// layer, they should start at the same time, even when there's already a
// running animation on the existing layer.
class LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers()
      : frame_count_with_pending_tree_(0) {}

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      AddAnimatedTransformToLayer(layer_tree_host()->root_layer(), 4, 1, 1);
    } else if (layer_tree_host()->source_frame_number() == 2) {
      AddOpacityTransitionToLayer(
          layer_tree_host()->root_layer(), 1, 0.f, 0.5f, true);

      scoped_refptr<Layer> layer = Layer::Create(layer_settings());
      layer_tree_host()->root_layer()->AddChild(layer);
      layer->set_layer_animation_delegate(this);
      layer->SetBounds(gfx::Size(4, 4));
      AddOpacityTransitionToLayer(layer.get(), 1, 0.f, 0.5f, true);
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->BlockNotifyReadyToActivateForTesting(true);
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // For the commit that added animations to new and existing layers, keep
    // blocking activation. We want to verify that even with activation blocked,
    // the animation on the layer that's already in the active tree won't get a
    // head start.
    if (host_impl->pending_tree()->source_frame_number() != 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const BeginFrameArgs& args) override {
    if (!host_impl->pending_tree() ||
        host_impl->pending_tree()->source_frame_number() != 2)
      return;

    frame_count_with_pending_tree_++;
    if (frame_count_with_pending_tree_ == 2) {
      host_impl->BlockNotifyReadyToActivateForTesting(false);
    }
  }

  void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                            bool has_unfinished_animation) override {
    LayerAnimationController* root_controller_impl =
        host_impl->active_tree()->root_layer()->layer_animation_controller();
    Animation* root_animation =
        root_controller_impl->GetAnimation(Animation::OPACITY);
    if (!root_animation || root_animation->run_state() != Animation::RUNNING)
      return;

    LayerAnimationController* child_controller_impl =
        host_impl->active_tree()->root_layer()->children()
            [0]->layer_animation_controller();
    Animation* child_animation =
        child_controller_impl->GetAnimation(Animation::OPACITY);
    EXPECT_EQ(Animation::RUNNING, child_animation->run_state());
    EXPECT_EQ(root_animation->start_time(), child_animation->start_time());
    root_controller_impl->AbortAnimations(Animation::OPACITY);
    root_controller_impl->AbortAnimations(Animation::TRANSFORM);
    child_controller_impl->AbortAnimations(Animation::OPACITY);
    EndTest();
  }

  void AfterTest() override {}

 private:
  int frame_count_with_pending_tree_;
};

// This test blocks activation which is not supported for single thread mode.
MULTI_THREAD_BLOCKNOTIFY_TEST_F(
    LayerTreeHostAnimationTestAnimationsAddedToNewAndExistingLayers);

// When a layer with an animation is removed from the tree and later re-added,
// the animation should resume.
class LayerTreeHostAnimationTestAnimatedLayerRemovedAndAdded
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestAnimatedLayerRemovedAndAdded() {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create(layer_settings());
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);
    AddOpacityTransitionToLayer(layer_.get(), 10000.0, 0.1f, 0.9f, true);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        layer_->RemoveFromParent();
        break;
      case 2:
        layer_tree_host()->root_layer()->AddChild(layer_);
        break;
    }
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_TRUE(host_impl->animation_registrar()->needs_animate_layers());
        break;
      case 1:
        EXPECT_FALSE(host_impl->animation_registrar()->needs_animate_layers());
        break;
      case 2:
        EXPECT_TRUE(host_impl->animation_registrar()->needs_animate_layers());
        EndTest();
        break;
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAnimatedLayerRemovedAndAdded);

class LayerTreeHostAnimationTestAddAnimationAfterAnimating
    : public LayerTreeHostAnimationTest {
 public:
  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    layer_ = Layer::Create(layer_settings());
    layer_->SetBounds(gfx::Size(4, 4));
    layer_tree_host()->root_layer()->AddChild(layer_);
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // First frame: add an animation to the root layer.
        AddAnimatedTransformToLayer(layer_tree_host()->root_layer(), 0.1, 5, 5);
        break;
      case 2:
        // Second frame: add an animation to the content layer. The root layer
        // animation has caused us to animate already during this frame.
        AddOpacityTransitionToLayer(layer_.get(), 0.1, 5, 5, false);
        break;
    }
  }

  void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) override {
    // After both animations have started, verify that they have valid
    // start times.
    if (host_impl->active_tree()->source_frame_number() < 2)
      return;
    AnimationRegistrar::AnimationControllerMap controllers_copy =
        host_impl->animation_registrar()
            ->active_animation_controllers_for_testing();
    EXPECT_EQ(2u, controllers_copy.size());
    for (auto& it : controllers_copy) {
      int id = it.first;
      if (id == host_impl->RootLayer()->id()) {
        Animation* anim = it.second->GetAnimation(Animation::TRANSFORM);
        EXPECT_GT((anim->start_time() - base::TimeTicks()).InSecondsF(), 0);
      } else if (id == host_impl->RootLayer()->children()[0]->id()) {
        Animation* anim = it.second->GetAnimation(Animation::OPACITY);
        EXPECT_GT((anim->start_time() - base::TimeTicks()).InSecondsF(), 0);
      }
      EndTest();
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<Layer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestAddAnimationAfterAnimating);

class LayerTreeHostAnimationTestNotifyAnimationFinished
    : public LayerTreeHostAnimationTest {
 public:
  LayerTreeHostAnimationTestNotifyAnimationFinished()
      : called_animation_started_(false), called_animation_finished_(false) {}

  void SetupTree() override {
    LayerTreeHostAnimationTest::SetupTree();
    picture_ = FakePictureLayer::Create(layer_settings(), &client_);
    picture_->SetBounds(gfx::Size(4, 4));
    picture_->set_layer_animation_delegate(this);
    layer_tree_host()->root_layer()->AddChild(picture_);
  }

  void BeginTest() override {
    layer_tree_host()->SetViewportSize(gfx::Size());
    PostAddLongAnimationToMainThread(picture_.get());
  }

  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {
    called_animation_started_ = true;
    layer_tree_host()->AnimateLayers(
        base::TimeTicks::FromInternalValue(std::numeric_limits<int64>::max()));
    PostSetNeedsCommitToMainThread();
  }

  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               Animation::TargetProperty target_property,
                               int group) override {
    called_animation_finished_ = true;
    EndTest();
  }

  void AfterTest() override {
    EXPECT_TRUE(called_animation_started_);
    EXPECT_TRUE(called_animation_finished_);
  }

 private:
  bool called_animation_started_;
  bool called_animation_finished_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> picture_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostAnimationTestNotifyAnimationFinished);

}  // namespace
}  // namespace cc
