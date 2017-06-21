// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_

#include <map>
#include <set>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/browser/android/java/gin_java_method_invocation_helper.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class ListValue;
}

namespace IPC {
class Message;
}

namespace content {

// This class handles injecting Java objects into a single RenderView. The Java
// object itself lives in the browser process on a background thread, while a
// proxy object is created in the renderer. An instance of this class exists
// for each RenderFrameHost.
class GinJavaBridgeDispatcherHost
    : public WebContentsObserver,
      public BrowserMessageFilter,
      public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:

  GinJavaBridgeDispatcherHost(WebContents* web_contents,
                              jobject retained_object_set);

  void AddNamedObject(
      const std::string& name,
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz);
  void RemoveNamedObject(const std::string& name);
  void SetAllowObjectContentsInspection(bool allow);

  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void DocumentAvailableInMainFrame() override;

  // BrowserMessageFilter
  using BrowserMessageFilter::Send;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;

  // GinJavaMethodInvocationHelper::DispatcherDelegate
  JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<GinJavaBridgeDispatcherHost>;

  typedef std::map<GinJavaBoundObject::ObjectID,
                   scoped_refptr<GinJavaBoundObject>> ObjectMap;

  ~GinJavaBridgeDispatcherHost() override;

  void AddBrowserFilterIfNeeded();

  // Run on any thread.
  GinJavaBoundObject::ObjectID AddObject(
      const base::android::JavaRef<jobject>& object,
      const base::android::JavaRef<jclass>& safe_annotation_clazz,
      bool is_named,
      int32 holder);
  scoped_refptr<GinJavaBoundObject> FindObject(
      GinJavaBoundObject::ObjectID object_id);
  bool FindObjectId(const base::android::JavaRef<jobject>& object,
                    GinJavaBoundObject::ObjectID* object_id);
  void RemoveFromRetainedObjectSetLocked(const JavaObjectWeakGlobalRef& ref);
  JavaObjectWeakGlobalRef RemoveHolderAndAdvanceLocked(
      int32 holder,
      ObjectMap::iterator* iter_ptr);

  // Run on the background thread.
  void OnGetMethods(GinJavaBoundObject::ObjectID object_id,
                    std::set<std::string>* returned_method_names);
  void OnHasMethod(GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      base::ListValue* result,
                      content::GinJavaBridgeError* error_code);
  void OnObjectWrapperDeleted(GinJavaBoundObject::ObjectID object_id);
  int GetCurrentRoutingID() const;
  void SetCurrentRoutingID(int routing_id);

  bool browser_filter_added_;

  typedef std::map<std::string, GinJavaBoundObject::ObjectID> NamedObjectMap;
  NamedObjectMap named_objects_;

  // The following objects are used on both threads, so locking must be used.

  // Every time a GinJavaBoundObject backed by a real Java object is
  // created/destroyed, we insert/remove a strong ref to that Java object into
  // this set so that it doesn't get garbage collected while it's still
  // potentially in use. Although the set is managed native side, it's owned
  // and defined in Java so that pushing refs into it does not create new GC
  // roots that would prevent ContentViewCore from being garbage collected.
  JavaObjectWeakGlobalRef retained_object_set_;
  // Note that retained_object_set_ does not need to be consistent
  // with objects_.
  ObjectMap objects_;
  base::Lock objects_lock_;

  // The following objects are only used on the background thread.
  bool allow_object_contents_inspection_;
  // The routing id of the RenderFrameHost whose request we are processing.
  int32 current_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_DISPATCHER_HOST_H_
