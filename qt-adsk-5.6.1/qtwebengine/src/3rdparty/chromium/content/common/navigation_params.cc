// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/common/content_switches.h"

namespace content {

// PlzNavigate
bool ShouldMakeNetworkRequestForURL(const GURL& url) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));

  // Data URLs, Javascript URLs and about:blank should not send a request to the
  // network stack.
  // TODO(clamy): same document navigations should not send requests to the
  // network stack. Neither should pushState/popState.
  return !url.SchemeIs(url::kDataScheme) && url != GURL(url::kAboutBlankURL) &&
         !url.SchemeIs(url::kJavaScriptScheme);
}

CommonNavigationParams::CommonNavigationParams()
    : transition(ui::PAGE_TRANSITION_LINK),
      navigation_type(FrameMsg_Navigate_Type::NORMAL),
      allow_download(true),
      report_type(FrameMsg_UILoadMetricsReportType::NO_REPORT) {
}

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool allow_download,
    base::TimeTicks ui_timestamp,
    FrameMsg_UILoadMetricsReportType::Value report_type,
    const GURL& base_url_for_data_url,
    const GURL& history_url_for_data_url)
    : url(url),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      allow_download(allow_download),
      ui_timestamp(ui_timestamp),
      report_type(report_type),
      base_url_for_data_url(base_url_for_data_url),
      history_url_for_data_url(history_url_for_data_url) {
}

CommonNavigationParams::~CommonNavigationParams() {
}

BeginNavigationParams::BeginNavigationParams()
    : load_flags(0), has_user_gesture(false) {
}

BeginNavigationParams::BeginNavigationParams(std::string method,
                                             std::string headers,
                                             int load_flags,
                                             bool has_user_gesture)
    : method(method),
      headers(headers),
      load_flags(load_flags),
      has_user_gesture(has_user_gesture) {
}

StartNavigationParams::StartNavigationParams()
    : is_post(false),
      should_replace_current_entry(false),
      transferred_request_child_id(-1),
      transferred_request_request_id(-1) {
}

StartNavigationParams::StartNavigationParams(
    bool is_post,
    const std::string& extra_headers,
    const std::vector<unsigned char>& browser_initiated_post_data,
    bool should_replace_current_entry,
    int transferred_request_child_id,
    int transferred_request_request_id)
    : is_post(is_post),
      extra_headers(extra_headers),
      browser_initiated_post_data(browser_initiated_post_data),
      should_replace_current_entry(should_replace_current_entry),
      transferred_request_child_id(transferred_request_child_id),
      transferred_request_request_id(transferred_request_request_id) {
}

StartNavigationParams::~StartNavigationParams() {
}

RequestNavigationParams::RequestNavigationParams()
    : is_overriding_user_agent(false),
      browser_navigation_start(base::TimeTicks::Now()),
      can_load_local_resources(false),
      request_time(base::Time::Now()),
      page_id(-1),
      nav_entry_id(0),
      is_same_document_history_load(false),
      has_committed_real_load(false),
      intended_as_new_entry(false),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(0),
      should_clear_history_list(false) {
}

RequestNavigationParams::RequestNavigationParams(
    bool is_overriding_user_agent,
    base::TimeTicks navigation_start,
    const std::vector<GURL>& redirects,
    bool can_load_local_resources,
    base::Time request_time,
    const PageState& page_state,
    int32 page_id,
    int nav_entry_id,
    bool is_same_document_history_load,
    bool has_committed_real_load,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    bool should_clear_history_list)
    : is_overriding_user_agent(is_overriding_user_agent),
      browser_navigation_start(navigation_start),
      redirects(redirects),
      can_load_local_resources(can_load_local_resources),
      request_time(request_time),
      page_state(page_state),
      page_id(page_id),
      nav_entry_id(nav_entry_id),
      is_same_document_history_load(is_same_document_history_load),
      has_committed_real_load(has_committed_real_load),
      intended_as_new_entry(intended_as_new_entry),
      pending_history_list_offset(pending_history_list_offset),
      current_history_list_offset(current_history_list_offset),
      current_history_list_length(current_history_list_length),
      should_clear_history_list(should_clear_history_list) {
}

RequestNavigationParams::~RequestNavigationParams() {
}

NavigationParams::NavigationParams(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params)
    : common_params(common_params),
      start_params(start_params),
      request_params(request_params) {
}

NavigationParams::~NavigationParams() {
}

}  // namespace content
