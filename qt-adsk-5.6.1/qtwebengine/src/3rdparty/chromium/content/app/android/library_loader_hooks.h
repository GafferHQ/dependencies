// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/basictypes.h"

namespace content {

// Register all content JNI functions now, rather than waiting for the process
// of fully loading the native library to complete.
bool EnsureJniRegistered(JNIEnv* env);

// Do the intialization of content needed immediately after the native library
// has loaded.
// This is designed to be used as a hook function to be passed to
// base::android::SetLibraryLoadedHook
bool LibraryLoaded(JNIEnv* env, jclass clazz);

}  // namespace content

#endif  // CONTENT_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
