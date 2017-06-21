// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_MANAGER_H_
#define STORAGE_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_MANAGER_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/fileapi/file_system_types.h"
#include "url/gurl.h"

namespace content {
class QuotaReservationManagerTest;
}

namespace storage {

class QuotaReservation;
class QuotaReservationBuffer;
class OpenFileHandle;
class OpenFileHandleContext;

class STORAGE_EXPORT QuotaReservationManager {
 public:
  // Callback for ReserveQuota. When this callback returns false, ReserveQuota
  // operation should be reverted.
  typedef base::Callback<bool(base::File::Error error, int64 delta)>
      ReserveQuotaCallback;

  // An abstraction of backing quota system.
  class STORAGE_EXPORT QuotaBackend {
   public:
    QuotaBackend() {}
    virtual ~QuotaBackend() {}

    // Reserves or reclaims |delta| of quota for |origin| and |type| pair.
    // Reserved quota should be counted as usage, but it should be on-memory
    // and be cleared by a browser restart.
    // Invokes |callback| upon completion with an error code.
    // |callback| should return false if it can't accept the reservation, in
    // that case, the backend should roll back the reservation.
    virtual void ReserveQuota(const GURL& origin,
                              FileSystemType type,
                              int64 delta,
                              const ReserveQuotaCallback& callback) = 0;

    // Reclaims |size| of quota for |origin| and |type|.
    virtual void ReleaseReservedQuota(const GURL& origin,
                                      FileSystemType type,
                                      int64 size) = 0;

    // Updates disk usage of |origin| and |type|.
    // Invokes |callback| upon completion with an error code.
    virtual void CommitQuotaUsage(const GURL& origin,
                                  FileSystemType type,
                                  int64 delta) = 0;

    virtual void IncrementDirtyCount(const GURL& origin,
                                    FileSystemType type) = 0;
    virtual void DecrementDirtyCount(const GURL& origin,
                                    FileSystemType type) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(QuotaBackend);
  };

  explicit QuotaReservationManager(scoped_ptr<QuotaBackend> backend);
  ~QuotaReservationManager();

  // The entry point of the quota reservation.  Creates new reservation object
  // for |origin| and |type|.
  scoped_refptr<QuotaReservation> CreateReservation(
      const GURL& origin,
      FileSystemType type);

 private:
  typedef std::map<std::pair<GURL, FileSystemType>, QuotaReservationBuffer*>
      ReservationBufferByOriginAndType;

  friend class QuotaReservation;
  friend class QuotaReservationBuffer;
  friend class content::QuotaReservationManagerTest;

  void ReserveQuota(const GURL& origin,
                    FileSystemType type,
                    int64 delta,
                    const ReserveQuotaCallback& callback);

  void ReleaseReservedQuota(const GURL& origin,
                            FileSystemType type,
                            int64 size);

  void CommitQuotaUsage(const GURL& origin,
                        FileSystemType type,
                        int64 delta);

  void IncrementDirtyCount(const GURL& origin, FileSystemType type);
  void DecrementDirtyCount(const GURL& origin, FileSystemType type);

  scoped_refptr<QuotaReservationBuffer> GetReservationBuffer(
      const GURL& origin,
      FileSystemType type);
  void ReleaseReservationBuffer(QuotaReservationBuffer* reservation_pool);

  scoped_ptr<QuotaBackend> backend_;

  // Not owned.  The destructor of ReservationBuffer should erase itself from
  // |reservation_buffers_| by calling ReleaseReservationBuffer.
  ReservationBufferByOriginAndType reservation_buffers_;

  base::SequenceChecker sequence_checker_;
  base::WeakPtrFactory<QuotaReservationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservationManager);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_QUOTA_QUOTA_RESERVATION_MANAGER_H_
