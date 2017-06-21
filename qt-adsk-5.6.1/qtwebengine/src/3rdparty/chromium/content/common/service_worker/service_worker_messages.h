// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_client_info.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/message_port_types.h"
#include "content/public/common/navigator_connect_client.h"
#include "content/public/common/platform_notification_data.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ServiceWorkerMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerError::ErrorType,
                          blink::WebServiceWorkerError::ErrorTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerEventResult,
                          blink::WebServiceWorkerEventResultLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerState,
                          blink::WebServiceWorkerStateLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerResponseType,
                          blink::WebServiceWorkerResponseTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerResponseError,
                          blink::WebServiceWorkerResponseErrorLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebServiceWorkerClientType,
                          blink::WebServiceWorkerClientTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerProviderType,
                          content::SERVICE_WORKER_PROVIDER_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerFetchRequest)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(request_context_type)
  IPC_STRUCT_TRAITS_MEMBER(frame_type)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(method)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(blob_size)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(credentials_mode)
  IPC_STRUCT_TRAITS_MEMBER(is_reload)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(content::ServiceWorkerFetchEventResult,
                          content::SERVICE_WORKER_FETCH_EVENT_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerResponse)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(status_text)
  IPC_STRUCT_TRAITS_MEMBER(response_type)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(blob_uuid)
  IPC_STRUCT_TRAITS_MEMBER(blob_size)
  IPC_STRUCT_TRAITS_MEMBER(stream_url)
  IPC_STRUCT_TRAITS_MEMBER(error)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(version_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerRegistrationObjectInfo)
  IPC_STRUCT_TRAITS_MEMBER(handle_id)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(registration_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerVersionAttributes)
  IPC_STRUCT_TRAITS_MEMBER(installing)
  IPC_STRUCT_TRAITS_MEMBER(waiting)
  IPC_STRUCT_TRAITS_MEMBER(active)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerClientInfo)
  IPC_STRUCT_TRAITS_MEMBER(client_uuid)
  IPC_STRUCT_TRAITS_MEMBER(page_visibility_state)
  IPC_STRUCT_TRAITS_MEMBER(is_focused)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(frame_type)
  IPC_STRUCT_TRAITS_MEMBER(client_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::ServiceWorkerClientQueryOptions)
  IPC_STRUCT_TRAITS_MEMBER(client_type)
  IPC_STRUCT_TRAITS_MEMBER(include_uncontrolled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ServiceWorkerMsg_MessageToDocument_Params)
  IPC_STRUCT_MEMBER(int, thread_id)
  IPC_STRUCT_MEMBER(int, provider_id)
  IPC_STRUCT_MEMBER(content::ServiceWorkerObjectInfo, service_worker_info)
  IPC_STRUCT_MEMBER(base::string16, message)
  IPC_STRUCT_MEMBER(std::vector<content::TransferredMessagePort>, message_ports)
  IPC_STRUCT_MEMBER(std::vector<int>, new_routing_ids)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebGeofencingEventType,
                          blink::WebGeofencingEventTypeLast)

IPC_STRUCT_TRAITS_BEGIN(content::NavigatorConnectClient)
  IPC_STRUCT_TRAITS_MEMBER(target_url)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(message_port_id)
IPC_STRUCT_TRAITS_END()

//---------------------------------------------------------------------------
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL5(ServiceWorkerHostMsg_RegisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* scope */,
                     GURL /* script_url */)

IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_UpdateServiceWorker,
                     int /* provider_id */,
                     int64 /* registration_id */)

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_UnregisterServiceWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     int64 /* registration_id */)

IPC_MESSAGE_CONTROL4(ServiceWorkerHostMsg_GetRegistration,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */,
                     GURL /* document_url */)

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_GetRegistrations,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */)

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_GetRegistrationForReady,
                     int /* thread_id */,
                     int /* request_id */,
                     int /* provider_id */)

// Sends a 'message' event to a service worker (renderer->browser).
IPC_MESSAGE_CONTROL3(
    ServiceWorkerHostMsg_PostMessageToWorker,
    int /* handle_id */,
    base::string16 /* message */,
    std::vector<content::TransferredMessagePort> /* sent_message_ports */)

// Informs the browser of a new ServiceWorkerProvider in the child process,
// |provider_id| is unique within its child process. When this provider is
// created for a document, |route_id| is the frame ID of it. When this provider
// is created for a Shared Worker, |route_id| is the Shared Worker route ID.
// When this provider is created for a Service Worker, |route_id| is
// MSG_ROUTING_NONE. |provider_type| identifies whether this provider is for
// Service Worker controllees (documents and Shared Workers) or for controllers
// (Service Workers).
IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_ProviderCreated,
                     int /* provider_id */,
                     int /* route_id */,
                     content::ServiceWorkerProviderType /* provider_type */)

// Informs the browser of a ServiceWorkerProvider being destroyed.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_ProviderDestroyed,
                     int /* provider_id */)

// Increments and decrements the ServiceWorker object's reference
// counting in the browser side. The ServiceWorker object is created
// with ref-count==1 initially.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_IncrementServiceWorkerRefCount,
                     int /* handle_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_DecrementServiceWorkerRefCount,
                     int /* handle_id */)

// Increments and decrements the ServiceWorkerRegistration object's reference
// counting in the browser side. The registration object is created with
// ref-count==1 initially.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_IncrementRegistrationRefCount,
                     int /* registration_handle_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_DecrementRegistrationRefCount,
                     int /* registration_handle_id */)

// Tells the browser to terminate a service worker. Used in layout tests to
// verify behavior when a service worker isn't running.
IPC_MESSAGE_CONTROL1(ServiceWorkerHostMsg_TerminateWorker,
                     int /* handle_id */)

// Informs the browser that |provider_id| is associated
// with a service worker script running context and
// |version_id| identifies which ServiceWorkerVersion.
IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_SetVersionId,
                     int /* provider_id */,
                     int64 /* version_id */)

// Informs the browser that event handling has finished.
// Routed to the target ServiceWorkerVersion.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_InstallEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_ActivateEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED3(ServiceWorkerHostMsg_FetchEventFinished,
                    int /* request_id */,
                    content::ServiceWorkerFetchEventResult,
                    content::ServiceWorkerResponse)
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_SyncEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_NotificationClickEventFinished,
                    int /* request_id */)
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_PushEventFinished,
                    int /* request_id */,
                    blink::WebServiceWorkerEventResult)
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_GeofencingEventFinished,
                    int /* request_id */)
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_CrossOriginConnectEventFinished,
                    int /* request_id */,
                    bool /* accept_connection */)

// Responds to a Ping from the browser.
// Routed to the target ServiceWorkerVersion.
IPC_MESSAGE_ROUTED0(ServiceWorkerHostMsg_Pong)

// Asks the browser to retrieve clients of the sender ServiceWorker.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_GetClients,
                    int /* request_id */,
                    content::ServiceWorkerClientQueryOptions)

// Sends a 'message' event to a client (renderer->browser).
IPC_MESSAGE_ROUTED3(
    ServiceWorkerHostMsg_PostMessageToClient,
    std::string /* uuid */,
    base::string16 /* message */,
    std::vector<content::TransferredMessagePort> /* sent_message_ports */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// cache |data| associated with |url|.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_SetCachedMetadata,
                    GURL /* url */,
                    std::vector<char> /* data */)

// ServiceWorker -> Browser message to request that the ServiceWorkerStorage
// clear the cache associated with |url|.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_ClearCachedMetadata, GURL /* url */)

// Ask the browser to open a tab/window (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_OpenWindow,
                    int /* request_id */,
                    GURL /* url */)

// Ask the browser to focus a client (renderer->browser).
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_FocusClient,
                    int /* request_id */,
                    std::string /* uuid */)

// Asks the browser to force this worker to become activated.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_SkipWaiting,
                    int /* request_id */)

// Asks the browser to have this worker take control of pages that match
// its scope.
IPC_MESSAGE_ROUTED1(ServiceWorkerHostMsg_ClaimClients,
                    int /* request_id */)

// Asks the browser to stash a message port, giving it a name.
IPC_MESSAGE_ROUTED2(ServiceWorkerHostMsg_StashMessagePort,
                    int /* message_port_id */,
                    base::string16 /* name */)

//---------------------------------------------------------------------------
// Messages sent from the browser to the child process.
//
// NOTE: All ServiceWorkerMsg messages not sent via EmbeddedWorker must have
// a thread_id as their first field so that ServiceWorkerMessageFilter can
// extract it and dispatch the message to the correct ServiceWorkerDispatcher
// on the correct thread.

// Informs the child process of the registration associated with the service
// worker.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_AssociateRegistrationWithServiceWorker,
                     int /* thread_id*/,
                     int /* provider_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Informs the child process that the given provider gets associated or
// disassociated with the registration.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_AssociateRegistration,
                     int /* thread_id */,
                     int /* provider_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DisassociateRegistration,
                     int /* thread_id */,
                     int /* provider_id */)

// Response to ServiceWorkerHostMsg_RegisterServiceWorker.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistered,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Response to ServiceWorkerHostMsg_UnregisterServiceWorker.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerUnregistered,
                     int /* thread_id */,
                     int /* request_id */,
                     bool /* is_success */)

// Response to ServiceWorkerHostMsg_GetRegistration.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistration,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Response to ServiceWorkerHostMsg_GetRegistrations.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistrations,
                     int /* thread_id */,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerRegistrationObjectInfo>,
                     std::vector<content::ServiceWorkerVersionAttributes>)

// Response to ServiceWorkerHostMsg_GetRegistrationForReady.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_DidGetRegistrationForReady,
                     int /* thread_id */,
                     int /* request_id */,
                     content::ServiceWorkerRegistrationObjectInfo,
                     content::ServiceWorkerVersionAttributes)

// Sent when any kind of registration error occurs during a
// RegisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerRegistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// UnregisterServiceWorker handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerUnregistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// GetRegistration handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerGetRegistrationError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent when any kind of registration error occurs during a
// GetRegistrations handler above.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_ServiceWorkerGetRegistrationsError,
                     int /* thread_id */,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Informs the child process that the ServiceWorker's state has changed.
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ServiceWorkerStateChanged,
                     int /* thread_id */,
                     int /* handle_id */,
                     blink::WebServiceWorkerState)

// Tells the child process to set service workers for the given provider.
IPC_MESSAGE_CONTROL5(ServiceWorkerMsg_SetVersionAttributes,
                     int /* thread_id */,
                     int /* provider_id */,
                     int /* registration_handle_id */,
                     int /* changed_mask */,
                     content::ServiceWorkerVersionAttributes)

// Informs the child process that new ServiceWorker enters the installation
// phase.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_UpdateFound,
                     int /* thread_id */,
                     int /* registration_handle_id */)

// Tells the child process to set the controller ServiceWorker for the given
// provider.
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_SetControllerServiceWorker,
                     int /* thread_id */,
                     int /* provider_id */,
                     content::ServiceWorkerObjectInfo,
                     bool /* should_notify_controllerchange */)

// Sends a 'message' event to a client document (browser->renderer).
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_MessageToDocument,
                     ServiceWorkerMsg_MessageToDocument_Params)

// Sent via EmbeddedWorker to dispatch events.
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_InstallEvent,
                     int /* request_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_ActivateEvent,
                     int /* request_id */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_FetchEvent,
                     int /* request_id */,
                     content::ServiceWorkerFetchRequest)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_SyncEvent,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_NotificationClickEvent,
                     int /* request_id */,
                     int64_t /* persistent_notification_id */,
                     content::PlatformNotificationData /* notification_data */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_PushEvent,
                     int /* request_id */,
                     std::string /* data */)
IPC_MESSAGE_CONTROL4(ServiceWorkerMsg_GeofencingEvent,
                     int /* request_id */,
                     blink::WebGeofencingEventType /* event_type */,
                     std::string /* region_id */,
                     blink::WebCircularGeofencingRegion /* region */)
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_CrossOriginConnectEvent,
                     int /* request_id */,
                     content::NavigatorConnectClient /* client */)
IPC_MESSAGE_CONTROL3(
    ServiceWorkerMsg_MessageToWorker,
    base::string16 /* message */,
    std::vector<content::TransferredMessagePort> /* sent_message_ports */,
    std::vector<int> /* new_routing_ids */)
IPC_MESSAGE_CONTROL4(
    ServiceWorkerMsg_CrossOriginMessageToWorker,
    content::NavigatorConnectClient /* client */,
    base::string16 /* message */,
    std::vector<content::TransferredMessagePort> /* sent_message_ports */,
    std::vector<int> /* new_routing_ids */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_DidSkipWaiting,
                     int /* request_id */)
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_DidClaimClients,
                     int /* request_id */)
IPC_MESSAGE_CONTROL3(ServiceWorkerMsg_ClaimClientsError,
                     int /* request_id */,
                     blink::WebServiceWorkerError::ErrorType /* code */,
                     base::string16 /* message */)

// Sent via EmbeddedWorker to Ping the worker, expecting a Pong in response.
IPC_MESSAGE_CONTROL0(ServiceWorkerMsg_Ping)

// Sent via EmbeddedWorker as a response of GetClients.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_DidGetClients,
                     int /* request_id */,
                     std::vector<content::ServiceWorkerClientInfo>)

// Sent via EmbeddedWorker as a response of OpenWindow.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_OpenWindowResponse,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker as an error response of OpenWindow.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_OpenWindowError,
                     int /* request_id */,
                     std::string /* message */ )

// Sent via EmbeddedWorker as a response of FocusClient.
IPC_MESSAGE_CONTROL2(ServiceWorkerMsg_FocusClientResponse,
                     int /* request_id */,
                     content::ServiceWorkerClientInfo /* client */)

// Sent via EmbeddedWorker to transfer a stashed message port to the worker.
IPC_MESSAGE_CONTROL3(
    ServiceWorkerMsg_SendStashedMessagePorts,
    std::vector<content::TransferredMessagePort> /* stashed_message_ports */,
    std::vector<int> /* new_routing_ids */,
    std::vector<base::string16> /* port_names */)
