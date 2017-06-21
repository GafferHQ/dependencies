// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_working_set.h"

#include "base/logging.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_response.h"

namespace content {

AppCacheWorkingSet::AppCacheWorkingSet() : is_disabled_(false) {}

AppCacheWorkingSet::~AppCacheWorkingSet() {
  DCHECK(caches_.empty());
  DCHECK(groups_.empty());
  DCHECK(groups_by_origin_.empty());
}

void AppCacheWorkingSet::Disable() {
  if (is_disabled_)
    return;
  is_disabled_ = true;
  caches_.clear();
  groups_.clear();
  groups_by_origin_.clear();
  response_infos_.clear();
}

void AppCacheWorkingSet::AddCache(AppCache* cache) {
  if (is_disabled_)
    return;
  DCHECK(cache->cache_id() != kAppCacheNoCacheId);
  int64 cache_id = cache->cache_id();
  DCHECK(caches_.find(cache_id) == caches_.end());
  caches_.insert(CacheMap::value_type(cache_id, cache));
}

void AppCacheWorkingSet::RemoveCache(AppCache* cache) {
  caches_.erase(cache->cache_id());
}

void AppCacheWorkingSet::AddGroup(AppCacheGroup* group) {
  if (is_disabled_)
    return;
  const GURL& url = group->manifest_url();
  DCHECK(groups_.find(url) == groups_.end());
  groups_.insert(GroupMap::value_type(url, group));
  groups_by_origin_[url.GetOrigin()].insert(GroupMap::value_type(url, group));
}

void AppCacheWorkingSet::RemoveGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  groups_.erase(url);

  GURL origin_url = url.GetOrigin();
  GroupMap* groups_in_origin = GetMutableGroupsInOrigin(origin_url);
  if (groups_in_origin) {
    groups_in_origin->erase(url);
    if (groups_in_origin->empty())
      groups_by_origin_.erase(origin_url);
  }
}

void AppCacheWorkingSet::AddResponseInfo(AppCacheResponseInfo* info) {
  if (is_disabled_)
    return;
  DCHECK(info->response_id() != kAppCacheNoResponseId);
  int64 response_id = info->response_id();
  DCHECK(response_infos_.find(response_id) == response_infos_.end());
  response_infos_.insert(ResponseInfoMap::value_type(response_id, info));
}

void AppCacheWorkingSet::RemoveResponseInfo(AppCacheResponseInfo* info) {
  response_infos_.erase(info->response_id());
}

}  // namespace
