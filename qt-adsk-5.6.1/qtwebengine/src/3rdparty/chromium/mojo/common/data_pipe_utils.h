// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DATA_PIPE_UTILS_H_
#define MOJO_SHELL_DATA_PIPE_UTILS_H_

#include <string>

#include "base/callback_forward.h"
#include "mojo/common/mojo_common_export.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"

namespace base {
class FilePath;
class TaskRunner;
}

namespace mojo {
namespace common {

// Asynchronously copies data from source to the destination file. The given
// |callback| is run upon completion. File writes will be scheduled to the
// given |task_runner|.
void MOJO_COMMON_EXPORT CopyToFile(
    ScopedDataPipeConsumerHandle source,
    const base::FilePath& destination,
    base::TaskRunner* task_runner,
    const base::Callback<void(bool /*success*/)>& callback);

void MOJO_COMMON_EXPORT
CopyFromFile(const base::FilePath& source,
             ScopedDataPipeProducerHandle destination,
             uint32_t skip,
             base::TaskRunner* task_runner,
             const base::Callback<void(bool /*success*/)>& callback);

// Copies the data from |source| into |contents| and returns true on success and
// false on error.  In case of I/O error, |contents| holds the data that could
// be read from source before the error occurred.
bool MOJO_COMMON_EXPORT BlockingCopyToString(
    ScopedDataPipeConsumerHandle source,
    std::string* contents);

bool MOJO_COMMON_EXPORT BlockingCopyFromString(
    const std::string& source,
    const ScopedDataPipeProducerHandle& destination);

// Synchronously copies data from source to the destination file returning true
// on success and false on error.  In case of an error, |destination| holds the
// data that could be read from the source before the error occured.
bool MOJO_COMMON_EXPORT BlockingCopyToFile(ScopedDataPipeConsumerHandle source,
                                           const base::FilePath& destination);

}  // namespace common
}  // namespace mojo

#endif  // MOJO_SHELL_DATA_PIPE_UTILS_H_
