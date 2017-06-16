// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
#define MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

// VideoCaptureDevice on Android. The VideoCaptureDevice API's are called
// by VideoCaptureManager on its own thread, while OnFrameAvailable is called
// on JAVA thread (i.e., UI thread). Both will access |state_| and |client_|,
// but only VideoCaptureManager would change their value.
class MEDIA_EXPORT VideoCaptureDeviceAndroid : public VideoCaptureDevice {
 public:
  // Automatically generated enum to interface with Java world.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
  enum AndroidImageFormat {
    // Android graphics ImageFormat mapping, see reference in:
    // http://developer.android.com/reference/android/graphics/ImageFormat.html
    ANDROID_IMAGE_FORMAT_NV21 = 17,
    ANDROID_IMAGE_FORMAT_YUV_420_888 = 35,
    ANDROID_IMAGE_FORMAT_YV12 = 842094169,
    ANDROID_IMAGE_FORMAT_UNKNOWN = 0,
  };

  explicit VideoCaptureDeviceAndroid(const Name& device_name);
  ~VideoCaptureDeviceAndroid() override;

  static VideoCaptureDevice* Create(const Name& device_name);
  static bool RegisterVideoCaptureDevice(JNIEnv* env);

  // Registers the Java VideoCaptureDevice pointer, used by the rest of the
  // methods of the class to operate the Java capture code. This method must be
  // called after the class constructor and before AllocateAndStart().
  bool Init();

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        scoped_ptr<Client> client) override;
  void StopAndDeAllocate() override;

  // Implement org.chromium.media.VideoCapture.nativeOnFrameAvailable.
  void OnFrameAvailable(
      JNIEnv* env,
      jobject obj,
      jbyteArray data,
      jint length,
      jint rotation);

  // Implement org.chromium.media.VideoCapture.nativeOnError.
  void OnError(JNIEnv* env, jobject obj, jstring message);

 private:
  enum InternalState {
    kIdle,  // The device is opened but not in use.
    kCapturing,  // Video is being captured.
    kError  // Hit error. User needs to recover by destroying the object.
  };

  VideoPixelFormat GetColorspace();
  void SetErrorState(const std::string& reason);

  // Prevent racing on accessing |state_| and |client_| since both could be
  // accessed from different threads.
  base::Lock lock_;
  InternalState state_;
  bool got_first_frame_;
  base::TimeTicks expected_next_frame_time_;
  base::TimeDelta frame_interval_;
  scoped_ptr<VideoCaptureDevice::Client> client_;

  Name device_name_;
  VideoCaptureFormat capture_format_;

  // Java VideoCaptureAndroid instance.
  base::android::ScopedJavaLocalRef<jobject> j_capture_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceAndroid);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_ANDROID_H_
