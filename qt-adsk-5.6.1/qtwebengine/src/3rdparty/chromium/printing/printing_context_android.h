// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_ANDROID_H_
#define PRINTING_PRINTING_CONTEXT_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "printing/printing_context.h"

namespace printing {

// Android subclass of PrintingContext. This class communicates with the
// Java side through JNI.
class PRINTING_EXPORT PrintingContextAndroid : public PrintingContext {
 public:
  explicit PrintingContextAndroid(Delegate* delegate);
  ~PrintingContextAndroid() override;

  // Called when the page is successfully written to a PDF using the file
  // descriptor specified, or when the printing operation failed.
  static void PdfWritingDone(int fd, bool success);

  // Called from Java, when printing settings from the user are ready or the
  // printing operation is canceled.
  void AskUserForSettingsReply(JNIEnv* env, jobject obj, jboolean success);

  // Called from Java, when a printing process initiated by a script finishes.
  void ShowSystemDialogDone(JNIEnv* env, jobject obj);

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          const PrintSettingsCallback& callback) override;
  Result UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result InitWithSettings(const PrintSettings& settings) override;
  Result NewDocument(const base::string16& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  gfx::NativeDrawingContext context() const override;

  // Registers JNI bindings for RegisterContext.
  static bool RegisterPrintingContext(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_printing_context_;

  // The callback from AskUserForSettings to be called when the settings are
  // ready on the Java side
  PrintSettingsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextAndroid);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_ANDROID_H_

