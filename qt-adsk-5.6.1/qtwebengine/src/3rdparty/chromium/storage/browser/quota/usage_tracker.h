// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_

#include <map>
#include <set>
#include <string>

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

class ClientUsageTracker;
class StorageMonitor;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
// An instance of this class is created per storage type.
class STORAGE_EXPORT UsageTracker : public QuotaTaskObserver {
 public:
  UsageTracker(const QuotaClientList& clients, StorageType type,
               SpecialStoragePolicy* special_storage_policy,
               StorageMonitor* storage_monitor);
  ~UsageTracker() override;

  StorageType type() const { return type_; }
  ClientUsageTracker* GetClientTracker(QuotaClient::ID client_id);

  void GetGlobalLimitedUsage(const UsageCallback& callback);
  void GetGlobalUsage(const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, const UsageCallback& callback);
  void UpdateUsageCache(QuotaClient::ID client_id,
                        const GURL& origin,
                        int64 delta);
  void GetCachedHostsUsage(std::map<std::string, int64>* host_usage) const;
  void GetCachedOrigins(std::set<GURL>* origins) const;
  bool IsWorking() const {
    return global_usage_callbacks_.HasCallbacks() ||
           host_usage_callbacks_.HasAnyCallbacks();
  }

  void SetUsageCacheEnabled(QuotaClient::ID client_id,
                            const GURL& origin,
                            bool enabled);

 private:
  struct AccumulateInfo {
    AccumulateInfo() : pending_clients(0), usage(0), unlimited_usage(0) {}
    int pending_clients;
    int64 usage;
    int64 unlimited_usage;
  };

  typedef std::map<QuotaClient::ID, ClientUsageTracker*> ClientTrackerMap;

  typedef CallbackQueue<UsageCallback, int64> UsageCallbackQueue;
  typedef CallbackQueue<GlobalUsageCallback, int64, int64>
      GlobalUsageCallbackQueue;
  typedef CallbackQueueMap<UsageCallback, std::string, int64>
      HostUsageCallbackMap;

  friend class ClientUsageTracker;
  void AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                          int64 limited_usage);
  void AccumulateClientGlobalUsage(AccumulateInfo* info,
                                   int64 usage,
                                   int64 unlimited_usage);
  void AccumulateClientHostUsage(AccumulateInfo* info,
                                 const std::string& host,
                                 int64 usage);

  const StorageType type_;
  ClientTrackerMap client_tracker_map_;

  UsageCallbackQueue global_limited_usage_callbacks_;
  GlobalUsageCallbackQueue global_usage_callbacks_;
  HostUsageCallbackMap host_usage_callbacks_;

  StorageMonitor* storage_monitor_;

  base::WeakPtrFactory<UsageTracker> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(UsageTracker);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_
