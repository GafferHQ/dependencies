// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace IPC {
class MessageFilter;
}

namespace network_hints {
class PrescientNetworkingDispatcher;
}  // namespace network_hints

namespace chromecast {
namespace shell {
class CastRenderProcessObserver;

class CastContentRendererClient : public content::ContentRendererClient {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static scoped_ptr<CastContentRendererClient> Create();

  ~CastContentRendererClient() override;

  // Adds any platform-specific bindings to the current frame.
  virtual void AddRendererNativeBindings(blink::WebLocalFrame* frame);

  // Returns any MessageFilters from the platform implementation that should
  // be added to the render process.
  virtual std::vector<scoped_refptr<IPC::MessageFilter>>
  GetRendererMessageFilters();

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void AddKeySystems(
      std::vector< ::media::KeySystemInfo>* key_systems) override;
#if !defined(OS_ANDROID)
  scoped_ptr<::media::RendererFactory> CreateMediaRendererFactory(
      content::RenderFrame* render_frame,
      const scoped_refptr<::media::GpuVideoAcceleratorFactories>& gpu_factories,
      const scoped_refptr<::media::MediaLog>& media_log) override;
#endif
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  void DeferMediaLoad(content::RenderFrame* render_frame,
                      const base::Closure& closure) override;

 protected:
  CastContentRendererClient();

 private:
  scoped_ptr<network_hints::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;
  scoped_ptr<CastRenderProcessObserver> cast_observer_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
