/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "talk/app/webrtc/java/jni/classreferenceholder.h"

#include "talk/app/webrtc/java/jni/jni_helpers.h"

namespace webrtc_jni {

// ClassReferenceHolder holds global reference to Java classes in app/webrtc.
class ClassReferenceHolder {
 public:
  explicit ClassReferenceHolder(JNIEnv* jni);
  ~ClassReferenceHolder();

  void FreeReferences(JNIEnv* jni);
  jclass GetClass(const std::string& name);

 private:
  void LoadClass(JNIEnv* jni, const std::string& name);

  std::map<std::string, jclass> classes_;
};

// Allocated in LoadGlobalClassReferenceHolder(),
// freed in FreeGlobalClassReferenceHolder().
static ClassReferenceHolder* g_class_reference_holder = nullptr;

void LoadGlobalClassReferenceHolder() {
  CHECK(g_class_reference_holder == nullptr);
  g_class_reference_holder = new ClassReferenceHolder(GetEnv());
}

void FreeGlobalClassReferenceHolder() {
  g_class_reference_holder->FreeReferences(AttachCurrentThreadIfNeeded());
  delete g_class_reference_holder;
  g_class_reference_holder = nullptr;
}

ClassReferenceHolder::ClassReferenceHolder(JNIEnv* jni) {
  LoadClass(jni, "java/nio/ByteBuffer");
  LoadClass(jni, "org/webrtc/AudioTrack");
  LoadClass(jni, "org/webrtc/DataChannel");
  LoadClass(jni, "org/webrtc/DataChannel$Buffer");
  LoadClass(jni, "org/webrtc/DataChannel$Init");
  LoadClass(jni, "org/webrtc/DataChannel$State");
  LoadClass(jni, "org/webrtc/IceCandidate");
#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  LoadClass(jni, "android/graphics/SurfaceTexture");
  LoadClass(jni, "org/webrtc/VideoCapturerAndroid");
  LoadClass(jni, "org/webrtc/VideoCapturerAndroid$NativeObserver");
  LoadClass(jni, "org/webrtc/EglBase");
  LoadClass(jni, "org/webrtc/MediaCodecVideoEncoder");
  LoadClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
  LoadClass(jni, "org/webrtc/MediaCodecVideoEncoder$VideoCodecType");
  LoadClass(jni, "org/webrtc/MediaCodecVideoDecoder");
  LoadClass(jni, "org/webrtc/MediaCodecVideoDecoder$DecoderOutputBufferInfo");
  LoadClass(jni, "org/webrtc/MediaCodecVideoDecoder$VideoCodecType");
  jclass j_egl_base_class = GetClass("org/webrtc/EglBase");
  jmethodID j_is_egl14_supported_method = jni->GetStaticMethodID(
      j_egl_base_class, "isEGL14Supported", "()Z");
  bool is_egl14_supported = jni->CallStaticBooleanMethod(
      j_egl_base_class, j_is_egl14_supported_method);
  CHECK_EXCEPTION(jni);
  if (is_egl14_supported) {
    LoadClass(jni, "android/opengl/EGLContext");
  }
#endif
  LoadClass(jni, "org/webrtc/MediaSource$State");
  LoadClass(jni, "org/webrtc/MediaStream");
  LoadClass(jni, "org/webrtc/MediaStreamTrack$State");
  LoadClass(jni, "org/webrtc/PeerConnection$BundlePolicy");
  LoadClass(jni, "org/webrtc/PeerConnection$RtcpMuxPolicy");
  LoadClass(jni, "org/webrtc/PeerConnection$IceConnectionState");
  LoadClass(jni, "org/webrtc/PeerConnection$IceGatheringState");
  LoadClass(jni, "org/webrtc/PeerConnection$IceTransportsType");
  LoadClass(jni, "org/webrtc/PeerConnection$TcpCandidatePolicy");
  LoadClass(jni, "org/webrtc/PeerConnection$SignalingState");
  LoadClass(jni, "org/webrtc/SessionDescription");
  LoadClass(jni, "org/webrtc/SessionDescription$Type");
  LoadClass(jni, "org/webrtc/StatsReport");
  LoadClass(jni, "org/webrtc/StatsReport$Value");
  LoadClass(jni, "org/webrtc/VideoRenderer$I420Frame");
  LoadClass(jni, "org/webrtc/VideoCapturer");
  LoadClass(jni, "org/webrtc/VideoTrack");
}

ClassReferenceHolder::~ClassReferenceHolder() {
  CHECK(classes_.empty()) << "Must call FreeReferences() before dtor!";
}

void ClassReferenceHolder::FreeReferences(JNIEnv* jni) {
  for (std::map<std::string, jclass>::const_iterator it = classes_.begin();
      it != classes_.end(); ++it) {
    jni->DeleteGlobalRef(it->second);
  }
  classes_.clear();
}

jclass ClassReferenceHolder::GetClass(const std::string& name) {
  std::map<std::string, jclass>::iterator it = classes_.find(name);
  CHECK(it != classes_.end()) << "Unexpected GetClass() call for: " << name;
  return it->second;
}

void ClassReferenceHolder::LoadClass(JNIEnv* jni, const std::string& name) {
  jclass localRef = jni->FindClass(name.c_str());
  CHECK_EXCEPTION(jni) << "error during FindClass: " << name;
  CHECK(localRef) << name;
  jclass globalRef = reinterpret_cast<jclass>(jni->NewGlobalRef(localRef));
  CHECK_EXCEPTION(jni) << "error during NewGlobalRef: " << name;
  CHECK(globalRef) << name;
  bool inserted = classes_.insert(std::make_pair(name, globalRef)).second;
  CHECK(inserted) << "Duplicate class name: " << name;
}

// Returns a global reference guaranteed to be valid for the lifetime of the
// process.
jclass FindClass(JNIEnv* jni, const char* name) {
  return g_class_reference_holder->GetClass(name);
}

}  // namespace webrtc_jni
