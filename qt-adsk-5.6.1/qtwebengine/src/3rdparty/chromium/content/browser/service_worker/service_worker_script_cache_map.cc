// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include "base/logging.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : owner_(owner), context_(context), weak_factory_(this) {
}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64 ServiceWorkerScriptCacheMap::LookupResourceId(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second.resource_id;
}

int64 ServiceWorkerScriptCacheMap::LookupResourceSize(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second.size_bytes;
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(
    const GURL& url, int64 resource_id) {
  DCHECK_EQ(kInvalidServiceWorkerResponseId, LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING)
      << owner_->status();
  if (!context_)
    return;  // Our storage has been wiped via DeleteAndStartOver.
  resource_map_[url] =
      ServiceWorkerDatabase::ResourceRecord(resource_id, url, -1);
  context_->storage()->StoreUncommittedResponseId(resource_id);
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url,
    int64 size_bytes,
    const net::URLRequestStatus& status,
    const std::string& status_message) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING ||
         owner_->status() == ServiceWorkerVersion::REDUNDANT);
  if (!context_)
    return;  // Our storage has been wiped via DeleteAndStartOver.
  if (!status.is_success()) {
    context_->storage()->DoomUncommittedResponse(LookupResourceId(url));
    resource_map_.erase(url);
    if (owner_->script_url() == url) {
      main_script_status_ = status;
      main_script_status_message_ = status_message;
    }
  } else {
    resource_map_[url].size_bytes = size_bytes;
  }
}

void ServiceWorkerScriptCacheMap::GetResources(
    std::vector<ServiceWorkerDatabase::ResourceRecord>* resources) {
  DCHECK(resources->empty());
  for (ResourceMap::const_iterator it = resource_map_.begin();
       it != resource_map_.end();
       ++it) {
    resources->push_back(it->second);
  }
}

void ServiceWorkerScriptCacheMap::SetResources(
    const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources) {
  DCHECK(resource_map_.empty());
  typedef std::vector<ServiceWorkerDatabase::ResourceRecord> RecordVector;
  for (RecordVector::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    resource_map_[it->url] = *it;
  }
}

void ServiceWorkerScriptCacheMap::WriteMetadata(
    const GURL& url,
    const std::vector<char>& data,
    const net::CompletionCallback& callback) {
  ResourceMap::iterator found = resource_map_.find(url);
  if (found == resource_map_.end() ||
      found->second.resource_id == kInvalidServiceWorkerResponseId) {
    callback.Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(data.size()));
  if (data.size())
    memmove(buffer->data(), &data[0], data.size());
  scoped_ptr<ServiceWorkerResponseMetadataWriter> writer;
  writer = context_->storage()->CreateResponseMetadataWriter(
      found->second.resource_id);
  ServiceWorkerResponseMetadataWriter* raw_writer = writer.get();
  raw_writer->WriteMetadata(
      buffer.get(), data.size(),
      base::Bind(&ServiceWorkerScriptCacheMap::OnMetadataWritten,
                 weak_factory_.GetWeakPtr(), Passed(&writer), callback));
}

void ServiceWorkerScriptCacheMap::ClearMetadata(
    const GURL& url,
    const net::CompletionCallback& callback) {
  WriteMetadata(url, std::vector<char>(), callback);
}

void ServiceWorkerScriptCacheMap::OnMetadataWritten(
    scoped_ptr<ServiceWorkerResponseMetadataWriter> writer,
    const net::CompletionCallback& callback,
    int result) {
  callback.Run(result);
}

}  // namespace content
