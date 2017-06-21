// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/threaded_ssl_private_key.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"

namespace net {

namespace {

void DoCallback(const base::WeakPtr<ThreadedSSLPrivateKey>& key,
                const ThreadedSSLPrivateKey::SignCallback& callback,
                std::vector<uint8_t>* signature,
                Error error) {
  if (!key)
    return;
  callback.Run(error, *signature);
}
}

class ThreadedSSLPrivateKey::Core
    : public base::RefCountedThreadSafe<ThreadedSSLPrivateKey::Core> {
 public:
  Core(scoped_ptr<ThreadedSSLPrivateKey::Delegate> delegate)
      : delegate_(delegate.Pass()) {}

  ThreadedSSLPrivateKey::Delegate* delegate() { return delegate_.get(); }

  Error SignDigest(SSLPrivateKey::Hash hash,
                   const base::StringPiece& input,
                   std::vector<uint8_t>* signature) {
    return delegate_->SignDigest(hash, input, signature);
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() {}

  scoped_ptr<ThreadedSSLPrivateKey::Delegate> delegate_;
};

ThreadedSSLPrivateKey::ThreadedSSLPrivateKey(
    scoped_ptr<ThreadedSSLPrivateKey::Delegate> delegate,
    scoped_refptr<base::TaskRunner> task_runner)
    : core_(new Core(delegate.Pass())),
      task_runner_(task_runner.Pass()),
      weak_factory_(this) {
}

ThreadedSSLPrivateKey::~ThreadedSSLPrivateKey() {
}

SSLPrivateKey::Type ThreadedSSLPrivateKey::GetType() {
  return core_->delegate()->GetType();
}

bool ThreadedSSLPrivateKey::SupportsHash(SSLPrivateKey::Hash hash) {
  return core_->delegate()->SupportsHash(hash);
}

size_t ThreadedSSLPrivateKey::GetMaxSignatureLengthInBytes() {
  return core_->delegate()->GetMaxSignatureLengthInBytes();
}

void ThreadedSSLPrivateKey::SignDigest(
    SSLPrivateKey::Hash hash,
    const base::StringPiece& input,
    const SSLPrivateKey::SignCallback& callback) {
  std::vector<uint8_t>* signature = new std::vector<uint8_t>;
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::Bind(&ThreadedSSLPrivateKey::Core::SignDigest, core_, hash,
                 input.as_string(), base::Unretained(signature)),
      base::Bind(&DoCallback, weak_factory_.GetWeakPtr(), callback,
                 base::Owned(signature)));
}

}  // namespace net
