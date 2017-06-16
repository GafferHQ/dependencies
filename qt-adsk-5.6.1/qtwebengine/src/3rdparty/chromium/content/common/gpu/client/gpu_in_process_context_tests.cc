// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <cmath>
#include <string>
#include <vector>

#include "content/public/test/unittest_test_suite.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface.h"

namespace {

using gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl;

class ContextTestBase : public testing::Test {
 public:
  void SetUp() override {
    blink::WebGraphicsContext3D::Attributes attributes;
    bool lose_context_when_out_of_memory = false;
    typedef WebGraphicsContext3DInProcessCommandBufferImpl WGC3DIPCBI;
    context_ = WGC3DIPCBI::CreateOffscreenContext(
        attributes, lose_context_when_out_of_memory);
    context_->InitializeOnCurrentThread();
    context_support_ = context_->GetContextSupport();
  }

  void TearDown() override { context_.reset(NULL); }

 protected:
  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context_;
  gpu::ContextSupport* context_support_;
};

}  // namespace

// Include the actual tests.
#define CONTEXT_TEST_F TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"
