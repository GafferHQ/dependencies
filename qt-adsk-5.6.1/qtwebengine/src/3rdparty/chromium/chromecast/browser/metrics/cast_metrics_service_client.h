// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
#define CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/metrics/metrics_service_client.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace metrics {
struct ClientInfo;
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace chromecast {

class CastService;

namespace metrics {

class ExternalMetrics;

class CastMetricsServiceClient : public ::metrics::MetricsServiceClient {
 public:
  ~CastMetricsServiceClient() override;

  static scoped_ptr<CastMetricsServiceClient> Create(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Initialize(CastService* cast_service);
  void Finalize();

  // metrics::MetricsServiceClient implementation:
  void SetMetricsClientId(const std::string& client_id) override;
  void OnRecordingDisabled() override;
  bool IsOffTheRecordSessionActive() override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  ::metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnLogUploadComplete() override;
  void StartGatheringMetrics(const base::Closure& done_callback) override;
  void CollectFinalMetrics(const base::Closure& done_callback) override;
  scoped_ptr< ::metrics::MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;

  // Starts/stops the metrics service.
  void EnableMetricsService(bool enabled);

  std::string client_id() const {
    return client_id_;
  }

 private:
  CastMetricsServiceClient(
      base::TaskRunner* io_task_runner,
      PrefService* pref_service,
      net::URLRequestContextGetter* request_context);

  // Returns whether or not metrics reporting is enabled.
  bool IsReportingEnabled();

  scoped_ptr< ::metrics::ClientInfo> LoadClientInfo();
  void StoreClientInfo(const ::metrics::ClientInfo& client_info);

  base::TaskRunner* const io_task_runner_;
  PrefService* const pref_service_;
  CastService* cast_service_;
  std::string client_id_;

#if defined(OS_LINUX)
  ExternalMetrics* external_metrics_;
#endif  // defined(OS_LINUX)
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr< ::metrics::MetricsStateManager> metrics_state_manager_;
  scoped_ptr< ::metrics::MetricsService> metrics_service_;
  net::URLRequestContextGetter* const request_context_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsServiceClient);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_CAST_METRICS_SERVICE_CLIENT_H_
