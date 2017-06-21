// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_CONTEXT_H_
#define DEVICE_USB_USB_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"

struct libusb_context;

namespace device {

typedef libusb_context* PlatformUsbContext;

// Ref-counted wrapper for libusb_context*.
// It also manages the life-cycle of UsbEventHandler.
// It is a blocking operation to delete UsbContext.
// Destructor must be called on FILE thread.
class UsbContext : public base::RefCountedThreadSafe<UsbContext> {
 public:
  PlatformUsbContext context() const { return context_; }

 protected:
  friend class UsbServiceImpl;
  friend class base::RefCountedThreadSafe<UsbContext>;

  explicit UsbContext(PlatformUsbContext context);
  virtual ~UsbContext();

 private:
  class UsbEventHandler;

  PlatformUsbContext context_;
  scoped_ptr<UsbEventHandler> event_handler_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UsbContext);
};

}  // namespace device

#endif  // DEVICE_USB_USB_CONTEXT_H_
