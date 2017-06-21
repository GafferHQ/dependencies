// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/context_provider_command_buffer.h"

#include <set>
#include <vector>

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "cc/output/managed_memory_policy.h"
#include "content/common/gpu/client/grcontext_for_webgraphicscontext3d.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace content {

class ContextProviderCommandBuffer::LostContextCallbackProxy
    : public blink::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  explicit LostContextCallbackProxy(ContextProviderCommandBuffer* provider)
      : provider_(provider) {
    provider_->context3d_->setContextLostCallback(this);
  }

  virtual ~LostContextCallbackProxy() {
    provider_->context3d_->setContextLostCallback(NULL);
  }

  virtual void onContextLost() {
    provider_->OnLostContext();
  }

 private:
  ContextProviderCommandBuffer* provider_;
};

scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::Create(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    CommandBufferContextType type) {
  if (!context3d)
    return NULL;

  return new ContextProviderCommandBuffer(context3d.Pass(), type);
}

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
    CommandBufferContextType type)
    : context3d_(context3d.Pass()),
      context_type_(type),
      debug_name_(CommandBufferContextTypeToString(type)),
      destroyed_(false) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(context3d_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);

  // Destroy references to the context3d_ before leaking it.
  if (context3d_->GetCommandBufferProxy()) {
    context3d_->GetCommandBufferProxy()->SetLock(nullptr);
    context3d_->GetCommandBufferProxy()->SetMemoryAllocationChangedCallback(
        CommandBufferProxyImpl::MemoryAllocationChangedCallback());
  }
  lost_context_callback_proxy_.reset();
}


CommandBufferProxyImpl* ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return context3d_->GetCommandBufferProxy();
}

WebGraphicsContext3DCommandBufferImpl*
ContextProviderCommandBuffer::WebContext3D() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_.get();
}

bool ContextProviderCommandBuffer::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (lost_context_callback_proxy_)
    return true;

  context3d_->SetContextType(context_type_);
  if (!context3d_->InitializeOnCurrentThread())
    return false;

  InitializeCapabilities();

  std::string unique_context_name =
      base::StringPrintf("%s-%p", debug_name_.c_str(), context3d_.get());
  context3d_->traceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());

  lost_context_callback_proxy_.reset(new LostContextCallbackProxy(this));
  context3d_->GetCommandBufferProxy()->SetMemoryAllocationChangedCallback(
      base::Bind(&ContextProviderCommandBuffer::OnMemoryAllocationChanged,
                 base::Unretained(this)));
  return true;
}

void ContextProviderCommandBuffer::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(context3d_);
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return context3d_->GetImplementation();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return context3d_->GetContextSupport();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new GrContextForWebGraphicsContext3D(context3d_.get()));

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

void ContextProviderCommandBuffer::InvalidateGrContext(uint32_t state) {
  if (gr_context_) {
    DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
    DCHECK(context_thread_checker_.CalledOnValidThread());
    gr_context_->get()->resetContext(state);
  }
}

void ContextProviderCommandBuffer::SetupLock() {
  DCHECK(context3d_);
  context3d_->GetCommandBufferProxy()->SetLock(&context_lock_);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  return &context_lock_;
}

cc::ContextProvider::Capabilities
ContextProviderCommandBuffer::ContextCapabilities() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  return capabilities_;
}

void ContextProviderCommandBuffer::VerifyContexts() {
  DCHECK(lost_context_callback_proxy_);  // Is bound to thread.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    OnLostContext();
}

void ContextProviderCommandBuffer::DeleteCachedResources() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(main_thread_lock_);
    if (destroyed_)
      return;
    destroyed_ = true;
  }
  if (!lost_context_callback_.is_null())
    base::ResetAndReturn(&lost_context_callback_).Run();
  if (gr_context_)
    gr_context_->OnLostContext();
}

void ContextProviderCommandBuffer::OnMemoryAllocationChanged(
    const gpu::MemoryAllocation& allocation) {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (memory_policy_changed_callback_.is_null())
    return;

  memory_policy_changed_callback_.Run(cc::ManagedMemoryPolicy(allocation));
}

void ContextProviderCommandBuffer::InitializeCapabilities() {
  Capabilities caps;
  caps.gpu = context3d_->GetImplementation()->capabilities();

  size_t mapped_memory_limit = context3d_->GetMappedMemoryLimit();
  caps.max_transfer_buffer_usage_bytes =
      mapped_memory_limit == WebGraphicsContext3DCommandBufferImpl::kNoLimit
      ? std::numeric_limits<size_t>::max() : mapped_memory_limit;

  capabilities_ = caps;
}

bool ContextProviderCommandBuffer::DestroyedOnMainThread() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  base::AutoLock lock(main_thread_lock_);
  return destroyed_;
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() ||
         lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

void ContextProviderCommandBuffer::SetMemoryPolicyChangedCallback(
    const MemoryPolicyChangedCallback& memory_policy_changed_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(memory_policy_changed_callback_.is_null() ||
         memory_policy_changed_callback.is_null());
  memory_policy_changed_callback_ = memory_policy_changed_callback;
}

}  // namespace content
