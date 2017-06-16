// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/public/browser/browser_accessibility_state.h"

namespace content {

// The BrowserAccessibilityState class is used to determine if Chrome should be
// customized for users with assistive technology, such as screen readers. We
// modify the behavior of certain user interfaces to provide a better experience
// for screen reader users. The way we detect a screen reader program is
// different for each platform.
//
// Screen Reader Detection
// (1) On windows many screen reader detection mechinisms will give false
// positives like relying on the SPI_GETSCREENREADER system parameter. In Chrome
// we attempt to dynamically detect a MSAA client screen reader by calling
// NotifiyWinEvent in NativeWidgetWin with a custom ID and wait to see if the ID
// is requested by a subsequent call to WM_GETOBJECT.
// (2) On mac we detect dynamically if VoiceOver is running.  We rely upon the
// undocumented accessibility attribute @"AXEnhancedUserInterface" which is set
// when VoiceOver is launched and unset when VoiceOver is closed.  This is an
// improvement over reading defaults preference values (which has no callback
// mechanism).
class CONTENT_EXPORT BrowserAccessibilityStateImpl
    : public base::RefCountedThreadSafe<BrowserAccessibilityStateImpl>,
      public BrowserAccessibilityState {
 public:
  BrowserAccessibilityStateImpl();

  static BrowserAccessibilityStateImpl* GetInstance();

  void EnableAccessibility() override;
  void DisableAccessibility() override;
  void ResetAccessibilityMode() override;
  void OnScreenReaderDetected() override;
  bool IsAccessibleBrowser() override;
  void AddHistogramCallback(base::Closure callback) override;

  void UpdateHistogramsForTesting() override;

  AccessibilityMode accessibility_mode() const { return accessibility_mode_; };

  // Adds the given accessibility mode to the current accessibility mode bitmap.
  void AddAccessibilityMode(AccessibilityMode mode);

  // Removes the given accessibility mode from the current accessibility mode
  // bitmap, managing the bits that are shared with other modes such that a
  // bit will only be turned off when all modes that depend on it have been
  // removed.
  void RemoveAccessibilityMode(AccessibilityMode mode);

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element.  This is a global switch to not use the "hot tracked" state
  // in a test.
  void set_disable_hot_tracking_for_testing(bool disable_hot_tracking) {
    disable_hot_tracking_ = disable_hot_tracking;
  }
  bool disable_hot_tracking_for_testing() const {
    return disable_hot_tracking_;
  }

 private:
  friend class base::RefCountedThreadSafe<BrowserAccessibilityStateImpl>;
  friend struct DefaultSingletonTraits<BrowserAccessibilityStateImpl>;

  // Resets accessibility_mode_ to the default value.
  void ResetAccessibilityModeValue();

  // Called a short while after startup to allow time for the accessibility
  // state to be determined. Updates histograms with the current state.
  void UpdateHistograms();

  // Leaky singleton, destructor generally won't be called.
  ~BrowserAccessibilityStateImpl() override;

  void UpdatePlatformSpecificHistograms();

  // Updates the accessibility mode of all web contents, including swapped out
  // ones. |add| specifies whether the mode should be added or removed.
  void AddOrRemoveFromAllWebContents(AccessibilityMode mode, bool add);

  AccessibilityMode accessibility_mode_;

  std::vector<base::Closure> histogram_callbacks_;

  bool disable_hot_tracking_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityStateImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_H_
