// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_io_thread.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbDeviceHandleTest : public ::testing::Test {
 public:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoopForUI);
    io_thread_.reset(new base::TestIOThread(base::TestIOThread::kAutoStart));
  }

 protected:
  scoped_ptr<base::TestIOThread> io_thread_;

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
};

class TestOpenCallback {
 public:
  TestOpenCallback()
      : callback_(
            base::Bind(&TestOpenCallback::SetResult, base::Unretained(this))) {}

  scoped_refptr<UsbDeviceHandle> WaitForResult() {
    run_loop_.Run();
    return device_handle_;
  }

  const UsbDevice::OpenCallback& callback() const { return callback_; }

 private:
  void SetResult(scoped_refptr<UsbDeviceHandle> device_handle) {
    device_handle_ = device_handle;
    run_loop_.Quit();
  }

  const UsbDevice::OpenCallback callback_;
  base::RunLoop run_loop_;
  scoped_refptr<UsbDeviceHandle> device_handle_;
};

class TestResultCallback {
 public:
  TestResultCallback()
      : callback_(base::Bind(&TestResultCallback::SetResult,
                             base::Unretained(this))) {}

  bool WaitForResult() {
    run_loop_.Run();
    return success_;
  }

  const UsbDeviceHandle::ResultCallback& callback() const { return callback_; }

 private:
  void SetResult(bool success) {
    success_ = success;
    run_loop_.Quit();
  }

  const UsbDeviceHandle::ResultCallback callback_;
  base::RunLoop run_loop_;
  bool success_;
};

class TestCompletionCallback {
 public:
  TestCompletionCallback()
      : callback_(base::Bind(&TestCompletionCallback::SetResult,
                             base::Unretained(this))) {}

  void WaitForResult() { run_loop_.Run(); }

  const UsbDeviceHandle::TransferCallback& callback() const {
    return callback_;
  }
  UsbTransferStatus status() const { return status_; }
  size_t transferred() const { return transferred_; }

 private:
  void SetResult(UsbTransferStatus status,
                 scoped_refptr<net::IOBuffer> buffer,
                 size_t transferred) {
    status_ = status;
    transferred_ = transferred;
    run_loop_.Quit();
  }

  const UsbDeviceHandle::TransferCallback callback_;
  base::RunLoop run_loop_;
  UsbTransferStatus status_;
  size_t transferred_;
};

TEST_F(UsbDeviceHandleTest, InterruptTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  scoped_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_->task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.callback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(0, claim_interface.callback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  scoped_refptr<net::IOBufferWithSize> in_buffer(new net::IOBufferWithSize(64));
  TestCompletionCallback in_completion;
  handle->InterruptTransfer(USB_DIRECTION_INBOUND, 0x81, in_buffer.get(),
                            in_buffer->size(),
                            5000,  // 5 second timeout
                            in_completion.callback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->InterruptTransfer(USB_DIRECTION_OUTBOUND, 0x01, out_buffer.get(),
                            out_buffer->size(),
                            5000,  // 5 second timeout
                            out_completion.callback());
  out_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->data()[i], in_buffer->data()[i])
        << "Mismatch at index " << i << ".";
  }

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, BulkTransfer) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  scoped_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_->task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.callback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(1, claim_interface.callback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  scoped_refptr<net::IOBufferWithSize> in_buffer(
      new net::IOBufferWithSize(512));
  TestCompletionCallback in_completion;
  handle->BulkTransfer(USB_DIRECTION_INBOUND, 0x82, in_buffer.get(),
                       in_buffer->size(),
                       5000,  // 5 second timeout
                       in_completion.callback());

  scoped_refptr<net::IOBufferWithSize> out_buffer(
      new net::IOBufferWithSize(in_buffer->size()));
  TestCompletionCallback out_completion;
  for (int i = 0; i < out_buffer->size(); ++i) {
    out_buffer->data()[i] = i;
  }

  handle->BulkTransfer(USB_DIRECTION_OUTBOUND, 0x02, out_buffer.get(),
                       out_buffer->size(),
                       5000,  // 5 second timeout
                       out_completion.callback());
  out_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, out_completion.status());
  EXPECT_EQ(static_cast<size_t>(out_buffer->size()),
            out_completion.transferred());

  in_completion.WaitForResult();
  ASSERT_EQ(USB_TRANSFER_COMPLETED, in_completion.status());
  EXPECT_EQ(static_cast<size_t>(in_buffer->size()),
            in_completion.transferred());
  for (size_t i = 0; i < in_completion.transferred(); ++i) {
    EXPECT_EQ(out_buffer->data()[i], in_buffer->data()[i])
        << "Mismatch at index " << i << ".";
  }

  handle->Close();
}

TEST_F(UsbDeviceHandleTest, SetInterfaceAlternateSetting) {
  if (!UsbTestGadget::IsTestEnabled()) {
    return;
  }

  scoped_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(io_thread_->task_runner());
  ASSERT_TRUE(gadget.get());
  ASSERT_TRUE(gadget->SetType(UsbTestGadget::ECHO));

  TestOpenCallback open_device;
  gadget->GetDevice()->Open(open_device.callback());
  scoped_refptr<UsbDeviceHandle> handle = open_device.WaitForResult();
  ASSERT_TRUE(handle.get());

  TestResultCallback claim_interface;
  handle->ClaimInterface(2, claim_interface.callback());
  ASSERT_TRUE(claim_interface.WaitForResult());

  TestResultCallback set_interface;
  handle->SetInterfaceAlternateSetting(2, 1, set_interface.callback());
  ASSERT_TRUE(set_interface.WaitForResult());

  handle->Close();
}

}  // namespace

}  // namespace device
