// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/input/top_controls_state.h"
#include "cc/resources/shared_bitmap.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/edit_command.h"
#include "content/common/frame_message_enums.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/navigation_gesture.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/top_controls_state.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/stats_collection_observer.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebIconURL.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPageSerializerClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "ui/base/window_open_disposition.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/content_detector.h"
#include "content/renderer/android/renderer_date_time_picker.h"
#include "third_party/WebKit/public/web/WebContentDetectionResult.h"
#endif

#if defined(COMPILER_MSVC)
// RenderViewImpl is a diamond-shaped hierarchy, with WebWidgetClient at the
// root. VS warns when we inherit the WebWidgetClient method implementations
// from RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class SkBitmap;
struct PP_NetAddress_Private;
struct ViewMsg_New_Params;
struct ViewMsg_Resize_Params;
struct ViewMsg_StopFinding_Params;

namespace base {
class CommandLine;
}

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebDOMMessageEvent;
class WebDataSource;
class WebDateTimeChooserCompletion;
class WebDragData;
class WebGestureEvent;
class WebIconURL;
class WebImage;
class WebPeerConnection00Handler;
class WebPeerConnection00HandlerClient;
class WebMouseEvent;
class WebPeerConnectionHandler;
class WebPeerConnectionHandlerClient;
class WebSpeechRecognizer;
class WebStorageNamespace;
class WebTouchEvent;
class WebURLRequest;
struct WebActiveWheelFlingParameters;
struct WebDateTimeChooserParams;
struct WebFileChooserParams;
struct WebFindOptions;
struct WebMediaPlayerAction;
struct WebPluginAction;
struct WebPoint;
struct WebWindowFeatures;

#if defined(OS_ANDROID)
class WebHitTestResult;
#endif
}  // namespace blink

namespace content {

class HistoryController;
class HistoryEntry;
class MouseLockDispatcher;
class PageState;
class PepperPluginInstanceImpl;
class RenderViewImplTest;
class RenderViewObserver;
class RenderViewTest;
class RendererDateTimePicker;
class RendererWebColorChooserImpl;
class SpeechRecognitionDispatcher;
class WebPluginDelegateProxy;
struct DropData;
struct FaviconURL;
struct FileChooserParams;
struct FileChooserFileInfo;
struct RenderViewImplParams;

#if defined(OS_ANDROID)
class WebMediaPlayerProxyAndroid;
#endif

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process.
//
// DEPRECATED: RenderViewImpl is being removed as part of the SiteIsolation
// project. New code should be added to RenderFrameImpl instead.
//
// For context, please see https://crbug.com/467770 and
// http://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewImpl
    : public RenderWidget,
      NON_EXPORTED_BASE(public blink::WebViewClient),
      NON_EXPORTED_BASE(public blink::WebPageSerializerClient),
      public RenderView,
      public base::SupportsWeakPtr<RenderViewImpl> {
 public:
  // Creates a new RenderView. |opener_id| is the routing ID of the RenderView
  // responsible for creating this RenderView. Note that if the original opener
  // has been closed, |window_was_created_with_opener| will be true and
  // |opener_id| will be MSG_ROUTING_NONE. When |swapped_out| is true, the
  // |proxy_routing_id| is specified, so a RenderFrameProxy can be created for
  // this RenderView's main RenderFrame.
  static RenderViewImpl* Create(const ViewMsg_New_Params& params,
                                CompositorDependencies* compositor_deps,
                                bool was_created_by_renderer);

  // Used by content_layouttest_support to hook into the creation of
  // RenderViewImpls.
  static void InstallCreateHook(
      RenderViewImpl* (*create_render_view_impl)(const ViewMsg_New_Params&));

  // Returns the RenderViewImpl containing the given WebView.
  static RenderViewImpl* FromWebView(blink::WebView* webview);

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

  // May return NULL when the view is closing.
  blink::WebView* webview() const;

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  const RendererPreferences& renderer_preferences() const {
    return renderer_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  MouseLockDispatcher* mouse_lock_dispatcher() {
    return mouse_lock_dispatcher_;
  }

  HistoryController* history_controller() {
    return history_controller_.get();
  }

  // Functions to add and remove observers for this object.
  void AddObserver(RenderViewObserver* observer);
  void RemoveObserver(RenderViewObserver* observer);

  // Returns the StatsCollectionObserver associated with this view, or NULL
  // if one wasn't created;
  StatsCollectionObserver* GetStatsCollectionObserver() {
    return stats_collection_observer_.get();
  }

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const FileChooserParams& params,
                           blink::WebFileChooserCompletion* completion);

#if defined(OS_ANDROID)
  void DismissDateTimeDialog();
#endif

  bool is_loading() const { return frames_in_progress_ != 0; }

  void FrameDidStartLoading(blink::WebFrame* frame);
  void FrameDidStopLoading(blink::WebFrame* frame);

  // Plugin-related functions --------------------------------------------------

#if defined(ENABLE_PLUGINS)
  // Get/set the plugin which will be used as to handle document find requests.
  void set_plugin_find_handler(PepperPluginInstanceImpl* plugin) {
    plugin_find_handler_ = plugin;
  }
  PepperPluginInstanceImpl* plugin_find_handler() {
    return plugin_find_handler_;
  }

  PepperPluginInstanceImpl* focused_pepper_plugin() {
    return focused_pepper_plugin_;
  }
  PepperPluginInstanceImpl* pepper_last_mouse_event_target() {
    return pepper_last_mouse_event_target_;
  }
  void set_pepper_last_mouse_event_target(PepperPluginInstanceImpl* plugin) {
    pepper_last_mouse_event_target_ = plugin;
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Informs the render view that the given plugin has gained or lost focus.
  void PluginFocusChanged(bool focused, int plugin_id);
#endif

#if defined(OS_MACOSX)
  // Starts plugin IME.
  void StartPluginIme();
#endif

  // Indicates that the given instance has been created.
  void PepperInstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  void RegisterPluginDelegate(WebPluginDelegateProxy* delegate);
  void UnregisterPluginDelegate(WebPluginDelegateProxy* delegate);
#endif  // ENABLE_PLUGINS

  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params);

  // Returns true if the focused element is editable text from the perspective
  // of IME support (also used for on-screen keyboard). Works correctly inside
  // supported PPAPI plugins.
  bool HasIMETextFocus();

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

  // Returns the length of the session history of this RenderView. Note that
  // this only coincides with the actual length of the session history if this
  // RenderView is the currently active RenderView of a WebContents.
  unsigned GetLocalSessionHistoryLengthForTesting() const;

  // Invokes OnSetFocus and marks the widget as active depending on the value
  // of |enable|. This is used for layout tests that need to control the focus
  // synchronously from the renderer.
  void SetFocusAndActivateForTesting(bool enable);

  // Change the device scale factor and force the compositor to resize.
  void SetDeviceScaleFactorForTesting(float factor);

  // Change the device ICC color profile while running a layout test.
  void SetDeviceColorProfileForTesting(const std::vector<char>& color_profile);
  void ResetDeviceColorProfileForTesting() override;

  // Used to force the size of a window when running layout tests.
  void ForceResizeForTesting(const gfx::Size& new_size);

  void UseSynchronousResizeModeForTesting(bool enable);

  // Control autoresize mode.
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  // IPC::Listener implementation ----------------------------------------------

  bool OnMessageReceived(const IPC::Message& msg) override;

  // blink::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  virtual void didFocus();
  virtual void didBlur();
  virtual void show(blink::WebNavigationPolicy policy);
  virtual bool requestPointerLock();
  virtual void requestPointerUnlock();
  virtual bool isPointerLocked();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled) override;
  virtual void onMouseDown(const blink::WebNode& mouse_down_node) override;

  virtual void initializeLayerTreeView() override;

  // blink::WebViewClient implementation --------------------------------------

  virtual blink::WebView* createView(blink::WebLocalFrame* creator,
                                     const blink::WebURLRequest& request,
                                     const blink::WebWindowFeatures& features,
                                     const blink::WebString& frame_name,
                                     blink::WebNavigationPolicy policy,
                                     bool suppress_opener);
  virtual blink::WebWidget* createPopupMenu(blink::WebPopupType popup_type);
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();
  virtual void printPage(blink::WebLocalFrame* frame);
  virtual bool enumerateChosenDirectory(
      const blink::WebString& path,
      blink::WebFileChooserCompletion* chooser_completion);
  virtual void saveImageFromDataURL(const blink::WebString& data_url);
  virtual void didCancelCompositionOnSelectionChange();
  virtual bool handleCurrentKeyboardEvent();
  virtual bool runFileChooser(
      const blink::WebFileChooserParams& params,
      blink::WebFileChooserCompletion* chooser_completion);
  void SetValidationMessageDirection(base::string16* main_text,
                                     blink::WebTextDirection main_text_hint,
                                     base::string16* sub_text,
                                     blink::WebTextDirection sub_text_hint);
  virtual void showValidationMessage(const blink::WebRect& anchor_in_root_view,
                                     const blink::WebString& main_text,
                                     blink::WebTextDirection main_text_hint,
                                     const blink::WebString& sub_text,
                                     blink::WebTextDirection hint) override;
  virtual void hideValidationMessage() override;
  virtual void moveValidationMessage(
      const blink::WebRect& anchor_in_root_view) override;
  virtual void setStatusText(const blink::WebString& text);
  virtual void setMouseOverURL(const blink::WebURL& url);
  virtual void setKeyboardFocusURL(const blink::WebURL& url);
  virtual void startDragging(blink::WebLocalFrame* frame,
                             const blink::WebDragData& data,
                             blink::WebDragOperationsMask mask,
                             const blink::WebImage& image,
                             const blink::WebPoint& imageOffset);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void focusedNodeChanged(const blink::WebNode& fromNode,
                                  const blink::WebNode& toNode);
  virtual void didUpdateLayout();
#if defined(OS_ANDROID) || defined(USE_AURA)
  virtual bool didTapMultipleTargets(
      const blink::WebSize& inner_viewport_offset,
      const blink::WebRect& touch_rect,
      const blink::WebVector<blink::WebRect>& target_rects);
#endif
  virtual blink::WebString acceptLanguages();
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual blink::WebSpeechRecognizer* speechRecognizer();
  virtual void zoomLimitsChanged(double minimum_level, double maximum_level);
  virtual void zoomLevelChanged();
  virtual void pageScaleFactorChanged();
  virtual double zoomLevelToZoomFactor(double zoom_level) const;
  virtual double zoomFactorToZoomLevel(double factor) const;
  virtual blink::WebPageVisibilityState visibilityState() const;
  virtual void draggableRegionsChanged();

#if defined(OS_ANDROID)
  virtual void scheduleContentIntent(const blink::WebURL& intent);
  virtual void cancelScheduledContentIntents();
  virtual blink::WebContentDetectionResult detectContentAround(
      const blink::WebHitTestResult& touch_hit);

  // Only used on Android since all other platforms implement
  // date and time input fields using MULTIPLE_FIELDS_UI
  virtual bool openDateTimeChooser(const blink::WebDateTimeChooserParams&,
                                   blink::WebDateTimeChooserCompletion*);
  virtual void didScrollWithKeyboard(const blink::WebSize& delta);
#endif

  // blink::WebPageSerializerClient implementation ----------------------------

  virtual void didSerializeDataForFrame(
      const blink::WebURL& frame_url,
      const blink::WebCString& data,
      PageSerializationStatus status) override;

  // RenderView implementation -------------------------------------------------

  bool Send(IPC::Message* message) override;
  RenderFrameImpl* GetMainRenderFrame() override;
  int GetRoutingID() const override;
  gfx::Size GetSize() const override;
  WebPreferences& GetWebkitPreferences() override;
  void SetWebkitPreferences(const WebPreferences& preferences) override;
  blink::WebView* GetWebView() override;
  bool IsEditableNode(const blink::WebNode& node) const override;
  bool ShouldDisplayScrollbars(int width, int height) const override;
  int GetEnabledBindings() const override;
  bool GetContentStateImmediately() const override;
  blink::WebPageVisibilityState GetVisibilityState() const override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void Repaint(const gfx::Size& size) override;
  void SetEditCommandForNextKeyEvent(const std::string& name,
                                     const std::string& value) override;
  void ClearEditCommands() override;
  SSLStatus GetSSLStatusOfFrame(blink::WebFrame* frame) const override;
  const std::string& GetAcceptLanguages() const override;
#if defined(OS_ANDROID)
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override;
#endif
  bool uses_temporary_zoom_level() const { return uses_temporary_zoom_level_; }

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

 protected:
  // RenderWidget overrides:
  void Close() override;
  void OnResize(const ViewMsg_Resize_Params& params) override;
  void DidInitiatePaint() override;
  void DidFlushPaint() override;
  gfx::Vector2d GetScrollOffset() override;
  void DidHandleKeyEvent() override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override;
  void OnSetFocus(bool enable) override;
  void OnWasHidden() override;
  void OnWasShown(bool needs_repainting,
                  const ui::LatencyInfo& latency_info) override;
  GURL GetURLForGraphicsContext3D() override;
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) override;
  void OnImeConfirmComposition(const base::string16& text,
                               const gfx::Range& replacement_range,
                               bool keep_selection) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  bool SetDeviceColorProfile(const std::vector<char>& color_profile) override;
  void OnOrientationChange() override;
  ui::TextInputType GetTextInputType() override;
  void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) override;
  void FocusChangeComplete() override;
  void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds) override;
  void GetCompositionRange(gfx::Range* range) override;
  bool CanComposeInline() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;

 protected:
  explicit RenderViewImpl(const ViewMsg_New_Params& params);

  void Initialize(const ViewMsg_New_Params& params,
                  CompositorDependencies* compositor_deps,
                  bool was_created_by_renderer);
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderViewImpl();

 private:
  // For unit tests.
  friend class DevToolsAgentTest;
  friend class RenderViewImplTest;
  friend class RenderViewTest;
  friend class RendererAccessibilityTest;

  // TODO(nasko): Temporarily friend RenderFrameImpl, so we don't duplicate
  // utility functions needed in both classes, while we move frame specific
  // code away from this class.
  friend class RenderFrameImpl;

  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameMessageAfterDetach);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, DecideNavigationPolicyForWebUI);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForError);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForCancellation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeTypeChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnSetTextDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnUpdateWebPreferences);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetEditableSelectionAndComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, StaleNavigationsIgnored);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, UpdateTargetURLWithInvalidURL);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           GetCompositionCharacterBoundsTest);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavigationHttpPost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ScreenMetricsEmulation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DecideNavigationPolicyHandlesAllTopLevel);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetHistoryLengthAndOffset);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ZoomLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, NavigateFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, BasicRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, TextInputTypeWithPepper);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           MessageOrderInDidChangeSelection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SendCandidateWindowEvents);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameClearedAfterClose);
  FRIEND_TEST_ALL_PREFIXES(SuppressErrorPageTest, Suppresses);
  FRIEND_TEST_ALL_PREFIXES(SuppressErrorPageTest, DoesNotSuppress);

  typedef std::map<GURL, double> HostZoomLevels;

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  // Old WebFrameClient implementations ----------------------------------------

  // RenderViewImpl used to be a WebFrameClient, but now RenderFrameImpl is the
  // WebFrameClient. However, many implementations of WebFrameClient methods
  // still live here and are called from RenderFrameImpl. These implementations
  // are to be moved to RenderFrameImpl <http://crbug.com/361761>.

  void didChangeIcon(blink::WebLocalFrame*, blink::WebIconURL::Type);
  void didUpdateCurrentHistoryItem(blink::WebLocalFrame* frame);
  void didChangeScrollOffset(blink::WebLocalFrame* frame);

  static Referrer GetReferrerFromRequest(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request);

  static WindowOpenDisposition NavigationPolicyToDisposition(
      blink::WebNavigationPolicy policy);

  void UpdateSessionHistory(blink::WebFrame* frame);
  void SendUpdateState(HistoryEntry* entry);

  void ApplyWebPreferencesInternal(const WebPreferences& prefs,
                                   blink::WebView* web_view,
                                   CompositorDependencies* compositor_deps);

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnMoveCaret(const gfx::Point& point);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnAllowBindings(int enabled_bindings_flags);
  void OnAllowScriptToClose(bool script_can_close);
  void OnCancelDownload(int32 download_id);
  void OnClearFocusedElement();
  void OnClosePage();
  void OnClose();

  void OnShowContextMenu(ui::MenuSourceType source_type,
                         const gfx::Point& location);
  void OnCopyImageAt(int x, int y);
  void OnSaveImageAt(int x, int y);
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnDragSourceEnded(const gfx::Point& client_point,
                         const gfx::Point& screen_point,
                         blink::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt,
                        int key_modifiers);
  void OnDragTargetDragEnter(const DropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             blink::WebDragOperationsMask operations_allowed,
                             int key_modifiers);
  void OnDragTargetDragLeave();
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            blink::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnEnablePreferredSizeChangedMode();
  void OnEnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  void OnDisableAutoResize(const gfx::Size& new_size);
  void OnEnumerateDirectoryResponse(int id,
                                    const std::vector<base::FilePath>& paths);
  void OnFileChooserResponse(
      const std::vector<content::FileChooserFileInfo>& files);
  void OnFind(int request_id,
              const base::string16&,
              const blink::WebFindOptions&);
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<base::FilePath>& local_paths,
      const base::FilePath& local_directory_name);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const blink::WebMediaPlayerAction& action);
  void OnPluginActionAt(const gfx::Point& location,
                        const blink::WebPluginAction& action);
  void OnMoveOrResizeStarted();
  void OnReleaseDisambiguationPopupBitmap(const cc::SharedBitmapId& id);
  void OnResetPageEncodingToDefault();
  void OnSetActive(bool active);
  void OnSetBackgroundOpaque(bool opaque);
  void OnExitFullscreen();
  void OnSetHistoryOffsetAndLength(int history_offset, int history_length);
  void OnSetInitialFocus(bool reverse);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnSetWebUIProperty(const std::string& name, const std::string& value);
  void OnSetZoomLevelForLoadingURL(const GURL& url, double zoom_level);
  void OnSetZoomLevelForView(bool uses_temporary_zoom_level, double level);
  void OnStopFinding(StopFindAction action);
  void OnSuppressDialogsUntilSwapOut();
  void OnThemeChanged();
  void OnUpdateTargetURLAck();
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnResetPageScale();
  void OnZoom(PageZoom zoom);
  void OnEnableViewSourceMode();
  void OnForceRedraw(int request_id);
  void OnSelectWordAroundCaret();
#if defined(OS_ANDROID)
  void OnActivateNearestFindResult(int request_id, float x, float y);
  void OnFindMatchRects(int current_version);
  void OnUndoScrollFocusedEditableNodeIntoRect();
  void OnUpdateTopControlsState(bool enable_hiding,
                                bool enable_showing,
                                bool animate);
  void OnExtractSmartClipData(const gfx::Rect& rect);
#elif defined(OS_MACOSX)
  void OnGetRenderedText();
  void OnPluginImeCompositionCompleted(const base::string16& text,
                                       int plugin_id);
  void OnSetInLiveResize(bool in_live_resize);
  void OnSetWindowVisibility(bool visible);
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
#endif

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------
  // Check whether the preferred size has changed.
  void CheckPreferredSize();

  // Gets the currently focused element, if any.
  blink::WebElement GetFocusedElement() const;

  // Called to get the WebPlugin to handle find requests in the document.
  // Returns NULL if there is no such WebPlugin.
  blink::WebPlugin* GetWebPluginForFind();

#if defined(OS_ANDROID)
  // Launch an Android content intent with the given URL.
  void LaunchAndroidContentIntent(const GURL& intent_url, size_t request_id);
#endif

  // Sends a reply to the current find operation handling if it was a
  // synchronous find request.
  void SendFindReply(int request_id,
                     int match_count,
                     int ordinal,
                     const blink::WebRect& selection_rect,
                     bool final_status_update);

  // Starts nav_state_sync_timer_ if it isn't already running.
  void StartNavStateSyncTimerIfNecessary();

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() {}
#endif

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // Tells the browser what the new list of favicons for the webpage is.
  void SendUpdateFaviconURL(const std::vector<FaviconURL>& urls);

  // Invoked from DidStopLoading(). Sends the current list of loaded favicons to
  // the browser.
  void DidStopLoadingIcons();

  // Coordinate conversion -----------------------------------------------------

  gfx::RectF ClientRectToPhysicalWindowRect(const gfx::RectF& rect) const;

  // RenderFrameImpl accessible state ------------------------------------------
  // The following section is the set of methods that RenderFrameImpl needs
  // to access RenderViewImpl state. The set of state variables are page-level
  // specific, so they don't belong in RenderFrameImpl and should remain in
  // this object.
  base::ObserverList<RenderViewObserver>& observers() { return observers_; }

  // TODO(nasko): Remove this method when we move to frame proxy objects, since
  // the concept of swapped out will be eliminated.
  void set_is_swapped_out(bool swapped_out) {
    is_swapped_out_ = swapped_out;
  }

  NavigationGesture navigation_gesture() {
    return navigation_gesture_;
  }
  void set_navigation_gesture(NavigationGesture gesture) {
    navigation_gesture_ = gesture;
  }

  // Platform specific theme preferences if any are updated here.
#if defined(OS_WIN)
  void UpdateThemePrefs();
#else
  void UpdateThemePrefs() {}
#endif

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  WebPreferences webkit_preferences_;
  RendererPreferences renderer_preferences_;

  HostZoomLevels host_zoom_levels_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  int enabled_bindings_;

  // If true, we send IPC messages when |preferred_size_| changes.
  bool send_preferred_size_changes_;

  // If non-empty, and |send_preferred_size_changes_| is true, disable drawing
  // scroll bars on windows smaller than this size.  Used for windows that the
  // browser resizes to the size of the content, such as browser action popups.
  // If a render view is set to the minimum size of its content, webkit may add
  // scroll bars.  This makes sense for fixed sized windows, but it does not
  // make sense when the size of the view was chosen to fit the content.
  // This setting ensures that no scroll bars are drawn.  The size limit exists
  // because if the view grows beyond a size known to the browser, scroll bars
  // should be drawn.
  gfx::Size disable_scrollbars_size_limit_;

  // Loading state -------------------------------------------------------------

  // The gesture that initiated the current navigation.
  // TODO(nasko): Move to RenderFrame, as this is per-frame state.
  NavigationGesture navigation_gesture_;

  // Used for popups.
  bool opened_by_user_gesture_;

  // Whether this RenderView was created by a frame that was suppressing its
  // opener. If so, we may want to load pages in a separate process.  See
  // decidePolicyForNavigation for details.
  bool opener_suppressed_;

  // Whether we must stop creating nested message loops for modal dialogs until
  // OnSwapOut is called.  This is necessary because modal dialogs have a
  // PageGroupLoadDeferrer on the stack that interferes with swapping out.
  bool suppress_dialogs_until_swap_out_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderViewImpl> nav_state_sync_timer_;

  // Page IDs ------------------------------------------------------------------
  // See documentation in RenderView.
  int32 page_id_;

  // The next available page ID to use for this RenderView.  These IDs are
  // specific to a given RenderView and the frames within it.
  int32 next_page_id_;

  // The offset of the current item in the history list.
  int history_list_offset_;

  // The RenderView's current impression of the history length.  This includes
  // any items that have committed in this process, but because of cross-process
  // navigations, the history may have some entries that were committed in other
  // processes.  We won't know about them until the next navigation in this
  // process.
  int history_list_length_;

  // Counter to track how many frames have sent start notifications but not stop
  // notifications. TODO(avi): Remove this once DidStartLoading/DidStopLoading
  // are gone.
  int frames_in_progress_;

  // UI state ------------------------------------------------------------------

  // The state of our target_url transmissions. When we receive a request to
  // send a URL to the browser, we set this to TARGET_INFLIGHT until an ACK
  // comes back - if a new request comes in before the ACK, we store the new
  // URL in pending_target_url_ and set the status to TARGET_PENDING. If an
  // ACK comes back and we are in TARGET_PENDING, we send the stored URL and
  // revert to TARGET_INFLIGHT.
  //
  // We don't need a queue of URLs to send, as only the latest is useful.
  enum {
    TARGET_NONE,
    TARGET_INFLIGHT,  // We have a request in-flight, waiting for an ACK
    TARGET_PENDING    // INFLIGHT + we have a URL waiting to be sent
  } target_url_status_;

  // The URL we show the user in the status bar. We use this to determine if we
  // want to send a new one (we do not need to send duplicates). It will be
  // equal to either |mouse_over_url_| or |focus_url_|, depending on which was
  // updated last.
  GURL target_url_;

  // The URL the user's mouse is hovering over.
  GURL mouse_over_url_;

  // The URL that has keyboard focus.
  GURL focus_url_;

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // Indicates whether this view overrides url-based zoom settings.
  bool uses_temporary_zoom_level_;

#if defined(OS_ANDROID)
  // Cache the old top controls state constraints. Used when updating
  // current value only without altering the constraints.
  TopControlsState top_controls_constraints_;
#endif

  // View ----------------------------------------------------------------------

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // Used to delay determining the preferred size (to avoid intermediate
  // states for the sizes).
  base::OneShotTimer<RenderViewImpl> check_preferred_size_timer_;

  // Bookkeeping to suppress redundant scroll and focus requests for an already
  // scrolled and focused editable node.
  bool has_scrolled_focused_editable_node_into_rect_;
  gfx::Rect rect_for_scrolled_focused_editable_node_;

  // Helper objects ------------------------------------------------------------

  RenderFrameImpl* main_render_frame_;

  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.  This is why we just store
  // weak references.

  // The speech recognition dispatcher attached to this view, lazily
  // initialized.
  SpeechRecognitionDispatcher* speech_recognition_dispatcher_;

  // Mouse Lock dispatcher attached to this view.
  MouseLockDispatcher* mouse_lock_dispatcher_;

  scoped_ptr<HistoryController> history_controller_;

#if defined(OS_ANDROID)
  // Android Specific ---------------------------------------------------------

  // Expected id of the next content intent launched. Used to prevent scheduled
  // intents to be launched if aborted.
  size_t expected_content_intent_id_;

  // List of click-based content detectors.
  typedef std::vector< linked_ptr<ContentDetector> > ContentDetectorList;
  ContentDetectorList content_detectors_;

  // A date/time picker object for date and time related input elements.
  scoped_ptr<RendererDateTimePicker> date_time_picker_client_;
#endif

  // Plugins -------------------------------------------------------------------

  // All the currently active plugin delegates for this RenderView; kept so
  // that we can enumerate them to send updates about things like window
  // location or tab focus and visibily. These are non-owning references.
  std::set<WebPluginDelegateProxy*> plugin_delegates_;

#if defined(OS_WIN)
  // The ID of the focused NPAPI plugin.
  int focused_plugin_id_;
#endif

#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* plugin_find_handler_;

  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // TODO(jam): these belong on RenderFrame, once the browser knows which frame
  // is focused and sends the IPCs which use these to the correct frame. Until
  // then, we must store these on RenderView as that's the one place that knows
  // about all the RenderFrames for a page.

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_pepper_plugin_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |pepper_last_mouse_event_target_| is not owned by this class. We depend on
  // the RenderFrameImpl to NULL it out when it destructs.
  PepperPluginInstanceImpl* pepper_last_mouse_event_target_;
#endif

  // Misc ----------------------------------------------------------------------

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque< linked_ptr<PendingFileChooser> > file_chooser_completions_;

  // The current directory enumeration callback
  std::map<int, blink::WebFileChooserCompletion*> enumeration_completions_;
  int enumeration_completion_id_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  base::ObserverList<RenderViewObserver> observers_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  scoped_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  // NOTE: stats_collection_observer_ should be the last members because their
  // constructors call the AddObservers method of RenderViewImpl.
  scoped_ptr<StatsCollectionObserver> stats_collection_observer_;

  typedef std::map<cc::SharedBitmapId, cc::SharedBitmap*> BitmapMap;
  BitmapMap disambiguation_bitmaps_;

  bool page_scale_factor_is_one_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
