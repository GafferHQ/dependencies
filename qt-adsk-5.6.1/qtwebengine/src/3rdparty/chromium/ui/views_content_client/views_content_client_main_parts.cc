// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client_main_parts.h"

#include "base/run_loop.h"
#include "content/public/browser/context_factory.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/views/test/desktop_test_views_delegate.h"

namespace ui {

ViewsContentClientMainParts::ViewsContentClientMainParts(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : views_content_client_(views_content_client) {
}

ViewsContentClientMainParts::~ViewsContentClientMainParts() {
}

void ViewsContentClientMainParts::PreMainMessageLoopRun() {
  ui::InitializeInputMethodForTesting();
  browser_context_.reset(new content::ShellBrowserContext(false, NULL));

  scoped_ptr<views::TestViewsDelegate> test_views_delegate(
      new views::DesktopTestViewsDelegate);
  test_views_delegate->set_context_factory(content::GetContextFactory());
  views_delegate_ = test_views_delegate.Pass();
}

void ViewsContentClientMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
  views_delegate_.reset();
}

bool ViewsContentClientMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop run_loop;
  run_loop.Run();
  return true;
}

}  // namespace ui
