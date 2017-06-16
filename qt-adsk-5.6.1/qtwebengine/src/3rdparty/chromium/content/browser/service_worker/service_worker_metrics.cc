// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

void RecordURLMetricOnUI(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetContentClient()->browser()->RecordURLMetric(
      "ServiceWorker.ControlledPageUrl", url);
}

bool ShouldExcludeForHistogram(const GURL& scope) {
  // Exclude NTP scope from UMA for now as it tends to dominate the stats
  // and makes the results largely skewed.
  // TOOD(kinuko): This should be temporary, revisit this once we have
  // better idea about what should be excluded in the UMA.
  // (UIThreadSearchTermsData::GoogleBaseURLValue() returns the google base
  // URL, but not available in content layer)
  const char google_like_scope_prefix[] = "https://www.google.";
  return base::StartsWith(scope.spec(), google_like_scope_prefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

enum EventHandledRatioType {
  EVENT_HANDLED_NONE,
  EVENT_HANDLED_SOME,
  EVENT_HANDLED_ALL,
  NUM_EVENT_HANDLED_RATIO_TYPE,
};

}  // namespace

void ServiceWorkerMetrics::CountInitDiskCacheResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.DiskCache.InitResult", result);
}

void ServiceWorkerMetrics::CountReadResponseResult(
    ServiceWorkerMetrics::ReadResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.ReadResponseResult",
                            result, NUM_READ_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountWriteResponseResult(
    ServiceWorkerMetrics::WriteResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.WriteResponseResult",
                            result, NUM_WRITE_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountOpenDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.OpenResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountReadDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.ReadResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountWriteDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.WriteResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDestroyDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.DestroyDatabaseResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDiskCacheMigrationResult(
    DiskCacheMigrationResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Storage.DiskCacheMigrationResult",
                            result, NUM_MIGRATION_RESULT_TYPES);
}

void ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
    DeleteAndStartOverResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Storage.DeleteAndStartOverResult",
                            result, NUM_DELETE_AND_START_OVER_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountControlledPageLoad(const GURL& url) {
  RecordAction(base::UserMetricsAction("ServiceWorker.ControlledPageLoad"));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RecordURLMetricOnUI, url));
}

void ServiceWorkerMetrics::RecordStartWorkerStatus(
    ServiceWorkerStatusCode status,
    bool is_installed) {
  if (is_installed) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StartNewWorker.Status", status,
                              SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordStartWorkerTime(const base::TimeDelta& time,
                                                 bool is_installed) {
  if (is_installed)
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartWorker.Time", time);
  else
    UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StartNewWorker.Time", time);
}

void ServiceWorkerMetrics::RecordStopWorkerStatus(StopWorkerStatus status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.StopWorker.Status", status,
                            NUM_STOP_STATUS_TYPES);
}

void ServiceWorkerMetrics::RecordStopWorkerTime(const base::TimeDelta& time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ServiceWorker.StopWorker.Time", time);
}

void ServiceWorkerMetrics::RecordActivateEventStatus(
    ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.ActivateEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
}

void ServiceWorkerMetrics::RecordInstallEventStatus(
    ServiceWorkerStatusCode status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.InstallEventStatus", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
}

void ServiceWorkerMetrics::RecordEventHandledRatio(const GURL& scope,
                                                   EventType event,
                                                   size_t handled_events,
                                                   size_t fired_events) {
  if (!fired_events || ShouldExcludeForHistogram(scope))
    return;
  EventHandledRatioType type = EVENT_HANDLED_SOME;
  if (fired_events == handled_events)
    type = EVENT_HANDLED_ALL;
  else if (handled_events == 0)
    type = EVENT_HANDLED_NONE;
  // For now Fetch is the only type that is recorded.
  DCHECK_EQ(EVENT_TYPE_FETCH, event);
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.EventHandledRatioType.Fetch", type,
                            NUM_EVENT_HANDLED_RATIO_TYPE);
}

void ServiceWorkerMetrics::RecordFetchEventStatus(
    bool is_main_resource,
    ServiceWorkerStatusCode status) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.MainResource.Status",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.FetchEvent.Subresource.Status",
                              status, SERVICE_WORKER_ERROR_MAX_VALUE);
  }
}

void ServiceWorkerMetrics::RecordURLRequestJobResult(
    bool is_main_resource,
    URLRequestJobResult result) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.MainResource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  } else {
    UMA_HISTOGRAM_ENUMERATION("ServiceWorker.URLRequestJob.Subresource.Result",
                              result, NUM_REQUEST_JOB_RESULT_TYPES);
  }
}

void ServiceWorkerMetrics::RecordStatusZeroResponseError(
    bool is_main_resource,
    blink::WebServiceWorkerResponseError error) {
  if (is_main_resource) {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.MainResource.StatusZeroError", error,
        blink::WebServiceWorkerResponseErrorLast + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ServiceWorker.URLRequestJob.Subresource.StatusZeroError", error,
        blink::WebServiceWorkerResponseErrorLast + 1);
  }
}

}  // namespace content
