// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/fake_message_center_tray_delegate.h"

#include "base/message_loop/message_loop.h"
#include "ui/message_center/message_center_tray.h"

namespace message_center {

FakeMessageCenterTrayDelegate::FakeMessageCenterTrayDelegate(
    MessageCenter* message_center,
    base::Closure quit_closure)
    : tray_(new MessageCenterTray(this, message_center)),
      quit_closure_(quit_closure),
      displayed_first_run_balloon_(false) {}

FakeMessageCenterTrayDelegate::~FakeMessageCenterTrayDelegate() {
}

void FakeMessageCenterTrayDelegate::OnMessageCenterTrayChanged() {
}

bool FakeMessageCenterTrayDelegate::ShowPopups() {
  return false;
}

void FakeMessageCenterTrayDelegate::HidePopups() {
}

bool FakeMessageCenterTrayDelegate::ShowMessageCenter() {
  return false;
}

void FakeMessageCenterTrayDelegate::HideMessageCenter() {
}

bool FakeMessageCenterTrayDelegate::ShowNotifierSettings() {
  return false;
}

bool FakeMessageCenterTrayDelegate::IsContextMenuEnabled() const {
  return false;
}

MessageCenterTray* FakeMessageCenterTrayDelegate::GetMessageCenterTray() {
  return tray_.get();
}

void FakeMessageCenterTrayDelegate::DisplayFirstRunBalloon() {
  displayed_first_run_balloon_ = true;
  base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
}

}  // namespace message_center
