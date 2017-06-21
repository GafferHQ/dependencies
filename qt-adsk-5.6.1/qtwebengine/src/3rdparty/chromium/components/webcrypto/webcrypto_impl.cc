// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/webcrypto_impl.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace webcrypto {

using webcrypto::Status;

namespace {

// ---------------------
// Threading
// ---------------------
//
// WebCrypto operations can be slow. For instance generating an RSA key can
// seconds.
//
// Moreover the underlying crypto libraries are not threadsafe when operating
// on the same key.
//
// The strategy used here is to run a sequenced worker pool for all WebCrypto
// operations (except structured cloning). This same pool is also used by
// requests started from Blink Web Workers.
//
// A few notes to keep in mind:
//
// * PostTaskAndReply() cannot be used for two reasons:
//
//   (1) Blink web worker threads do not have an associated message loop so
//       construction of the reply callback will crash.
//
//   (2) PostTaskAndReply() handles failure posting the reply by leaking the
//       callback, rather than destroying it. In the case of Web Workers this
//       condition is reachable via normal execution, since Web Workers can
//       be stopped before the WebCrypto operation has finished. A policy of
//       leaking would therefore be problematic.
//
// * blink::WebArrayBuffer is NOT threadsafe, and should therefore be allocated
//   on the target Blink thread.
//
//   TODO(eroman): Is there any way around this? Copying the result between
//                 threads is silly.
//
// * WebCryptoAlgorithm and WebCryptoKey are threadsafe (however the key's
//   handle(), which wraps an NSS/OpenSSL type, may not be and should only be
//   used from the webcrypto thread).
//
// * blink::WebCryptoResult is not threadsafe and should only be operated on
//   the target Blink thread. HOWEVER, it is safe to delete it from any thread.
//   This can happen if by the time the operation has completed in the crypto
//   worker pool, the Blink worker thread that initiated the request is gone.
//   Posting back to the origin thread will fail, and the WebCryptoResult will
//   be deleted while running in the crypto worker pool.
class CryptoThreadPool {
 public:
  CryptoThreadPool()
      : worker_pool_(new base::SequencedWorkerPool(1, "WebCrypto")),
        task_runner_(worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
            worker_pool_->GetSequenceToken(),
            base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)) {}

  static bool PostTask(const tracked_objects::Location& from_here,
                       const base::Closure& task);

 private:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

base::LazyInstance<CryptoThreadPool>::Leaky crypto_thread_pool =
    LAZY_INSTANCE_INITIALIZER;

bool CryptoThreadPool::PostTask(const tracked_objects::Location& from_here,
                                const base::Closure& task) {
  return crypto_thread_pool.Get().task_runner_->PostTask(from_here, task);
}

void CompleteWithThreadPoolError(blink::WebCryptoResult* result) {
  result->completeWithError(blink::WebCryptoErrorTypeOperation,
                            "Failed posting to crypto worker pool");
}

void CompleteWithError(const Status& status, blink::WebCryptoResult* result) {
  DCHECK(status.IsError());

  result->completeWithError(status.error_type(),
                            blink::WebString::fromUTF8(status.error_details()));
}

void CompleteWithBufferOrError(const Status& status,
                               const std::vector<uint8_t>& buffer,
                               blink::WebCryptoResult* result) {
  if (status.IsError()) {
    CompleteWithError(status, result);
  } else {
    if (buffer.size() > UINT_MAX) {
      // WebArrayBuffers have a smaller range than std::vector<>, so
      // theoretically this could overflow.
      CompleteWithError(Status::ErrorUnexpected(), result);
    } else {
      result->completeWithBuffer(vector_as_array(&buffer),
                                 static_cast<unsigned int>(buffer.size()));
    }
  }
}

void CompleteWithKeyOrError(const Status& status,
                            const blink::WebCryptoKey& key,
                            blink::WebCryptoResult* result) {
  if (status.IsError()) {
    CompleteWithError(status, result);
  } else {
    result->completeWithKey(key);
  }
}

// Gets a task runner for the current thread.
scoped_refptr<base::TaskRunner> GetCurrentBlinkThread() {
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  return base::ThreadTaskRunnerHandle::Get();
}

// --------------------------------------------------------------------
// State
// --------------------------------------------------------------------
//
// Explicit state classes are used rather than base::Bind(). This is done
// both for clarity, but also to avoid extraneous allocations for things
// like passing buffers and result objects between threads.
//
// BaseState is the base class common to all of the async operations, and
// keeps track of the thread to complete on, the error state, and the
// callback into Blink.
//
// Ownership of the State object is passed between the crypto thread and the
// Blink thread. Under normal completion it is destroyed on the Blink thread.
// However it may also be destroyed on the crypto thread if the Blink thread
// has vanished (which can happen for Blink web worker threads).

struct BaseState {
  explicit BaseState(const blink::WebCryptoResult& result)
      : origin_thread(GetCurrentBlinkThread()), result(result) {}

  bool cancelled() { return result.cancelled(); }

  scoped_refptr<base::TaskRunner> origin_thread;

  webcrypto::Status status;
  blink::WebCryptoResult result;

 protected:
  // Since there is no virtual destructor, must not delete directly as a
  // BaseState.
  ~BaseState() {}
};

struct EncryptState : public BaseState {
  EncryptState(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const unsigned char* data,
               unsigned int data_size,
               const blink::WebCryptoResult& result)
      : BaseState(result),
        algorithm(algorithm),
        key(key),
        data(data, data + data_size) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey key;
  const std::vector<uint8_t> data;

  std::vector<uint8_t> buffer;
};

typedef EncryptState DecryptState;
typedef EncryptState DigestState;

struct GenerateKeyState : public BaseState {
  GenerateKeyState(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   const blink::WebCryptoResult& result)
      : BaseState(result),
        algorithm(algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoAlgorithm algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  webcrypto::GenerateKeyResult generate_key_result;
};

struct ImportKeyState : public BaseState {
  ImportKeyState(blink::WebCryptoKeyFormat format,
                 const unsigned char* key_data,
                 unsigned int key_data_size,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result)
      : BaseState(result),
        format(format),
        key_data(key_data, key_data + key_data_size),
        algorithm(algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoKeyFormat format;
  const std::vector<uint8_t> key_data;
  const blink::WebCryptoAlgorithm algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey key;
};

struct ExportKeyState : public BaseState {
  ExportKeyState(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 const blink::WebCryptoResult& result)
      : BaseState(result), format(format), key(key) {}

  const blink::WebCryptoKeyFormat format;
  const blink::WebCryptoKey key;

  std::vector<uint8_t> buffer;
};

typedef EncryptState SignState;

struct VerifySignatureState : public BaseState {
  VerifySignatureState(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const unsigned char* signature,
                       unsigned int signature_size,
                       const unsigned char* data,
                       unsigned int data_size,
                       const blink::WebCryptoResult& result)
      : BaseState(result),
        algorithm(algorithm),
        key(key),
        signature(signature, signature + signature_size),
        data(data, data + data_size),
        verify_result(false) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey key;
  const std::vector<uint8_t> signature;
  const std::vector<uint8_t> data;

  bool verify_result;
};

struct WrapKeyState : public BaseState {
  WrapKeyState(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrap_algorithm,
               const blink::WebCryptoResult& result)
      : BaseState(result),
        format(format),
        key(key),
        wrapping_key(wrapping_key),
        wrap_algorithm(wrap_algorithm) {}

  const blink::WebCryptoKeyFormat format;
  const blink::WebCryptoKey key;
  const blink::WebCryptoKey wrapping_key;
  const blink::WebCryptoAlgorithm wrap_algorithm;

  std::vector<uint8_t> buffer;
};

struct UnwrapKeyState : public BaseState {
  UnwrapKeyState(blink::WebCryptoKeyFormat format,
                 const unsigned char* wrapped_key,
                 unsigned wrapped_key_size,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& unwrap_algorithm,
                 const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result)
      : BaseState(result),
        format(format),
        wrapped_key(wrapped_key, wrapped_key + wrapped_key_size),
        wrapping_key(wrapping_key),
        unwrap_algorithm(unwrap_algorithm),
        unwrapped_key_algorithm(unwrapped_key_algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoKeyFormat format;
  const std::vector<uint8_t> wrapped_key;
  const blink::WebCryptoKey wrapping_key;
  const blink::WebCryptoAlgorithm unwrap_algorithm;
  const blink::WebCryptoAlgorithm unwrapped_key_algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey unwrapped_key;
};

struct DeriveBitsState : public BaseState {
  DeriveBitsState(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  unsigned int length_bits,
                  const blink::WebCryptoResult& result)
      : BaseState(result),
        algorithm(algorithm),
        base_key(base_key),
        length_bits(length_bits) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey base_key;
  const unsigned int length_bits;

  std::vector<uint8_t> derived_bytes;
};

struct DeriveKeyState : public BaseState {
  DeriveKeyState(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result)
      : BaseState(result),
        algorithm(algorithm),
        base_key(base_key),
        import_algorithm(import_algorithm),
        key_length_algorithm(key_length_algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey base_key;
  const blink::WebCryptoAlgorithm import_algorithm;
  const blink::WebCryptoAlgorithm key_length_algorithm;
  bool extractable;
  blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey derived_key;
};

// --------------------------------------------------------------------
// Wrapper functions
// --------------------------------------------------------------------
//
// * The methods named Do*() run on the crypto thread.
// * The methods named Do*Reply() run on the target Blink thread

void DoEncryptReply(scoped_ptr<EncryptState> state) {
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoEncrypt(scoped_ptr<EncryptState> passed_state) {
  EncryptState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::Encrypt(state->algorithm, state->key,
                         webcrypto::CryptoData(state->data), &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoEncryptReply, Passed(&passed_state)));
}

void DoDecryptReply(scoped_ptr<DecryptState> state) {
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoDecrypt(scoped_ptr<DecryptState> passed_state) {
  DecryptState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::Decrypt(state->algorithm, state->key,
                         webcrypto::CryptoData(state->data), &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoDecryptReply, Passed(&passed_state)));
}

void DoDigestReply(scoped_ptr<DigestState> state) {
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoDigest(scoped_ptr<DigestState> passed_state) {
  DigestState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::Digest(
      state->algorithm, webcrypto::CryptoData(state->data), &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoDigestReply, Passed(&passed_state)));
}

void DoGenerateKeyReply(scoped_ptr<GenerateKeyState> state) {
  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->generate_key_result.Complete(&state->result);
  }
}

void DoGenerateKey(scoped_ptr<GenerateKeyState> passed_state) {
  GenerateKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::GenerateKey(state->algorithm, state->extractable,
                             state->usages, &state->generate_key_result);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoGenerateKeyReply, Passed(&passed_state)));
}

void DoImportKeyReply(scoped_ptr<ImportKeyState> state) {
  CompleteWithKeyOrError(state->status, state->key, &state->result);
}

void DoImportKey(scoped_ptr<ImportKeyState> passed_state) {
  ImportKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::ImportKey(
      state->format, webcrypto::CryptoData(state->key_data), state->algorithm,
      state->extractable, state->usages, &state->key);
  if (state->status.IsSuccess()) {
    DCHECK(state->key.handle());
    DCHECK(!state->key.algorithm().isNull());
    DCHECK_EQ(state->extractable, state->key.extractable());
  }

  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoImportKeyReply, Passed(&passed_state)));
}

void DoExportKeyReply(scoped_ptr<ExportKeyState> state) {
  if (state->format != blink::WebCryptoKeyFormatJwk) {
    CompleteWithBufferOrError(state->status, state->buffer, &state->result);
    return;
  }

  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->result.completeWithJson(
        reinterpret_cast<const char*>(vector_as_array(&state->buffer)),
        static_cast<unsigned int>(state->buffer.size()));
  }
}

void DoExportKey(scoped_ptr<ExportKeyState> passed_state) {
  ExportKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::ExportKey(state->format, state->key, &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoExportKeyReply, Passed(&passed_state)));
}

void DoSignReply(scoped_ptr<SignState> state) {
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoSign(scoped_ptr<SignState> passed_state) {
  SignState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::Sign(state->algorithm, state->key,
                      webcrypto::CryptoData(state->data), &state->buffer);

  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoSignReply, Passed(&passed_state)));
}

void DoVerifyReply(scoped_ptr<VerifySignatureState> state) {
  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->result.completeWithBoolean(state->verify_result);
  }
}

void DoVerify(scoped_ptr<VerifySignatureState> passed_state) {
  VerifySignatureState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::Verify(
      state->algorithm, state->key, webcrypto::CryptoData(state->signature),
      webcrypto::CryptoData(state->data), &state->verify_result);

  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoVerifyReply, Passed(&passed_state)));
}

void DoWrapKeyReply(scoped_ptr<WrapKeyState> state) {
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoWrapKey(scoped_ptr<WrapKeyState> passed_state) {
  WrapKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::WrapKey(state->format, state->key, state->wrapping_key,
                         state->wrap_algorithm, &state->buffer);

  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoWrapKeyReply, Passed(&passed_state)));
}

void DoUnwrapKeyReply(scoped_ptr<UnwrapKeyState> state) {
  CompleteWithKeyOrError(state->status, state->unwrapped_key, &state->result);
}

void DoUnwrapKey(scoped_ptr<UnwrapKeyState> passed_state) {
  UnwrapKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::UnwrapKey(
      state->format, webcrypto::CryptoData(state->wrapped_key),
      state->wrapping_key, state->unwrap_algorithm,
      state->unwrapped_key_algorithm, state->extractable, state->usages,
      &state->unwrapped_key);

  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoUnwrapKeyReply, Passed(&passed_state)));
}

void DoDeriveBitsReply(scoped_ptr<DeriveBitsState> state) {
  CompleteWithBufferOrError(state->status, state->derived_bytes,
                            &state->result);
}

void DoDeriveBits(scoped_ptr<DeriveBitsState> passed_state) {
  DeriveBitsState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::DeriveBits(state->algorithm, state->base_key,
                            state->length_bits, &state->derived_bytes);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoDeriveBitsReply, Passed(&passed_state)));
}

void DoDeriveKeyReply(scoped_ptr<DeriveKeyState> state) {
  CompleteWithKeyOrError(state->status, state->derived_key, &state->result);
}

void DoDeriveKey(scoped_ptr<DeriveKeyState> passed_state) {
  DeriveKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::DeriveKey(
      state->algorithm, state->base_key, state->import_algorithm,
      state->key_length_algorithm, state->extractable, state->usages,
      &state->derived_key);
  state->origin_thread->PostTask(
      FROM_HERE, base::Bind(DoDeriveKeyReply, Passed(&passed_state)));
}

}  // namespace

WebCryptoImpl::WebCryptoImpl() {
}

WebCryptoImpl::~WebCryptoImpl() {
}

void WebCryptoImpl::encrypt(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const unsigned char* data,
                            unsigned int data_size,
                            blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());

  scoped_ptr<EncryptState> state(
      new EncryptState(algorithm, key, data, data_size, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoEncrypt, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::decrypt(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const unsigned char* data,
                            unsigned int data_size,
                            blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());

  scoped_ptr<DecryptState> state(
      new DecryptState(algorithm, key, data, data_size, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoDecrypt, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::digest(const blink::WebCryptoAlgorithm& algorithm,
                           const unsigned char* data,
                           unsigned int data_size,
                           blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());

  scoped_ptr<DigestState> state(new DigestState(
      algorithm, blink::WebCryptoKey::createNull(), data, data_size, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoDigest, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::generateKey(const blink::WebCryptoAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());

  scoped_ptr<GenerateKeyState> state(
      new GenerateKeyState(algorithm, extractable, usages, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoGenerateKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::importKey(blink::WebCryptoKeyFormat format,
                              const unsigned char* key_data,
                              unsigned int key_data_size,
                              const blink::WebCryptoAlgorithm& algorithm,
                              bool extractable,
                              blink::WebCryptoKeyUsageMask usages,
                              blink::WebCryptoResult result) {
  scoped_ptr<ImportKeyState> state(new ImportKeyState(
      format, key_data, key_data_size, algorithm, extractable, usages, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoImportKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::exportKey(blink::WebCryptoKeyFormat format,
                              const blink::WebCryptoKey& key,
                              blink::WebCryptoResult result) {
  scoped_ptr<ExportKeyState> state(new ExportKeyState(format, key, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoExportKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::sign(const blink::WebCryptoAlgorithm& algorithm,
                         const blink::WebCryptoKey& key,
                         const unsigned char* data,
                         unsigned int data_size,
                         blink::WebCryptoResult result) {
  scoped_ptr<SignState> state(
      new SignState(algorithm, key, data, data_size, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoSign, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::verifySignature(const blink::WebCryptoAlgorithm& algorithm,
                                    const blink::WebCryptoKey& key,
                                    const unsigned char* signature,
                                    unsigned int signature_size,
                                    const unsigned char* data,
                                    unsigned int data_size,
                                    blink::WebCryptoResult result) {
  scoped_ptr<VerifySignatureState> state(new VerifySignatureState(
      algorithm, key, signature, signature_size, data, data_size, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoVerify, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::wrapKey(blink::WebCryptoKeyFormat format,
                            const blink::WebCryptoKey& key,
                            const blink::WebCryptoKey& wrapping_key,
                            const blink::WebCryptoAlgorithm& wrap_algorithm,
                            blink::WebCryptoResult result) {
  scoped_ptr<WrapKeyState> state(
      new WrapKeyState(format, key, wrapping_key, wrap_algorithm, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoWrapKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::unwrapKey(
    blink::WebCryptoKeyFormat format,
    const unsigned char* wrapped_key,
    unsigned wrapped_key_size,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& unwrap_algorithm,
    const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result) {
  scoped_ptr<UnwrapKeyState> state(new UnwrapKeyState(
      format, wrapped_key, wrapped_key_size, wrapping_key, unwrap_algorithm,
      unwrapped_key_algorithm, extractable, usages, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoUnwrapKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::deriveBits(const blink::WebCryptoAlgorithm& algorithm,
                               const blink::WebCryptoKey& base_key,
                               unsigned int length_bits,
                               blink::WebCryptoResult result) {
  scoped_ptr<DeriveBitsState> state(
      new DeriveBitsState(algorithm, base_key, length_bits, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoDeriveBits, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::deriveKey(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& base_key,
    const blink::WebCryptoAlgorithm& import_algorithm,
    const blink::WebCryptoAlgorithm& key_length_algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result) {
  scoped_ptr<DeriveKeyState> state(
      new DeriveKeyState(algorithm, base_key, import_algorithm,
                         key_length_algorithm, extractable, usages, result));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::Bind(DoDeriveKey, Passed(&state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

blink::WebCryptoDigestor* WebCryptoImpl::createDigestor(
    blink::WebCryptoAlgorithmId algorithm_id) {
  return webcrypto::CreateDigestor(algorithm_id).release();
}

bool WebCryptoImpl::deserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const unsigned char* key_data,
    unsigned key_data_size,
    blink::WebCryptoKey& key) {
  return webcrypto::DeserializeKeyForClone(
      algorithm, type, extractable, usages,
      webcrypto::CryptoData(key_data, key_data_size), &key);
}

bool WebCryptoImpl::serializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char>& key_data) {
  return webcrypto::SerializeKeyForClone(key, &key_data);
}

}  // namespace webcrypto
