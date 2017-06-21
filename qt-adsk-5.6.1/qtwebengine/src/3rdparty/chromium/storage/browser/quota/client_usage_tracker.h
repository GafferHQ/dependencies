// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

namespace storage {

class StorageMonitor;
class UsageTracker;

// This class holds per-client usage tracking information and caches per-host
// usage data.  An instance of this class is created per client.
class ClientUsageTracker : public SpecialStoragePolicy::Observer,
                           public base::NonThreadSafe,
                           public base::SupportsWeakPtr<ClientUsageTracker> {
 public:
  typedef base::Callback<void(int64 limited_usage,
                              int64 unlimited_usage)> HostUsageAccumulator;
  typedef base::Callback<void(const GURL& origin,
                              int64 usage)> OriginUsageAccumulator;
  typedef std::map<std::string, std::set<GURL> > OriginSetByHost;

  ClientUsageTracker(UsageTracker* tracker,
                     QuotaClient* client,
                     StorageType type,
                     SpecialStoragePolicy* special_storage_policy,
                     StorageMonitor* storage_monitor);
  ~ClientUsageTracker() override;

  void GetGlobalLimitedUsage(const UsageCallback& callback);
  void GetGlobalUsage(const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, const UsageCallback& callback);
  void UpdateUsageCache(const GURL& origin, int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;
  int64 GetCachedOriginsUsage(const std::set<GURL>& origins,
                              std::vector<GURL>* origins_not_in_cache);
  bool IsUsageCacheEnabledForOrigin(const GURL& origin) const;
  void SetUsageCacheEnabled(const GURL& origin, bool enabled);

 private:
  typedef CallbackQueueMap<HostUsageAccumulator, std::string, int64, int64>
      HostUsageAccumulatorMap;

  typedef std::set<std::string> HostSet;
  typedef std::map<GURL, int64> UsageMap;
  typedef std::map<std::string, UsageMap> HostUsageMap;

  struct AccumulateInfo {
    int pending_jobs;
    int64 limited_usage;
    int64 unlimited_usage;

    AccumulateInfo()
        : pending_jobs(0), limited_usage(0), unlimited_usage(0) {}
  };

  void AccumulateLimitedOriginUsage(AccumulateInfo* info,
                                    const UsageCallback& callback,
                                    int64 usage);
  void DidGetOriginsForGlobalUsage(const GlobalUsageCallback& callback,
                                   const std::set<GURL>& origins);
  void AccumulateHostUsage(AccumulateInfo* info,
                           const GlobalUsageCallback& callback,
                           int64 limited_usage,
                           int64 unlimited_usage);

  void DidGetOriginsForHostUsage(const std::string& host,
                                 const std::set<GURL>& origins);

  void GetUsageForOrigins(const std::string& host,
                          const std::set<GURL>& origins);
  void AccumulateOriginUsage(AccumulateInfo* info,
                             const std::string& host,
                             const GURL& origin,
                             int64 usage);

  void DidGetHostUsageAfterUpdate(const GURL& origin, int64 usage);

  // Methods used by our GatherUsage tasks, as a task makes progress
  // origins and hosts are added incrementally to the cache.
  void AddCachedOrigin(const GURL& origin, int64 usage);
  void AddCachedHost(const std::string& host);

  int64 GetCachedHostUsage(const std::string& host) const;
  int64 GetCachedGlobalUnlimitedUsage();
  bool GetCachedOriginUsage(const GURL& origin, int64* usage) const;

  // SpecialStoragePolicy::Observer overrides
  void OnGranted(const GURL& origin, int change_flags) override;
  void OnRevoked(const GURL& origin, int change_flags) override;
  void OnCleared() override;

  bool IsStorageUnlimited(const GURL& origin) const;

  UsageTracker* tracker_;
  QuotaClient* client_;
  const StorageType type_;
  StorageMonitor* storage_monitor_;

  int64 global_limited_usage_;
  int64 global_unlimited_usage_;
  bool global_usage_retrieved_;
  HostSet cached_hosts_;
  HostUsageMap cached_usage_by_host_;

  OriginSetByHost non_cached_limited_origins_by_host_;
  OriginSetByHost non_cached_unlimited_origins_by_host_;

  HostUsageAccumulatorMap host_usage_accumulators_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(ClientUsageTracker);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_CLIENT_USAGE_TRACKER_H_
