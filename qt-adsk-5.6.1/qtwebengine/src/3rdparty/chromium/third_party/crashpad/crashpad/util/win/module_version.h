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

#ifndef CRASHPAD_UTIL_WIN_MODULE_VERSION_H_
#define CRASHPAD_UTIL_WIN_MODULE_VERSION_H_

#include <windows.h>

#include "base/files/file_path.h"

namespace crashpad {

//! \brief Retrieve the type and version information from a given module (exe,
//!     dll, etc.)
//!
//! \param[in] path The path to the module to be inspected.
//! \param[out] vs_fixedfileinfo The VS_FIXEDFILEINFO on success. `dwFileFlags`
//!     will have been masked with `dwFileFlagsMask` already.
//!
//! \return `true` on success, or `false` on failure with a message logged.
bool GetModuleVersionAndType(const base::FilePath& path,
                             VS_FIXEDFILEINFO* vs_fixedfileinfo);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_MODULE_VERSION_H_
