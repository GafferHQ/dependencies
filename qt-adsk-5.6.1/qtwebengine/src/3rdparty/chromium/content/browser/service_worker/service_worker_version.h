// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebGeofencingEventType.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

class GURL;

namespace blink {
struct WebCircularGeofencingRegion;
}

namespace net {
class HttpResponseInfo;
}

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerURLRequestJob;
struct NavigatorConnectClient;
struct PlatformNotificationData;
struct ServiceWorkerClientInfo;
struct ServiceWorkerVersionInfo;
struct TransferredMessagePort;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is activated. This class connects the actual script with a
// running worker.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>),
      public EmbeddedWorkerInstance::Listener {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              ServiceWorkerFetchEventResult,
                              const ServiceWorkerResponse&)> FetchCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode,
                              bool /* accept_connction */)>
      CrossOriginConnectCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode, const std::vector<int>&)>
      SendStashedPortsCallback;

  enum RunningStatus {
    STOPPED = EmbeddedWorkerInstance::STOPPED,
    STARTING = EmbeddedWorkerInstance::STARTING,
    RUNNING = EmbeddedWorkerInstance::RUNNING,
    STOPPING = EmbeddedWorkerInstance::STOPPING,
  };

  // Current version status; some of the status (e.g. INSTALLED and ACTIVATED)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVATED,   // Activation is finished and can run as activated.
    REDUNDANT,   // The version is no longer running as activated, due to
                 // unregistration or replace.
  };

  class Listener {
   public:
    virtual void OnRunningStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnMainScriptHttpResponseInfoSet(
        ServiceWorkerVersion* version) {}
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) {}
    virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                        int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    virtual void OnControlleeAdded(ServiceWorkerVersion* version,
                                   ServiceWorkerProviderHost* provider_host) {}
    virtual void OnControlleeRemoved(ServiceWorkerVersion* version,
                                     ServiceWorkerProviderHost* provider_host) {
    }
    // Fires when a version transitions from having a controllee to not.
    virtual void OnNoControllees(ServiceWorkerVersion* version) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version) {}

   protected:
    virtual ~Listener() {}
  };

  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      const GURL& script_url,
      int64 version_id,
      base::WeakPtr<ServiceWorkerContextCore> context);

  int64 version_id() const { return version_id_; }
  int64 registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& scope() const { return scope_; }
  RunningStatus running_status() const {
    return static_cast<RunningStatus>(embedded_worker_->status());
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(const base::Closure& callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  void StartWorker(const StatusCallback& callback);

  // Stops an embedded worker for this version.
  // This returns OK (success) if the worker is already stopped.
  void StopWorker(const StatusCallback& callback);

  // Schedules an update to be run 'soon'.
  void ScheduleUpdate();

  // If an update is scheduled but not yet started, this resets the timer
  // delaying the start time by a 'small' amount.
  void DeferScheduledUpdate();

  // Starts an update now.
  void StartUpdate();

  // Sends a message event to the associated embedded worker.
  void DispatchMessageEvent(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Sends install event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify install completion.
  //
  // This must be called when the status() is NEW. Calling this changes
  // the version's status to INSTALLING.
  // Upon completion, the version's status will be changed to INSTALLED
  // on success, or back to NEW on failure.
  void DispatchInstallEvent(const StatusCallback& callback);

  // Sends activate event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify activation completion.
  //
  // This must be called when the status() is INSTALLED. Calling this changes
  // the version's status to ACTIVATING.
  // Upon completion, the version's status will be changed to ACTIVATED
  // on success, or back to INSTALLED on failure.
  void DispatchActivateEvent(const StatusCallback& callback);

  // Sends fetch event to the associated embedded worker and calls
  // |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVATED. Calling this in other
  // statuses will result in an error SERVICE_WORKER_ERROR_FAILED.
  void DispatchFetchEvent(const ServiceWorkerFetchRequest& request,
                          const base::Closure& prepare_callback,
                          const FetchCallback& fetch_callback);

  // Sends sync event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets a response from the worker to
  // notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchSyncEvent(const StatusCallback& callback);

  // Sends notificationclick event to the associated embedded worker and
  // asynchronously calls |callback| when it errors out or it gets a response
  // from the worker to notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchNotificationClickEvent(
      const StatusCallback& callback,
      int64_t persistent_notification_id,
      const PlatformNotificationData& notification_data);

  // Sends push event to the associated embedded worker and asynchronously calls
  // |callback| when it errors out or it gets a response from the worker to
  // notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchPushEvent(const StatusCallback& callback,
                         const std::string& data);

  // Sends geofencing event to the associated embedded worker and asynchronously
  // calls |callback| when it errors out or it gets a response from the worker
  // to notify completion.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchGeofencingEvent(
      const StatusCallback& callback,
      blink::WebGeofencingEventType event_type,
      const std::string& region_id,
      const blink::WebCircularGeofencingRegion& region);

  // Sends a cross origin connect event to the associated embedded worker and
  // asynchronously calls |callback| with the response from the worker.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchCrossOriginConnectEvent(
      const CrossOriginConnectCallback& callback,
      const NavigatorConnectClient& client);

  // Sends a cross origin message event to the associated embedded worker and
  // asynchronously calls |callback| when the message was sent (or failed to
  // sent).
  // It is the responsibility of the code calling this method to make sure that
  // any transferred message ports are put on hold while potentially a process
  // for the service worker is spun up.
  //
  // This must be called when the status() is ACTIVATED.
  void DispatchCrossOriginMessageEvent(
      const NavigatorConnectClient& client,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Transfers one or more stashed message ports to the associated embedded
  // worker, and asynchronously calls |callback| with the new route ids for the
  // transferred ports as soon as the ports are sent to the renderer.
  // Once the ports are received by the renderer, the ports themselves will
  // inform MessagePortService, which will enable actual messages to be sent.
  void SendStashedMessagePorts(
      const std::vector<TransferredMessagePort>& stashed_message_ports,
      const std::vector<base::string16>& port_names,
      const SendStashedPortsCallback& callback);

  // Adds and removes |provider_host| as a controllee of this ServiceWorker.
  // A potential controllee is a host having the version as its .installing
  // or .waiting version.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(ServiceWorkerProviderHost* provider_host);

  // Returns if it has controllee.
  bool HasControllee() const { return !controllee_map_.empty(); }

  // Adds and removes |request_job| as a dependent job not to stop the
  // ServiceWorker while |request_job| is reading the stream of the fetch event
  // response from the ServiceWorker.
  void AddStreamingURLRequestJob(const ServiceWorkerURLRequestJob* request_job);
  void RemoveStreamingURLRequestJob(
      const ServiceWorkerURLRequestJob* request_job);

  // Adds and removes Listeners.
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // Reports the error message to |listeners_|.
  void ReportError(ServiceWorkerStatusCode status,
                   const std::string& status_message);

  // Sets the status code to pass to StartWorker callbacks if start fails.
  void SetStartWorkerStatusCode(ServiceWorkerStatusCode status);

  // Sets this version's status to REDUNDANT and deletes its resources.
  // The version must not have controllees.
  void Doom();
  bool is_redundant() const { return status_ == REDUNDANT; }

  bool skip_waiting() const { return skip_waiting_; }
  void set_skip_waiting(bool skip_waiting) { skip_waiting_ = skip_waiting; }

  bool force_bypass_cache_for_scripts() {
    return force_bypass_cache_for_scripts_;
  }
  void set_force_bypass_cache_for_scripts(bool force_bypass_cache_for_scripts) {
    force_bypass_cache_for_scripts_ = force_bypass_cache_for_scripts;
  }

  void SetDevToolsAttached(bool attached);

  // Sets the HttpResponseInfo used to load the main script.
  // This HttpResponseInfo will be used for all responses sent back from the
  // service worker, as the effective security of these responses is equivalent
  // to that of the ServiceWorker.
  void SetMainScriptHttpResponseInfo(const net::HttpResponseInfo& http_info);
  const net::HttpResponseInfo* GetMainScriptHttpResponseInfo();

  // Simulate ping timeout. Should be used for tests-only.
  void SimulatePingTimeoutForTesting();

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class ServiceWorkerURLRequestJobTest;
  friend class ServiceWorkerVersionBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, IdleTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, SetDevToolsAttached);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_FreshWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           StaleUpdate_NonActiveWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_StartWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest, StaleUpdate_RunningWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTest,
                           StaleUpdate_DoNotDeferTimer);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWaitForeverInFetchTest, RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerFailToStartTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);

  class Metrics;
  class PingController;

  typedef ServiceWorkerVersion self;
  using ServiceWorkerClients = std::vector<ServiceWorkerClientInfo>;

  enum RequestType {
    REQUEST_ACTIVATE,
    REQUEST_INSTALL,
    REQUEST_FETCH,
    REQUEST_SYNC,
    REQUEST_NOTIFICATION_CLICK,
    REQUEST_PUSH,
    REQUEST_GEOFENCING,
    REQUEST_CROSS_ORIGIN_CONNECT
  };

  struct RequestInfo {
    RequestInfo(int id, RequestType type);
    ~RequestInfo();
    int id;
    RequestType type;
    base::TimeTicks time;
  };

  // Timeout for the worker to start.
  static const int kStartWorkerTimeoutMinutes;
  // Timeout for a request to be handled.
  static const int kRequestTimeoutMinutes;

  ~ServiceWorkerVersion() override;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnScriptLoaded() override;
  void OnStarting() override;
  void OnStarted() override;
  void OnStopping() override;
  void OnStopped(EmbeddedWorkerInstance::Status old_status) override;
  void OnDetached(EmbeddedWorkerInstance::Status old_status) override;
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnStartSentAndScriptEvaluated(ServiceWorkerStatusCode status);

  void DispatchInstallEventAfterStartWorker(const StatusCallback& callback);
  void DispatchActivateEventAfterStartWorker(const StatusCallback& callback);

  void DispatchMessageEventInternal(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      const StatusCallback& callback);

  // Message handlers.

  // This corresponds to the spec's matchAll(options) steps.
  void OnGetClients(int request_id,
                    const ServiceWorkerClientQueryOptions& options);

  void OnActivateEventFinished(int request_id,
                               blink::WebServiceWorkerEventResult result);
  void OnInstallEventFinished(int request_id,
                              blink::WebServiceWorkerEventResult result);
  void OnFetchEventFinished(int request_id,
                            ServiceWorkerFetchEventResult result,
                            const ServiceWorkerResponse& response);
  void OnSyncEventFinished(int request_id,
                           blink::WebServiceWorkerEventResult result);
  void OnNotificationClickEventFinished(int request_id);
  void OnPushEventFinished(int request_id,
                           blink::WebServiceWorkerEventResult result);
  void OnGeofencingEventFinished(int request_id);
  void OnCrossOriginConnectEventFinished(int request_id,
                                         bool accept_connection);
  void OnOpenWindow(int request_id, GURL url);
  void DidOpenWindow(int request_id,
                     int render_process_id,
                     int render_frame_id);
  void OnOpenWindowFinished(int request_id,
                            const std::string& client_uuid,
                            const ServiceWorkerClientInfo& client_info);

  void OnSetCachedMetadata(const GURL& url, const std::vector<char>& data);
  void OnSetCachedMetadataFinished(int64 callback_id, int result);
  void OnClearCachedMetadata(const GURL& url);
  void OnClearCachedMetadataFinished(int64 callback_id, int result);

  void OnPostMessageToClient(
      const std::string& client_uuid,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);
  void OnFocusClient(int request_id, const std::string& client_uuid);
  void OnSkipWaiting(int request_id);
  void OnClaimClients(int request_id);
  void OnPongFromWorker();
  void OnStashMessagePort(int message_port_id, const base::string16& name);

  void OnFocusClientFinished(int request_id,
                             const std::string& client_uuid,
                             const ServiceWorkerClientInfo& client);

  void DidEnsureLiveRegistrationForStartWorker(
      const StatusCallback& callback,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& protect);
  void StartWorkerInternal();

  void DidSkipWaiting(int request_id);

  void GetWindowClients(int request_id,
                        const ServiceWorkerClientQueryOptions& options);
  void DidGetWindowClients(int request_id,
                           const ServiceWorkerClientQueryOptions& options,
                           scoped_ptr<ServiceWorkerClients> clients);
  void GetNonWindowClients(int request_id,
                           const ServiceWorkerClientQueryOptions& options,
                           ServiceWorkerClients* clients);
  void OnGetClientsFinished(int request_id,
                            const ServiceWorkerClients& clients);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();

  // Called by PingController for ping protocol.
  ServiceWorkerStatusCode PingWorker();
  void OnPingTimeout();

  // Stops the worker if it is idle (has no in-flight requests) or timed out
  // ping.
  void StopWorkerIfIdle();
  bool HasInflightRequests() const;

  // RecordStartWorkerResult is added as a start callback by StartTimeoutTimer
  // and records metrics about startup.
  void RecordStartWorkerResult(ServiceWorkerStatusCode status);

  template <typename IDMAP>
  void RemoveCallbackAndStopIfRedundant(IDMAP* callbacks, int request_id);

  template <typename CallbackType>
  int AddRequest(const CallbackType& callback,
                 IDMap<CallbackType, IDMapOwnPointer>* callback_map,
                 RequestType request_type);

  bool OnRequestTimeout(const RequestInfo& info);
  void SetAllRequestTimes(const base::TimeTicks& ticks);

  // Returns the reason the embedded worker failed to start, using information
  // inaccessible to EmbeddedWorkerInstance. Returns |default_code| if it can't
  // deduce a reason.
  ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      ServiceWorkerStatusCode default_code);

  // Sets |stale_time_| if this worker is stale, causing an update to eventually
  // occur once the worker stops or is running too long.
  void MarkIfStale();

  void FoundRegistrationForUpdate(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  void OnStoppedInternal(EmbeddedWorkerInstance::Status old_status);

  const int64 version_id_;
  const int64 registration_id_;
  const GURL script_url_;
  const GURL scope_;

  Status status_ = NEW;
  scoped_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;

  // Message callbacks. (Update HasInflightRequests() too when you update this
  // list.)
  IDMap<StatusCallback, IDMapOwnPointer> activate_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> install_callbacks_;
  IDMap<FetchCallback, IDMapOwnPointer> fetch_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> sync_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> notification_click_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> push_callbacks_;
  IDMap<StatusCallback, IDMapOwnPointer> geofencing_callbacks_;
  IDMap<CrossOriginConnectCallback, IDMapOwnPointer>
      cross_origin_connect_callbacks_;

  std::set<const ServiceWorkerURLRequestJob*> streaming_url_request_jobs_;

  std::map<std::string, ServiceWorkerProviderHost*> controllee_map_;
  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::ObserverList<Listener> listeners_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer<ServiceWorkerVersion> update_timer_;

  // Starts running in StartWorker and continues until the worker is stopped.
  base::RepeatingTimer<ServiceWorkerVersion> timeout_timer_;
  // Holds the time the worker last started being considered idle.
  base::TimeTicks idle_time_;
  // Holds the time that the outstanding StartWorker() request started.
  base::TimeTicks start_time_;
  // Holds the time the worker entered STOPPING status.
  base::TimeTicks stop_time_;
  // Holds the time the worker was detected as stale and needs updating. We try
  // to update once the worker stops, but will also update if it stays alive too
  // long.
  base::TimeTicks stale_time_;

  // New requests are added to |requests_| along with their entry in a callback
  // map. The timeout timer periodically checks |requests_| for entries that
  // should time out or have already been fulfilled (i.e., removed from the
  // callback map).
  std::queue<RequestInfo> requests_;

  bool skip_waiting_ = false;
  bool skip_recording_startup_time_ = false;
  bool force_bypass_cache_for_scripts_ = false;
  bool is_update_scheduled_ = false;

  std::vector<int> pending_skip_waiting_requests_;
  scoped_ptr<net::HttpResponseInfo> main_script_http_info_;

  // The status when StartWorker was invoked. Used for UMA.
  Status prestart_status_ = NEW;
  // If not OK, the reason that StartWorker failed. Used for
  // running |start_callbacks_|.
  ServiceWorkerStatusCode start_worker_status_ = SERVICE_WORKER_OK;

  scoped_ptr<PingController> ping_controller_;
  scoped_ptr<Metrics> metrics_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
