// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

enum PresentationSessionState {
  PRESENTATION_SESSION_STATE_CONNECTED,
  PRESENTATION_SESSION_STATE_DISCONNECTED
};

// Represents a presentation session that has been established via either
// browser actions or Presentation API.
struct CONTENT_EXPORT PresentationSessionInfo {
  PresentationSessionInfo(const std::string& presentation_url,
                          const std::string& presentation_id);
  ~PresentationSessionInfo();

  const std::string presentation_url;
  const std::string presentation_id;
};

// Possible reasons why an attempt to create a presentation session failed.
enum PresentationErrorType {
  PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
  PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
  PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
  PRESENTATION_ERROR_UNKNOWN,
};

// Struct returned when an attempt to create a presentation session failed.
struct CONTENT_EXPORT PresentationError {
  PresentationError(PresentationErrorType error_type,
                    const std::string& message);
  ~PresentationError();

  const PresentationErrorType error_type;
  const std::string message;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_H_
