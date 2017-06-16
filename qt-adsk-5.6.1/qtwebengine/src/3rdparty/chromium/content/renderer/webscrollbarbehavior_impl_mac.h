// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBSCROLLBARBEHAVIOR_IMPL_MAC_H_
#define CONTENT_RENDERER_WEBSCROLLBARBEHAVIOR_IMPL_MAC_H_

#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace content {

class WebScrollbarBehaviorImpl : public blink::WebScrollbarBehavior {
 public:
  WebScrollbarBehaviorImpl();

  virtual bool shouldCenterOnThumb(
      blink::WebScrollbarBehavior::Button mouseButton,
      bool shiftKeyPressed,
      bool altKeyPressed);

  void set_jump_on_track_click(bool jump_on_track_click) {
    jump_on_track_click_ = jump_on_track_click;
  }

 private:
  // The current value of AppleScrollerPagingBehavior from NSUserDefaults.
  bool jump_on_track_click_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBSCROLLBARBEHAVIOR_IMPL_MAC_H_
