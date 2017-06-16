// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/content_handler_factory.h"

#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/common/message_pump_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {

namespace {

class ApplicationThread : public base::PlatformThread::Delegate {
 public:
  ApplicationThread(
      scoped_refptr<base::SingleThreadTaskRunner> handler_thread,
      const base::Callback<void(ApplicationThread*)>& termination_callback,
      ContentHandlerFactory::Delegate* handler_delegate,
      InterfaceRequest<Application> application_request,
      URLResponsePtr response)
      : handler_thread_(handler_thread),
        termination_callback_(termination_callback),
        handler_delegate_(handler_delegate),
        application_request_(application_request.Pass()),
        response_(response.Pass()) {}

 private:
  void ThreadMain() override {
    handler_delegate_->RunApplication(application_request_.Pass(),
                                      response_.Pass());
    handler_thread_->PostTask(FROM_HERE,
                              base::Bind(termination_callback_, this));
  }

  scoped_refptr<base::SingleThreadTaskRunner> handler_thread_;
  base::Callback<void(ApplicationThread*)> termination_callback_;
  ContentHandlerFactory::Delegate* handler_delegate_;
  InterfaceRequest<Application> application_request_;
  URLResponsePtr response_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationThread);
};

class ContentHandlerImpl : public ContentHandler {
 public:
  ContentHandlerImpl(ContentHandlerFactory::Delegate* delegate,
                     InterfaceRequest<ContentHandler> request)
      : delegate_(delegate),
        binding_(this, request.Pass()),
        weak_factory_(this) {}
  ~ContentHandlerImpl() override {
    // We're shutting down and doing cleanup. Cleanup may trigger calls back to
    // OnThreadEnd(). As we're doing the cleanup here we don't want to do it in
    // OnThreadEnd() as well. InvalidateWeakPtrs() ensures we don't get any
    // calls to OnThreadEnd().
    weak_factory_.InvalidateWeakPtrs();
    for (auto thread : active_threads_) {
      base::PlatformThread::Join(thread.second);
      delete thread.first;
    }
  }

 private:
  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<Application> application_request,
                        URLResponsePtr response) override {
    ApplicationThread* thread = new ApplicationThread(
        base::ThreadTaskRunnerHandle::Get(),
        base::Bind(&ContentHandlerImpl::OnThreadEnd,
                   weak_factory_.GetWeakPtr()),
        delegate_, application_request.Pass(), response.Pass());
    base::PlatformThreadHandle handle;
    bool launched = base::PlatformThread::Create(0, thread, &handle);
    DCHECK(launched);
    active_threads_[thread] = handle;
  }

  void OnThreadEnd(ApplicationThread* thread) {
    DCHECK(active_threads_.find(thread) != active_threads_.end());
    base::PlatformThreadHandle handle = active_threads_[thread];
    active_threads_.erase(thread);
    base::PlatformThread::Join(handle);
    delete thread;
  }

  ContentHandlerFactory::Delegate* delegate_;
  std::map<ApplicationThread*, base::PlatformThreadHandle> active_threads_;
  StrongBinding<ContentHandler> binding_;
  base::WeakPtrFactory<ContentHandlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

}  // namespace

ContentHandlerFactory::ContentHandlerFactory(Delegate* delegate)
    : delegate_(delegate) {
}

ContentHandlerFactory::~ContentHandlerFactory() {
}

void ContentHandlerFactory::ManagedDelegate::RunApplication(
    InterfaceRequest<Application> application_request,
    URLResponsePtr response) {
  base::MessageLoop loop(common::MessagePumpMojo::Create());
  auto application =
      this->CreateApplication(application_request.Pass(), response.Pass());
  if (application)
    loop.Run();
}

void ContentHandlerFactory::Create(ApplicationConnection* connection,
                                   InterfaceRequest<ContentHandler> request) {
  new ContentHandlerImpl(delegate_, request.Pass());
}

}  // namespace mojo
