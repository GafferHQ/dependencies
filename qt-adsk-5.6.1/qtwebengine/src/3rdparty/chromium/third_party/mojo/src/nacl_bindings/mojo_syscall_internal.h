// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_
#define MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_

#include <type_traits>

#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

namespace {

class ScopedCopyLock {
 public:
  explicit ScopedCopyLock(struct NaClApp* nap) : nap_(nap) {
    NaClCopyTakeLock(nap_);
  }
  ~ScopedCopyLock() { NaClCopyDropLock(nap_); }

 private:
  struct NaClApp* nap_;
};

static inline uintptr_t NaClUserToSysAddrArray(struct NaClApp* nap,
                                               uint32_t uaddr,
                                               size_t count,
                                               size_t size) {
  // TODO(ncbray): overflow checking
  size_t range = count * size;
  return NaClUserToSysAddrRange(nap, uaddr, range);
}

// We don't use plain-old memcpy because reads and writes to the untrusted
// address space from trusted code must be volatile.  Non-volatile memory
// operations are dangerous because a compiler would be free to materialize a
// second load from the same memory address or materialize a load from a memory
// address that was stored, and assume the materialized load would return the
// same value as the previous load or store.  Data races could cause the
// materialized load to return a different value, however, which could lead to
// time of check vs. time of use problems, or worse.  For this binding code in
// particular, where memcpy is being called with a constant size, it is entirely
// conceivable the function will be inlined, unrolled, and optimized.
static inline void memcpy_volatile_out(void volatile* dst,
                                       const void* src,
                                       size_t n) {
  char volatile* c_dst = static_cast<char volatile*>(dst);
  const char* c_src = static_cast<const char*>(src);
  for (size_t i = 0; i < n; i++) {
    c_dst[i] = c_src[i];
  }
}

template <typename T>
bool ConvertPointerInput(struct NaClApp* nap, uint32_t user_ptr, T** value) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddr(nap, user_ptr);
    if (temp != kNaClBadAddress) {
      *value = reinterpret_cast<T*>(temp);
      return true;
    }
  } else {
    *value = nullptr;
    return true;
  }
  *value = nullptr;  // Paranoia.
  return false;
}

template <typename T>
bool ConvertPointerInOut(struct NaClApp* nap,
                         uint32_t user_ptr,
                         bool optional,
                         T** value,
                         uint32_t volatile** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(uint32_t));
    if (temp != kNaClBadAddress) {
      uint32_t volatile* converted_ptr =
          reinterpret_cast<uint32_t volatile*>(temp);
      uint32_t raw_value = *converted_ptr;
      if (!raw_value) {
        *sys_ptr = converted_ptr;
        *value = nullptr;
        return true;
      }
      uintptr_t temp = NaClUserToSysAddr(nap, raw_value);
      if (temp != kNaClBadAddress) {
        *sys_ptr = converted_ptr;
        *value = reinterpret_cast<T*>(temp);
        return true;
      }
    }
  } else if (optional) {
    *sys_ptr = nullptr;
    *value = nullptr;  // Paranoia.
    return true;
  }
  *sys_ptr = nullptr;  // Paranoia.
  *value = nullptr;    // Paranoia.
  return false;
}

template <typename T>
void CopyOutPointer(struct NaClApp* nap, T* value, uint32_t volatile* sys_ptr) {
  if (value) {
    // Will kill the process if value if the pointer does not lie in the
    // sandbox.
    uintptr_t temp = NaClSysToUser(nap, reinterpret_cast<uintptr_t>(value));
    *sys_ptr = static_cast<uint32_t>(temp);
  } else {
    *sys_ptr = 0;
  }
}

template <typename T>
bool ConvertScalarInput(struct NaClApp* nap, uint32_t user_ptr, T* value) {
  static_assert(!std::is_pointer<T>::value, "do not use for pointer types");
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *value = *reinterpret_cast<T volatile*>(temp);
      return true;
    }
  }
  return false;
}

template <typename T>
bool ConvertScalarOutput(struct NaClApp* nap,
                         uint32_t user_ptr,
                         bool optional,
                         T volatile** sys_ptr) {
  static_assert(!std::is_pointer<T>::value, "do not use for pointer types");
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T volatile*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  *sys_ptr = 0;  // Paranoia.
  return false;
}

template <typename T>
bool ConvertScalarInOut(struct NaClApp* nap,
                        uint32_t user_ptr,
                        bool optional,
                        T* value,
                        T volatile** sys_ptr) {
  static_assert(!std::is_pointer<T>::value, "do not use for pointer types");
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      T volatile* converted = reinterpret_cast<T volatile*>(temp);
      *sys_ptr = converted;
      *value = *converted;
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    *value = static_cast<T>(0);  // Paranoia.
    return true;
  }
  *sys_ptr = 0;                // Paranoia.
  *value = static_cast<T>(0);  // Paranoia.
  return false;
}

template <typename T>
bool ConvertArray(struct NaClApp* nap,
                  uint32_t user_ptr,
                  uint32_t length,
                  size_t element_size,
                  bool optional,
                  T** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp =
        NaClUserToSysAddrArray(nap, user_ptr, length, element_size);
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

template <typename T>
bool ConvertBytes(struct NaClApp* nap,
                  uint32_t user_ptr,
                  uint32_t length,
                  bool optional,
                  T** sys_ptr) {
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, length);
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

// TODO(ncbray): size validation and complete copy.
// TODO(ncbray): ensure non-null / missized structs are covered by a test case.
template <typename T>
bool ConvertExtensibleStructInput(struct NaClApp* nap,
                                  uint32_t user_ptr,
                                  bool optional,
                                  T** sys_ptr) {
  static_assert(!std::is_pointer<T>::value, "do not use for pointer types");
  if (user_ptr) {
    uintptr_t temp = NaClUserToSysAddrRange(nap, user_ptr, sizeof(T));
    if (temp != kNaClBadAddress) {
      *sys_ptr = reinterpret_cast<T*>(temp);
      return true;
    }
  } else if (optional) {
    *sys_ptr = 0;
    return true;
  }
  return false;
}

}  // namespace

#endif  // MOJO_NACL_MOJO_SYSCALL_INTERNAL_H_
