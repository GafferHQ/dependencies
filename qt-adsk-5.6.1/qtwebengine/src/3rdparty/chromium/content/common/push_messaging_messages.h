// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for push messaging.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include "content/public/common/push_messaging_status.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushError.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START PushMessagingMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::PushRegistrationStatus,
                          content::PUSH_REGISTRATION_STATUS_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(content::PushGetRegistrationStatus,
                          content::PUSH_GETREGISTRATION_STATUS_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebPushPermissionStatus,
    blink::WebPushPermissionStatus::WebPushPermissionStatusLast)

IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebPushError::ErrorType,
    blink::WebPushError::ErrorType::ErrorTypeLast)

// Messages sent from the browser to the child process.

IPC_MESSAGE_ROUTED2(PushMessagingMsg_SubscribeFromDocumentSuccess,
                    int32_t /* request_id */,
                    GURL /* push_endpoint */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_SubscribeFromWorkerSuccess,
                     int32_t /* request_id */,
                     GURL /* push_endpoint */)

IPC_MESSAGE_ROUTED2(PushMessagingMsg_SubscribeFromDocumentError,
                    int32_t /* request_id */,
                    content::PushRegistrationStatus /* status */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_SubscribeFromWorkerError,
                     int32_t /* request_id */,
                     content::PushRegistrationStatus /* status */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_UnsubscribeSuccess,
                     int32_t /* request_id */,
                     bool /* did_unsubscribe */)

IPC_MESSAGE_CONTROL3(PushMessagingMsg_UnsubscribeError,
                     int32_t /* request_id */,
                     blink::WebPushError::ErrorType /* error_type */,
                     std::string /* error_message */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_GetRegistrationSuccess,
                     int32_t /* request_id */,
                     GURL /* push_endpoint */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_GetRegistrationError,
                     int32_t /* request_id */,
                     content::PushGetRegistrationStatus /* status */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_GetPermissionStatusSuccess,
                     int32_t /* request_id */,
                     blink::WebPushPermissionStatus /* status */)

IPC_MESSAGE_CONTROL2(PushMessagingMsg_GetPermissionStatusError,
                     int32_t /* request_id */,
                     blink::WebPushError::ErrorType /* error_type */)

// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL5(PushMessagingHostMsg_SubscribeFromDocument,
                     int32_t /* render_frame_id */,
                     int32_t /* request_id */,
                     std::string /* sender_id */,
                     bool /* user_visible */,
                     int64_t /* service_worker_registration_id */)

IPC_MESSAGE_CONTROL3(PushMessagingHostMsg_SubscribeFromWorker,
                     int32_t /* request_id */,
                     int64_t /* service_worker_registration_id */,
                     bool /* user_visible */)

IPC_MESSAGE_CONTROL2(PushMessagingHostMsg_Unsubscribe,
                     int32_t /* request_id */,
                     int64_t /* service_worker_registration_id */)

IPC_MESSAGE_CONTROL2(PushMessagingHostMsg_GetRegistration,
                     int32_t /* request_id */,
                     int64_t /* service_worker_registration_id */)

IPC_MESSAGE_CONTROL3(PushMessagingHostMsg_GetPermissionStatus,
                     int32_t /* request_id */,
                     int64_t /* service_worker_registration_id */,
                     bool /* user_visible */)
