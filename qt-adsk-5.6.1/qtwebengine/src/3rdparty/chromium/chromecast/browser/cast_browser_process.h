// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class PrefService;

namespace breakpad {
class CrashDumpManager;
}  // namespace breakpad

namespace net {
class NetLog;
}  // namespace net

namespace chromecast {
class CastService;
class CastScreen;
class ConnectivityChecker;

namespace metrics {
class CastMetricsHelper;
class CastMetricsServiceClient;
}  // namespace metrics

namespace shell {
class CastBrowserContext;
class CastResourceDispatcherHostDelegate;
class RemoteDebuggingServer;

class CastBrowserProcess {
 public:
  // Gets the global instance of CastBrowserProcess. Does not create lazily and
  // assumes the instance already exists.
  static CastBrowserProcess* GetInstance();

  CastBrowserProcess();
  virtual ~CastBrowserProcess();

  void SetBrowserContext(scoped_ptr<CastBrowserContext> browser_context);
  void SetCastService(scoped_ptr<CastService> cast_service);
#if defined(USE_AURA)
  void SetCastScreen(scoped_ptr<CastScreen> cast_screen);
#endif  // defined(USE_AURA)
  void SetMetricsHelper(scoped_ptr<metrics::CastMetricsHelper> metrics_helper);
  void SetMetricsServiceClient(
      scoped_ptr<metrics::CastMetricsServiceClient> metrics_service_client);
  void SetPrefService(scoped_ptr<PrefService> pref_service);
  void SetRemoteDebuggingServer(
      scoped_ptr<RemoteDebuggingServer> remote_debugging_server);
  void SetResourceDispatcherHostDelegate(
      scoped_ptr<CastResourceDispatcherHostDelegate> delegate);
#if defined(OS_ANDROID)
  void SetCrashDumpManager(
      scoped_ptr<breakpad::CrashDumpManager> crash_dump_manager);
#endif  // defined(OS_ANDROID)
  void SetConnectivityChecker(
      scoped_refptr<ConnectivityChecker> connectivity_checker);
  void SetNetLog(net::NetLog* net_log);

  CastBrowserContext* browser_context() const { return browser_context_.get(); }
  CastService* cast_service() const { return cast_service_.get(); }
#if defined(USE_AURA)
  CastScreen* cast_screen() const { return cast_screen_.get(); }
#endif  // defined(USE_AURA)
  metrics::CastMetricsServiceClient* metrics_service_client() const {
    return metrics_service_client_.get();
  }
  PrefService* pref_service() const { return pref_service_.get(); }
  CastResourceDispatcherHostDelegate* resource_dispatcher_host_delegate()
      const {
    return resource_dispatcher_host_delegate_.get();
  }
  ConnectivityChecker* connectivity_checker() const {
    return connectivity_checker_.get();
  }
  net::NetLog* net_log() const { return net_log_; }

 private:
  // Note: The following order should match the order they are set in
  // CastBrowserMainParts.
  scoped_ptr<metrics::CastMetricsHelper> metrics_helper_;
#if defined(USE_AURA)
  scoped_ptr<CastScreen> cast_screen_;
#endif  // defined(USE_AURA)
  scoped_ptr<PrefService> pref_service_;
  scoped_refptr<ConnectivityChecker> connectivity_checker_;
  scoped_ptr<CastBrowserContext> browser_context_;
  scoped_ptr<metrics::CastMetricsServiceClient> metrics_service_client_;
  scoped_ptr<CastResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;
#if defined(OS_ANDROID)
  scoped_ptr<breakpad::CrashDumpManager> crash_dump_manager_;
#endif  // defined(OS_ANDROID)
  scoped_ptr<RemoteDebuggingServer> remote_debugging_server_;

  net::NetLog* net_log_;

  // Note: CastService must be destroyed before others.
  scoped_ptr<CastService> cast_service_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserProcess);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
