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

#include "client/crash_report_database.h"

#include <errno.h>
#include <fcntl.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/posix/eintr_wrapper.h"
#include "base/scoped_generic.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "client/settings.h"
#include "util/file/file_io.h"
#include "util/mac/xattr.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

namespace {

const char kWriteDirectory[] = "new";
const char kUploadPendingDirectory[] = "pending";
const char kCompletedDirectory[] = "completed";

const char kSettings[] = "settings.dat";

const char* const kReportDirectories[] = {
  kWriteDirectory,
  kUploadPendingDirectory,
  kCompletedDirectory,
};

const char kCrashReportFileExtension[] = "dmp";

const char kXattrUUID[] = "uuid";
const char kXattrCollectorID[] = "id";
const char kXattrCreationTime[] = "creation_time";
const char kXattrIsUploaded[] = "uploaded";
const char kXattrLastUploadTime[] = "last_upload_time";
const char kXattrUploadAttemptCount[] = "upload_count";

const char kXattrDatabaseInitialized[] = "initialized";

// Ensures that the node at |path| is a directory, and creates it if it does
// not exist. If the |path| points to a file, rather than a directory, or the
// directory could not be created, returns false. Otherwise, returns true,
// indicating that |path| already was or now is a directory.
bool CreateOrEnsureDirectoryExists(const base::FilePath& path) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return true;
  } else if (errno == EEXIST) {
    struct stat st;
    if (stat(path.value().c_str(), &st) != 0) {
      PLOG(ERROR) << "stat";
      return false;
    }
    if (S_ISDIR(st.st_mode)) {
      return true;
    } else {
      LOG(ERROR) << "not a directory";
      return false;
    }
  } else {
    PLOG(ERROR) << "mkdir";
    return false;
  }
}

//! \brief A CrashReportDatabase that uses HFS+ extended attributes to store
//!     report metadata.
//!
//! The database maintains three directories of reports: `"new"` to hold crash
//! reports that are in the process of being written, `"completed"` to hold
//! reports that have been written and are awaiting upload, and `"uploaded"` to
//! hold reports successfully uploaded to a collection server. If the user has
//! opted out of report collection, reports will still be written and moved
//! to the completed directory, but they just will not be uploaded.
//!
//! The database stores its metadata in extended filesystem attributes. To
//! ensure safe access, the report file is locked using `O_EXLOCK` during all
//! extended attribute operations. The lock should be obtained using
//! ObtainReportLock().
class CrashReportDatabaseMac : public CrashReportDatabase {
 public:
  explicit CrashReportDatabaseMac(const base::FilePath& path);
  virtual ~CrashReportDatabaseMac();

  bool Initialize();

  // CrashReportDatabase:
  Settings* GetSettings() override;
  OperationStatus PrepareNewCrashReport(NewReport** report) override;
  OperationStatus FinishedWritingCrashReport(NewReport* report,
                                             UUID* uuid) override;
  OperationStatus ErrorWritingCrashReport(NewReport* report) override;
  OperationStatus LookUpCrashReport(const UUID& uuid,
                                    Report* report) override;
  OperationStatus GetPendingReports(std::vector<Report>* reports) override;
  OperationStatus GetCompletedReports(std::vector<Report>* reports) override;
  OperationStatus GetReportForUploading(const UUID& uuid,
                                        const Report** report) override;
  OperationStatus RecordUploadAttempt(const Report* report,
                                      bool successful,
                                      const std::string& id) override;
  OperationStatus SkipReportUpload(const UUID& uuid) override;

 private:
  //! \brief A private extension of the Report class that maintains bookkeeping
  //!    information of the database.
  struct UploadReport : public Report {
    //! \brief Stores the flock of the file for the duration of
    //!     GetReportForUploading() and RecordUploadAttempt().
    int lock_fd;
  };

  //! \brief Locates a crash report in the database by UUID.
  //!
  //! \param[in] uuid The UUID of the crash report to locate.
  //!
  //! \return The full path to the report file, or an empty path if it cannot be
  //!     found.
  base::FilePath LocateCrashReport(const UUID& uuid);

  //! \brief Obtains an exclusive advisory lock on a file.
  //!
  //! The flock is used to prevent cross-process concurrent metadata reads or
  //! writes. While xattrs do not observe the lock, if the lock-then-mutate
  //! protocol is observed by all clients of the database, it still enforces
  //! synchronization.
  //!
  //! This does not block, and so callers must ensure that the lock is valid
  //! after calling.
  //!
  //! \param[in] path The path of the file to lock.
  //!
  //! \return A scoped lock object. If the result is not valid, an error is
  //!     logged.
  static base::ScopedFD ObtainReportLock(const base::FilePath& path);

  //! \brief Reads all the database xattrs from a file into a Report. The file
  //!     must be locked with ObtainReportLock.
  //!
  //! \param[in] path The path of the report.
  //! \param[out] report The object into which data will be read.
  //!
  //! \return `true` if all the metadata was read successfully, `false`
  //!     otherwise.
  static bool ReadReportMetadataLocked(const base::FilePath& path,
                                       Report* report);

  //! \brief Reads the metadata from all the reports in a database subdirectory.
  //!      Invalid reports are skipped.
  //!
  //! \param[in] path The database subdirectory path.
  //! \param[out] reports An empty vector of reports, which will be filled.
  //!
  //! \return The operation status code.
  static OperationStatus ReportsInDirectory(const base::FilePath& path,
                                            std::vector<Report>* reports);


  //! \brief Creates a database xattr name from the short constant name.
  //!
  //! \param[in] name The short name of the extended attribute.
  //!
  //! \return The long name of the extended attribute.
  static std::string XattrName(const base::StringPiece& name);

  base::FilePath base_dir_;
  Settings settings_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabaseMac);
};

CrashReportDatabaseMac::CrashReportDatabaseMac(const base::FilePath& path)
    : CrashReportDatabase(),
      base_dir_(path),
      settings_(base_dir_.Append(kSettings)),
      initialized_() {
}

CrashReportDatabaseMac::~CrashReportDatabaseMac() {}

bool CrashReportDatabaseMac::Initialize() {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  // Check if the database already exists.
  if (!CreateOrEnsureDirectoryExists(base_dir_))
    return false;

  // Create the three processing directories for the database.
  for (size_t i = 0; i < arraysize(kReportDirectories); ++i) {
    if (!CreateOrEnsureDirectoryExists(base_dir_.Append(kReportDirectories[i])))
      return false;
  }

  if (!settings_.Initialize())
    return false;

  // Write an xattr as the last step, to ensure the filesystem has support for
  // them. This attribute will never be read.
  if (!WriteXattrBool(base_dir_, XattrName(kXattrDatabaseInitialized), true))
    return false;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

Settings* CrashReportDatabaseMac::GetSettings() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &settings_;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::PrepareNewCrashReport(NewReport** out_report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  scoped_ptr<NewReport> report(new NewReport());

  uuid_t uuid_gen;
  uuid_generate(uuid_gen);
  report->uuid.InitializeFromBytes(uuid_gen);

  report->path =
      base_dir_.Append(kWriteDirectory)
          .Append(report->uuid.ToString() + "." + kCrashReportFileExtension);

  report->handle = HANDLE_EINTR(open(report->path.value().c_str(),
                                     O_CREAT | O_WRONLY | O_EXCL | O_EXLOCK,
                                     0600));
  if (report->handle < 0) {
    PLOG(ERROR) << "open " << report->path.value();
    return kFileSystemError;
  }

  // TODO(rsesek): Potentially use an fsetxattr() here instead.
  if (!WriteXattr(
          report->path, XattrName(kXattrUUID), report->uuid.ToString())) {
    PLOG_IF(ERROR, IGNORE_EINTR(close(report->handle)) != 0) << "close";
    return kDatabaseError;
  }

  *out_report = report.release();

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::FinishedWritingCrashReport(NewReport* report,
                                                   UUID* uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // Takes ownership of the |handle| and the O_EXLOCK.
  base::ScopedFD lock(report->handle);

  // Take ownership of the report.
  scoped_ptr<NewReport> scoped_report(report);

  // Get the report's UUID to return.
  std::string uuid_string;
  if (ReadXattr(report->path, XattrName(kXattrUUID),
                &uuid_string) != XattrStatus::kOK ||
      !uuid->InitializeFromString(uuid_string)) {
    LOG(ERROR) << "Failed to read UUID for crash report "
               << report->path.value();
    return kDatabaseError;
  }

  if (*uuid != report->uuid) {
    LOG(ERROR) << "UUID mismatch for crash report " << report->path.value();
    return kDatabaseError;
  }

  // Record the creation time of this report.
  if (!WriteXattrTimeT(report->path, XattrName(kXattrCreationTime),
                       time(nullptr))) {
    return kDatabaseError;
  }

  // Move the report to its new location for uploading.
  base::FilePath new_path =
      base_dir_.Append(kUploadPendingDirectory).Append(report->path.BaseName());
  if (rename(report->path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << report->path.value() << " to "
                << new_path.value();
    return kFileSystemError;
  }

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::ErrorWritingCrashReport(NewReport* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // Takes ownership of the |handle| and the O_EXLOCK.
  base::ScopedFD lock(report->handle);

  // Take ownership of the report.
  scoped_ptr<NewReport> scoped_report(report);

  // Remove the file that the report would have been written to had no error
  // occurred.
  if (unlink(report->path.value().c_str()) != 0) {
    PLOG(ERROR) << "unlink " << report->path.value();
    return kFileSystemError;
  }

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::LookUpCrashReport(const UUID& uuid,
                                          CrashReportDatabase::Report* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path = LocateCrashReport(uuid);
  if (path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(path));
  if (!lock.is_valid())
    return kBusyError;

  *report = Report();
  report->file_path = path;
  if (!ReadReportMetadataLocked(path, report))
    return kDatabaseError;

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetPendingReports(
    std::vector<CrashReportDatabase::Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  return ReportsInDirectory(base_dir_.Append(kUploadPendingDirectory), reports);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetCompletedReports(
    std::vector<CrashReportDatabase::Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  return ReportsInDirectory(base_dir_.Append(kCompletedDirectory), reports);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetReportForUploading(const UUID& uuid,
                                              const Report** report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath report_path = LocateCrashReport(uuid);
  if (report_path.empty())
    return kReportNotFound;

  scoped_ptr<UploadReport> upload_report(new UploadReport());
  upload_report->file_path = report_path;

  base::ScopedFD lock(ObtainReportLock(report_path));
  if (!lock.is_valid())
    return kBusyError;

  if (!ReadReportMetadataLocked(report_path, upload_report.get()))
    return kDatabaseError;

  upload_report->lock_fd = lock.release();
  *report = upload_report.release();
  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::RecordUploadAttempt(const Report* report,
                                            bool successful,
                                            const std::string& id) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  DCHECK(report);
  DCHECK(successful || id.empty());

  base::FilePath report_path = LocateCrashReport(report->uuid);
  if (report_path.empty())
    return kReportNotFound;

  scoped_ptr<const UploadReport> upload_report(
      static_cast<const UploadReport*>(report));

  base::ScopedFD lock(upload_report->lock_fd);
  if (!lock.is_valid())
    return kBusyError;

  if (successful) {
    base::FilePath new_path =
        base_dir_.Append(kCompletedDirectory).Append(report_path.BaseName());
    if (rename(report_path.value().c_str(), new_path.value().c_str()) != 0) {
      PLOG(ERROR) << "rename " << report_path.value() << " to "
                  << new_path.value();
      return kFileSystemError;
    }
    report_path = new_path;
  }

  if (!WriteXattrBool(report_path, XattrName(kXattrIsUploaded), successful)) {
    return kDatabaseError;
  }
  if (!WriteXattr(report_path, XattrName(kXattrCollectorID), id)) {
    return kDatabaseError;
  }

  time_t now = time(nullptr);
  if (!WriteXattrTimeT(report_path, XattrName(kXattrLastUploadTime), now)) {
    return kDatabaseError;
  }

  int upload_attempts = 0;
  std::string name = XattrName(kXattrUploadAttemptCount);
  if (ReadXattrInt(report_path, name, &upload_attempts) ==
          XattrStatus::kOtherError) {
    return kDatabaseError;
  }
  if (!WriteXattrInt(report_path, name, ++upload_attempts)) {
    return kDatabaseError;
  }

  if (!settings_.SetLastUploadAttemptTime(now)) {
    return kDatabaseError;
  }

  return kNoError;
}

CrashReportDatabase::OperationStatus CrashReportDatabaseMac::SkipReportUpload(
    const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath report_path = LocateCrashReport(uuid);
  if (report_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(report_path));
  if (!lock.is_valid())
    return kBusyError;

  base::FilePath new_path =
      base_dir_.Append(kCompletedDirectory).Append(report_path.BaseName());
  if (rename(report_path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << report_path.value() << " to "
                << new_path.value();
    return kFileSystemError;
  }

  return kNoError;
}

base::FilePath CrashReportDatabaseMac::LocateCrashReport(const UUID& uuid) {
  const std::string target_uuid = uuid.ToString();
  for (size_t i = 0; i < arraysize(kReportDirectories); ++i) {
    base::FilePath path =
       base_dir_.Append(kReportDirectories[i])
                .Append(target_uuid + "." + kCrashReportFileExtension);

    // Test if the path exists.
    struct stat st;
    if (lstat(path.value().c_str(), &st)) {
      continue;
    }

    // Check that the UUID of the report matches.
    std::string uuid_string;
    if (ReadXattr(path, XattrName(kXattrUUID),
                  &uuid_string) == XattrStatus::kOK &&
        uuid_string == target_uuid) {
      return path;
    }
  }

  return base::FilePath();
}

// static
base::ScopedFD CrashReportDatabaseMac::ObtainReportLock(
    const base::FilePath& path) {
  int fd = HANDLE_EINTR(open(path.value().c_str(),
                             O_RDONLY | O_EXLOCK | O_NONBLOCK));
  PLOG_IF(ERROR, fd < 0) << "open lock " << path.value();
  return base::ScopedFD(fd);
}

// static
bool CrashReportDatabaseMac::ReadReportMetadataLocked(
    const base::FilePath& path, Report* report) {
  std::string uuid_string;
  if (ReadXattr(path, XattrName(kXattrUUID),
                &uuid_string) != XattrStatus::kOK ||
      !report->uuid.InitializeFromString(uuid_string)) {
    return false;
  }

  if (ReadXattrTimeT(path, XattrName(kXattrCreationTime),
                     &report->creation_time) != XattrStatus::kOK) {
    return false;
  }

  report->id = std::string();
  if (ReadXattr(path, XattrName(kXattrCollectorID),
                &report->id) == XattrStatus::kOtherError) {
    return false;
  }

  report->uploaded = false;
  if (ReadXattrBool(path, XattrName(kXattrIsUploaded),
                    &report->uploaded) == XattrStatus::kOtherError) {
    return false;
  }

  report->last_upload_attempt_time = 0;
  if (ReadXattrTimeT(path, XattrName(kXattrLastUploadTime),
                     &report->last_upload_attempt_time) ==
          XattrStatus::kOtherError) {
    return false;
  }

  report->upload_attempts = 0;
  if (ReadXattrInt(path, XattrName(kXattrUploadAttemptCount),
                   &report->upload_attempts) == XattrStatus::kOtherError) {
    return false;
  }

  return true;
}

// static
CrashReportDatabase::OperationStatus CrashReportDatabaseMac::ReportsInDirectory(
    const base::FilePath& path,
    std::vector<CrashReportDatabase::Report>* reports) {
  base::mac::ScopedNSAutoreleasePool pool;

  DCHECK(reports->empty());

  NSError* error = nil;
  NSArray* paths = [[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:base::SysUTF8ToNSString(path.value())
                          error:&error];
  if (error) {
    LOG(ERROR) << "Failed to enumerate reports in directory " << path.value()
               << ": " << [[error description] UTF8String];
    return kFileSystemError;
  }

  reports->reserve([paths count]);
  for (NSString* entry in paths) {
    Report report;
    report.file_path = path.Append([entry fileSystemRepresentation]);
    base::ScopedFD lock(ObtainReportLock(report.file_path));
    if (!lock.is_valid())
      continue;

    if (!ReadReportMetadataLocked(report.file_path, &report)) {
      LOG(WARNING) << "Failed to read report metadata for "
                   << report.file_path.value();
      continue;
    }
    reports->push_back(report);
  }

  return kNoError;
}

// static
std::string CrashReportDatabaseMac::XattrName(const base::StringPiece& name) {
  return base::StringPrintf("com.googlecode.crashpad.%s", name.data());
}

}  // namespace

// static
scoped_ptr<CrashReportDatabase> CrashReportDatabase::Initialize(
    const base::FilePath& path) {
  scoped_ptr<CrashReportDatabaseMac> database_mac(
      new CrashReportDatabaseMac(path));
  if (!database_mac->Initialize())
    database_mac.reset();

  return scoped_ptr<CrashReportDatabase>(database_mac.release());
}

}  // namespace crashpad
