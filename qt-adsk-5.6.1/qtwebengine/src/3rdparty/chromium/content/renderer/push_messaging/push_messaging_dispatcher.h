// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_

#include <stdint.h>
#include <string>

#include "base/id_map.h"
#include "content/public/common/push_messaging_status.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushClient.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

class GURL;

namespace blink {
struct WebPushSubscriptionOptions;
}

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

struct Manifest;

class PushMessagingDispatcher : public RenderFrameObserver,
                                public blink::WebPushClient {
 public:
  explicit PushMessagingDispatcher(RenderFrame* render_frame);
  virtual ~PushMessagingDispatcher();

 private:
  // RenderFrame::Observer implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebPushClient implementation.
  virtual void subscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks);

  void DoSubscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      blink::WebPushSubscriptionCallbacks* callbacks,
      const Manifest& manifest);

  void OnSubscribeFromDocumentSuccess(int32_t request_id,
                                      const GURL& endpoint);

  void OnSubscribeFromDocumentError(int32_t request_id,
                                    PushRegistrationStatus status);

  IDMap<blink::WebPushSubscriptionCallbacks, IDMapOwnPointer>
      subscription_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
