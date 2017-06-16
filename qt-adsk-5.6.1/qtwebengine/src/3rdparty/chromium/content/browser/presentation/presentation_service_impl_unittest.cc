// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/common/presentation_constants.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

namespace {

// Matches mojo structs.
MATCHER_P(Equals, expected, "") {
  return expected.Equals(arg);
}

const char kPresentationId[] = "presentationId";
const char kPresentationUrl[] = "http://foo.com/index.html";

bool ArePresentationSessionsEqual(
    const presentation::PresentationSessionInfo& expected,
    const presentation::PresentationSessionInfo& actual) {
  return expected.url == actual.url && expected.id == actual.id;
}

bool ArePresentationSessionMessagesEqual(
    const presentation::SessionMessage* expected,
    const presentation::SessionMessage* actual) {
  return expected->presentation_url == actual->presentation_url &&
         expected->presentation_id == actual->presentation_id &&
         expected->type == actual->type &&
         expected->message == actual->message &&
         expected->data.Equals(actual->data);
}

void DoNothing(
    presentation::PresentationSessionInfoPtr info,
    presentation::PresentationErrorPtr error) {
}

}  // namespace

class MockPresentationServiceDelegate : public PresentationServiceDelegate {
 public:
  MOCK_METHOD3(AddObserver,
      void(int render_process_id,
           int render_frame_id,
           PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
      void(int render_process_id, int render_frame_id));
  MOCK_METHOD3(AddScreenAvailabilityListener,
      bool(
          int render_process_id,
          int routing_id,
          PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD3(RemoveScreenAvailabilityListener,
      void(
          int render_process_id,
          int routing_id,
          PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(Reset,
      void(
          int render_process_id,
          int routing_id));
  MOCK_METHOD4(SetDefaultPresentationUrl,
      void(
          int render_process_id,
          int routing_id,
          const std::string& default_presentation_url,
          const std::string& default_presentation_id));
  MOCK_METHOD5(StartSession,
      void(
          int render_process_id,
          int render_frame_id,
          const std::string& presentation_url,
          const PresentationSessionSuccessCallback& success_cb,
          const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD6(JoinSession,
      void(
          int render_process_id,
          int render_frame_id,
          const std::string& presentation_url,
          const std::string& presentation_id,
          const PresentationSessionSuccessCallback& success_cb,
          const PresentationSessionErrorCallback& error_cb));
  MOCK_METHOD3(CloseSession,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(ListenForSessionMessages,
      void(
          int render_process_id,
          int render_frame_id,
          const PresentationSessionMessageCallback& message_cb));
  MOCK_METHOD4(SendMessageRawPtr,
      void(
          int render_process_id,
          int render_frame_id,
          PresentationSessionMessage* message_request,
          const SendMessageCallback& send_message_cb));
  void SendMessage(int render_process_id,
                   int render_frame_id,
                   scoped_ptr<PresentationSessionMessage> message_request,
                   const SendMessageCallback& send_message_cb) override {
    SendMessageRawPtr(
        render_process_id,
        render_frame_id,
        message_request.release(),
        send_message_cb);
  }
  MOCK_METHOD3(
      ListenForSessionStateChange,
      void(int render_process_id,
           int render_frame_id,
           const content::SessionStateChangedCallback& state_changed_cb));
};

class MockPresentationServiceClient :
    public presentation::PresentationServiceClient {
 public:
  MOCK_METHOD1(OnScreenAvailabilityUpdated, void(bool available));
  void OnSessionStateChanged(
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationSessionState new_state) override {
    OnSessionStateChanged(*session_info, new_state);
  }
  MOCK_METHOD2(OnSessionStateChanged,
               void(const presentation::PresentationSessionInfo& session_info,
                    presentation::PresentationSessionState new_state));
  void OnScreenAvailabilityNotSupported() override {
    NOTIMPLEMENTED();
  }
};

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest() : default_session_started_count_(0) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    auto request = mojo::GetProxy(&service_ptr_);
    EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
    service_impl_.reset(new PresentationServiceImpl(
        contents()->GetMainFrame(), contents(), &mock_delegate_));
    service_impl_->Bind(request.Pass());

    presentation::PresentationServiceClientPtr client_ptr;
    client_binding_.reset(
        new mojo::Binding<presentation::PresentationServiceClient>(
            &mock_client_, mojo::GetProxy(&client_ptr)));
    service_impl_->SetClient(client_ptr.Pass());
  }

  void TearDown() override {
    service_ptr_.reset();
    if (service_impl_.get()) {
      EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
      service_impl_.reset();
    }
    RenderViewHostImplTestHarness::TearDown();
  }

  void ListenForScreenAvailabilityAndWait(bool delegate_success) {
    base::RunLoop run_loop;
    // This will call to |service_impl_| via mojo. Process the message
    // using RunLoop.
    // The callback shouldn't be invoked since there is no availability
    // result yet.
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener(_, _, _))
        .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            Return(delegate_success)));
    service_ptr_->ListenForScreenAvailability();
    run_loop.Run();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void RunLoopFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void SaveQuitClosureAndRunLoop() {
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    run_loop_quit_closure_.Reset();
  }

  void SimulateScreenAvailabilityChangeAndWait(bool available) {
    auto* listener = service_impl_->screen_availability_listener_.get();
    ASSERT_TRUE(listener);

    base::RunLoop run_loop;
    EXPECT_CALL(mock_client_, OnScreenAvailabilityUpdated(available))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    listener->OnScreenAvailabilityChanged(available);
    run_loop.Run();
  }

  void ExpectReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _)).Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->default_presentation_url_.empty());
    EXPECT_TRUE(service_impl_->default_presentation_id_.empty());
    EXPECT_FALSE(service_impl_->screen_availability_listener_.get());
    EXPECT_FALSE(service_impl_->default_session_start_context_.get());
    EXPECT_FALSE(service_impl_->on_session_messages_callback_.get());
  }

  void ExpectNewSessionMojoCallbackSuccess(
      presentation::PresentationSessionInfoPtr info,
      presentation::PresentationErrorPtr error) {
    EXPECT_FALSE(info.is_null());
    EXPECT_TRUE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectNewSessionMojoCallbackError(
      presentation::PresentationSessionInfoPtr info,
      presentation::PresentationErrorPtr error) {
    EXPECT_TRUE(info.is_null());
    EXPECT_FALSE(error.is_null());
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectDefaultSessionStarted(
      const presentation::PresentationSessionInfo& expected_session,
      presentation::PresentationSessionInfoPtr actual_session) {
    ASSERT_TRUE(!actual_session.is_null());
    EXPECT_TRUE(ArePresentationSessionsEqual(
        expected_session, *actual_session));
    ++default_session_started_count_;
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectDefaultSessionNull(
      presentation::PresentationSessionInfoPtr actual_session) {
    EXPECT_TRUE(actual_session.is_null());
    ++default_session_started_count_;
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectSessionMessages(
      mojo::Array<presentation::SessionMessagePtr> actual_msgs) {
    EXPECT_TRUE(actual_msgs.size() == expected_msgs_.size());
    for (size_t i = 0; i < actual_msgs.size(); ++i) {
      EXPECT_TRUE(ArePresentationSessionMessagesEqual(expected_msgs_[i].get(),
                                                      actual_msgs[i].get()));
    }
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void ExpectSendMessageMojoCallback(bool success) {
    EXPECT_TRUE(success);
    EXPECT_FALSE(service_impl_->send_message_callback_);
    if (!run_loop_quit_closure_.is_null())
      run_loop_quit_closure_.Run();
  }

  void RunListenForSessionMessages(std::string& text_msg,
                                   std::vector<uint8_t>& binary_data) {


    expected_msgs_ = mojo::Array<presentation::SessionMessagePtr>::New(2);
    expected_msgs_[0] = presentation::SessionMessage::New();
    expected_msgs_[0]->presentation_url = kPresentationUrl;
    expected_msgs_[0]->presentation_id = kPresentationId;
    expected_msgs_[0]->type =
        presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_TEXT;
    expected_msgs_[0]->message = text_msg;
    expected_msgs_[1] = presentation::SessionMessage::New();
    expected_msgs_[1]->presentation_url = kPresentationUrl;
    expected_msgs_[1]->presentation_id = kPresentationId;
    expected_msgs_[1]->type = presentation::PresentationMessageType::
        PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER;
    expected_msgs_[1]->data = mojo::Array<uint8_t>::From(binary_data);

    service_ptr_->ListenForSessionMessages(
        base::Bind(&PresentationServiceImplTest::ExpectSessionMessages,
                   base::Unretained(this)));

    base::RunLoop run_loop;
    base::Callback<void(scoped_ptr<ScopedVector<PresentationSessionMessage>>)>
        message_cb;
    EXPECT_CALL(mock_delegate_, ListenForSessionMessages(_, _, _))
        .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                        SaveArg<2>(&message_cb)));
    run_loop.Run();

    scoped_ptr<ScopedVector<PresentationSessionMessage>> messages(
        new ScopedVector<PresentationSessionMessage>());
    messages->push_back(
        content::PresentationSessionMessage::CreateStringMessage(
            kPresentationUrl, kPresentationId,
            scoped_ptr<std::string>(new std::string(text_msg))));
    messages->push_back(
        content::PresentationSessionMessage::CreateArrayBufferMessage(
            kPresentationUrl, kPresentationId,
            scoped_ptr<std::vector<uint8_t>>(
                new std::vector<uint8_t>(binary_data))));
    message_cb.Run(messages.Pass());
    SaveQuitClosureAndRunLoop();
  }

  MockPresentationServiceDelegate mock_delegate_;

  scoped_ptr<PresentationServiceImpl> service_impl_;
  mojo::InterfacePtr<presentation::PresentationService> service_ptr_;

  MockPresentationServiceClient mock_client_;
  scoped_ptr<mojo::Binding<presentation::PresentationServiceClient>>
      client_binding_;

  base::Closure run_loop_quit_closure_;
  int default_session_started_count_;
  mojo::Array<presentation::SessionMessagePtr> expected_msgs_;
};

TEST_F(PresentationServiceImplTest, ListenForScreenAvailability) {
  ListenForScreenAvailabilityAndWait(true);

  SimulateScreenAvailabilityChangeAndWait(true);
  SimulateScreenAvailabilityChangeAndWait(false);
  SimulateScreenAvailabilityChangeAndWait(true);
}

TEST_F(PresentationServiceImplTest, Reset) {
  ListenForScreenAvailabilityAndWait(true);

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  ListenForScreenAvailabilityAndWait(true);

  ExpectReset();
  service_impl_->DidNavigateAnyFrame(
      contents()->GetMainFrame(),
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateOtherFrame) {
  ListenForScreenAvailabilityAndWait(true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->DidNavigateAnyFrame(
      nullptr,
      content::LoadCommittedDetails(),
      content::FrameNavigateParams());

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChangeAndWait(true);
}

TEST_F(PresentationServiceImplTest, ThisRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(true);

  ExpectReset();

  // Since the frame matched the service, |service_impl_| will be deleted.
  PresentationServiceImpl* service = service_impl_.release();
  EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
  service->RenderFrameDeleted(contents()->GetMainFrame());
}

TEST_F(PresentationServiceImplTest, OtherRenderFrameDeleted) {
  ListenForScreenAvailabilityAndWait(true);

  // TODO(imcheng): How to get a different RenderFrameHost?
  service_impl_->RenderFrameDeleted(nullptr);

  // Availability is reported and callback should be invoked since listener
  // has not been deleted.
  SimulateScreenAvailabilityChangeAndWait(true);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  ListenForScreenAvailabilityAndWait(false);
  ASSERT_FALSE(service_impl_->screen_availability_listener_.get());
}

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrl) {
  std::string url1("http://fooUrl");
  std::string dpu_id("dpuId");
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url1), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url1, dpu_id);
  EXPECT_EQ(url1, service_impl_->default_presentation_url_);

  // Now there should be a listener for DPU = |url1|.
  ListenForScreenAvailabilityAndWait(true);
  auto* listener = service_impl_->screen_availability_listener_.get();
  ASSERT_TRUE(listener);
  EXPECT_EQ(url1, listener->GetPresentationUrl());

  std::string url2("http://barUrl");
  // Sets different DPU.
  // Adds listener for url2 and removes listener for url1.
  EXPECT_CALL(
      mock_delegate_,
      AddScreenAvailabilityListener(_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      mock_delegate_,
      RemoveScreenAvailabilityListener(_, _, _))
      .Times(1);
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(url2), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(url2, dpu_id);
  EXPECT_EQ(url2, service_impl_->default_presentation_url_);

  listener = service_impl_->screen_availability_listener_.get();
  ASSERT_TRUE(listener);
  EXPECT_EQ(url2, listener->GetPresentationUrl());
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrl) {
  std::string dpu_id("dpuId");
  EXPECT_CALL(mock_delegate_,
      SetDefaultPresentationUrl(_, _, Eq(kPresentationUrl), Eq(dpu_id)))
      .Times(1);
  service_impl_->SetDefaultPresentationURL(kPresentationUrl, dpu_id);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(kPresentationUrl, service_impl_->default_presentation_url_);

  // Same URL as before; no-ops.
  service_impl_->SetDefaultPresentationURL(kPresentationUrl, dpu_id);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  EXPECT_EQ(kPresentationUrl, service_impl_->default_presentation_url_);
}

TEST_F(PresentationServiceImplTest, StartSessionSuccess) {
  service_ptr_->StartSession(
      kPresentationUrl,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, Eq(kPresentationUrl), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<3>(&success_cb)));
  run_loop.Run();
  success_cb.Run(PresentationSessionInfo(kPresentationUrl, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionError) {
  service_ptr_->StartSession(
      kPresentationUrl,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, StartSession(_, _, Eq(kPresentationUrl), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionSuccess) {
  service_ptr_->JoinSession(
      kPresentationUrl,
      kPresentationId,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackSuccess,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationSessionInfo&)> success_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(kPresentationUrl), Eq(kPresentationId), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<4>(&success_cb)));
  run_loop.Run();
  success_cb.Run(PresentationSessionInfo(kPresentationUrl, kPresentationId));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, JoinSessionError) {
  service_ptr_->JoinSession(
      kPresentationUrl,
      kPresentationId,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
          base::Unretained(this)));
  base::RunLoop run_loop;
  base::Callback<void(const PresentationError&)> error_cb;
  EXPECT_CALL(mock_delegate_, JoinSession(
      _, _, Eq(kPresentationUrl), Eq(kPresentationId), _, _))
      .WillOnce(DoAll(
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
            SaveArg<5>(&error_cb)));
  run_loop.Run();
  error_cb.Run(PresentationError(PRESENTATION_ERROR_UNKNOWN, "Error message"));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, CloseSession) {
  service_ptr_->CloseSession(kPresentationUrl, kPresentationId);
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, CloseSession(_, _, Eq(kPresentationId)))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();
}

TEST_F(PresentationServiceImplTest, ListenForSessionMessages) {
  std::string text_msg("123");
  std::vector<uint8_t> binary_data(3, '\1');
  RunListenForSessionMessages(text_msg, binary_data);
}

TEST_F(PresentationServiceImplTest, ListenForSessionMessagesWithEmptyMsg) {
  std::string text_msg("");
  std::vector<uint8_t> binary_data{};
  RunListenForSessionMessages(text_msg, binary_data);
}

TEST_F(PresentationServiceImplTest, ReceiveSessionMessagesAfterReset) {
  std::string text_msg("123");
  expected_msgs_ = mojo::Array<presentation::SessionMessagePtr>();
  service_ptr_->ListenForSessionMessages(
      base::Bind(&PresentationServiceImplTest::ExpectSessionMessages,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(scoped_ptr<ScopedVector<PresentationSessionMessage>>)>
      message_cb;
  EXPECT_CALL(mock_delegate_, ListenForSessionMessages(_, _, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
                      SaveArg<2>(&message_cb)));
  run_loop.Run();

  scoped_ptr<ScopedVector<PresentationSessionMessage>> messages(
      new ScopedVector<PresentationSessionMessage>());
  messages->push_back(content::PresentationSessionMessage::CreateStringMessage(
      kPresentationUrl, kPresentationId,
      scoped_ptr<std::string>(new std::string(text_msg))));
  ExpectReset();
  service_impl_->Reset();
  message_cb.Run(messages.Pass());
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, StartSessionInProgress) {
  std::string presentation_url1("http://fooUrl");
  std::string presentation_url2("http://barUrl");
  service_ptr_->StartSession(presentation_url1,
                             base::Bind(&DoNothing));
  // This request should fail immediately, since there is already a StartSession
  // in progress.
  service_ptr_->StartSession(
      presentation_url2,
      base::Bind(
          &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
          base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ListenForDefaultSessionStart) {
  presentation::PresentationSessionInfo expected_session;
  expected_session.url = kPresentationUrl;
  expected_session.id = kPresentationId;
  service_ptr_->ListenForDefaultSessionStart(
      base::Bind(&PresentationServiceImplTest::ExpectDefaultSessionStarted,
                 base::Unretained(this),
                 expected_session));
  RunLoopFor(base::TimeDelta::FromMilliseconds(50));
  service_impl_->OnDefaultPresentationStarted(
      content::PresentationSessionInfo(kPresentationUrl, kPresentationId));
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(1, default_session_started_count_);
}

TEST_F(PresentationServiceImplTest, ListenForDefaultSessionStartAfterSet) {
  // Note that the callback will only pick up presentation_url2/id2 since
  // ListenForDefaultSessionStart wasn't called yet when the DPU was still
  // presentation_url1.
  std::string presentation_url1("http://fooUrl1");
  std::string presentation_id1("presentationId1");
  std::string presentation_url2("http://fooUrl2");
  std::string presentation_id2("presentationId2");
  service_impl_->OnDefaultPresentationStarted(
      content::PresentationSessionInfo(presentation_url1, presentation_id1));

  presentation::PresentationSessionInfo expected_session;
  expected_session.url = presentation_url2;
  expected_session.id = presentation_id2;
  service_ptr_->ListenForDefaultSessionStart(
      base::Bind(&PresentationServiceImplTest::ExpectDefaultSessionStarted,
                 base::Unretained(this),
                 expected_session));
  RunLoopFor(base::TimeDelta::FromMilliseconds(50));
  service_impl_->OnDefaultPresentationStarted(
      content::PresentationSessionInfo(presentation_url2, presentation_id2));
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(1, default_session_started_count_);
}

TEST_F(PresentationServiceImplTest, DefaultSessionStartReset) {
  service_ptr_->ListenForDefaultSessionStart(
      base::Bind(&PresentationServiceImplTest::ExpectDefaultSessionNull,
                 base::Unretained(this)));
  RunLoopFor(TestTimeouts::tiny_timeout());

  ExpectReset();
  service_impl_->Reset();
  ExpectCleanState();
  SaveQuitClosureAndRunLoop();
  EXPECT_EQ(1, default_session_started_count_);
}

TEST_F(PresentationServiceImplTest, SendStringMessage) {
  std::string message("Test presentation session message");

  presentation::SessionMessagePtr message_request(
      presentation::SessionMessage::New());
  message_request->presentation_url = kPresentationUrl;
  message_request->presentation_id = kPresentationId;
  message_request->type = presentation::PresentationMessageType::
                          PRESENTATION_MESSAGE_TYPE_TEXT;
  message_request->message = message;
  service_ptr_->SendSessionMessage(
      message_request.Pass(),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendMessageMojoCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(
      _, _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
          SaveArg<2>(&test_message),
          SaveArg<3>(&send_message_cb)));
  run_loop.Run();

  EXPECT_TRUE(test_message);
  EXPECT_EQ(kPresentationUrl, test_message->presentation_url);
  EXPECT_EQ(kPresentationId, test_message->presentation_id);
  EXPECT_FALSE(test_message->is_binary());
  EXPECT_TRUE(test_message->message.get()->size() <=
              kMaxPresentationSessionMessageSize);
  EXPECT_EQ(message, *(test_message->message.get()));
  EXPECT_FALSE(test_message->data);
  delete test_message;
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBuffer) {
  // Test Array buffer data.
  const uint8 buffer[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
  std::vector<uint8> data;
  data.assign(buffer, buffer + sizeof(buffer));

  presentation::SessionMessagePtr message_request(
      presentation::SessionMessage::New());
  message_request->presentation_url = kPresentationUrl;
  message_request->presentation_id = kPresentationId;
  message_request->type = presentation::PresentationMessageType::
                          PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER;
  message_request->data = mojo::Array<uint8>::From(data);
  service_ptr_->SendSessionMessage(
      message_request.Pass(),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendMessageMojoCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(
      _, _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
          SaveArg<2>(&test_message),
          SaveArg<3>(&send_message_cb)));
  run_loop.Run();

  EXPECT_TRUE(test_message);
  EXPECT_EQ(kPresentationUrl, test_message->presentation_url);
  EXPECT_EQ(kPresentationId, test_message->presentation_id);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::ARRAY_BUFFER, test_message->type);
  EXPECT_FALSE(test_message->message);
  EXPECT_EQ(data.size(), test_message->data.get()->size());
  EXPECT_TRUE(test_message->data.get()->size() <=
              kMaxPresentationSessionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data.get())[0], sizeof(buffer)));
  delete test_message;
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendArrayBufferWithExceedingLimit) {
  // Create buffer with size exceeding the limit.
  // Use same size as in content::kMaxPresentationSessionMessageSize.
  const size_t kMaxBufferSizeInBytes = 64 * 1024; // 64 KB.
  uint8 buffer[kMaxBufferSizeInBytes+1];
  memset(buffer, 0, kMaxBufferSizeInBytes+1);
  std::vector<uint8> data;
  data.assign(buffer, buffer + sizeof(buffer));

  presentation::SessionMessagePtr message_request(
      presentation::SessionMessage::New());
  message_request->presentation_url = kPresentationUrl;
  message_request->presentation_id = kPresentationId;
  message_request->type = presentation::PresentationMessageType::
                          PRESENTATION_MESSAGE_TYPE_ARRAY_BUFFER;
  message_request->data = mojo::Array<uint8>::From(data);
  service_ptr_->SendSessionMessage(
      message_request.Pass(),
      base::Bind(
          &PresentationServiceImplTest::ExpectSendMessageMojoCallback,
          base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(
      _, _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
          SaveArg<2>(&test_message),
          SaveArg<3>(&send_message_cb)));
  run_loop.Run();

  EXPECT_FALSE(test_message);
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, SendBlobData) {
  const uint8 buffer[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::vector<uint8> data;
  data.assign(buffer, buffer + sizeof(buffer));

  presentation::SessionMessagePtr message_request(
      presentation::SessionMessage::New());
  message_request->presentation_url = kPresentationUrl;
  message_request->presentation_id = kPresentationId;
  message_request->type =
      presentation::PresentationMessageType::PRESENTATION_MESSAGE_TYPE_BLOB;
  message_request->data = mojo::Array<uint8>::From(data);
  service_ptr_->SendSessionMessage(
      message_request.Pass(),
      base::Bind(&PresentationServiceImplTest::ExpectSendMessageMojoCallback,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  base::Callback<void(bool)> send_message_cb;
  PresentationSessionMessage* test_message = nullptr;
  EXPECT_CALL(mock_delegate_, SendMessageRawPtr(_, _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
          SaveArg<2>(&test_message),
          SaveArg<3>(&send_message_cb)));
  run_loop.Run();

  EXPECT_TRUE(test_message);
  EXPECT_EQ(kPresentationUrl, test_message->presentation_url);
  EXPECT_EQ(kPresentationId, test_message->presentation_id);
  EXPECT_TRUE(test_message->is_binary());
  EXPECT_EQ(PresentationMessageType::BLOB, test_message->type);
  EXPECT_FALSE(test_message->message);
  EXPECT_EQ(data.size(), test_message->data.get()->size());
  EXPECT_TRUE(test_message->data.get()->size() <=
              kMaxPresentationSessionMessageSize);
  EXPECT_EQ(0, memcmp(buffer, &(*test_message->data.get())[0], sizeof(buffer)));
  delete test_message;
  send_message_cb.Run(true);
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, MaxPendingJoinSessionRequests) {
  const char* presentation_url = "http://fooUrl%d";
  const char* presentation_id = "presentationId%d";
  int num_requests = PresentationServiceImpl::kMaxNumQueuedSessionRequests;
  int i = 0;
  EXPECT_CALL(mock_delegate_, JoinSession(_, _, _, _, _, _))
      .Times(num_requests);
  for (; i < num_requests; ++i) {
    service_ptr_->JoinSession(
        base::StringPrintf(presentation_url, i),
        base::StringPrintf(presentation_id, i),
        base::Bind(&DoNothing));
  }

  // Exceeded maximum queue size, should invoke mojo callback with error.
  service_ptr_->JoinSession(
        base::StringPrintf(presentation_url, i),
        base::StringPrintf(presentation_id, i),
        base::Bind(
            &PresentationServiceImplTest::ExpectNewSessionMojoCallbackError,
            base::Unretained(this)));
  SaveQuitClosureAndRunLoop();
}

TEST_F(PresentationServiceImplTest, ListenForSessionStateChange) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, ListenForSessionStateChange(_, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_ptr_->ListenForSessionStateChange();
  run_loop.Run();

  presentation::PresentationSessionInfo session_info;
  session_info.url = kPresentationUrl;
  session_info.id = kPresentationId;

  EXPECT_CALL(mock_client_,
              OnSessionStateChanged(
                  Equals(session_info),
                  presentation::PRESENTATION_SESSION_STATE_CONNECTED));
  service_impl_->OnSessionStateChanged(
      content::PresentationSessionInfo(kPresentationUrl, kPresentationId),
      content::PRESENTATION_SESSION_STATE_CONNECTED);
}

}  // namespace content
