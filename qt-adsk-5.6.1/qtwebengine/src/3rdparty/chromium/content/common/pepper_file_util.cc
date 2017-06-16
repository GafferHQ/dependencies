// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_file_util.h"

namespace content {

storage::FileSystemType PepperFileSystemTypeToFileSystemType(
    PP_FileSystemType type) {
  switch (type) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      return storage::kFileSystemTypeTemporary;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      return storage::kFileSystemTypePersistent;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      return storage::kFileSystemTypeExternal;
    default:
      return storage::kFileSystemTypeUnknown;
  }
}

int IntegerFromSyncSocketHandle(
    const base::SyncSocket::Handle& socket_handle) {
#if defined(OS_WIN)
  return reinterpret_cast<int>(socket_handle);
#elif defined(OS_POSIX)
  return socket_handle;
#else
#error Platform not supported.
#endif
}

}  // namespace content
