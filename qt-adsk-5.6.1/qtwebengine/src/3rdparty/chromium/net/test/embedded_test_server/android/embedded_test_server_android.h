// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

// The C++ side of the Java EmbeddedTestServer.
class EmbeddedTestServerAndroid {
 public:
  EmbeddedTestServerAndroid(JNIEnv* env, jobject obj);
  ~EmbeddedTestServerAndroid();

  void Destroy(JNIEnv* env, jobject obj);

  jboolean InitializeAndWaitUntilReady(JNIEnv* env, jobject jobj);

  jboolean ShutdownAndWaitUntilComplete(JNIEnv* env, jobject jobj);

  base::android::ScopedJavaLocalRef<jstring>
  GetURL(JNIEnv* jenv, jobject jobj, jstring jrelative_url) const;

  void ServeFilesFromDirectory(JNIEnv* env,
                               jobject jobj,
                               jstring jdirectory_path);

  static bool RegisterEmbeddedTestServerAndroid(JNIEnv* env);

 private:
  JavaObjectWeakGlobalRef weak_java_server_;

  EmbeddedTestServer test_server_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedTestServerAndroid);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_ANDROID_EMBEDDED_TEST_SERVER_ANDROID_H_
