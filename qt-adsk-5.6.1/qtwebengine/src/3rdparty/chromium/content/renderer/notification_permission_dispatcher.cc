// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_permission_dispatcher.h"

#include "base/bind.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/modules/notifications/WebNotificationPermissionCallback.h"

namespace content {

NotificationPermissionDispatcher::NotificationPermissionDispatcher(
    RenderFrame* render_frame) : RenderFrameObserver(render_frame) {
}

NotificationPermissionDispatcher::~NotificationPermissionDispatcher() {
}

void NotificationPermissionDispatcher::RequestPermission(
    const blink::WebSecurityOrigin& origin,
    blink::WebNotificationPermissionCallback* callback) {
  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  int request_id = pending_requests_.Add(callback);

  permission_service_->RequestPermission(
      PERMISSION_NAME_NOTIFICATIONS,
      origin.toString().utf8(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&NotificationPermissionDispatcher::OnPermissionRequestComplete,
                 base::Unretained(this),
                 request_id));
}

void NotificationPermissionDispatcher::OnPermissionRequestComplete(
    int request_id, PermissionStatus status) {
  blink::WebNotificationPermissionCallback* callback =
      pending_requests_.Lookup(request_id);
  DCHECK(callback);

  blink::WebNotificationPermission permission =
      blink::WebNotificationPermissionDefault;
  switch (status) {
    case PERMISSION_STATUS_GRANTED:
      permission = blink::WebNotificationPermissionAllowed;
      break;
    case PERMISSION_STATUS_DENIED:
      permission = blink::WebNotificationPermissionDenied;
      break;
    case PERMISSION_STATUS_ASK:
      permission = blink::WebNotificationPermissionDefault;
      break;
  }

  callback->permissionRequestComplete(permission);
  pending_requests_.Remove(request_id);
}

}  // namespace content
