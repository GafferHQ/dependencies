// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACE_UPLOADER_H_
#define CONTENT_PUBLIC_BROWSER_TRACE_UPLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace content {

// Used to implement a trace upload service for use in about://tracing,
// which gets requested through the TracingDelegate.
class TraceUploader {
 public:
  // This should be called when the tracing is complete.
  // The bool denotes success or failure, the string is feedback
  // to show in the Tracing UI.
  typedef base::Callback<void(bool, const std::string&)> UploadDoneCallback;
  // Call this to update the progress UI with the current bytes uploaded,
  // as well as the total.
  typedef base::Callback<void(int64, int64)> UploadProgressCallback;

  virtual ~TraceUploader() {}

  enum UploadMode { COMPRESSED_UPLOAD, UNCOMPRESSED_UPLOAD };

  // Compresses and uploads the given file contents.
  virtual void DoUpload(const std::string& file_contents,
                        UploadMode upload_mode,
                        scoped_ptr<base::DictionaryValue> metadata,
                        const UploadProgressCallback& progress_callback,
                        const UploadDoneCallback& done_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACE_UPLOADER_H_
