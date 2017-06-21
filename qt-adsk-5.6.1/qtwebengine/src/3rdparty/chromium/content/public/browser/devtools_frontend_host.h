// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_H_

#include <string>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// This class dispatches messages between DevTools frontend and Delegate
// which is implemented by the embedder.
// This allows us to avoid exposing DevTools frontend messages through
// the content public API.
class DevToolsFrontendHost {
 public:
  // Delegate actually handles messages from frontend.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Message is coming from frontend to the embedder.
    virtual void HandleMessageFromDevToolsFrontend(
        const std::string& message) = 0;

    // Message is coming from frontend to the backend.
    // TODO(dgozman): remove this by making one of the possible messages
    // passed via the method above.
    virtual void HandleMessageFromDevToolsFrontendToBackend(
        const std::string& message) = 0;
  };

  // Creates a new DevToolsFrontendHost for RenderFrameHost where DevTools
  // frontend is loaded.
  CONTENT_EXPORT static DevToolsFrontendHost* Create(
      RenderFrameHost* frontend_main_frame,
      Delegate* delegate);

  CONTENT_EXPORT virtual ~DevToolsFrontendHost() {}

  CONTENT_EXPORT virtual void BadMessageRecieved() {}

  // Returns bundled DevTools frontend resource by |path|. Returns empty string
  // if |path| does not correspond to any frontend resource.
  CONTENT_EXPORT static base::StringPiece GetFrontendResource(
      const std::string& path);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_FRONTEND_HOST_H_
