// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_

#include <string>

#include "content/public/browser/download_item.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

enum DownloadType {
  SRC_ACTIVE_DOWNLOAD,
  SRC_HISTORY_IMPORT,
  SRC_SAVE_PAGE_AS
};

// Returns NetLog parameters when a DownloadItem is activated.
scoped_ptr<base::Value> ItemActivatedNetLogCallback(
    const DownloadItem* download_item,
    DownloadType download_type,
    const std::string* file_name,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is checked for danger.
scoped_ptr<base::Value> ItemCheckedNetLogCallback(
    DownloadDangerType danger_type,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is renamed.
scoped_ptr<base::Value> ItemRenamedNetLogCallback(
    const base::FilePath* old_filename,
    const base::FilePath* new_filename,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is interrupted.
scoped_ptr<base::Value> ItemInterruptedNetLogCallback(
    DownloadInterruptReason reason,
    int64 bytes_so_far,
    const std::string* hash_state,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is resumed.
scoped_ptr<base::Value> ItemResumingNetLogCallback(
    bool user_initiated,
    DownloadInterruptReason reason,
    int64 bytes_so_far,
    const std::string* hash_state,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is completing.
scoped_ptr<base::Value> ItemCompletingNetLogCallback(
    int64 bytes_so_far,
    const std::string* final_hash,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is finished.
scoped_ptr<base::Value> ItemFinishedNetLogCallback(
    bool auto_opened,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadItem is canceled.
scoped_ptr<base::Value> ItemCanceledNetLogCallback(
    int64 bytes_so_far,
    const std::string* hash_state,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadFile is opened.
scoped_ptr<base::Value> FileOpenedNetLogCallback(
    const base::FilePath* file_name,
    int64 start_offset,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadFile is opened.
scoped_ptr<base::Value> FileStreamDrainedNetLogCallback(
    size_t stream_size,
    size_t num_buffers,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a DownloadFile is renamed.
scoped_ptr<base::Value> FileRenamedNetLogCallback(
    const base::FilePath* old_filename,
    const base::FilePath* new_filename,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters when a File has an error.
scoped_ptr<base::Value> FileErrorNetLogCallback(
    const char* operation,
    net::Error net_error,
    net::NetLogCaptureMode capture_mode);

// Returns NetLog parameters for a download interruption.
scoped_ptr<base::Value> FileInterruptedNetLogCallback(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason,
    net::NetLogCaptureMode capture_mode);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_NET_LOG_PARAMETERS_H_
