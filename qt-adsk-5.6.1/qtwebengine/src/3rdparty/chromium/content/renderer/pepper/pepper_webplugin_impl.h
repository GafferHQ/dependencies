// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ppapi/c/pp_var.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "ui/gfx/geometry/rect.h"

struct _NPP;

namespace blink {
struct WebPluginParams;
struct WebPrintParams;
}

namespace content {

class PepperPluginInstanceImpl;
class PluginInstanceThrottlerImpl;
class PluginModule;
class PPB_URLLoader_Impl;
class RenderFrameImpl;

class PepperWebPluginImpl : public blink::WebPlugin {
 public:
  PepperWebPluginImpl(PluginModule* module,
                      const blink::WebPluginParams& params,
                      RenderFrameImpl* render_frame,
                      scoped_ptr<PluginInstanceThrottlerImpl> throttler);

  PepperPluginInstanceImpl* instance() { return instance_.get(); }

  // blink::WebPlugin implementation.
  virtual blink::WebPluginContainer* container() const;
  virtual bool initialize(blink::WebPluginContainer* container);
  virtual void destroy();
  virtual v8::Local<v8::Object> v8ScriptableObject(
      v8::Isolate* isolate) override;
  virtual bool getFormValue(blink::WebString& value);
  virtual void layoutIfNeeded() override { }
  virtual void paint(blink::WebCanvas* canvas, const blink::WebRect& rect);
  virtual void updateGeometry(
      const blink::WebRect& window_rect,
      const blink::WebRect& clip_rect,
      const blink::WebRect& unobscured_rect,
      const blink::WebVector<blink::WebRect>& cut_outs_rects,
      bool is_visible);
  virtual void updateFocus(bool focused, blink::WebFocusType focus_type);
  virtual void updateVisibility(bool visible);
  virtual bool acceptsInputEvents();
  virtual bool handleInputEvent(const blink::WebInputEvent& event,
                                blink::WebCursorInfo& cursor_info);
  virtual void didReceiveResponse(const blink::WebURLResponse& response);
  virtual void didReceiveData(const char* data, int data_length);
  virtual void didFinishLoading();
  virtual void didFailLoading(const blink::WebURLError&);
  virtual void didFinishLoadingFrameRequest(const blink::WebURL& url,
                                            void* notify_data);
  virtual void didFailLoadingFrameRequest(const blink::WebURL& url,
                                          void* notify_data,
                                          const blink::WebURLError& error);
  virtual bool hasSelection() const;
  virtual blink::WebString selectionAsText() const;
  virtual blink::WebString selectionAsMarkup() const;
  virtual blink::WebURL linkAtPosition(const blink::WebPoint& position) const;
  virtual bool getPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* preset_options);
  virtual bool startFind(const blink::WebString& search_text,
                         bool case_sensitive,
                         int identifier);
  virtual void selectFindResult(bool forward);
  virtual void stopFind();
  virtual bool supportsPaginatedPrint() override;
  virtual bool isPrintScalingDisabled() override;

  virtual int printBegin(const blink::WebPrintParams& print_params) override;
  void printPage(int page_number, blink::WebCanvas* canvas) override;
  virtual void printEnd() override;

  virtual bool canRotateView() override;
  virtual void rotateView(RotationType type) override;
  virtual bool isPlaceholder() override;

 private:
  friend class base::DeleteHelper<PepperWebPluginImpl>;

  virtual ~PepperWebPluginImpl();
  struct InitData;

  scoped_ptr<InitData> init_data_;  // Cleared upon successful initialization.
  // True if the instance represents the entire document in a frame instead of
  // being an embedded resource.
  bool full_frame_;
  scoped_ptr<PluginInstanceThrottlerImpl> throttler_;
  scoped_refptr<PepperPluginInstanceImpl> instance_;
  gfx::Rect plugin_rect_;
  PP_Var instance_object_;
  blink::WebPluginContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(PepperWebPluginImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBPLUGIN_IMPL_H_
