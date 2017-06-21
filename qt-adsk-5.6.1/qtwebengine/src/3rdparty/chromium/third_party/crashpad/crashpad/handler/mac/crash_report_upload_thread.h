// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_HANDLER_MAC_CRASH_REPORT_UPLOAD_THREAD_H_
#define CRASHPAD_HANDLER_MAC_CRASH_REPORT_UPLOAD_THREAD_H_

#include "base/basictypes.h"

#include <pthread.h>

#include <string>

#include "client/crash_report_database.h"
#include "util/synchronization/semaphore.h"

namespace crashpad {

//! \brief A thread that processes pending crash reports in a
//!     CrashReportDatabase by uploading them or marking them as completed
//!     without upload, as desired.
//!
//! A producer of crash reports should notify an object of this class that a new
//! report has been added to the database by calling ReportPending().
//!
//! Independently of being triggered by ReportPending(), objects of this class
//! periodically examine the database for pending reports. This allows failed
//! upload attempts for reports left in the pending state to be retried. It also
//! catches reports that are added without a ReportPending() signal being
//! caught. This may happen if crash reports are added to the database by other
//! processes.
class CrashReportUploadThread {
 public:
  //! \brief Constructs a new object.
  //!
  //! \param[in] database The database to upload crash reports from.
  //! \param[in] url The URL of the server to upload crash reports to.
  CrashReportUploadThread(CrashReportDatabase* database,
                          const std::string& url);
  ~CrashReportUploadThread();

  //! \brief Starts a dedicated upload thread, which executes ThreadMain().
  //!
  //! This method may only be be called on a newly-constructed object or after
  //! a call to Stop().
  void Start();

  //! \brief Stops the upload thread.
  //!
  //! The upload thread will terminate after completing whatever task it is
  //! performing. If it is not performing any task, it will terminate
  //! immediately. This method blocks while waiting for the upload thread to
  //! terminate.
  //!
  //! This method must only be called after Start(). If Start() has been called,
  //! this method must be called before destroying an object of this class.
  //!
  //! This method may be called from any thread other than the upload thread.
  //! It is expected to only be called from the same thread that called Start().
  void Stop();

  //! \brief Informs the upload thread that a new pending report has been added
  //!     to the database.
  //!
  //! This method may be called from any thread.
  void ReportPending();

 private:
  //! \brief The result code from UploadReport().
  enum class UploadResult {
    //! \brief The crash report was uploaded successfully.
    kSuccess,

    //! \brief The crash report upload failed in such a way that recovery is
    //!     impossible.
    //!
    //! No further upload attempts should be made for the report.
    kPermanentFailure,

    //! \brief The crash report upload failed, but it might succeed again if
    //!     retried in the future.
    //!
    //! If the report has not already been retried too many times, the caller
    //! may arrange to call UploadReport() for the report again in the future,
    //! after a suitable delay.
    kRetry,
  };

  //! \brief Calls ProcessPendingReports() in response to ReportPending() having
  //!     been called on any thread, as well as periodically on a timer.
  void ThreadMain();

  //! \brief Obtains all pending reports from the database, and calls
  //!     ProcessPendingReport() to process each one.
  void ProcessPendingReports();

  //! \brief Processes a single pending report from the database.
  //!
  //! \param[in] report The crash report to process.
  //!
  //! If report upload is enabled, this method attempts to upload \a report by
  //! calling UplaodReport(). If the upload is successful, the report will be
  //! marked as “completed” in the database. If the upload fails and more
  //! retries are desired, the report’s upload-attempt count and
  //! last-upload-attempt time will be updated in the database and it will
  //! remain in the “pending” state. If the upload fails and no more retries are
  //! desired, or report upload is disabled, it will be marked as “completed” in
  //! the database without ever having been uploaded.
  void ProcessPendingReport(const CrashReportDatabase::Report& report);

  //! \brief Attempts to upload a crash report.
  //!
  //! \param[in] report The report to upload. The caller is responsible for
  //!     calling CrashReportDatabase::GetReportForUploading() before calling
  //!     this method, and for calling
  //!     CrashReportDatabase::RecordUploadAttempt() after calling this method.
  //! \param[out] response_body If the upload attempt is successful, this will
  //!     be set to the response body sent by the server. Breakpad-type servers
  //!     provide the crash ID assigned by the server in the response body.
  //!
  //! \return A member of UploadResult indicating the result of the upload
  //!    attempt.
  UploadResult UploadReport(const CrashReportDatabase::Report* report,
                            std::string* response_body);

  //! \brief Cals ThreadMain().
  //!
  //! \param[in] arg A pointer to the object on which to invoke ThreadMain().
  //!
  //! \return `nullptr`.
  static void* RunThreadMain(void* arg);

  std::string url_;
  CrashReportDatabase* database_;  // weak
  Semaphore semaphore_;  // TODO(mark): Use a condition variable instead?
  pthread_t thread_;
  bool running_;
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_MAC_CRASH_REPORT_UPLOAD_THREAD_H_
