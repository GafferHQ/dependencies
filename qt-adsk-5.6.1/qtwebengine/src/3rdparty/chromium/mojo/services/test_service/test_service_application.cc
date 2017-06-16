// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_application.h"

#include <assert.h>

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/test_service/test_service_impl.h"
#include "mojo/services/test_service/test_time_service_impl.h"

namespace mojo {
namespace test {

TestServiceApplication::TestServiceApplication()
    : ref_count_(0), app_impl_(nullptr) {
}

TestServiceApplication::~TestServiceApplication() {
}

void TestServiceApplication::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
}

bool TestServiceApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<TestService>(this);
  connection->AddService<TestTimeService>(this);
  return true;
}

void TestServiceApplication::Create(ApplicationConnection* connection,
                                    InterfaceRequest<TestService> request) {
  new TestServiceImpl(app_impl_, this, request.Pass());
  AddRef();
}

void TestServiceApplication::Create(ApplicationConnection* connection,
                                    InterfaceRequest<TestTimeService> request) {
  new TestTimeServiceImpl(app_impl_, request.Pass());
}

void TestServiceApplication::AddRef() {
  assert(ref_count_ >= 0);
  ref_count_++;
}

void TestServiceApplication::ReleaseRef() {
  assert(ref_count_ > 0);
  ref_count_--;
  if (ref_count_ <= 0)
    base::MessageLoop::current()->Quit();
}

}  // namespace test
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::test::TestServiceApplication);
  return runner.Run(shell_handle);
}
