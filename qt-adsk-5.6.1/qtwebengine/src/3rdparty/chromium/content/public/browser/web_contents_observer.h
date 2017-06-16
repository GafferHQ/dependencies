// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/security_style.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace content {

class NavigationEntry;
class RenderFrameHost;
class RenderViewHost;
class WebContents;
class WebContentsImpl;
struct AXEventNotificationDetails;
struct FaviconURL;
struct FrameNavigateParams;
struct LoadCommittedDetails;
struct LoadFromMemoryCacheDetails;
struct Referrer;
struct ResourceRedirectDetails;
struct ResourceRequestDetails;
struct SecurityStyleExplanations;

// An observer API implemented by classes which are interested in various page
// load events from WebContents.  They also get a chance to filter IPC messages.
//
// Since a WebContents can be a delegate to almost arbitrarily many
// RenderViewHosts, it is important to check in those WebContentsObserver
// methods which take a RenderViewHost that the event came from the
// RenderViewHost the observer cares about.
//
// Usually, observers should only care about the current RenderViewHost as
// returned by GetRenderViewHost().
//
// TODO(creis, jochen): Hide the fact that there are several RenderViewHosts
// from the WebContentsObserver API. http://crbug.com/173325
class CONTENT_EXPORT WebContentsObserver : public IPC::Listener,
                                           public IPC::Sender {
 public:
  // Called when a RenderFrame for |render_frame_host| is created in the
  // renderer process. Use |RenderFrameDeleted| to listen for when this
  // RenderFrame goes away.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Called when a RenderFrame for |render_frame_host| is deleted or the
  // renderer process in which it runs it has died. Use |RenderFrameCreated| to
  // listen for when RenderFrame objects are created.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // This method is invoked whenever one of the current frames of a WebContents
  // swaps its RenderFrameHost with another one; for example because that frame
  // navigated and the new content is in a different process. The
  // RenderFrameHost that has been replaced is in |old_host|, which can be
  // nullptr if the old RenderFrameHost was shut down or a new frame has been
  // created and no old RenderFrameHost exists.
  //
  // This method, in combination with |FrameDeleted|, is appropriate for
  // observers wishing to track the set of current RenderFrameHosts -- i.e.,
  // those hosts that would be visited by calling WebContents::ForEachFrame.
  virtual void RenderFrameHostChanged(RenderFrameHost* old_host,
                                      RenderFrameHost* new_host) {}

  // This method is invoked when a subframe associated with a WebContents is
  // deleted or the WebContents is destroyed and the top-level frame is deleted.
  // Use |RenderFrameHostChanged| to listen for when a RenderFrameHost object is
  // made the current host for a frame.
  virtual void FrameDeleted(RenderFrameHost* render_frame_host) {}

  // This is called when a RVH is created for a WebContents, but not if it's an
  // interstitial.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called for every RenderFrameHost that's created for an interstitial.
  virtual void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) {}

  // This method is invoked when the RenderView of the current RenderViewHost
  // is ready, e.g. because we recreated it after a crash.
  virtual void RenderViewReady() {}

  // This method is invoked when a RenderViewHost of the WebContents is
  // deleted. Note that this does not always happen when the WebContents starts
  // to use a different RenderViewHost, as the old RenderViewHost might get
  // just swapped out.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // This method is invoked when the process for the current RenderView crashes.
  // The WebContents continues to use the RenderViewHost, e.g. when the user
  // reloads the current page. When the RenderViewHost itself is deleted, the
  // RenderViewDeleted method will be invoked.
  //
  // Note that this is equivalent to
  // RenderProcessHostObserver::RenderProcessExited().
  virtual void RenderProcessGone(base::TerminationStatus status) {}

  // This method is invoked when a WebContents swaps its visible RenderViewHost
  // with another one, possibly changing processes. The RenderViewHost that has
  // been replaced is in |old_host|, which is nullptr if the old RVH was shut
  // down.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) {}

  // This method is invoked after the WebContents decides which RenderFrameHost
  // to use for the next browser-initiated navigation, but before the navigation
  // starts.  It is not called for most renderer-initiated navigations, and it
  // does not guarantee that the navigation will commit (e.g., 204s, downloads).
  //
  // DEPRECATED.  This method is difficult to use correctly and should be
  // removed.  TODO(creis): Remove in http://crbug.com/424641.
  virtual void AboutToNavigateRenderFrame(RenderFrameHost* old_host,
                                          RenderFrameHost* new_host) {}

  // This method is invoked after the browser process starts a navigation to a
  // pending NavigationEntry. It is not called for renderer-initiated
  // navigations unless they are sent to the browser process via OpenURL. It may
  // be called multiple times for a given navigation, such as a typed URL
  // followed by a cross-process client or server redirect.
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      NavigationController::ReloadType reload_type) {}

  // |render_frame_host| is the RenderFrameHost for which the provisional load
  // is happening.
  //
  // Since the URL validation will strip error URLs, or srcdoc URLs, the boolean
  // flags |is_error_page| and |is_iframe_srcdoc| will indicate that the not
  // validated URL was either an error page or an iframe srcdoc.
  //
  // Note that during a cross-process navigation, several provisional loads
  // can be on-going in parallel.
  virtual void DidStartProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) {}

  // This method is invoked when the provisional load was successfully
  // committed.
  //
  // If the navigation only changed the reference fragment, or was triggered
  // using the history API (e.g. window.history.replaceState), we will receive
  // this signal without a prior DidStartProvisionalLoadForFrame signal.
  virtual void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) {}

  // This method is invoked when the provisional load failed.
  virtual void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) {}

  // If the provisional load corresponded to the main frame, this method is
  // invoked in addition to DidCommitProvisionalLoadForFrame.
  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}

  // And regardless of what frame navigated, this method is invoked after
  // DidCommitProvisionalLoadForFrame was invoked.
  virtual void DidNavigateAnyFrame(
      RenderFrameHost* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) {}

  // This method is invoked when the SecurityStyle of the WebContents changes.
  // |security_style| is the new SecurityStyle. |security_style_explanations|
  // contains human-readable strings explaining why the SecurityStyle of the
  // page has been downgraded.
  virtual void SecurityStyleChanged(
      SecurityStyle security_style,
      const SecurityStyleExplanations& security_style_explanations) {}

  // This method is invoked once the window.document object of the main frame
  // was created.
  virtual void DocumentAvailableInMainFrame() {}

  // This method is invoked once the onload handler of the main frame has
  // completed.
  virtual void DocumentOnLoadCompletedInMainFrame() {}

  // This method is invoked when the document in the given frame finished
  // loading. At this point, scripts marked as defer were executed, and
  // content scripts marked "document_end" get injected into the frame.
  virtual void DocumentLoadedInFrame(RenderFrameHost* render_frame_host) {}

  // This method is invoked when the navigation is done, i.e. the spinner of
  // the tab will stop spinning, and the onload event was dispatched.
  //
  // If the WebContents is displaying replacement content, e.g. network error
  // pages, DidFinishLoad is invoked for frames that were not sending
  // navigational events before. It is safe to ignore these events.
  virtual void DidFinishLoad(RenderFrameHost* render_frame_host,
                             const GURL& validated_url) {}

  // This method is like DidFinishLoad, but when the load failed or was
  // cancelled, e.g. window.stop() is invoked.
  virtual void DidFailLoad(RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           int error_code,
                           const base::string16& error_description,
                           bool was_ignored_by_handler) {}

  // This method is invoked when content was loaded from an in-memory cache.
  virtual void DidLoadResourceFromMemoryCache(
      const LoadFromMemoryCacheDetails& details) {}

  // This method is invoked when a response has been received for a resource
  // request.
  virtual void DidGetResourceResponseStart(
      const ResourceRequestDetails& details) {}

  // This method is invoked when a redirect was received while requesting a
  // resource.
  virtual void DidGetRedirectForResourceRequest(
      RenderFrameHost* render_frame_host,
      const ResourceRedirectDetails& details) {}

  // This method is invoked when a new non-pending navigation entry is created.
  // This corresponds to one NavigationController entry being created
  // (in the case of new navigations) or renavigated to (for back/forward
  // navigations).
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) {}

  // This method is invoked when a new WebContents was created in response to
  // an action in the observed WebContents, e.g. a link with target=_blank was
  // clicked. The |source_render_frame_host| is the frame in which the action
  // took place.
  virtual void DidOpenRequestedURL(WebContents* new_contents,
                                   RenderFrameHost* source_render_frame_host,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   ui::PageTransition transition) {}

  // This method is invoked when the renderer process has completed its first
  // paint after a non-empty layout.
  virtual void DidFirstVisuallyNonEmptyPaint() {}

  // These two methods correspond to the points in time when the spinner of the
  // tab starts and stops spinning.
  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}

  // When WebContents::Stop() is called, the WebContents stops loading and then
  // invokes this method. If there are ongoing navigations, their respective
  // failure methods will also be invoked.
  virtual void NavigationStopped() {}

  // This indicates that the next navigation was triggered by a user gesture.
  virtual void DidGetUserGesture() {}

  // This method is invoked when a RenderViewHost of this WebContents was
  // configured to ignore UI events, and an UI event took place.
  virtual void DidGetIgnoredUIEvent() {}

  // These methods are invoked every time the WebContents changes visibility.
  virtual void WasShown() {}
  virtual void WasHidden() {}

  // Invoked when the main frame changes size.
  virtual void MainFrameWasResized(bool width_changed) {}

  // Invoked when the given frame changes its window.name property.
  virtual void FrameNameChanged(RenderFrameHost* render_frame_host,
                                const std::string& name) {}

  // This methods is invoked when the title of the WebContents is set. If the
  // title was explicitly set, |explicit_set| is true, otherwise the title was
  // synthesized and |explicit_set| is false.
  virtual void TitleWasSet(NavigationEntry* entry, bool explicit_set) {}

  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) {}

  // These methods are invoked when a Pepper plugin instance is created/deleted
  // in the DOM.
  virtual void PepperInstanceCreated() {}
  virtual void PepperInstanceDeleted() {}

  // Notification that a plugin has crashed.
  // |plugin_pid| is the process ID identifying the plugin process. Note that
  // this ID is supplied by the renderer process, so should not be trusted.
  // Besides, the corresponding process has probably died at this point. The ID
  // may even have been reused by a new process.
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) {}

  // Notification that the given plugin has hung or become unhung. This
  // notification is only for Pepper plugins.
  //
  // The plugin_child_id is the unique child process ID from the plugin. Note
  // that this ID is supplied by the renderer process, so should be validated
  // before it's used for anything in case there's an exploited renderer
  // process.
  virtual void PluginHungStatusChanged(int plugin_child_id,
                                       const base::FilePath& plugin_path,
                                       bool is_hung) {}

  // Invoked when WebContents::Clone() was used to clone a WebContents.
  virtual void DidCloneToNewWebContents(WebContents* old_web_contents,
                                        WebContents* new_web_contents) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. After the whole loop over all WebContentsObservers has been
  // finished, web_contents() returns nullptr.
  virtual void WebContentsDestroyed() {}

  // Called when the user agent override for a WebContents has been changed.
  virtual void UserAgentOverrideSet(const std::string& user_agent) {}

  // Invoked when new FaviconURL candidates are received from the renderer
  // process.
  virtual void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) {}

  // Invoked when a pepper plugin creates and shows or destroys a fullscreen
  // RenderWidget.
  virtual void DidShowFullscreenWidget(int routing_id) {}
  virtual void DidDestroyFullscreenWidget(int routing_id) {}

  // Invoked when the renderer process has toggled the tab into/out of
  // fullscreen mode.
  virtual void DidToggleFullscreenModeForTab(bool entered_fullscreen) {}

  // Invoked when an interstitial page is attached or detached.
  virtual void DidAttachInterstitialPage() {}
  virtual void DidDetachInterstitialPage() {}

  // Invoked before a form repost warning is shown.
  virtual void BeforeFormRepostWarningShow() {}

  // Invoked when the beforeunload handler fires. The time is from the renderer
  // process.
  virtual void BeforeUnloadFired(const base::TimeTicks& proceed_time) {}

  // Invoked when a user cancels a before unload dialog.
  virtual void BeforeUnloadDialogCancelled() {}

  // Invoked when an accessibility event is received from the renderer process.
  virtual void AccessibilityEventReceived(
      const std::vector<AXEventNotificationDetails>& details) {}

  // Invoked when theme color is changed to |theme_color|.
  virtual void DidChangeThemeColor(SkColor theme_color) {}

  // Invoked when media is playing.
  virtual void MediaStartedPlaying() {}

  // Invoked when media is paused.
  virtual void MediaPaused() {}

  // Invoked when media session has changed its state.
  virtual void MediaSessionStateChanged(bool is_controllable,
                                        bool is_suspended) {}

  // Invoked if an IPC message is coming from a specific RenderFrameHost.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 RenderFrameHost* render_frame_host);

  // Notification that |contents| has gained focus.
  virtual void OnWebContentsFocused() {}

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;
  int routing_id() const;

  WebContents* web_contents() const;

 protected:
  // Use this constructor when the object is tied to a single WebContents for
  // its entire lifetime.
  explicit WebContentsObserver(WebContents* web_contents);

  // Use this constructor when the object wants to observe a WebContents for
  // part of its lifetime.  It can then call Observe() to start and stop
  // observing.
  WebContentsObserver();

  ~WebContentsObserver() override;

  // Start observing a different WebContents; used with the default constructor.
  void Observe(WebContents* web_contents);

 private:
  friend class WebContentsImpl;

  void ResetWebContents();

  WebContentsImpl* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
