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

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crash_report_database.h"
#include "client/settings.h"
#include "tools/tool_support.h"
#include "util/file/file_io.h"
#include "util/file/file_reader.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace {

void Usage(const base::FilePath& me) {
  fprintf(stderr,
"Usage: %" PRFilePath " [OPTION]... PID\n"
"Operate on Crashpad crash report databases.\n"
"\n"
"  -d, --database=PATH             operate on the crash report database at PATH\n"
"      --show-client-id            show the client ID\n"
"      --show-uploads-enabled      show whether uploads are enabled\n"
"      --show-last-upload-attempt-time\n"
"                                  show the last-upload-attempt time\n"
"      --show-pending-reports      show reports eligible for upload\n"
"      --show-completed-reports    show reports not eligible for upload\n"
"      --show-all-report-info      with --show-*-reports, show more information\n"
"      --show-report=UUID          show report stored under UUID\n"
"      --set-uploads-enabled=BOOL  enable or disable uploads\n"
"      --set-last-upload-attempt-time=TIME\n"
"                                  set the last-upload-attempt time to TIME\n"
"      --new-report=PATH           submit a new report at PATH\n"
"      --utc                       show and set UTC times instead of local\n"
"      --help                      display this help and exit\n"
"      --version                   output version information and exit\n",
          me.value().c_str());
  ToolSupport::UsageTail(me);
}

struct Options {
  std::vector<UUID> show_reports;
  std::vector<base::FilePath> new_report_paths;
  const char* database;
  const char* set_last_upload_attempt_time_string;
  time_t set_last_upload_attempt_time;
  bool show_client_id;
  bool show_uploads_enabled;
  bool show_last_upload_attempt_time;
  bool show_pending_reports;
  bool show_completed_reports;
  bool show_all_report_info;
  bool set_uploads_enabled;
  bool has_set_uploads_enabled;
  bool utc;
};

// Converts |string| to |boolean|, returning true if a conversion could be
// performed, and false without setting |boolean| if no conversion could be
// performed. Various string representations of a boolean are recognized
// case-insensitively.
bool StringToBool(const char* string, bool* boolean) {
  const char* const kFalseWords[] = {
      "0",
      "false",
      "no",
      "off",
      "disabled",
      "clear",
  };
  const char* const kTrueWords[] = {
      "1",
      "true",
      "yes",
      "on",
      "enabled",
      "set",
  };

  for (size_t index = 0; index < arraysize(kFalseWords); ++index) {
    if (base::strcasecmp(string, kFalseWords[index]) == 0) {
      *boolean = false;
      return true;
    }
  }

  for (size_t index = 0; index < arraysize(kTrueWords); ++index) {
    if (base::strcasecmp(string, kTrueWords[index]) == 0) {
      *boolean = true;
      return true;
    }
  }

  return false;
}

// Converts |boolean| to a string, either "true" or "false".
std::string BoolToString(bool boolean) {
  return std::string(boolean ? "true" : "false");
}

// Converts |string| to |time|, returning true if a conversion could be
// performed, and false without setting |boolean| if no conversion could be
// performed. Various time formats are recognized, including several string
// representations and a numeric time_t representation. The special string
// "never" is recognized as |string| and converts to a |time| value of 0. |utc|,
// when true, causes |string| to be interpreted as a UTC time rather than a
// local time when the time zone is ambiguous.
bool StringToTime(const char* string, time_t* time, bool utc) {
  if (base::strcasecmp(string, "never") == 0) {
    *time = 0;
    return true;
  }

  const char* end = string + strlen(string);

  const char* const kFormats[] = {
      "%Y-%m-%d %H:%M:%S %Z",
      "%Y-%m-%d %H:%M:%S",
      "%+",
  };

  for (size_t index = 0; index < arraysize(kFormats); ++index) {
    tm time_tm;
    const char* strptime_result = strptime(string, kFormats[index], &time_tm);
    if (strptime_result == end) {
      if (utc) {
        *time = timegm(&time_tm);
      } else {
        *time = mktime(&time_tm);
      }

      return true;
    }
  }

  char* end_result;
  errno = 0;
  long long strtoll_result = strtoll(string, &end_result, 0);
  if (end_result == end && errno == 0 &&
      base::IsValueInRangeForNumericType<time_t>(strtoll_result)) {
    *time = strtoll_result;
    return true;
  }

  return false;
}

// Converts |time_tt| to a string, and returns it. |utc| determines whether the
// converted time will reference local time or UTC. If |time_tt| is 0, the
// string "never" will be returned as a special case.
std::string TimeToString(time_t time_tt, bool utc) {
  if (time_tt == 0) {
    return std::string("never");
  }

  tm time_tm;
  if (utc) {
    gmtime_r(&time_tt, &time_tm);
  } else {
    localtime_r(&time_tt, &time_tm);
  }

  char string[64];
  CHECK_NE(
      strftime(string, arraysize(string), "%Y-%m-%d %H:%M:%S %Z", &time_tm),
      0u);

  return std::string(string);
}

// Shows information about a single |report|. |space_count| is the number of
// spaces to print before each line that is printed. |utc| determines whether
// times should be shown in UTC or the local time zone.
void ShowReport(const CrashReportDatabase::Report& report,
                size_t space_count,
                bool utc) {
  std::string spaces(space_count, ' ');

  printf("%sPath: %" PRFilePath "\n",
         spaces.c_str(),
         report.file_path.value().c_str());
  if (!report.id.empty()) {
    printf("%sRemote ID: %s\n", spaces.c_str(), report.id.c_str());
  }
  printf("%sCreation time: %s\n",
         spaces.c_str(),
         TimeToString(report.creation_time, utc).c_str());
  printf("%sUploaded: %s\n",
         spaces.c_str(),
         BoolToString(report.uploaded).c_str());
  printf("%sLast upload attempt time: %s\n",
         spaces.c_str(),
         TimeToString(report.last_upload_attempt_time, utc).c_str());
  printf("%sUpload attempts: %d\n", spaces.c_str(), report.upload_attempts);
}

// Shows information about a vector of |reports|. |space_count| is the number of
// spaces to print before each line that is printed. |options| will be consulted
// to determine whether to show expanded information
// (options.show_all_report_info) and what time zone to use when showing
// expanded information (options.utc).
void ShowReports(const std::vector<CrashReportDatabase::Report>& reports,
                 size_t space_count,
                 const Options& options) {
  std::string spaces(space_count, ' ');
  const char* colon = options.show_all_report_info ? ":" : "";

  for (const CrashReportDatabase::Report& report : reports) {
    printf("%s%s%s\n", spaces.c_str(), report.uuid.ToString().c_str(), colon);
    if (options.show_all_report_info) {
      ShowReport(report, space_count + 2, options.utc);
    }
  }
}

int DatabaseUtilMain(int argc, char* argv[]) {
  const base::FilePath argv0(
      ToolSupport::CommandLineArgumentToFilePathStringType(argv[0]));
  const base::FilePath me(argv0.BaseName());

  enum OptionFlags {
    // “Short” (single-character) options.
    kOptionDatabase = 'd',

    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionShowClientID,
    kOptionShowUploadsEnabled,
    kOptionShowLastUploadAttemptTime,
    kOptionShowPendingReports,
    kOptionShowCompletedReports,
    kOptionShowAllReportInfo,
    kOptionShowReport,
    kOptionSetUploadsEnabled,
    kOptionSetLastUploadAttemptTime,
    kOptionNewReport,
    kOptionUTC,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  const option long_options[] = {
      {"database", required_argument, nullptr, kOptionDatabase},
      {"show-client-id", no_argument, nullptr, kOptionShowClientID},
      {"show-uploads-enabled", no_argument, nullptr, kOptionShowUploadsEnabled},
      {"show-last-upload-attempt-time",
       no_argument,
       nullptr,
       kOptionShowLastUploadAttemptTime},
      {"show-pending-reports", no_argument, nullptr, kOptionShowPendingReports},
      {"show-completed-reports",
       no_argument,
       nullptr,
       kOptionShowCompletedReports},
      {"show-all-report-info", no_argument, nullptr, kOptionShowAllReportInfo},
      {"show-report", required_argument, nullptr, kOptionShowReport},
      {"set-uploads-enabled",
       required_argument,
       nullptr,
       kOptionSetUploadsEnabled},
      {"set-last-upload-attempt-time",
       required_argument,
       nullptr,
       kOptionSetLastUploadAttemptTime},
      {"new-report", required_argument, nullptr, kOptionNewReport},
      {"utc", no_argument, nullptr, kOptionUTC},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  Options options = {};

  int opt;
  while ((opt = getopt_long(argc, argv, "d:", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionDatabase: {
        options.database = optarg;
        break;
      }
      case kOptionShowClientID: {
        options.show_client_id = true;
        break;
      }
      case kOptionShowUploadsEnabled: {
        options.show_uploads_enabled = true;
        break;
      }
      case kOptionShowLastUploadAttemptTime: {
        options.show_last_upload_attempt_time = true;
        break;
      }
      case kOptionShowPendingReports: {
        options.show_pending_reports = true;
        break;
      }
      case kOptionShowCompletedReports: {
        options.show_completed_reports = true;
        break;
      }
      case kOptionShowAllReportInfo: {
        options.show_all_report_info = true;
        break;
      }
      case kOptionShowReport: {
        UUID uuid;
        if (!uuid.InitializeFromString(optarg)) {
          ToolSupport::UsageHint(me, "--show-report requires a UUID");
          return EXIT_FAILURE;
        }
        options.show_reports.push_back(uuid);
        break;
      }
      case kOptionSetUploadsEnabled: {
        if (!StringToBool(optarg, &options.set_uploads_enabled)) {
          ToolSupport::UsageHint(me, "--set-uploads-enabled requires a BOOL");
          return EXIT_FAILURE;
        }
        options.has_set_uploads_enabled = true;
        break;
      }
      case kOptionSetLastUploadAttemptTime: {
        options.set_last_upload_attempt_time_string = optarg;
        break;
      }
      case kOptionNewReport: {
        options.new_report_paths.push_back(base::FilePath(
            ToolSupport::CommandLineArgumentToFilePathStringType(optarg)));
        break;
      }
      case kOptionUTC: {
        options.utc = true;
        break;
      }
      case kOptionHelp: {
        Usage(me);
        return EXIT_SUCCESS;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
      }
    }
  }
  argc -= optind;
  argv += optind;

  if (!options.database) {
    ToolSupport::UsageHint(me, "--database is required");
    return EXIT_FAILURE;
  }

  // This conversion couldn’t happen in the option-processing loop above because
  // it depends on options.utc, which may have been set after
  // options.set_last_upload_attempt_time_string.
  if (options.set_last_upload_attempt_time_string) {
    if (!StringToTime(options.set_last_upload_attempt_time_string,
                      &options.set_last_upload_attempt_time,
                      options.utc)) {
      ToolSupport::UsageHint(me,
                             "--set-last-upload-attempt-time requires a TIME");
      return EXIT_FAILURE;
    }
  }

  // --new-report is treated as a show operation because it produces output.
  const size_t show_operations = options.show_client_id +
                                 options.show_uploads_enabled +
                                 options.show_last_upload_attempt_time +
                                 options.show_pending_reports +
                                 options.show_completed_reports +
                                 options.show_reports.size() +
                                 options.new_report_paths.size();
  const size_t set_operations =
      options.has_set_uploads_enabled +
      (options.set_last_upload_attempt_time_string != nullptr);

  if (show_operations + set_operations == 0) {
    ToolSupport::UsageHint(me, "nothing to do");
    return EXIT_FAILURE;
  }

  scoped_ptr<CrashReportDatabase> database(CrashReportDatabase::Initialize(
      base::FilePath(ToolSupport::CommandLineArgumentToFilePathStringType(
          options.database))));
  if (!database) {
    return EXIT_FAILURE;
  }

  Settings* settings = database->GetSettings();

  // Handle the “show” options before the “set” options so that when they’re
  // specified together, the “show” option reflects the initial state.

  if (options.show_client_id) {
    UUID client_id;
    if (!settings->GetClientID(&client_id)) {
      return EXIT_FAILURE;
    }

    const char* prefix = (show_operations > 1) ? "Client ID: " : "";

    printf("%s%s\n", prefix, client_id.ToString().c_str());
  }

  if (options.show_uploads_enabled) {
    bool uploads_enabled;
    if (!settings->GetUploadsEnabled(&uploads_enabled)) {
      return EXIT_FAILURE;
    }

    const char* prefix = (show_operations > 1) ? "Uploads enabled: " : "";

    printf("%s%s\n", prefix, BoolToString(uploads_enabled).c_str());
  }

  if (options.show_last_upload_attempt_time) {
    time_t last_upload_attempt_time;
    if (!settings->GetLastUploadAttemptTime(&last_upload_attempt_time)) {
      return EXIT_FAILURE;
    }

    const char* prefix =
        (show_operations > 1) ? "Last upload attempt time: " : "";

    printf("%s%s (%ld)\n",
           prefix,
           TimeToString(last_upload_attempt_time, options.utc).c_str(),
           static_cast<long>(last_upload_attempt_time));
  }

  if (options.show_pending_reports) {
    std::vector<CrashReportDatabase::Report> pending_reports;
    if (database->GetPendingReports(&pending_reports) !=
        CrashReportDatabase::kNoError) {
      return EXIT_FAILURE;
    }

    if (show_operations > 1) {
      printf("Pending reports:\n");
    }

    ShowReports(pending_reports, show_operations > 1 ? 2 : 0, options);
  }

  if (options.show_completed_reports) {
    std::vector<CrashReportDatabase::Report> completed_reports;
    if (database->GetCompletedReports(&completed_reports) !=
        CrashReportDatabase::kNoError) {
      return EXIT_FAILURE;
    }

    if (show_operations > 1) {
      printf("Completed reports:\n");
    }

    ShowReports(completed_reports, show_operations > 1 ? 2 : 0, options);
  }

  for (const UUID& uuid : options.show_reports) {
    CrashReportDatabase::Report report;
    CrashReportDatabase::OperationStatus status =
        database->LookUpCrashReport(uuid, &report);
    if (status == CrashReportDatabase::kNoError) {
      if (show_operations > 1) {
        printf("Report %s:\n", uuid.ToString().c_str());
      }
      ShowReport(report, show_operations > 1 ? 2 : 0, options.utc);
    } else if (status == CrashReportDatabase::kReportNotFound) {
      // If only asked to do one thing, a failure to find the single requested
      // report should result in a failure exit status.
      if (show_operations + set_operations == 1) {
        fprintf(
            stderr, "%" PRFilePath ": Report not found\n", me.value().c_str());
        return EXIT_FAILURE;
      }
      printf("Report %s not found\n", uuid.ToString().c_str());
    } else {
      return EXIT_FAILURE;
    }
  }

  if (options.has_set_uploads_enabled &&
      !settings->SetUploadsEnabled(options.set_uploads_enabled)) {
    return EXIT_FAILURE;
  }

  if (options.set_last_upload_attempt_time_string &&
      !settings->SetLastUploadAttemptTime(
          options.set_last_upload_attempt_time)) {
    return EXIT_FAILURE;
  }

  for (const base::FilePath new_report_path : options.new_report_paths) {
    FileReader file_reader;
    if (!file_reader.Open(new_report_path)) {
      return EXIT_FAILURE;
    }

    CrashReportDatabase::NewReport* new_report;
    CrashReportDatabase::OperationStatus status =
        database->PrepareNewCrashReport(&new_report);
    if (status != CrashReportDatabase::kNoError) {
      return EXIT_FAILURE;
    }

    CrashReportDatabase::CallErrorWritingCrashReport
        call_error_writing_crash_report(database.get(), new_report);

    char buf[4096];
    ssize_t read_result;
    while ((read_result = file_reader.Read(buf, sizeof(buf))) > 0) {
      if (!LoggingWriteFile(new_report->handle, buf, read_result)) {
        return EXIT_FAILURE;
      }
    }
    if (read_result < 0) {
      return EXIT_FAILURE;
    }

    call_error_writing_crash_report.Disarm();

    UUID uuid;
    status = database->FinishedWritingCrashReport(new_report, &uuid);
    if (status != CrashReportDatabase::kNoError) {
      return EXIT_FAILURE;
    }

    const char* prefix = (show_operations > 1) ? "New report ID: " : "";
    printf("%s%s\n", prefix, uuid.ToString().c_str());
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

#if defined(OS_POSIX)
int main(int argc, char* argv[]) {
  return crashpad::DatabaseUtilMain(argc, argv);
}
#elif defined(OS_WIN)
int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::DatabaseUtilMain);
}
#endif  // OS_POSIX
