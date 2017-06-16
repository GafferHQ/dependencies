// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDERER_GAMEPAD_PROVIDER_H_
#define CONTENT_PUBLIC_RENDERER_RENDERER_GAMEPAD_PROVIDER_H_

#include "content/public/renderer/platform_event_observer.h"

namespace blink {
class WebGamepadListener;
class WebGamepads;
}

namespace content {

// Provides gamepad data and events for blink.
class RendererGamepadProvider
    : public PlatformEventObserver<blink::WebGamepadListener> {
 public:
  explicit RendererGamepadProvider(RenderThread* thread)
      : PlatformEventObserver<blink::WebGamepadListener>(thread) { }

  ~RendererGamepadProvider() override {}

  // Provides latest snapshot of gamepads.
  virtual void SampleGamepads(blink::WebGamepads& gamepads) = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(RendererGamepadProvider);
};

} // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDERER_GAMEPAD_PROVIDER_H_
