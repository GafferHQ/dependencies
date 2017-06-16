// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorAnimationDelegate_h
#define WebCompositorAnimationDelegate_h

#include "WebCommon.h"
#include "WebCompositorAnimation.h"

#define WEB_COMPOSITOR_ANIMATION_DELEGATE_TAKES_GROUP 1

namespace blink {

class BLINK_PLATFORM_EXPORT WebCompositorAnimationDelegate {
public:
    virtual ~WebCompositorAnimationDelegate() { }

    virtual void notifyAnimationStarted(double monotonicTime, int group) = 0;
    virtual void notifyAnimationFinished(double monotonicTime, int group) = 0;
};

} // namespace blink

#endif // WebCompositorAnimationDelegate_h
