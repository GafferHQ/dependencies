// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_MOJO_H_
#define CONTENT_RENDERER_WEB_UI_MOJO_H_

#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"

namespace gin {
class PerContextData;
}

namespace content {

class WebUIMojoContextState;

// WebUIMojo is responsible for enabling the renderer side of mojo bindings.
// It creates (and destroys) a WebUIMojoContextState at the appropriate times
// and handles the necessary browser messages. WebUIMojo destroys itself when
// the RendererView it is created with is destroyed.
class WebUIMojo
    : public RenderViewObserver,
      public RenderViewObserverTracker<WebUIMojo> {
 public:
  explicit WebUIMojo(RenderView* render_view);

 private:
  class MainFrameObserver : public RenderFrameObserver {
   public:
    explicit MainFrameObserver(WebUIMojo* web_ui_mojo);
    ~MainFrameObserver() override;

    // RenderFrameObserver overrides:
    void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                  int world_id) override;
    void DidFinishDocumentLoad() override;
    // MainFrameObserver is inline owned by WebUIMojo and should not be
    // destroyed when the main RenderFrame is deleted. Overriding the
    // OnDestruct method allows this object to remain alive and be cleaned
    // up as part of WebUIMojo deletion.
    void OnDestruct() override;

   private:
    WebUIMojo* web_ui_mojo_;

    DISALLOW_COPY_AND_ASSIGN(MainFrameObserver);
  };

  ~WebUIMojo() override;

  void CreateContextState();
  void DestroyContextState(v8::Local<v8::Context> context);

  // Invoked when the frame finishes loading. Invokes Run() on the
  // WebUIMojoContextState.
  void OnDidFinishDocumentLoad();

  WebUIMojoContextState* GetContextState();

  // RenderViewObserver overrides:
  void DidCreateDocumentElement(blink::WebLocalFrame* frame) override;
  void DidClearWindowObject(blink::WebLocalFrame* frame) override;

  MainFrameObserver main_frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojo);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_MOJO_H_
