// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/child_process_launcher_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "jni/ChildProcessLauncher_jni.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/surface_texture.h"

using base::android::AttachCurrentThread;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::StartChildProcessCallback;

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render frame ID and player ID.
static void SetSurfacePeer(
    const base::android::JavaRef<jobject>& surface,
    base::ProcessHandle render_process_handle,
    int render_frame_id,
    int player_id) {
  int render_process_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      render_process_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }
  if (!render_process_id) {
    DVLOG(1) << "Cannot find render process for render_process_handle "
             << render_process_handle;
    return;
  }

  RenderFrameHostImpl* frame =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame) {
    DVLOG(1) << "Cannot find frame for render_frame_id " << render_frame_id;
    return;
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));
  BrowserMediaPlayerManager* player_manager =
      web_contents->media_web_contents_observer()->GetMediaPlayerManager(frame);
  if (!player_manager) {
    DVLOG(1) << "Cannot find the media player manager for frame " << frame;
    return;
  }

  media::MediaPlayerAndroid* player = player_manager->GetPlayer(player_id);
  if (!player) {
    DVLOG(1) << "Cannot find media player for player_id " << player_id;
    return;
  }

  if (player != player_manager->GetFullscreenPlayer()) {
    gfx::ScopedJavaSurface scoped_surface(surface);
    player->SetVideoSurface(scoped_surface.Pass());
  }
}

}  // anonymous namespace

// Called from ChildProcessLauncher.java when the ChildProcess was
// started.
// |client_context| is the pointer to StartChildProcessCallback which was
// passed in from StartChildProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the ChildProcess could not be created.
static void OnChildProcessStarted(JNIEnv*,
                                  jclass,
                                  jlong client_context,
                                  jint handle) {
  StartChildProcessCallback* callback =
      reinterpret_cast<StartChildProcessCallback*>(client_context);
  if (handle)
    callback->Run(static_cast<base::ProcessHandle>(handle));
  delete callback;
}

void StartChildProcess(
    const base::CommandLine::StringVector& argv,
    int child_process_id,
    scoped_ptr<content::FileDescriptorInfo> files_to_register,
    const std::map<int, base::MemoryMappedFile::Region>& regions,
    const StartChildProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register->GetMappingSize();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jclass> j_file_info_class = base::android::GetClass(
      env, "org/chromium/content/browser/FileDescriptorInfo");
  ScopedJavaLocalRef<jobjectArray> j_file_infos(
      env, env->NewObjectArray(file_count, j_file_info_class.obj(), NULL));
  base::android::CheckException(env);

  for (size_t i = 0; i < file_count; ++i) {
    int fd = files_to_register->GetFDAt(i);
    PCHECK(0 <= fd);
    int id = files_to_register->GetIDAt(i);
    bool auto_close = files_to_register->OwnsFD(fd);
    int64 offset = 0L;
    int64 size = 0L;
    auto found_region_iter = regions.find(id);
    if (found_region_iter != regions.end()) {
      offset = found_region_iter->second.offset;
      size = found_region_iter->second.size;
    }
    ScopedJavaLocalRef<jobject> j_file_info =
        Java_ChildProcessLauncher_makeFdInfo(env, id, fd, auto_close, offset,
                                             size);
    PCHECK(j_file_info.obj());
    env->SetObjectArrayElement(j_file_infos.obj(), i, j_file_info.obj());
    if (auto_close) {
      ignore_result(files_to_register->ReleaseFD(fd).release());
    }
  }

  Java_ChildProcessLauncher_start(
      env, base::android::GetApplicationContext(), j_argv.obj(),
      child_process_id, j_file_infos.obj(),
      reinterpret_cast<intptr_t>(new StartChildProcessCallback(callback)));
}

void StopChildProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_stop(env, static_cast<jint>(handle));
}

bool IsChildProcessOomProtected(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_isOomProtected(env,
      static_cast<jint>(handle));
}

void SetChildProcessInForeground(base::ProcessHandle handle,
    bool in_foreground) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_setInForeground(env,
      static_cast<jint>(handle), static_cast<jboolean>(in_foreground));
}

void EstablishSurfacePeer(
    JNIEnv* env, jclass clazz,
    jint pid, jobject surface, jint primary_id, jint secondary_id) {
  ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  if (jsurface.is_null())
    return;

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, jsurface, pid, primary_id, secondary_id));
}

void RegisterViewSurface(int surface_id, jobject j_surface) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_registerViewSurface(env, surface_id, j_surface);
}

void UnregisterViewSurface(int surface_id) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_unregisterViewSurface(env, surface_id);
}

void CreateSurfaceTextureSurface(int surface_texture_id,
                                 int client_id,
                                 gfx::SurfaceTexture* surface_texture) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_createSurfaceTextureSurface(
      env,
      surface_texture_id,
      client_id,
      surface_texture->j_surface_texture().obj());
}

void DestroySurfaceTextureSurface(int surface_texture_id, int client_id) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_destroySurfaceTextureSurface(
      env, surface_texture_id, client_id);
}

gfx::ScopedJavaSurface GetSurfaceTextureSurface(int surface_texture_id,
                                                int client_id) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return gfx::ScopedJavaSurface::AcquireExternalSurface(
      Java_ChildProcessLauncher_getSurfaceTextureSurface(
          env, surface_texture_id, client_id).obj());
}

jboolean IsSingleProcess(JNIEnv* env, jclass clazz) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

bool RegisterChildProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
