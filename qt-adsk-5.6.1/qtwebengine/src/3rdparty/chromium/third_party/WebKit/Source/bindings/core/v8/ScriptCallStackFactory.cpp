/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "bindings/core/v8/ScriptCallStackFactory.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/JSONValues.h"
#include "wtf/text/StringBuilder.h"

#include <v8-debug.h>

namespace blink {

class ExecutionContext;

static ScriptCallFrame toScriptCallFrame(v8::Local<v8::StackFrame> frame)
{
    StringBuilder stringBuilder;
    stringBuilder.appendNumber(frame->GetScriptId());
    String scriptId = stringBuilder.toString();
    String sourceName;
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    if (!sourceNameValue.IsEmpty())
        sourceName = toCoreString(sourceNameValue);

    String functionName;
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    if (!functionNameValue.IsEmpty())
        functionName = toCoreString(functionNameValue);

    int sourceLineNumber = frame->GetLineNumber();
    int sourceColumn = frame->GetColumn();
    return ScriptCallFrame(functionName, scriptId, sourceName, sourceLineNumber, sourceColumn);
}

static void toScriptCallFramesVector(v8::Local<v8::StackTrace> stackTrace, Vector<ScriptCallFrame>& scriptCallFrames, size_t maxStackSize, bool emptyStackIsAllowed, v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    int frameCount = stackTrace->GetFrameCount();
    if (frameCount > static_cast<int>(maxStackSize))
        frameCount = maxStackSize;
    for (int i = 0; i < frameCount; i++) {
        v8::Local<v8::StackFrame> stackFrame = stackTrace->GetFrame(i);
        scriptCallFrames.append(toScriptCallFrame(stackFrame));
    }
    if (!frameCount && !emptyStackIsAllowed) {
        // Successfully grabbed stack trace, but there are no frames. It may happen in case
        // when a bound function is called from native code for example.
        // Fallback to setting lineNumber to 0, and source and function name to "undefined".
        scriptCallFrames.append(ScriptCallFrame());
    }
}

static PassRefPtrWillBeRawPtr<ScriptCallStack> createScriptCallStack(v8::Isolate* isolate, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize, bool emptyStackIsAllowed)
{
    ASSERT(isolate->InContext());
    ASSERT(!stackTrace.IsEmpty());
    v8::HandleScope scope(isolate);
    Vector<ScriptCallFrame> scriptCallFrames;
    toScriptCallFramesVector(stackTrace, scriptCallFrames, maxStackSize, emptyStackIsAllowed, isolate);
    RefPtrWillBeRawPtr<ScriptCallStack> callStack = ScriptCallStack::create(scriptCallFrames);
    if (InspectorInstrumentation::hasFrontends() && maxStackSize > 1)
        InspectorInstrumentation::appendAsyncCallStack(currentExecutionContext(isolate), callStack.get());
    return callStack.release();
}

PassRefPtrWillBeRawPtr<ScriptCallStack> createScriptCallStack(v8::Isolate* isolate, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize)
{
    return createScriptCallStack(isolate, stackTrace, maxStackSize, true);
}

PassRefPtrWillBeRawPtr<ScriptCallStack> createScriptCallStack(size_t maxStackSize, bool emptyStackIsAllowed)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (!isolate->InContext())
        return nullptr;
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::StackTrace> stackTrace(v8::StackTrace::CurrentStackTrace(isolate, maxStackSize, stackTraceOptions));
    return createScriptCallStack(isolate, stackTrace, maxStackSize, emptyStackIsAllowed);
}

PassRefPtrWillBeRawPtr<ScriptCallStack> createScriptCallStackForConsole(size_t maxStackSize, bool emptyStackIsAllowed)
{
    size_t stackSize = 1;
    if (InspectorInstrumentation::hasFrontends()) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        if (!isolate->InContext())
            return nullptr;
        if (InspectorInstrumentation::consoleAgentEnabled(currentExecutionContext(isolate)))
            stackSize = maxStackSize;
    }
    return createScriptCallStack(stackSize, emptyStackIsAllowed);
}

PassRefPtrWillBeRawPtr<ScriptArguments> createScriptArguments(ScriptState* scriptState, const v8::FunctionCallbackInfo<v8::Value>& v8arguments, unsigned skipArgumentCount)
{
    Vector<ScriptValue> arguments;
    for (int i = skipArgumentCount; i < v8arguments.Length(); ++i)
        arguments.append(ScriptValue(scriptState, v8arguments[i]));

    return ScriptArguments::create(scriptState, arguments);
}

} // namespace blink
