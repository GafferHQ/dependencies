// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_POPUP_ALIGNMENT_DELEGATE_H_
#define UI_MESSAGE_CENTER_VIEWS_POPUP_ALIGNMENT_DELEGATE_H_

#include "ui/message_center/message_center_export.h"

namespace gfx {
class Display;
class Point;
class Rect;
}

namespace message_center {

class MessagePopupCollection;

class MESSAGE_CENTER_EXPORT PopupAlignmentDelegate {
 public:
  PopupAlignmentDelegate();

  void set_collection(MessagePopupCollection* collection) {
    collection_ = collection;
  }

  // Returns the x-origin for the given toast bounds in the current work area.
  virtual int GetToastOriginX(const gfx::Rect& toast_bounds) const = 0;

  // Returns the baseline height of the current work area. That is the starting
  // point if there are no other toasts.
  virtual int GetBaseLine() const = 0;

  // Returns the height of the bottom of the current work area.
  virtual int GetWorkAreaBottom() const  = 0;

  // Returns true if the toast should be aligned top down.
  virtual bool IsTopDown() const = 0;

  // Returns true if the toasts are positioned at the left side of the desktop
  // so that their reveal animation should happen from left side.
  virtual bool IsFromLeft() const = 0;

  // Called when a new toast appears or toasts are rearranged in the |display|.
  // The subclass may override this method to check the current desktop status
  // so that the toasts are arranged at the correct place.
  virtual void RecomputeAlignment(const gfx::Display& display) = 0;

 protected:
  virtual ~PopupAlignmentDelegate();

  void DoUpdateIfPossible();

 private:
  MessagePopupCollection* collection_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_POPUP_ALIGNMENT_DELEGATE_H_
