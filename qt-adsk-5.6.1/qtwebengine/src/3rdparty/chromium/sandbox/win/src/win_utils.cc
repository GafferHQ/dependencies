// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win_utils.h"

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/win/pe_image.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace {

// Holds the information about a known registry key.
struct KnownReservedKey {
  const wchar_t* name;
  HKEY key;
};

// Contains all the known registry key by name and by handle.
const KnownReservedKey kKnownKey[] = {
    { L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT },
    { L"HKEY_CURRENT_USER", HKEY_CURRENT_USER },
    { L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
    { L"HKEY_USERS", HKEY_USERS},
    { L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
    { L"HKEY_PERFORMANCE_TEXT", HKEY_PERFORMANCE_TEXT},
    { L"HKEY_PERFORMANCE_NLSTEXT", HKEY_PERFORMANCE_NLSTEXT},
    { L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
    { L"HKEY_DYN_DATA", HKEY_DYN_DATA}
};

// These functions perform case independent path comparisons.
bool EqualPath(const base::string16& first, const base::string16& second) {
  return _wcsicmp(first.c_str(), second.c_str()) == 0;
}

bool EqualPath(const base::string16& first, size_t first_offset,
               const base::string16& second, size_t second_offset) {
  return _wcsicmp(first.c_str() + first_offset,
                  second.c_str() + second_offset) == 0;
}

bool EqualPath(const base::string16& first,
               const wchar_t* second, size_t second_len) {
  return _wcsnicmp(first.c_str(), second, second_len) == 0;
}

bool EqualPath(const base::string16& first, size_t first_offset,
               const wchar_t* second, size_t second_len) {
  return _wcsnicmp(first.c_str() + first_offset, second, second_len) == 0;
}

// Returns true if |path| starts with "\??\" and returns a path without that
// component.
bool IsNTPath(const base::string16& path, base::string16* trimmed_path ) {
  if ((path.size() < sandbox::kNTPrefixLen) ||
      (0 != path.compare(0, sandbox::kNTPrefixLen, sandbox::kNTPrefix))) {
    *trimmed_path = path;
    return false;
  }

  *trimmed_path = path.substr(sandbox::kNTPrefixLen);
  return true;
}

// Returns true if |path| starts with "\Device\" and returns a path without that
// component.
bool IsDevicePath(const base::string16& path, base::string16* trimmed_path ) {
  if ((path.size() < sandbox::kNTDevicePrefixLen) ||
      (!EqualPath(path, sandbox::kNTDevicePrefix,
                  sandbox::kNTDevicePrefixLen))) {
    *trimmed_path = path;
    return false;
  }

  *trimmed_path = path.substr(sandbox::kNTDevicePrefixLen);
  return true;
}

bool StartsWithDriveLetter(const base::string16& path) {
  if (path.size() < 3)
    return false;

  if (path[1] != L':' || path[2] != L'\\')
    return false;

  return (path[0] >= 'a' && path[0] <= 'z') ||
         (path[0] >= 'A' && path[0] <= 'Z');
}

const wchar_t kNTDotPrefix[] = L"\\\\.\\";
const size_t kNTDotPrefixLen = arraysize(kNTDotPrefix) - 1;

// Removes "\\\\.\\" from the path.
void RemoveImpliedDevice(base::string16* path) {
  if (0 == path->compare(0, kNTDotPrefixLen, kNTDotPrefix))
    *path = path->substr(kNTDotPrefixLen);
}

}  // namespace

namespace sandbox {

// Returns true if the provided path points to a pipe.
bool IsPipe(const base::string16& path) {
  size_t start = 0;
  if (0 == path.compare(0, sandbox::kNTPrefixLen, sandbox::kNTPrefix))
    start = sandbox::kNTPrefixLen;

  const wchar_t kPipe[] = L"pipe\\";
  if (path.size() < start + arraysize(kPipe) - 1)
    return false;

  return EqualPath(path, start, kPipe, arraysize(kPipe) - 1);
}

HKEY GetReservedKeyFromName(const base::string16& name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name == kKnownKey[i].name)
      return kKnownKey[i].key;
  }

  return NULL;
}

bool ResolveRegistryName(base::string16 name, base::string16* resolved_name) {
  for (size_t i = 0; i < arraysize(kKnownKey); ++i) {
    if (name.find(kKnownKey[i].name) == 0) {
      HKEY key;
      DWORD disposition;
      if (ERROR_SUCCESS != ::RegCreateKeyEx(kKnownKey[i].key, L"", 0, NULL, 0,
                                            MAXIMUM_ALLOWED, NULL, &key,
                                            &disposition))
        return false;

      bool result = GetPathFromHandle(key, resolved_name);
      ::RegCloseKey(key);

      if (!result)
        return false;

      *resolved_name += name.substr(wcslen(kKnownKey[i].name));
      return true;
    }
  }

  return false;
}

// |full_path| can have any of the following forms:
//    \??\c:\some\foo\bar
//    \Device\HarddiskVolume0\some\foo\bar
//    \??\HarddiskVolume0\some\foo\bar
DWORD IsReparsePoint(const base::string16& full_path, bool* result) {
  // Check if it's a pipe. We can't query the attributes of a pipe.
  if (IsPipe(full_path)) {
    *result = FALSE;
    return ERROR_SUCCESS;
  }

  base::string16 path;
  bool nt_path = IsNTPath(full_path, &path);
  bool has_drive = StartsWithDriveLetter(path);
  bool is_device_path = IsDevicePath(path, &path);

  if (!has_drive && !is_device_path && !nt_path)
    return ERROR_INVALID_NAME;

  bool added_implied_device = false;
  if (!has_drive) {
      path = base::string16(kNTDotPrefix) + path;
      added_implied_device = true;
  }

  base::string16::size_type last_pos = base::string16::npos;
  bool passed_once = false;

  do {
    path = path.substr(0, last_pos);

    DWORD attributes = ::GetFileAttributes(path.c_str());
    if (INVALID_FILE_ATTRIBUTES == attributes) {
      DWORD error = ::GetLastError();
      if (error != ERROR_FILE_NOT_FOUND &&
          error != ERROR_PATH_NOT_FOUND &&
          error != ERROR_INVALID_NAME) {
        // Unexpected error.
        if (passed_once && added_implied_device &&
            (path.rfind(L'\\') == kNTDotPrefixLen - 1)) {
          break;
        }
        NOTREACHED_NT();
        return error;
      }
    } else if (FILE_ATTRIBUTE_REPARSE_POINT & attributes) {
      // This is a reparse point.
      *result = true;
      return ERROR_SUCCESS;
    }

    passed_once = true;
    last_pos = path.rfind(L'\\');
  } while (last_pos > 2);  // Skip root dir.

  *result = false;
  return ERROR_SUCCESS;
}

// We get a |full_path| of the forms accepted by IsReparsePoint(), and the name
// we'll get from |handle| will be \device\harddiskvolume1\some\foo\bar.
bool SameObject(HANDLE handle, const wchar_t* full_path) {
  // Check if it's a pipe.
  if (IsPipe(full_path))
    return true;

  base::string16 actual_path;
  if (!GetPathFromHandle(handle, &actual_path))
    return false;

  base::string16 path(full_path);
  DCHECK_NT(!path.empty());

  // This may end with a backslash.
  const wchar_t kBackslash = '\\';
  if (path[path.length() - 1] == kBackslash)
    path = path.substr(0, path.length() - 1);

  // Perfect match (case-insesitive check).
  if (EqualPath(actual_path, path))
    return true;

  bool nt_path = IsNTPath(path, &path);
  bool has_drive = StartsWithDriveLetter(path);

  if (!has_drive && nt_path) {
    base::string16 simple_actual_path;
    if (!IsDevicePath(actual_path, &simple_actual_path))
      return false;

    // Perfect match (case-insesitive check).
    return (EqualPath(simple_actual_path, path));
  }

  if (!has_drive)
    return false;

  // We only need 3 chars, but let's alloc a buffer for four.
  wchar_t drive[4] = {0};
  wchar_t vol_name[MAX_PATH];
  memcpy(drive, &path[0], 2 * sizeof(*drive));

  // We'll get a double null terminated string.
  DWORD vol_length = ::QueryDosDeviceW(drive, vol_name, MAX_PATH);
  if (vol_length < 2 || vol_length == MAX_PATH)
    return false;

  // Ignore the nulls at the end.
  vol_length = static_cast<DWORD>(wcslen(vol_name));

  // The two paths should be the same length.
  if (vol_length + path.size() - 2 != actual_path.size())
    return false;

  // Check up to the drive letter.
  if (!EqualPath(actual_path, vol_name, vol_length))
    return false;

  // Check the path after the drive letter.
  if (!EqualPath(actual_path, vol_length, path, 2))
    return false;

  return true;
}

// Paths like \Device\HarddiskVolume0\some\foo\bar are assumed to be already
// expanded.
bool ConvertToLongPath(const base::string16& short_path,
                       base::string16* long_path) {
  if (IsPipe(short_path)) {
    // TODO(rvargas): Change the signature to use a single argument.
    long_path->assign(short_path);
    return true;
  }

  base::string16 path;
  if (IsDevicePath(short_path, &path))
    return false;

  bool is_nt_path = IsNTPath(path, &path);
  bool added_implied_device = false;
  if (!StartsWithDriveLetter(path) && is_nt_path) {
    path = base::string16(kNTDotPrefix) + path;
    added_implied_device = true;
  }

  DWORD size = MAX_PATH;
  scoped_ptr<wchar_t[]> long_path_buf(new wchar_t[size]);

  DWORD return_value = ::GetLongPathName(path.c_str(), long_path_buf.get(),
                                         size);
  while (return_value >= size) {
    size *= 2;
    long_path_buf.reset(new wchar_t[size]);
    return_value = ::GetLongPathName(path.c_str(), long_path_buf.get(), size);
  }

  DWORD last_error = ::GetLastError();
  if (0 == return_value && (ERROR_FILE_NOT_FOUND == last_error ||
                            ERROR_PATH_NOT_FOUND == last_error ||
                            ERROR_INVALID_NAME == last_error)) {
    // The file does not exist, but maybe a sub path needs to be expanded.
    base::string16::size_type last_slash = path.rfind(L'\\');
    if (base::string16::npos == last_slash)
      return false;

    base::string16 begin = path.substr(0, last_slash);
    base::string16 end = path.substr(last_slash);
    if (!ConvertToLongPath(begin, &begin))
      return false;

    // Ok, it worked. Let's reset the return value.
    path = begin + end;
    return_value = 1;
  } else if (0 != return_value) {
    path = long_path_buf.get();
  }

  if (return_value != 0) {
    if (added_implied_device)
      RemoveImpliedDevice(&path);

    if (is_nt_path) {
      *long_path = kNTPrefix;
      *long_path += path;
    } else {
      *long_path = path;
    }

    return true;
  }

  return false;
}

bool GetPathFromHandle(HANDLE handle, base::string16* path) {
  NtQueryObjectFunction NtQueryObject = NULL;
  ResolveNTFunctionPtr("NtQueryObject", &NtQueryObject);

  OBJECT_NAME_INFORMATION initial_buffer;
  OBJECT_NAME_INFORMATION* name = &initial_buffer;
  ULONG size = sizeof(initial_buffer);
  // Query the name information a first time to get the size of the name.
  // Windows XP requires that the size of the buffer passed in here be != 0.
  NTSTATUS status = NtQueryObject(handle, ObjectNameInformation, name, size,
                                  &size);

  scoped_ptr<BYTE[]> name_ptr;
  if (size) {
    name_ptr.reset(new BYTE[size]);
    name = reinterpret_cast<OBJECT_NAME_INFORMATION*>(name_ptr.get());

    // Query the name information a second time to get the name of the
    // object referenced by the handle.
    status = NtQueryObject(handle, ObjectNameInformation, name, size, &size);
  }

  if (STATUS_SUCCESS != status)
    return false;

  path->assign(name->ObjectName.Buffer, name->ObjectName.Length /
                                        sizeof(name->ObjectName.Buffer[0]));
  return true;
}

bool GetNtPathFromWin32Path(const base::string16& path,
                            base::string16* nt_path) {
  HANDLE file = ::CreateFileW(path.c_str(), 0,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  bool rv = GetPathFromHandle(file, nt_path);
  ::CloseHandle(file);
  return rv;
}

bool WriteProtectedChildMemory(HANDLE child_process, void* address,
                               const void* buffer, size_t length) {
  // First, remove the protections.
  DWORD old_protection;
  if (!::VirtualProtectEx(child_process, address, length,
                          PAGE_WRITECOPY, &old_protection))
    return false;

  SIZE_T written;
  bool ok = ::WriteProcessMemory(child_process, address, buffer, length,
                                 &written) && (length == written);

  // Always attempt to restore the original protection.
  if (!::VirtualProtectEx(child_process, address, length,
                          old_protection, &old_protection))
    return false;

  return ok;
}

};  // namespace sandbox

void ResolveNTFunctionPtr(const char* name, void* ptr) {
  static volatile HMODULE ntdll = NULL;

  if (!ntdll) {
    HMODULE ntdll_local = ::GetModuleHandle(sandbox::kNtdllName);
    // Use PEImage to sanity-check that we have a valid ntdll handle.
    base::win::PEImage ntdll_peimage(ntdll_local);
    CHECK_NT(ntdll_peimage.VerifyMagic());
    // Race-safe way to set static ntdll.
    ::InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(&ntdll), ntdll_local, NULL);

  }

  CHECK_NT(ntdll);
  FARPROC* function_ptr = reinterpret_cast<FARPROC*>(ptr);
  *function_ptr = ::GetProcAddress(ntdll, name);
  CHECK_NT(*function_ptr);
}
