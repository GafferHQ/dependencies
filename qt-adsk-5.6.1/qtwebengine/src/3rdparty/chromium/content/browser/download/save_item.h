// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/browser/download/save_types.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {
class SavePackage;

// One SaveItem per save file. This is the model class that stores all the
// state for one save file.
class SaveItem {
 public:
  enum SaveState {
    WAIT_START,
    IN_PROGRESS,
    COMPLETE,
    CANCELED
  };

  SaveItem(const GURL& url,
           const Referrer& referrer,
           SavePackage* package,
           SaveFileCreateInfo::SaveFileSource save_source);

  ~SaveItem();

  void Start();

  // Received a new chunk of data.
  void Update(int64 bytes_so_far);

  // Cancel saving item.
  void Cancel();

  // Saving operation completed.
  void Finish(int64 size, bool is_success);

  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete() const;

  // Update path for SaveItem, the actual file is renamed on the file thread.
  void Rename(const base::FilePath& full_path);

  void SetSaveId(int32 save_id);

  void SetTotalBytes(int64 total_bytes);

  // Accessors.
  SaveState state() const { return state_; }
  const base::FilePath& full_path() const { return full_path_; }
  const base::FilePath& file_name() const { return file_name_; }
  const GURL& url() const { return url_; }
  const Referrer& referrer() const { return referrer_; }
  int64 total_bytes() const { return total_bytes_; }
  int64 received_bytes() const { return received_bytes_; }
  int32 save_id() const { return save_id_; }
  bool has_final_name() const { return has_final_name_; }
  bool success() const { return is_success_; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return save_source_;
  }
  SavePackage* package() const { return package_; }

 private:
  // Internal helper for maintaining consistent received and total sizes.
  void UpdateSize(int64 size);

  // Request ID assigned by the ResourceDispatcherHost.
  int32 save_id_;

  // Full path to the save item file.
  base::FilePath full_path_;

  // Short display version of the file.
  base::FilePath file_name_;

  // The URL for this save item.
  GURL url_;
  Referrer referrer_;

  // Total bytes expected.
  int64 total_bytes_;

  // Current received bytes.
  int64 received_bytes_;

  // The current state of this save item.
  SaveState state_;

  // Specifies if this name is a final or not.
  bool has_final_name_;

  // Flag indicates whether SaveItem has error while in saving process.
  bool is_success_;

  SaveFileCreateInfo::SaveFileSource save_source_;

  // Our owning object.
  SavePackage* package_;

  DISALLOW_COPY_AND_ASSIGN(SaveItem);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_ITEM_H_
