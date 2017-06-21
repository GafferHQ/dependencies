/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ATOMICOPS_H_
#define WEBRTC_BASE_ATOMICOPS_H_

#if defined(WEBRTC_WIN)
// Include winsock2.h before including <windows.h> to maintain consistency with
// win32.h.  We can't include win32.h directly here since it pulls in
// headers such as basictypes.h which causes problems in Chromium where webrtc
// exists as two separate projects, webrtc and libjingle.
#include <winsock2.h>
#include <windows.h>
#endif  // defined(WEBRTC_WIN)

namespace rtc {
class AtomicOps {
 public:
#if defined(WEBRTC_WIN)
  // Assumes sizeof(int) == sizeof(LONG), which it is on Win32 and Win64.
  static int Increment(volatile int* i) {
    return ::InterlockedIncrement(reinterpret_cast<volatile LONG*>(i));
  }
  static int Decrement(volatile int* i) {
    return ::InterlockedDecrement(reinterpret_cast<volatile LONG*>(i));
  }
  static int Load(volatile const int* i) {
    return *i;
  }
  static void Store(volatile int* i, int value) {
    *i = value;
  }
  static int CompareAndSwap(volatile int* i, int old_value, int new_value) {
    return ::InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(i),
                                        new_value,
                                        old_value);
  }
#else
  static int Increment(volatile int* i) {
    return __sync_add_and_fetch(i, 1);
  }
  static int Decrement(volatile int* i) {
    return __sync_sub_and_fetch(i, 1);
  }
  static int Load(volatile const int* i) {
    // Adding 0 is a no-op, so const_cast is fine.
    return __sync_add_and_fetch(const_cast<volatile int*>(i), 0);
  }
  static void Store(volatile int* i, int value) {
    __sync_synchronize();
    *i = value;
  }
  static int CompareAndSwap(volatile int* i, int old_value, int new_value) {
    return __sync_val_compare_and_swap(i, old_value, new_value);
  }
#endif
};



}

#endif  // WEBRTC_BASE_ATOMICOPS_H_
