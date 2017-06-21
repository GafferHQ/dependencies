// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_UI_RESOURCE_CLIENT_ANDROID_H_
#define UI_ANDROID_RESOURCES_UI_RESOURCE_CLIENT_ANDROID_H_

#include "cc/resources/ui_resource_client.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// Android's UIResourceClient has one extra callback (UIResourceIsInvalid).
// This signal is intended for the case when the LayerTreeHost is cleared and
// the user needs to recreate their resources.
// TODO(powei): This interface can be removed once crbug.com/374906 has been
// addressed.
class UI_ANDROID_EXPORT UIResourceClientAndroid : public cc::UIResourceClient {
 public:
  // This method indicates that the UI resource the user holds is no longer
  // valid. The user should not call DeleteUIResource on any resource generated
  // before this signal.
  virtual void UIResourceIsInvalid() = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_UI_RESOURCE_CLIENT_ANDROID_H_
