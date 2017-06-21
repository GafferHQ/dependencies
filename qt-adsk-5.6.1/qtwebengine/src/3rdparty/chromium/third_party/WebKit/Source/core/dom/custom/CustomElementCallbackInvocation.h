/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CustomElementCallbackInvocation_h
#define CustomElementCallbackInvocation_h

#include "core/dom/custom/CustomElementLifecycleCallbacks.h"
#include "core/dom/custom/CustomElementProcessingStep.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CustomElementCallbackInvocation : public CustomElementProcessingStep {
    WTF_MAKE_NONCOPYABLE(CustomElementCallbackInvocation);
public:
    static PassOwnPtrWillBeRawPtr<CustomElementCallbackInvocation> createInvocation(PassRefPtrWillBeRawPtr<CustomElementLifecycleCallbacks>, CustomElementLifecycleCallbacks::CallbackType);
    static PassOwnPtrWillBeRawPtr<CustomElementCallbackInvocation> createAttributeChangedInvocation(PassRefPtrWillBeRawPtr<CustomElementLifecycleCallbacks>, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue);

protected:
    CustomElementCallbackInvocation(PassRefPtrWillBeRawPtr<CustomElementLifecycleCallbacks> callbacks)
        : m_callbacks(callbacks)
    {
    }

    CustomElementLifecycleCallbacks* callbacks() { return m_callbacks.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    RefPtrWillBeMember<CustomElementLifecycleCallbacks> m_callbacks;
};

} // namespace blink

#endif // CustomElementCallbackInvocation_h
