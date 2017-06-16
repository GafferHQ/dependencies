// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/system/base_run_loop.h"

#include <jni.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "jni/BaseRunLoop_jni.h"
#include "mojo/common/message_pump_mojo.h"

namespace mojo {
namespace android {

static jlong CreateBaseRunLoop(JNIEnv* env, jobject jcaller) {
  base::MessageLoop* message_loop =
      new base::MessageLoop(common::MessagePumpMojo::Create());
  return reinterpret_cast<uintptr_t>(message_loop);
}

static void Run(JNIEnv* env, jobject jcaller, jlong runLoopID) {
  reinterpret_cast<base::MessageLoop*>(runLoopID)->Run();
}

static void RunUntilIdle(JNIEnv* env, jobject jcaller, jlong runLoopID) {
  reinterpret_cast<base::MessageLoop*>(runLoopID)->RunUntilIdle();
}

static void Quit(JNIEnv* env, jobject jcaller, jlong runLoopID) {
  reinterpret_cast<base::MessageLoop*>(runLoopID)->Quit();
}

static void RunJavaRunnable(
    const base::android::ScopedJavaGlobalRef<jobject>& runnable_ref) {
  Java_BaseRunLoop_runRunnable(base::android::AttachCurrentThread(),
                               runnable_ref.obj());
}

static void PostDelayedTask(JNIEnv* env,
                            jobject jcaller,
                            jlong runLoopID,
                            jobject runnable,
                            jlong delay) {
  base::android::ScopedJavaGlobalRef<jobject> runnable_ref;
  // ScopedJavaGlobalRef do not hold onto the env reference, so it is safe to
  // use it across threads. |RunJavaRunnable| will acquire a new JNIEnv before
  // running the Runnable.
  runnable_ref.Reset(env, runnable);
  reinterpret_cast<base::MessageLoop*>(runLoopID)->PostDelayedTask(
      FROM_HERE, base::Bind(&RunJavaRunnable, runnable_ref),
      base::TimeDelta::FromMicroseconds(delay));
}

static void DeleteMessageLoop(JNIEnv* env, jobject jcaller, jlong runLoopID) {
  base::MessageLoop* message_loop =
      reinterpret_cast<base::MessageLoop*>(runLoopID);
  delete message_loop;
}

bool RegisterBaseRunLoop(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace mojo


