// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_test_base.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace test {

namespace {
// Share the application URL with multiple application tests.
String g_url;

// Application request handle passed from the shell in MojoMain, stored in
// between SetUp()/TearDown() so we can (re-)intialize new ApplicationImpls.
InterfaceRequest<Application> g_application_request;

// Shell pointer passed in the initial mojo.Application.Initialize() call,
// stored in between initial setup and the first test and between SetUp/TearDown
// calls so we can (re-)initialize new ApplicationImpls.
ShellPtr g_shell;

class ShellGrabber : public Application {
 public:
  explicit ShellGrabber(InterfaceRequest<Application> application_request)
      : binding_(this, application_request.Pass()) {}

  void WaitForInitialize() {
    // Initialize is always the first call made on Application.
    MOJO_CHECK(binding_.WaitForIncomingMethodCall());
  }

 private:
  // Application implementation.
  void Initialize(ShellPtr shell, const mojo::String& url) override {
    g_url = url;
    g_application_request = binding_.Unbind();
    g_shell = shell.Pass();
  }

  void AcceptConnection(const String& requestor_url,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        const String& url) override {
    MOJO_CHECK(false);
  }

  void OnQuitRequested(const Callback<void(bool)>& callback) override {
    MOJO_CHECK(false);
  }

  Binding<Application> binding_;
};

}  // namespace

MojoResult RunAllTests(MojoHandle application_request_handle) {
  {
    // This loop is used for init, and then destroyed before running tests.
    Environment::InstantiateDefaultRunLoop();

    // Grab the shell handle.
    ShellGrabber grabber(
        MakeRequest<Application>(MakeScopedHandle(
            MessagePipeHandle(application_request_handle))));
    grabber.WaitForInitialize();
    MOJO_CHECK(g_shell);
    MOJO_CHECK(g_application_request.is_pending());

    int argc = 0;
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    const char** argv = new const char* [cmd_line->argv().size() + 1];
#if defined(OS_WIN)
    std::vector<std::string> local_strings;
#endif
    for (auto& arg : cmd_line->argv()) {
#if defined(OS_WIN)
      local_strings.push_back(base::WideToUTF8(arg));
      argv[argc++] = local_strings.back().c_str();
#else
      argv[argc++] = arg.c_str();
#endif
    }
    argv[argc] = nullptr;

    testing::InitGoogleTest(&argc, const_cast<char**>(&(argv[0])));

    Environment::DestroyDefaultRunLoop();
  }

  int result = RUN_ALL_TESTS();

  // Shut down our message pipes before exiting.
  (void)g_application_request.PassMessagePipe();
  (void)g_shell.PassInterface();

  return (result == 0) ? MOJO_RESULT_OK : MOJO_RESULT_UNKNOWN;
}

ApplicationTestBase::ApplicationTestBase() : application_impl_(nullptr) {
}

ApplicationTestBase::~ApplicationTestBase() {
}

ApplicationDelegate* ApplicationTestBase::GetApplicationDelegate() {
  return &default_application_delegate_;
}

void ApplicationTestBase::SetUp() {
  // A run loop is recommended for ApplicationImpl initialization and
  // communication.
  if (ShouldCreateDefaultRunLoop())
    Environment::InstantiateDefaultRunLoop();

  MOJO_CHECK(g_application_request.is_pending());
  MOJO_CHECK(g_shell);

  // New applications are constructed for each test to avoid persisting state.
  application_impl_ = new ApplicationImpl(GetApplicationDelegate(),
                                          g_application_request.Pass());

  // Fake application initialization.
  application_impl_->Initialize(g_shell.Pass(), g_url);
}

void ApplicationTestBase::TearDown() {
  MOJO_CHECK(!g_application_request.is_pending());
  MOJO_CHECK(!g_shell);

  application_impl_->UnbindConnections(&g_application_request, &g_shell);
  delete application_impl_;
  if (ShouldCreateDefaultRunLoop())
    Environment::DestroyDefaultRunLoop();
}

bool ApplicationTestBase::ShouldCreateDefaultRunLoop() {
  return true;
}


}  // namespace test
}  // namespace mojo
