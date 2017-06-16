// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_DATA_CONSUMER_HANDLE_IMPL_H_
#define CONTENT_CHILD_WEB_DATA_CONSUMER_HANDLE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "mojo/common/handle_watcher.h"
#include "third_party/WebKit/public/platform/WebDataConsumerHandle.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"

namespace content {

class CONTENT_EXPORT WebDataConsumerHandleImpl final
    : public NON_EXPORTED_BASE(blink::WebDataConsumerHandle) {
  typedef mojo::ScopedDataPipeConsumerHandle Handle;
  class Context;

 public:
  class CONTENT_EXPORT ReaderImpl final : public NON_EXPORTED_BASE(Reader) {
   public:
    ReaderImpl(scoped_refptr<Context> context, Client* client);
    virtual ~ReaderImpl();
    virtual Result read(void* data, size_t size, Flags flags, size_t* readSize);
    virtual Result beginRead(const void** buffer,
                             Flags flags,
                             size_t* available);
    virtual Result endRead(size_t readSize);

   private:
    Result HandleReadResult(MojoResult);
    void StartWatching();
    void OnHandleGotReadable(MojoResult);

    scoped_refptr<Context> context_;
    mojo::common::HandleWatcher handle_watcher_;
    Client* client_;
  };
  scoped_ptr<Reader> ObtainReader(Client* client);

  explicit WebDataConsumerHandleImpl(Handle handle);
  virtual ~WebDataConsumerHandleImpl();

 private:
  virtual ReaderImpl* obtainReaderInternal(Client* client);
  const char* debugName() const override;

  scoped_refptr<Context> context_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_DATA_CONSUMER_HANDLE_IMPL_H_
