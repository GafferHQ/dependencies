// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptAsyncCallStack_h
#define ScriptAsyncCallStack_h

#include "core/InspectorTypeBuilder.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace blink {

class ScriptAsyncCallStack : public RefCountedWillBeGarbageCollectedFinalized<ScriptAsyncCallStack> {
public:
    static PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> create(const String&, PassRefPtrWillBeRawPtr<ScriptCallStack>, PassRefPtrWillBeRawPtr<ScriptAsyncCallStack>);

    ~ScriptAsyncCallStack();

    PassRefPtr<TypeBuilder::Console::AsyncStackTrace> buildInspectorObject() const;

    DECLARE_TRACE();

private:
    ScriptAsyncCallStack(const String&, PassRefPtrWillBeRawPtr<ScriptCallStack>, PassRefPtrWillBeRawPtr<ScriptAsyncCallStack>);

    String m_description;
    RefPtrWillBeMember<ScriptCallStack> m_callStack;
    RefPtrWillBeMember<ScriptAsyncCallStack> m_asyncStackTrace;
};

} // namespace blink

#endif // ScriptAsyncCallStack_h
