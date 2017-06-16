// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DEFAULT_CAPTURE_CLIENT_H_
#define UI_AURA_CLIENT_DEFAULT_CAPTURE_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/capture_client.h"

namespace aura {
namespace client {

class AURA_EXPORT DefaultCaptureClient : public client::CaptureClient {
 public:
  explicit DefaultCaptureClient(Window* root_window);
  ~DefaultCaptureClient() override;

 private:
  // Overridden from client::CaptureClient:
  void SetCapture(Window* window) override;
  void ReleaseCapture(Window* window) override;
  Window* GetCaptureWindow() override;
  Window* GetGlobalCaptureWindow() override;

  Window* root_window_;
  Window* capture_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultCaptureClient);
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DEFAULT_CAPTURE_CLIENT_H_
