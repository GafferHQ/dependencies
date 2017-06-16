// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_
#define CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_

#include "base/containers/hash_tables.h"
#include "content/child/npapi/np_channel_base.h"
#include "ipc/ipc_channel_handle.h"

namespace IPC {
class AttachmentBroker;
}

namespace content {
class NPObjectBase;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding PluginChannel.
class PluginChannelHost : public NPChannelBase {
 public:
#if defined(OS_MACOSX)
  // TODO(shess): Debugging for http://crbug.com/97285 .  See comment
  // in plugin_channel_host.cc.
  static bool* GetRemoveTrackingFlag();
#endif
  static PluginChannelHost* GetPluginChannelHost(
      const IPC::ChannelHandle& channel_handle,
      base::SingleThreadTaskRunner* ipc_task_runner,
      IPC::AttachmentBroker* broker);

  bool Init(base::SingleThreadTaskRunner* ipc_task_runner,
            bool create_pipe_now,
            base::WaitableEvent* shutdown_event,
            IPC::AttachmentBroker* broker) override;

  int GenerateRouteID() override;

  void AddRoute(int route_id, IPC::Listener* listener,
                NPObjectBase* npobject);
  void RemoveRoute(int route_id);

  // NPChannelBase override:
  bool Send(IPC::Message* msg) override;

  // IPC::Listener override
  void OnChannelError() override;

  static void Broadcast(IPC::Message* message) {
    NPChannelBase::Broadcast(message);
  }

  bool expecting_shutdown() { return expecting_shutdown_; }

 private:
  // Called on the render thread
  PluginChannelHost();
  ~PluginChannelHost() override;

  static NPChannelBase* ClassFactory() { return new PluginChannelHost(); }

  bool OnControlMessageReceived(const IPC::Message& message) override;
  void OnSetException(const std::string& message);
  void OnPluginShuttingDown();

  // Keep track of all the registered WebPluginDelegeProxies to
  // inform about OnChannelError
  typedef base::hash_map<int, IPC::Listener*> ProxyMap;
  ProxyMap proxies_;

  // True if we are expecting the plugin process to go away - in which case,
  // don't treat it as a crash.
  bool expecting_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PluginChannelHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NPAPI_PLUGIN_CHANNEL_HOST_H_
