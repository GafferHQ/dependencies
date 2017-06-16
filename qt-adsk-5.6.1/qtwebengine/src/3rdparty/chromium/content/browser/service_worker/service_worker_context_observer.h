// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextObserver {
 public:
  struct ErrorInfo {
    ErrorInfo(const base::string16& message,
              int line,
              int column,
              const GURL& url)
        : error_message(message),
          line_number(line),
          column_number(column),
          source_url(url) {}
    const base::string16 error_message;
    const int line_number;
    const int column_number;
    const GURL source_url;
  };
  struct ConsoleMessage {
    ConsoleMessage(int source_identifier,
                   int message_level,
                   const base::string16& message,
                   int line_number,
                   const GURL& source_url)
        : source_identifier(source_identifier),
          message_level(message_level),
          message(message),
          line_number(line_number),
          source_url(source_url) {}
    const int source_identifier;
    const int message_level;
    const base::string16 message;
    const int line_number;
    const GURL source_url;
  };
  virtual void OnNewLiveRegistration(int64 registration_id,
                                     const GURL& pattern) {}
  virtual void OnNewLiveVersion(int64 version_id,
                                int64 registration_id,
                                const GURL& script_url) {}
  virtual void OnRunningStateChanged(
      int64 version_id,
      ServiceWorkerVersion::RunningStatus running_status) {}
  virtual void OnVersionStateChanged(int64 version_id,
                                     ServiceWorkerVersion::Status status) {}
  virtual void OnMainScriptHttpResponseInfoSet(
      int64 version_id,
      base::Time script_response_time,
      base::Time script_last_modified) {}
  virtual void OnErrorReported(int64 version_id,
                               int process_id,
                               int thread_id,
                               const ErrorInfo& info) {}
  virtual void OnReportConsoleMessage(int64 version_id,
                                      int process_id,
                                      int thread_id,
                                      const ConsoleMessage& message) {}
  virtual void OnControlleeAdded(int64 version_id,
                                 const std::string& uuid,
                                 int process_id,
                                 int route_id,
                                 ServiceWorkerProviderType type) {}
  virtual void OnControlleeRemoved(int64 version_id, const std::string& uuid) {}
  virtual void OnRegistrationStored(int64 registration_id,
                                    const GURL& pattern) {}
  virtual void OnRegistrationDeleted(int64 registration_id,
                                     const GURL& pattern) {}

  // Notified when the storage corruption recovery is completed and all stored
  // data is wiped out.
  virtual void OnStorageWiped() {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
