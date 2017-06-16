// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <string>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/referrer.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponseType.h"

namespace content {

namespace {

// This class ensures that the cache and the entry have a lifetime as long as
// the blob that is created to contain them.
class CacheStorageCacheDataHandle
    : public storage::BlobDataBuilder::DataHandle {
 public:
  CacheStorageCacheDataHandle(const scoped_refptr<CacheStorageCache>& cache,
                              disk_cache::ScopedEntryPtr entry)
      : cache_(cache), entry_(entry.Pass()) {}

 private:
  ~CacheStorageCacheDataHandle() override {}

  scoped_refptr<CacheStorageCache> cache_;
  disk_cache::ScopedEntryPtr entry_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCacheDataHandle);
};

typedef base::Callback<void(scoped_ptr<CacheMetadata>)> MetadataCallback;

enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY };

// The maximum size of an individual cache. Ultimately cache size is controlled
// per-origin.
const int kMaxCacheBytes = 512 * 1024 * 1024;

void NotReachedCompletionCallback(int rv) {
  NOTREACHED();
}

blink::WebServiceWorkerResponseType ProtoResponseTypeToWebResponseType(
    CacheResponse::ResponseType response_type) {
  switch (response_type) {
    case CacheResponse::BASIC_TYPE:
      return blink::WebServiceWorkerResponseTypeBasic;
    case CacheResponse::CORS_TYPE:
      return blink::WebServiceWorkerResponseTypeCORS;
    case CacheResponse::DEFAULT_TYPE:
      return blink::WebServiceWorkerResponseTypeDefault;
    case CacheResponse::ERROR_TYPE:
      return blink::WebServiceWorkerResponseTypeError;
    case CacheResponse::OPAQUE_TYPE:
      return blink::WebServiceWorkerResponseTypeOpaque;
  }
  NOTREACHED();
  return blink::WebServiceWorkerResponseTypeOpaque;
}

CacheResponse::ResponseType WebResponseTypeToProtoResponseType(
    blink::WebServiceWorkerResponseType response_type) {
  switch (response_type) {
    case blink::WebServiceWorkerResponseTypeBasic:
      return CacheResponse::BASIC_TYPE;
    case blink::WebServiceWorkerResponseTypeCORS:
      return CacheResponse::CORS_TYPE;
    case blink::WebServiceWorkerResponseTypeDefault:
      return CacheResponse::DEFAULT_TYPE;
    case blink::WebServiceWorkerResponseTypeError:
      return CacheResponse::ERROR_TYPE;
    case blink::WebServiceWorkerResponseTypeOpaque:
      return CacheResponse::OPAQUE_TYPE;
  }
  NOTREACHED();
  return CacheResponse::OPAQUE_TYPE;
}

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback);
void ReadMetadataDidReadMetadata(
    disk_cache::Entry* entry,
    const MetadataCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv);

bool VaryMatches(const ServiceWorkerHeaderMap& request,
                 const ServiceWorkerHeaderMap& cached_request,
                 const ServiceWorkerHeaderMap& response) {
  ServiceWorkerHeaderMap::const_iterator vary_iter = response.find("vary");
  if (vary_iter == response.end())
    return true;

  for (const std::string& trimmed :
       base::SplitString(vary_iter->second, ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (trimmed == "*")
      return false;

    ServiceWorkerHeaderMap::const_iterator request_iter = request.find(trimmed);
    ServiceWorkerHeaderMap::const_iterator cached_request_iter =
        cached_request.find(trimmed);

    // If the header exists in one but not the other, no match.
    if ((request_iter == request.end()) !=
        (cached_request_iter == cached_request.end()))
      return false;

    // If the header exists in one, it exists in both. Verify that the values
    // are equal.
    if (request_iter != request.end() &&
        request_iter->second != cached_request_iter->second)
      return false;
  }

  return true;
}

void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(entry->GetDataSize(INDEX_HEADERS)));

  net::CompletionCallback read_header_callback =
      base::Bind(ReadMetadataDidReadMetadata, entry, callback, buffer);

  int read_rv = entry->ReadData(INDEX_HEADERS, 0, buffer.get(), buffer->size(),
                                read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_header_callback.Run(read_rv);
}

void ReadMetadataDidReadMetadata(
    disk_cache::Entry* entry,
    const MetadataCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv) {
  if (rv != buffer->size()) {
    callback.Run(scoped_ptr<CacheMetadata>());
    return;
  }

  scoped_ptr<CacheMetadata> metadata(new CacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    callback.Run(scoped_ptr<CacheMetadata>());
    return;
  }

  callback.Run(metadata.Pass());
}

}  // namespace

// The state needed to pass between CacheStorageCache::Keys callbacks.
struct CacheStorageCache::KeysContext {
  explicit KeysContext(const CacheStorageCache::RequestsCallback& callback)
      : original_callback(callback),
        out_keys(new CacheStorageCache::Requests()),
        enumerated_entry(NULL) {}

  ~KeysContext() {
    for (size_t i = 0, max = entries.size(); i < max; ++i)
      entries[i]->Close();
    if (enumerated_entry)
      enumerated_entry->Close();
  }

  // The callback passed to the Keys() function.
  CacheStorageCache::RequestsCallback original_callback;

  // The vector of open entries in the backend.
  Entries entries;

  // The output of the Keys function.
  scoped_ptr<CacheStorageCache::Requests> out_keys;

  // Used for enumerating cache entries.
  scoped_ptr<disk_cache::Backend::Iterator> backend_iterator;
  disk_cache::Entry* enumerated_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeysContext);
};

// The state needed to pass between CacheStorageCache::Put callbacks.
struct CacheStorageCache::PutContext {
  PutContext(
      const GURL& origin,
      scoped_ptr<ServiceWorkerFetchRequest> request,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle,
      const CacheStorageCache::ErrorCallback& callback,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy)
      : origin(origin),
        request(request.Pass()),
        response(response.Pass()),
        blob_data_handle(blob_data_handle.Pass()),
        callback(callback),
        request_context_getter(request_context_getter),
        quota_manager_proxy(quota_manager_proxy) {}

  // Input parameters to the Put function.
  GURL origin;
  scoped_ptr<ServiceWorkerFetchRequest> request;
  scoped_ptr<ServiceWorkerResponse> response;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;
  CacheStorageCache::ErrorCallback callback;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy;
  disk_cache::ScopedEntryPtr cache_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

// static
scoped_refptr<CacheStorageCache> CacheStorageCache::CreateMemoryCache(
    const GURL& origin,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_refptr(
      new CacheStorageCache(origin, base::FilePath(), request_context_getter,
                            quota_manager_proxy, blob_context));
}

// static
scoped_refptr<CacheStorageCache> CacheStorageCache::CreatePersistentCache(
    const GURL& origin,
    const base::FilePath& path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_refptr(new CacheStorageCache(
      origin, path, request_context_getter, quota_manager_proxy, blob_context));
}

CacheStorageCache::~CacheStorageCache() {
}

base::WeakPtr<CacheStorageCache> CacheStorageCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CacheStorageCache::Match(scoped_ptr<ServiceWorkerFetchRequest> request,
                              const ResponseCallback& callback) {
  switch (backend_state_) {
    case BACKEND_UNINITIALIZED:
      InitBackend();
      break;
    case BACKEND_CLOSED:
      callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                   scoped_ptr<ServiceWorkerResponse>(),
                   scoped_ptr<storage::BlobDataHandle>());
      return;
    case BACKEND_OPEN:
      DCHECK(backend_);
      break;
  }

  ResponseCallback pending_callback =
      base::Bind(&CacheStorageCache::PendingResponseCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::MatchImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(request.Pass()), pending_callback));
}

void CacheStorageCache::BatchOperation(
    const std::vector<CacheStorageBatchOperation>& operations,
    const ErrorCallback& callback) {
  switch (backend_state_) {
    case BACKEND_UNINITIALIZED:
      InitBackend();
      break;
    case BACKEND_CLOSED:
      callback.Run(CACHE_STORAGE_ERROR_STORAGE);
      return;
    case BACKEND_OPEN:
      DCHECK(backend_);
      break;
  }

  scoped_ptr<ErrorCallback> callback_copy(new ErrorCallback(callback));
  ErrorCallback* callback_ptr = callback_copy.get();
  base::Closure barrier_closure = base::BarrierClosure(
      operations.size(), base::Bind(&CacheStorageCache::BatchDidAllOperations,
                                    this, base::Passed(callback_copy.Pass())));
  ErrorCallback completion_callback =
      base::Bind(&CacheStorageCache::BatchDidOneOperation, this,
                 barrier_closure, callback_ptr);

  for (const auto& operation : operations) {
    switch (operation.operation_type) {
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT:
        Put(operation, completion_callback);
        break;
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE:
        DCHECK_EQ(1u, operations.size());
        Delete(operation, completion_callback);
        break;
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED:
        NOTREACHED();
        // TODO(nhiroki): This should return "TypeError".
        // http://crbug.com/425505
        completion_callback.Run(CACHE_STORAGE_ERROR_STORAGE);
        break;
    }
  }
}

void CacheStorageCache::BatchDidOneOperation(
    const base::Closure& barrier_closure,
    ErrorCallback* callback,
    CacheStorageError error) {
  if (callback->is_null() || error == CACHE_STORAGE_OK) {
    barrier_closure.Run();
    return;
  }
  callback->Run(error);
  callback->Reset();  // Only call the callback once.

  barrier_closure.Run();
}

void CacheStorageCache::BatchDidAllOperations(
    scoped_ptr<ErrorCallback> callback) {
  if (callback->is_null())
    return;
  callback->Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::Keys(const RequestsCallback& callback) {
  switch (backend_state_) {
    case BACKEND_UNINITIALIZED:
      InitBackend();
      break;
    case BACKEND_CLOSED:
      callback.Run(CACHE_STORAGE_ERROR_STORAGE, scoped_ptr<Requests>());
      return;
    case BACKEND_OPEN:
      DCHECK(backend_);
      break;
  }

  RequestsCallback pending_callback =
      base::Bind(&CacheStorageCache::PendingRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorageCache::KeysImpl,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           pending_callback));
}

void CacheStorageCache::Close(const base::Closure& callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_)
      << "Was CacheStorageCache::Close() called twice?";

  base::Closure pending_callback =
      base::Bind(&CacheStorageCache::PendingClosure,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  scheduler_->ScheduleOperation(base::Bind(&CacheStorageCache::CloseImpl,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           pending_callback));
}

int64 CacheStorageCache::MemoryBackedSize() const {
  if (backend_state_ != BACKEND_OPEN || !memory_only_)
    return 0;

  scoped_ptr<disk_cache::Backend::Iterator> backend_iter =
      backend_->CreateIterator();
  disk_cache::Entry* entry = nullptr;

  int64 sum = 0;

  std::vector<disk_cache::Entry*> entries;
  int rv = net::OK;
  while ((rv = backend_iter->OpenNextEntry(
              &entry, base::Bind(NotReachedCompletionCallback))) == net::OK) {
    entries.push_back(entry);  // Open the entries without mutating them.
  }
  DCHECK_NE(net::ERR_IO_PENDING, rv)
      << "Memory cache operations should be synchronous.";

  for (disk_cache::Entry* entry : entries) {
    sum += entry->GetDataSize(INDEX_HEADERS) +
           entry->GetDataSize(INDEX_RESPONSE_BODY);
    entry->Close();
  }

  return sum;
}

CacheStorageCache::CacheStorageCache(
    const GURL& origin,
    const base::FilePath& path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : origin_(origin),
      path_(path),
      request_context_getter_(request_context_getter),
      quota_manager_proxy_(quota_manager_proxy),
      blob_storage_context_(blob_context),
      backend_state_(BACKEND_UNINITIALIZED),
      scheduler_(new CacheStorageScheduler()),
      initializing_(false),
      memory_only_(path.empty()),
      weak_ptr_factory_(this) {
}

void CacheStorageCache::MatchImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                                  const ResponseCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<disk_cache::Entry*> scoped_entry_ptr(new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  ServiceWorkerFetchRequest* request_ptr = request.get();

  net::CompletionCallback open_entry_callback =
      base::Bind(&CacheStorageCache::MatchDidOpenEntry,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(request.Pass()),
                 callback, base::Passed(scoped_entry_ptr.Pass()));

  int rv = backend_->OpenEntry(request_ptr->url.spec(), entry_ptr,
                               open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::MatchDidOpenEntry(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ResponseCallback& callback,
    scoped_ptr<disk_cache::Entry*> entry_ptr,
    int rv) {
  if (rv != net::OK) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }
  disk_cache::ScopedEntryPtr entry(*entry_ptr);

  MetadataCallback headers_callback = base::Bind(
      &CacheStorageCache::MatchDidReadMetadata, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(request.Pass()), callback, base::Passed(entry.Pass()));

  ReadMetadata(*entry_ptr, headers_callback);
}

void CacheStorageCache::MatchDidReadMetadata(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ResponseCallback& callback,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<CacheMetadata> metadata) {
  if (!metadata) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<ServiceWorkerResponse> response(new ServiceWorkerResponse(
      request->url, metadata->response().status_code(),
      metadata->response().status_text(),
      ProtoResponseTypeToWebResponseType(metadata->response().response_type()),
      ServiceWorkerHeaderMap(), "", 0, GURL(),
      blink::WebServiceWorkerResponseErrorUnknown));

  if (metadata->response().has_url())
    response->url = GURL(metadata->response().url());

  for (int i = 0; i < metadata->response().headers_size(); ++i) {
    const CacheHeaderMap header = metadata->response().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    response->headers.insert(std::make_pair(header.name(), header.value()));
  }

  ServiceWorkerHeaderMap cached_request_headers;
  for (int i = 0; i < metadata->request().headers_size(); ++i) {
    const CacheHeaderMap header = metadata->request().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    cached_request_headers[header.name()] = header.value();
  }

  if (!VaryMatches(request->headers, cached_request_headers,
                   response->headers)) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  if (entry->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
    callback.Run(CACHE_STORAGE_OK, response.Pass(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  if (!blob_storage_context_) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  // Create a blob with the response body data.
  response->blob_size = entry->GetDataSize(INDEX_RESPONSE_BODY);
  response->blob_uuid = base::GenerateGUID();
  storage::BlobDataBuilder blob_data(response->blob_uuid);

  disk_cache::Entry* temp_entry = entry.get();
  blob_data.AppendDiskCacheEntry(
      new CacheStorageCacheDataHandle(this, entry.Pass()), temp_entry,
      INDEX_RESPONSE_BODY);
  scoped_ptr<storage::BlobDataHandle> blob_data_handle(
      blob_storage_context_->AddFinishedBlob(&blob_data));
  callback.Run(CACHE_STORAGE_OK, response.Pass(), blob_data_handle.Pass());
}

void CacheStorageCache::Put(const CacheStorageBatchOperation& operation,
                            const ErrorCallback& callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT, operation.operation_type);

  scoped_ptr<ServiceWorkerFetchRequest> request(new ServiceWorkerFetchRequest(
      operation.request.url, operation.request.method,
      operation.request.headers, operation.request.referrer,
      operation.request.is_reload));

  // We don't support streaming for cache.
  DCHECK(operation.response.stream_url.is_empty());
  scoped_ptr<ServiceWorkerResponse> response(new ServiceWorkerResponse(
      operation.response.url, operation.response.status_code,
      operation.response.status_text, operation.response.response_type,
      operation.response.headers, operation.response.blob_uuid,
      operation.response.blob_size, operation.response.stream_url,
      operation.response.error));

  scoped_ptr<storage::BlobDataHandle> blob_data_handle;

  if (!response->blob_uuid.empty()) {
    if (!blob_storage_context_) {
      callback.Run(CACHE_STORAGE_ERROR_STORAGE);
      return;
    }
    blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response->blob_uuid);
    if (!blob_data_handle) {
      callback.Run(CACHE_STORAGE_ERROR_STORAGE);
      return;
    }
  }

  ErrorCallback pending_callback =
      base::Bind(&CacheStorageCache::PendingErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  scoped_ptr<PutContext> put_context(new PutContext(
      origin_, request.Pass(), response.Pass(), blob_data_handle.Pass(),
      pending_callback, request_context_getter_, quota_manager_proxy_));

  scheduler_->ScheduleOperation(base::Bind(&CacheStorageCache::PutImpl,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           base::Passed(put_context.Pass())));
}

void CacheStorageCache::PutImpl(scoped_ptr<PutContext> put_context) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  scoped_ptr<ServiceWorkerFetchRequest> request_copy(
      new ServiceWorkerFetchRequest(*put_context->request));

  DeleteImpl(request_copy.Pass(), base::Bind(&CacheStorageCache::PutDidDelete,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             base::Passed(put_context.Pass())));
}

void CacheStorageCache::PutDidDelete(scoped_ptr<PutContext> put_context,
                                     CacheStorageError delete_error) {
  if (backend_state_ != BACKEND_OPEN) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  scoped_ptr<disk_cache::Entry*> scoped_entry_ptr(new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  ServiceWorkerFetchRequest* request_ptr = put_context->request.get();
  disk_cache::Backend* backend_ptr = backend_.get();

  net::CompletionCallback create_entry_callback = base::Bind(
      &CacheStorageCache::PutDidCreateEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(scoped_entry_ptr.Pass()), base::Passed(put_context.Pass()));

  int create_rv = backend_ptr->CreateEntry(request_ptr->url.spec(), entry_ptr,
                                           create_entry_callback);

  if (create_rv != net::ERR_IO_PENDING)
    create_entry_callback.Run(create_rv);
}

void CacheStorageCache::PutDidCreateEntry(
    scoped_ptr<disk_cache::Entry*> entry_ptr,
    scoped_ptr<PutContext> put_context,
    int rv) {
  if (rv != net::OK) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_EXISTS);
    return;
  }
  put_context->cache_entry.reset(*entry_ptr);

  CacheMetadata metadata;
  CacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->request->headers.begin();
       it != put_context->request->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    CacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  CacheResponse* response_metadata = metadata.mutable_response();
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(
      WebResponseTypeToProtoResponseType(put_context->response->response_type));
  response_metadata->set_url(put_context->response->url.spec());
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->response->headers.begin();
       it != put_context->response->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    CacheHeaderMap* header_map = response_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  scoped_ptr<std::string> serialized(new std::string());
  if (!metadata.SerializeToString(serialized.get())) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer(
      new net::StringIOBuffer(serialized.Pass()));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = put_context->cache_entry.get();

  net::CompletionCallback write_headers_callback = base::Bind(
      &CacheStorageCache::PutDidWriteHeaders, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(put_context.Pass()), buffer->size());

  rv = temp_entry_ptr->WriteData(INDEX_HEADERS, 0 /* offset */, buffer.get(),
                                 buffer->size(), write_headers_callback,
                                 true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    write_headers_callback.Run(rv);
}

void CacheStorageCache::PutDidWriteHeaders(scoped_ptr<PutContext> put_context,
                                           int expected_bytes,
                                           int rv) {
  if (rv != expected_bytes) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  // The metadata is written, now for the response content. The data is streamed
  // from the blob into the cache entry.

  if (put_context->response->blob_uuid.empty()) {
    if (put_context->quota_manager_proxy.get()) {
      put_context->quota_manager_proxy->NotifyStorageModified(
          storage::QuotaClient::kServiceWorkerCache, put_context->origin,
          storage::kStorageTypeTemporary,
          put_context->cache_entry->GetDataSize(INDEX_HEADERS));
    }

    put_context->callback.Run(CACHE_STORAGE_OK);
    return;
  }

  DCHECK(put_context->blob_data_handle);

  disk_cache::ScopedEntryPtr entry(put_context->cache_entry.Pass());
  put_context->cache_entry = NULL;

  CacheStorageBlobToDiskCache* blob_to_cache =
      new CacheStorageBlobToDiskCache();
  BlobToDiskCacheIDMap::KeyType blob_to_cache_key =
      active_blob_to_disk_cache_writers_.Add(blob_to_cache);

  // Grab some pointers before passing put_context in Bind.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter =
      put_context->request_context_getter;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle =
      put_context->blob_data_handle.Pass();

  blob_to_cache->StreamBlobToCache(
      entry.Pass(), INDEX_RESPONSE_BODY, request_context_getter,
      blob_data_handle.Pass(),
      base::Bind(&CacheStorageCache::PutDidWriteBlobToCache,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(put_context.Pass()), blob_to_cache_key));
}

void CacheStorageCache::PutDidWriteBlobToCache(
    scoped_ptr<PutContext> put_context,
    BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
    disk_cache::ScopedEntryPtr entry,
    bool success) {
  DCHECK(entry);
  put_context->cache_entry = entry.Pass();

  active_blob_to_disk_cache_writers_.Remove(blob_to_cache_key);

  if (!success) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  if (put_context->quota_manager_proxy.get()) {
    put_context->quota_manager_proxy->NotifyStorageModified(
        storage::QuotaClient::kServiceWorkerCache, put_context->origin,
        storage::kStorageTypeTemporary,
        put_context->cache_entry->GetDataSize(INDEX_HEADERS) +
            put_context->cache_entry->GetDataSize(INDEX_RESPONSE_BODY));
  }

  put_context->callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::Delete(const CacheStorageBatchOperation& operation,
                               const ErrorCallback& callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE,
            operation.operation_type);

  scoped_ptr<ServiceWorkerFetchRequest> request(new ServiceWorkerFetchRequest(
      operation.request.url, operation.request.method,
      operation.request.headers, operation.request.referrer,
      operation.request.is_reload));

  ErrorCallback pending_callback =
      base::Bind(&CacheStorageCache::PendingErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::DeleteImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(request.Pass()), pending_callback));
}

void CacheStorageCache::DeleteImpl(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ErrorCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }
  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  ServiceWorkerFetchRequest* request_ptr = request.get();

  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::DeleteDidOpenEntry, weak_ptr_factory_.GetWeakPtr(),
      origin_, base::Passed(request.Pass()), callback,
      base::Passed(entry.Pass()), quota_manager_proxy_);

  int rv = backend_->OpenEntry(request_ptr->url.spec(), entry_ptr,
                               open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::DeleteDidOpenEntry(
    const GURL& origin,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ErrorCallback& callback,
    scoped_ptr<disk_cache::Entry*> entry_ptr,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    int rv) {
  if (rv != net::OK) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  DCHECK(entry_ptr);
  disk_cache::ScopedEntryPtr entry(*entry_ptr);

  if (quota_manager_proxy.get()) {
    quota_manager_proxy->NotifyStorageModified(
        storage::QuotaClient::kServiceWorkerCache, origin,
        storage::kStorageTypeTemporary,
        -1 * (entry->GetDataSize(INDEX_HEADERS) +
              entry->GetDataSize(INDEX_RESPONSE_BODY)));
  }

  entry->Doom();
  callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::KeysImpl(const RequestsCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE, scoped_ptr<Requests>());
    return;
  }

  // 1. Iterate through all of the entries, open them, and add them to a vector.
  // 2. For each open entry:
  //  2.1. Read the headers into a protobuf.
  //  2.2. Copy the protobuf into a ServiceWorkerFetchRequest (a "key").
  //  2.3. Push the response into a vector of requests to be returned.
  // 3. Return the vector of requests (keys).

  // The entries have to be loaded into a vector first because enumeration loops
  // forever if you read data from a cache entry while enumerating.

  scoped_ptr<KeysContext> keys_context(new KeysContext(callback));

  keys_context->backend_iterator = backend_->CreateIterator();
  disk_cache::Backend::Iterator& iterator = *keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;

  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::KeysDidOpenNextEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(keys_context.Pass()));

  int rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::KeysDidOpenNextEntry(
    scoped_ptr<KeysContext> keys_context,
    int rv) {
  if (rv == net::ERR_FAILED) {
    DCHECK(!keys_context->enumerated_entry);
    // Enumeration is complete, extract the requests from the entries.
    Entries::iterator iter = keys_context->entries.begin();
    KeysProcessNextEntry(keys_context.Pass(), iter);
    return;
  }

  if (rv < 0) {
    keys_context->original_callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                                        scoped_ptr<Requests>());
    return;
  }

  if (backend_state_ != BACKEND_OPEN) {
    keys_context->original_callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                                        scoped_ptr<Requests>());
    return;
  }

  // Store the entry.
  keys_context->entries.push_back(keys_context->enumerated_entry);
  keys_context->enumerated_entry = NULL;

  // Enumerate the next entry.
  disk_cache::Backend::Iterator& iterator = *keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;
  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::KeysDidOpenNextEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(keys_context.Pass()));

  rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::KeysProcessNextEntry(
    scoped_ptr<KeysContext> keys_context,
    const Entries::iterator& iter) {
  if (iter == keys_context->entries.end()) {
    // All done. Return all of the keys.
    keys_context->original_callback.Run(CACHE_STORAGE_OK,
                                        keys_context->out_keys.Pass());
    return;
  }

  ReadMetadata(*iter, base::Bind(&CacheStorageCache::KeysDidReadMetadata,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 base::Passed(keys_context.Pass()), iter));
}

void CacheStorageCache::KeysDidReadMetadata(
    scoped_ptr<KeysContext> keys_context,
    const Entries::iterator& iter,
    scoped_ptr<CacheMetadata> metadata) {
  disk_cache::Entry* entry = *iter;

  if (metadata) {
    keys_context->out_keys->push_back(ServiceWorkerFetchRequest(
        GURL(entry->GetKey()), metadata->request().method(),
        ServiceWorkerHeaderMap(), Referrer(), false));

    ServiceWorkerHeaderMap& req_headers =
        keys_context->out_keys->back().headers;

    for (int i = 0; i < metadata->request().headers_size(); ++i) {
      const CacheHeaderMap header = metadata->request().headers(i);
      DCHECK_EQ(std::string::npos, header.name().find('\0'));
      DCHECK_EQ(std::string::npos, header.value().find('\0'));
      req_headers.insert(std::make_pair(header.name(), header.value()));
    }
  } else {
    entry->Doom();
  }

  KeysProcessNextEntry(keys_context.Pass(), iter + 1);
}

void CacheStorageCache::CloseImpl(const base::Closure& callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_);

  backend_state_ = BACKEND_CLOSED;
  backend_.reset();
  callback.Run();
}

void CacheStorageCache::CreateBackend(const ErrorCallback& callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  scoped_ptr<ScopedBackendPtr> backend_ptr(new ScopedBackendPtr());

  // Temporary pointer so that backend_ptr can be Pass()'d in Bind below.
  ScopedBackendPtr* backend = backend_ptr.get();

  net::CompletionCallback create_cache_callback =
      base::Bind(&CacheStorageCache::CreateBackendDidCreate,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(backend_ptr.Pass()));

  // TODO(jkarlin): Use the cache task runner that ServiceWorkerCacheCore
  // has for disk caches.
  int rv = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, path_, kMaxCacheBytes,
      false, /* force */
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE).get(),
      NULL, backend, create_cache_callback);
  if (rv != net::ERR_IO_PENDING)
    create_cache_callback.Run(rv);
}

void CacheStorageCache::CreateBackendDidCreate(
    const CacheStorageCache::ErrorCallback& callback,
    scoped_ptr<ScopedBackendPtr> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  backend_ = backend_ptr->Pass();
  callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::InitBackend() {
  DCHECK_EQ(BACKEND_UNINITIALIZED, backend_state_);

  if (initializing_)
    return;

  DCHECK(!scheduler_->ScheduledOperations());
  initializing_ = true;

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorageCache::CreateBackend, weak_ptr_factory_.GetWeakPtr(),
      base::Bind(&CacheStorageCache::InitDone,
                 weak_ptr_factory_.GetWeakPtr())));
}

void CacheStorageCache::InitDone(CacheStorageError error) {
  initializing_ = false;
  backend_state_ = (error == CACHE_STORAGE_OK && backend_ &&
                    backend_state_ == BACKEND_UNINITIALIZED)
                       ? BACKEND_OPEN
                       : BACKEND_CLOSED;

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.InitBackendResult", error,
                            CACHE_STORAGE_ERROR_LAST + 1);

  scheduler_->CompleteOperationAndRunNext();
}

void CacheStorageCache::PendingClosure(const base::Closure& callback) {
  base::WeakPtr<CacheStorageCache> cache = weak_ptr_factory_.GetWeakPtr();

  callback.Run();
  if (cache)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorageCache::PendingErrorCallback(const ErrorCallback& callback,
                                             CacheStorageError error) {
  base::WeakPtr<CacheStorageCache> cache = weak_ptr_factory_.GetWeakPtr();

  callback.Run(error);
  if (cache)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorageCache::PendingResponseCallback(
    const ResponseCallback& callback,
    CacheStorageError error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  base::WeakPtr<CacheStorageCache> cache = weak_ptr_factory_.GetWeakPtr();

  callback.Run(error, response.Pass(), blob_data_handle.Pass());
  if (cache)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorageCache::PendingRequestsCallback(
    const RequestsCallback& callback,
    CacheStorageError error,
    scoped_ptr<Requests> requests) {
  base::WeakPtr<CacheStorageCache> cache = weak_ptr_factory_.GetWeakPtr();

  callback.Run(error, requests.Pass());
  if (cache)
    scheduler_->CompleteOperationAndRunNext();
}

}  // namespace content
