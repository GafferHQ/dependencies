// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_proxy.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/trees/swap_promise_monitor.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/renderer/input/input_handler_proxy_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

using blink::WebActiveWheelFlingParameters;
using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebSize;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using testing::Field;

namespace content {
namespace {

double InSecondsF(const base::TimeTicks& time) {
  return (time - base::TimeTicks()).InSecondsF();
}

WebGestureEvent CreateFling(base::TimeTicks timestamp,
                            WebGestureDevice source_device,
                            WebFloatPoint velocity,
                            WebPoint point,
                            WebPoint global_point,
                            int modifiers) {
  WebGestureEvent fling;
  fling.type = WebInputEvent::GestureFlingStart;
  fling.sourceDevice = source_device;
  fling.timeStampSeconds = (timestamp - base::TimeTicks()).InSecondsF();
  fling.data.flingStart.velocityX = velocity.x;
  fling.data.flingStart.velocityY = velocity.y;
  fling.x = point.x;
  fling.y = point.y;
  fling.globalX = global_point.x;
  fling.globalY = global_point.y;
  fling.modifiers = modifiers;
  return fling;
}

WebGestureEvent CreateFling(WebGestureDevice source_device,
                            WebFloatPoint velocity,
                            WebPoint point,
                            WebPoint global_point,
                            int modifiers) {
  return CreateFling(base::TimeTicks(),
                     source_device,
                     velocity,
                     point,
                     global_point,
                     modifiers);
}

class MockInputHandler : public cc::InputHandler {
 public:
  MockInputHandler() {}
  ~MockInputHandler() override {}

  MOCK_METHOD0(PinchGestureBegin, void());
  MOCK_METHOD2(PinchGestureUpdate,
               void(float magnify_delta, const gfx::Point& anchor));
  MOCK_METHOD0(PinchGestureEnd, void());

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  MOCK_METHOD2(ScrollBegin,
               ScrollStatus(const gfx::Point& viewport_point,
                            cc::InputHandler::ScrollInputType type));
  MOCK_METHOD1(RootScrollBegin,
               ScrollStatus(cc::InputHandler::ScrollInputType type));
  MOCK_METHOD2(ScrollAnimated,
               ScrollStatus(const gfx::Point& viewport_point,
                            const gfx::Vector2dF& scroll_delta));
  MOCK_METHOD2(ScrollBy,
               cc::InputHandlerScrollResult(
                   const gfx::Point& viewport_point,
                   const gfx::Vector2dF& scroll_delta));
  MOCK_METHOD2(ScrollVerticallyByPage,
               bool(const gfx::Point& viewport_point,
                    cc::ScrollDirection direction));
  MOCK_METHOD0(ScrollEnd, void());
  MOCK_METHOD0(FlingScrollBegin, cc::InputHandler::ScrollStatus());

  scoped_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override {
    return scoped_ptr<cc::SwapPromiseMonitor>();
  }

  cc::ScrollElasticityHelper* CreateScrollElasticityHelper() override {
    return NULL;
  }

  void BindToClient(cc::InputHandlerClient* client) override {}

  void MouseMoveAt(const gfx::Point& mouse_position) override {}

  MOCK_METHOD2(IsCurrentlyScrollingLayerAt,
               bool(const gfx::Point& point,
                    cc::InputHandler::ScrollInputType type));

  MOCK_METHOD1(HaveWheelEventHandlersAt, bool(const gfx::Point& point));
  MOCK_METHOD1(DoTouchEventsBlockScrollAt, bool(const gfx::Point& point));

  void SetRootLayerScrollOffsetDelegate(
      cc::LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate)
      override {}

  void OnRootLayerDelegatedScrollOffsetChanged() override {}

  DISALLOW_COPY_AND_ASSIGN(MockInputHandler);
};

// A simple WebGestureCurve implementation that flings at a constant velocity
// indefinitely.
class FakeWebGestureCurve : public blink::WebGestureCurve {
 public:
  FakeWebGestureCurve(const blink::WebFloatSize& velocity,
                      const blink::WebFloatSize& cumulative_scroll)
      : velocity_(velocity), cumulative_scroll_(cumulative_scroll) {}

  virtual ~FakeWebGestureCurve() {}

  // Returns false if curve has finished and can no longer be applied.
  virtual bool apply(double time, blink::WebGestureCurveTarget* target) {
    blink::WebFloatSize displacement(velocity_.width * time,
                                     velocity_.height * time);
    blink::WebFloatSize increment(
        displacement.width - cumulative_scroll_.width,
        displacement.height - cumulative_scroll_.height);
    cumulative_scroll_ = displacement;
    // scrollBy() could delete this curve if the animation is over, so don't
    // touch any member variables after making that call.
    return target->scrollBy(increment, velocity_);
  }

 private:
  blink::WebFloatSize velocity_;
  blink::WebFloatSize cumulative_scroll_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebGestureCurve);
};

class MockInputHandlerProxyClient
    : public content::InputHandlerProxyClient {
 public:
  MockInputHandlerProxyClient() {}
  ~MockInputHandlerProxyClient() override {}

  void WillShutdown() override {}

  MOCK_METHOD1(TransferActiveWheelFlingAnimation,
               void(const WebActiveWheelFlingParameters&));

  blink::WebGestureCurve* CreateFlingAnimationCurve(
      WebGestureDevice deviceSource,
      const WebFloatPoint& velocity,
      const WebSize& cumulative_scroll) override {
    return new FakeWebGestureCurve(
        blink::WebFloatSize(velocity.x, velocity.y),
        blink::WebFloatSize(cumulative_scroll.width, cumulative_scroll.height));
  }

  MOCK_METHOD1(DidOverscroll, void(const DidOverscrollParams&));
  void DidStopFlinging() override {}
  void DidAnimateForInput() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClient);
};

class MockInputHandlerProxyClientWithDidAnimateForInput
    : public MockInputHandlerProxyClient {
 public:
  MockInputHandlerProxyClientWithDidAnimateForInput() {}
  ~MockInputHandlerProxyClientWithDidAnimateForInput() override {}

  MOCK_METHOD0(DidAnimateForInput, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClientWithDidAnimateForInput);
};

WebTouchPoint CreateWebTouchPoint(WebTouchPoint::State state, float x,
                                  float y) {
  WebTouchPoint point;
  point.state = state;
  point.screenPosition = WebFloatPoint(x, y);
  point.position = WebFloatPoint(x, y);
  return point;
}

class InputHandlerProxyTest : public testing::Test {
 public:
  InputHandlerProxyTest()
      : expected_disposition_(InputHandlerProxy::DID_HANDLE) {
    input_handler_.reset(
        new content::InputHandlerProxy(&mock_input_handler_, &mock_client_));
    scroll_result_did_scroll_.did_scroll = true;
    scroll_result_did_not_scroll_.did_scroll = false;
  }

  ~InputHandlerProxyTest() {
    input_handler_.reset();
  }

// This is defined as a macro because when an expectation is not satisfied the
// only output you get
// out of gmock is the line number that set the expectation.
#define VERIFY_AND_RESET_MOCKS()                                              \
  do {                                                                        \
    testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);          \
    testing::Mock::VerifyAndClearExpectations(&mock_client_);                 \
  } while (false)

  void StartFling(base::TimeTicks timestamp,
                  WebGestureDevice source_device,
                  WebFloatPoint velocity,
                  WebPoint position) {
    expected_disposition_ = InputHandlerProxy::DID_HANDLE;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
    gesture_.type = WebInputEvent::GestureScrollBegin;
    gesture_.sourceDevice = source_device;
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
        .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
    EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());

    gesture_ =
        CreateFling(timestamp, source_device, velocity, position, position, 0);
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();
  }

  void CancelFling(base::TimeTicks timestamp) {
    gesture_.timeStampSeconds = InSecondsF(timestamp);
    gesture_.type = WebInputEvent::GestureFlingCancel;
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();
  }

 protected:
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  scoped_ptr<content::InputHandlerProxy> input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  WebGestureEvent gesture_;
  InputHandlerProxy::EventDisposition expected_disposition_;
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
};

TEST_F(InputHandlerProxyTest, MouseWheelByPageMainThread) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  WebMouseWheelEvent wheel;
  wheel.type = WebInputEvent::MouseWheel;
  wheel.scrollByPage = true;

  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(InputHandlerProxyTest, MouseWheelWithCtrlNotScroll) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  WebMouseWheelEvent wheel;
  wheel.type = WebInputEvent::MouseWheel;
  wheel.modifiers = WebInputEvent::ControlKey;
  wheel.canScroll = false;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
}

TEST_F(InputHandlerProxyTest, GestureScrollStarted) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_,input_handler_->HandleInputEvent(gesture_));

  // The event should not be marked as handled if scrolling is not possible.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // Mark the event as handled if scroll happens.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollEnd;
  gesture_.data.scrollUpdate.deltaY = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureScrollOnMainThread) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollEnd;
  gesture_.data.scrollUpdate.deltaY = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd()).WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureScrollIgnored) {
  // We shouldn't handle the GestureScrollBegin.
  // Instead, we should get a DROP_EVENT result, indicating
  // that we could determine that there's nothing that could scroll or otherwise
  // react to this gesture sequence and thus we should drop the whole gesture
  // sequence on the floor, except for the ScrollEnd.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_IGNORED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  gesture_.type = WebInputEvent::GestureScrollEnd;
  EXPECT_CALL(mock_input_handler_, ScrollEnd()).WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureScrollBeginThatTargetViewport) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, RootScrollBegin(testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.data.scrollBegin.targetViewport = true;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GesturePinch) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchBegin;
  EXPECT_CALL(mock_input_handler_, HaveWheelEventHandlersAt(testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 1.5;
  gesture_.x = 7;
  gesture_.y = 13;
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 0.5;
  gesture_.data.pinchUpdate.zoomDisabled = true;
  gesture_.x = 9;
  gesture_.y = 6;
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(gesture_));
  gesture_.data.pinchUpdate.zoomDisabled = false;

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 0.5;
  gesture_.x = 9;
  gesture_.y = 6;
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchEnd;
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GesturePinchWithWheelHandler) {
  // We will send the synthetic wheel event to the widget.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchBegin;
  EXPECT_CALL(mock_input_handler_, HaveWheelEventHandlersAt(testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 1.5;
  gesture_.x = 7;
  gesture_.y = 13;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 0.5;
  gesture_.x = 9;
  gesture_.y = 6;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchEnd;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_F(InputHandlerProxyTest, GesturePinchAfterScrollOnMainThread) {
  // Scrolls will start by being sent to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // However, after the pinch gesture starts, they should go to the impl
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchBegin;
  EXPECT_CALL(mock_input_handler_, HaveWheelEventHandlersAt(testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 1.5;
  gesture_.x = 7;
  gesture_.y = 13;
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchUpdate;
  gesture_.data.pinchUpdate.scale = 0.5;
  gesture_.x = 9;
  gesture_.y = 6;
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GesturePinchEnd;
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After the pinch gesture ends, they should go to back to the main
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.type = WebInputEvent::GestureScrollEnd;
  gesture_.data.scrollUpdate.deltaY = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd())
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingStartedTouchpad) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.data.flingStart.velocityX = 10;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Verify that a GestureFlingCancel during an animation cancels it.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_F(InputHandlerProxyTest, GestureFlingOnMainThreadTouchpad) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // Since we returned ScrollStatusOnMainThread from scrollBegin, ensure the
  // input handler knows it's scrolling off the impl thread
  ASSERT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  // Even if we didn't start a fling ourselves, we still need to send the cancel
  // event to the widget.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_F(InputHandlerProxyTest, GestureFlingIgnoredTouchpad) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_IGNORED));

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  // Since the previous fling was ignored, we should also be dropping the next
  // fling_cancel.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchpad;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_F(InputHandlerProxyTest, GestureFlingAnimatesTouchpad) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  // Note that for trackpad, wheel events with the Control modifier are
  // special (reserved for zoom), so don't set that here.
  int modifiers = WebInputEvent::ShiftKey | WebInputEvent::AltKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchpad,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // The first animate call should let us pick up an animation start time, but
  // we shouldn't actually move anywhere just yet. The first frame after the
  // fling start will typically include the last scroll from the gesture that
  // lead to the scroll (either wheel or gesture scroll), so there should be no
  // visible hitch.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .Times(0);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second call should start scrolling in the -X direction.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Let's say on the third call we hit a non-scrollable region. We should abort
  // the fling and not scroll.
  // We also should pass the current fling parameters out to the client so the
  // rest of the fling can be
  // transferred to the main thread.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollEnd()).Times(0);
  // Expected wheel fling animation parameters:
  // *) fling_delta and fling_point should match the original GestureFlingStart
  // event
  // *) startTime should be 10 to match the time parameter of the first
  // Animate() call after the GestureFlingStart
  // *) cumulativeScroll depends on the curve, but since we've animated in the
  // -X direction the X value should be < 0
  EXPECT_CALL(
      mock_client_,
      TransferActiveWheelFlingAnimation(testing::AllOf(
          testing::Field(&WebActiveWheelFlingParameters::delta,
                         testing::Eq(fling_delta)),
          testing::Field(&WebActiveWheelFlingParameters::point,
                         testing::Eq(fling_point)),
          testing::Field(&WebActiveWheelFlingParameters::globalPoint,
                         testing::Eq(fling_global_point)),
          testing::Field(&WebActiveWheelFlingParameters::modifiers,
                         testing::Eq(modifiers)),
          testing::Field(&WebActiveWheelFlingParameters::startTime,
                         testing::Eq(10)),
          testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
                         testing::Field(&WebSize::width, testing::Gt(0))))));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  // Since we've aborted the fling, the next animation should be a no-op and
  // should not result in another
  // frame being requested.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .Times(0);
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  // Since we've transferred the fling to the main thread, we need to pass the
  // next GestureFlingCancel to the main
  // thread as well.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingTransferResetsTouchpad) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // Start a gesture fling in the -X direction with zero Y movement.
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  // Note that for trackpad, wheel events with the Control modifier are
  // special (reserved for zoom), so don't set that here.
  int modifiers = WebInputEvent::ShiftKey | WebInputEvent::AltKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchpad,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Start the fling animation at time 10. This shouldn't actually scroll, just
  // establish a start time.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .Times(0);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second call should start scrolling in the -X direction.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Let's say on the third call we hit a non-scrollable region. We should abort
  // the fling and not scroll.
  // We also should pass the current fling parameters out to the client so the
  // rest of the fling can be
  // transferred to the main thread.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollEnd()).Times(0);

  // Expected wheel fling animation parameters:
  // *) fling_delta and fling_point should match the original GestureFlingStart
  // event
  // *) startTime should be 10 to match the time parameter of the first
  // Animate() call after the GestureFlingStart
  // *) cumulativeScroll depends on the curve, but since we've animated in the
  // -X direction the X value should be < 0
  EXPECT_CALL(
      mock_client_,
      TransferActiveWheelFlingAnimation(testing::AllOf(
          testing::Field(&WebActiveWheelFlingParameters::delta,
                         testing::Eq(fling_delta)),
          testing::Field(&WebActiveWheelFlingParameters::point,
                         testing::Eq(fling_point)),
          testing::Field(&WebActiveWheelFlingParameters::globalPoint,
                         testing::Eq(fling_global_point)),
          testing::Field(&WebActiveWheelFlingParameters::modifiers,
                         testing::Eq(modifiers)),
          testing::Field(&WebActiveWheelFlingParameters::startTime,
                         testing::Eq(10)),
          testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
                         testing::Field(&WebSize::width, testing::Gt(0))))));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  // Since we've aborted the fling, the next animation should be a no-op and
  // should not result in another
  // frame being requested.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .Times(0);
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Since we've transferred the fling to the main thread, we need to pass the
  // next GestureFlingCancel to the main
  // thread as well.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
  input_handler_->MainThreadHasStoppedFlinging();

  // Start a second gesture fling, this time in the +Y direction with no X.
  fling_delta = WebFloatPoint(0, -1000);
  fling_point = WebPoint(95, 87);
  fling_global_point = WebPoint(32, 71);
  modifiers = WebInputEvent::AltKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchpad,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Start the second fling animation at time 30.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .Times(0);
  time = base::TimeTicks() + base::TimeDelta::FromSeconds(30);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Tick the second fling once normally.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Then abort the second fling.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollEnd()).Times(0);

  // We should get parameters from the second fling, nothing from the first
  // fling should "leak".
  EXPECT_CALL(
      mock_client_,
      TransferActiveWheelFlingAnimation(testing::AllOf(
          testing::Field(&WebActiveWheelFlingParameters::delta,
                         testing::Eq(fling_delta)),
          testing::Field(&WebActiveWheelFlingParameters::point,
                         testing::Eq(fling_point)),
          testing::Field(&WebActiveWheelFlingParameters::globalPoint,
                         testing::Eq(fling_global_point)),
          testing::Field(&WebActiveWheelFlingParameters::modifiers,
                         testing::Eq(modifiers)),
          testing::Field(&WebActiveWheelFlingParameters::startTime,
                         testing::Eq(30)),
          testing::Field(&WebActiveWheelFlingParameters::cumulativeScroll,
                         testing::Field(&WebSize::height, testing::Lt(0))))));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingStartedTouchscreen) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.data.flingStart.velocityX = 10;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd());

  // Verify that a GestureFlingCancel during an animation cancels it.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingOnMainThreadTouchscreen) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_ON_MAIN_THREAD));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, FlingScrollBegin()).Times(0);

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Even if we didn't start a fling ourselves, we still need to send the cancel
  // event to the widget.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_F(InputHandlerProxyTest, GestureFlingIgnoredTouchscreen) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  // Flings ignored by the InputHandler should be dropped, signalling the end
  // of the touch scroll sequence.
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_IGNORED));

  gesture_.type = WebInputEvent::GestureFlingStart;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Subsequent scrolls should behave normally, even without an intervening
  // GestureFlingCancel, as the original GestureFlingStart was dropped.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingAnimatesTouchscreen) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  // Note that for touchscreen the control modifier is not special.
  int modifiers = WebInputEvent::ControlKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // The first animate call should let us pick up an animation start time, but
  // we shouldn't actually move anywhere just yet. The first frame after the
  // fling start will typically include the last scroll from the gesture that
  // lead to the scroll (either wheel or gesture scroll), so there should be no
  // visible hitch.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second call should start scrolling in the -X direction.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingWithValidTimestamp) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey;
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // With a valid time stamp, the first animate call should skip start time
  // initialization and immediately begin scroll update production. This reduces
  // the likelihood of a hitch between the scroll preceding the fling and
  // the first scroll generated by the fling.
  // Scrolling should start in the -X direction.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += dt;
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingWithInvalidTimestamp) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta start_time_offset = base::TimeDelta::FromMilliseconds(10);
  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey;
  gesture_.timeStampSeconds = start_time_offset.InSecondsF();
  gesture_.data.flingStart.velocityX = fling_delta.x;
  gesture_.data.flingStart.velocityY = fling_delta.y;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  gesture_.x = fling_point.x;
  gesture_.y = fling_point.y;
  gesture_.globalX = fling_global_point.x;
  gesture_.globalY = fling_global_point.y;
  gesture_.modifiers = modifiers;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // Event though a time stamp was provided for the fling event, it will be
  // ignored as its too far in the past relative to the first animate call's
  // timestamp.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  base::TimeTicks time =
      base::TimeTicks() + start_time_offset + base::TimeDelta::FromSeconds(1);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Further animation ticks should update the fling as usual.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest,
       GestureScrollOnImplThreadFlagClearedAfterFling) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey | WebInputEvent::AltKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should still be true after
  // a GestureFlingStart is sent.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // The first animate call should let us pick up an animation start time, but
  // we shouldn't actually move anywhere just yet. The first frame after the
  // fling start will typically include the last scroll from the gesture that
  // lead to the scroll (either wheel or gesture scroll), so there should be no
  // visible hitch.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second call should start scrolling in the -X direction.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should be false once
  // the fling has finished (note no GestureScrollEnd has been sent).
  EXPECT_TRUE(!input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest,
       BeginScrollWhenGestureScrollOnImplThreadFlagIsSet) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey | WebInputEvent::AltKey;
  gesture_ = CreateFling(blink::WebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should still be true after
  // a GestureFlingStart is sent.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // gesture_scroll_on_impl_thread_ is still true when this scroll begins. As a
  // result, this scroll begin will cancel the previous fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingStopsAtContentEdge) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.flingStart.velocityX = fling_delta.x;
  gesture_.data.flingStart.velocityY = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The first animate doesn't cause any scrolling.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second animate starts scrolling in the positive X and Y directions.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The third animate overscrolls in the positive Y direction but scrolls
  // somewhat.
  cc::InputHandlerScrollResult overscroll;
  overscroll.did_scroll = true;
  overscroll.did_overscroll_root = true;
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(0, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(0, 10);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(testing::AllOf(
          testing::Field(
              &DidOverscrollParams::accumulated_overscroll,
              testing::Eq(overscroll.accumulated_root_overscroll)),
          testing::Field(
              &DidOverscrollParams::latest_overscroll_delta,
              testing::Eq(overscroll.unused_scroll_delta)),
          testing::Field(
              &DidOverscrollParams::current_fling_velocity,
              testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The next call to animate will no longer scroll vertically.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Eq(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyTest, GestureFlingNotCancelledBySmallTimeDelta) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey;
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  // With an animation timestamp equivalent to the starting timestamp, the
  // animation will simply be rescheduled.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // A small time delta should not stop the fling, even if the client
  // reports no scrolling.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  time += base::TimeDelta::FromMicroseconds(5);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // A time delta of zero should not stop the fling, and neither should it
  // trigger scrolling on the client.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // Lack of movement on the client, with a non-trivial scroll delta, should
  // terminate the fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(1))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_F(InputHandlerProxyTest, GestureFlingCancelledAfterBothAxesStopScrolling) {
  cc::InputHandlerScrollResult overscroll;
  overscroll.did_scroll = true;
  overscroll.did_overscroll_root = true;

  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.flingStart.velocityX = fling_delta.x;
  gesture_.data.flingStart.velocityY = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The first animate doesn't cause any scrolling.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The second animate starts scrolling in the positive X and Y directions.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The third animate hits the bottom content edge.
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(0, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(0, 100);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(testing::AllOf(
          testing::Field(
              &DidOverscrollParams::accumulated_overscroll,
              testing::Eq(overscroll.accumulated_root_overscroll)),
          testing::Field(
              &DidOverscrollParams::latest_overscroll_delta,
              testing::Eq(overscroll.unused_scroll_delta)),
          testing::Field(
              &DidOverscrollParams::current_fling_velocity,
              testing::Property(&gfx::Vector2dF::y, testing::Lt(0))))));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The next call to animate will no longer scroll vertically.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y, testing::Eq(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The next call will hit the right edge.
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(100, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(100, 0);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(testing::AllOf(
          testing::Field(
              &DidOverscrollParams::accumulated_overscroll,
              testing::Eq(overscroll.accumulated_root_overscroll)),
          testing::Field(
              &DidOverscrollParams::latest_overscroll_delta,
              testing::Eq(overscroll.unused_scroll_delta)),
          testing::Field(
              &DidOverscrollParams::current_fling_velocity,
              testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The next call to animate will no longer scroll horizontally or vertically,
  // and the fling should be cancelled.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_)).Times(0);
  time += base::TimeDelta::FromMilliseconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_F(InputHandlerProxyTest, MultiTouchPointHitTestNegative) {
  // None of the three touch points fall in the touch region. So the event
  // should be dropped.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              DoTouchEventsBlockScrollAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0))))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              DoTouchEventsBlockScrollAt(
                  testing::Property(&gfx::Point::x, testing::Lt(0))))
      .WillOnce(testing::Return(false));

  WebTouchEvent touch;
  touch.type = WebInputEvent::TouchStart;

  touch.touchesLength = 3;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::StateStationary, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::StatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::StatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, MultiTouchPointHitTestPositive) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              DoTouchEventsBlockScrollAt(
                  testing::Property(&gfx::Point::x, testing::Eq(0))))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              DoTouchEventsBlockScrollAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0))))
      .WillOnce(testing::Return(true));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch;
  touch.type = WebInputEvent::TouchStart;

  touch.touchesLength = 3;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::StatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::StatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::StatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, GestureFlingCancelledByKeyboardEvent) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Keyboard events received during a scroll should have no effect.
  WebKeyboardEvent key_event;
  key_event.type = WebInputEvent::KeyDown;
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(key_event));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // On the fling start, animation should be scheduled, but no scrolling occurs.
  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.flingStart.velocityX = fling_delta.x;
  gesture_.data.flingStart.velocityY = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Keyboard events received during a fling should cancel the active fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(key_event));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The call to animate should have no effect, as the fling was cancelled.
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // A fling cancel should be dropped, as there is nothing to cancel.
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_F(InputHandlerProxyTest, GestureFlingWithNegativeTimeDelta) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  gesture_.type = WebInputEvent::GestureScrollBegin;
  gesture_.sourceDevice = blink::WebGestureDeviceTouchscreen;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebPoint fling_point = WebPoint(7, 13);
  WebPoint fling_global_point = WebPoint(17, 23);
  int modifiers = WebInputEvent::ControlKey;
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_global_point,
                         modifiers);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // If we get a negative time delta, that is, the Animation tick time happens
  // before the fling's start time then we should *not* try scrolling and
  // instead reset the fling start time.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::_)).Times(0);
  time -= base::TimeDelta::FromMilliseconds(5);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // The first call should have reset the start time so subsequent calls should
  // generate scroll events.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  input_handler_->Animate(time + base::TimeDelta::FromMilliseconds(1));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, FlingBoost) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  base::TimeTicks last_animate_time = time;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Now cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if it hits the same
  // scrolling layer.
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Animate calls within the deferred cancellation window should continue.
  time += dt;
  float expected_delta =
      (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureScrollUpdates in the same direction and at sufficient speed should
  // be swallowed by the fling.
  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaX = fling_delta.x;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Animate calls within the deferred cancellation window should continue.
  time += dt;
  expected_delta = (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureFlingStart in the same direction and at sufficient speed should
  // boost the active fling.

  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_point,
                         0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  time += dt;
  // Note we get *2x* as much delta because 2 flings have combined.
  expected_delta = 2 * (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // Repeated GestureFlingStarts should accumulate.

  CancelFling(time);
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         fling_delta,
                         fling_point,
                         fling_point,
                         0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  time += dt;
  // Note we get *3x* as much delta because 3 flings have combined.
  expected_delta = 3 * (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureFlingCancel should terminate the fling if no boosting gestures are
  // received within the timeout window.

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureFlingCancel;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  time += base::TimeDelta::FromMilliseconds(100);
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfScrollTargetsDifferentLayer) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // If the GestureScrollBegin targets a different layer, the fling should be
  // cancelled and the scroll should be handled as usual.
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfScrollDelayed) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if it hits the same
  // scrolling layer.
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // If no GestureScrollUpdate or GestureFlingStart is received within the
  // timeout window, the fling should be cancelled and scrolling should resume.
  time += base::TimeDelta::FromMilliseconds(100);
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfNotAnimated) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Animate fling once.
  time += dt;
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  input_handler_->Animate(time);

  // Cancel the fling after long delay of no animate. The fling cancellation
  // should be deferred to allow fling boosting events to arrive.
  time += base::TimeDelta::FromMilliseconds(100);
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if it hits the same
  // scrolling layer.
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Should exit scroll bosting on GestureScrollUpdate due to long delay
  // since last animate. Cancel old fling and start new scroll.
  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaY = -40;
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_, testing::_))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfFlingInDifferentDirection) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // If the new fling is orthogonal to the existing fling, no boosting should
  // take place, with the new fling replacing the old.
  WebFloatPoint orthogonal_fling_delta =
      WebFloatPoint(fling_delta.y, -fling_delta.x);
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         orthogonal_fling_delta,
                         fling_point,
                         fling_point,
                         0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Note that the new fling delta uses the orthogonal, unboosted fling
  // velocity.
  time += dt;
  float expected_delta = dt.InSecondsF() * -orthogonal_fling_delta.y;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::y,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfScrollInDifferentDirection) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if it hits the same
  // scrolling layer.
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // If the GestureScrollUpdate is in a different direction than the fling,
  // the fling should be cancelled and scrolling should resume.
  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaX = -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(fling_delta.x))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfFlingTooSlow) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // If the new fling is too slow, no boosting should take place, with the new
  // fling replacing the old.
  WebFloatPoint small_fling_delta = WebFloatPoint(100, 0);
  gesture_ = CreateFling(time,
                         blink::WebGestureDeviceTouchscreen,
                         small_fling_delta,
                         fling_point,
                         fling_point,
                         0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Note that the new fling delta uses the *slow*, unboosted fling velocity.
  time += dt;
  float expected_delta = dt.InSecondsF() * -small_fling_delta.x;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, NoFlingBoostIfPreventBoostingFlagIsSet) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);

  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  EXPECT_CALL(mock_input_handler_, ScrollEnd());

  // Cancel the fling. The fling cancellation should not be deferred because of
  // prevent boosting flag set.
  gesture_.data.flingCancel.preventBoosting = true;
  time += dt;
  CancelFling(time);

  // VERIFY_AND_RESET_MOCKS already called by CancelFling
}

TEST_F(InputHandlerProxyTest, FlingBoostTerminatedDuringScrollSequence) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  base::TimeTicks last_animate_time = time;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebPoint fling_point = WebPoint(7, 13);
  StartFling(
      time, blink::WebGestureDeviceTouchscreen, fling_delta, fling_point);

  // Now cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling.
  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollBegin;
  EXPECT_CALL(mock_input_handler_,
              IsCurrentlyScrollingLayerAt(testing::_, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Now animate the fling to completion (in this case, the fling should
  // terminate because the input handler reports a failed scroll). As the fling
  // was cancelled during an active scroll sequence, a synthetic
  // GestureScrollBegin should be processed, resuming the scroll.
  time += dt;
  float expected_delta =
      (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  input_handler_->Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // Subsequent GestureScrollUpdates after the cancelled, boosted fling should
  // cause scrolling as usual.
  time += dt;
  expected_delta = 7.3f;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollUpdate;
  gesture_.data.scrollUpdate.deltaX = -expected_delta;
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::_,
                       testing::Property(&gfx::Vector2dF::x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GestureScrollEnd should terminate the resumed scroll properly.
  time += dt;
  gesture_.timeStampSeconds = InSecondsF(time);
  gesture_.type = WebInputEvent::GestureScrollEnd;
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_F(InputHandlerProxyTest, DidReceiveInputEvent_ForFling) {
  testing::StrictMock<MockInputHandlerProxyClientWithDidAnimateForInput>
      mock_client;
  input_handler_.reset(
        new content::InputHandlerProxy(&mock_input_handler_, &mock_client));

  gesture_.type = WebInputEvent::GestureFlingStart;
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.flingStart.velocityX = fling_delta.x;
  gesture_.data.flingStart.velocityY = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(cc::InputHandler::SCROLL_STARTED));
  EXPECT_CALL(mock_input_handler_, ScrollEnd());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
      input_handler_->HandleInputEvent(gesture_));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());
  EXPECT_CALL(mock_client, DidAnimateForInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  input_handler_->Animate(time);

  testing::Mock::VerifyAndClearExpectations(&mock_client);
}

} // namespace
} // namespace content
