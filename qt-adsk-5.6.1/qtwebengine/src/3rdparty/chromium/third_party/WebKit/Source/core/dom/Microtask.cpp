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

#include "config.h"
#include "core/dom/Microtask.h"

#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/Task.h"
#include "public/platform/WebThread.h"
#include <v8.h>

namespace blink {

void Microtask::performCheckpoint(v8::Isolate* isolate)
{
    V8PerIsolateData* isolateData = V8PerIsolateData::from(isolate);
    ASSERT(isolateData);
    if (isolateData->recursionLevel() || isolateData->performingMicrotaskCheckpoint() || isolateData->destructionPending() || ScriptForbiddenScope::isScriptForbidden())
        return;
    isolateData->setPerformingMicrotaskCheckpoint(true);
    {
        // Ensure that end-of-task-or-microtask actions are performed.
        V8RecursionScope recursionScope(isolate);
        isolate->RunMicrotasks();
    }
    isolateData->setPerformingMicrotaskCheckpoint(false);
}

bool Microtask::performingCheckpoint(v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->performingMicrotaskCheckpoint();
}

static void microtaskFunctionCallback(void* data)
{
    OwnPtr<WebThread::Task> task = adoptPtr(static_cast<WebThread::Task*>(data));
    task->run();
}

void Microtask::enqueueMicrotask(PassOwnPtr<WebThread::Task> callback)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    isolate->EnqueueMicrotask(&microtaskFunctionCallback, callback.leakPtr());
}

void Microtask::enqueueMicrotask(PassOwnPtr<Closure> callback)
{
    enqueueMicrotask(adoptPtr(new Task(callback)));
}

} // namespace blink
