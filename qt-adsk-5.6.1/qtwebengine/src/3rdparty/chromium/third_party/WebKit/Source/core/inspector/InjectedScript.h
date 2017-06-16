/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InjectedScript_h
#define InjectedScript_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/InspectorTypeBuilder.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InjectedScriptNative.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"

namespace blink {

class JSONValue;
class Node;
class ScriptFunctionCall;

typedef String ErrorString;
PassRefPtr<JSONValue> toJSONValue(const ScriptValue&);


class InjectedScript final {
public:
    InjectedScript();
    ~InjectedScript();

    bool isEmpty() const { return m_injectedScriptObject.isEmpty(); }
    ScriptState* scriptState() const
    {
        ASSERT(!isEmpty());
        return m_injectedScriptObject.scriptState();
    }

    void evaluate(
        ErrorString*,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
        TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<TypeBuilder::Debugger::ExceptionDetails>*);
    void callFunctionOn(
        ErrorString*,
        const String& objectId,
        const String& expression,
        const String& arguments,
        bool returnByValue,
        bool generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
        TypeBuilder::OptOutput<bool>* wasThrown);
    void evaluateOnCallFrame(
        ErrorString*,
        const ScriptValue& callFrames,
        const Vector<ScriptValue>& asyncCallStacks,
        const String& callFrameId,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>* result,
        TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<TypeBuilder::Debugger::ExceptionDetails>*);
    void restartFrame(ErrorString*, const ScriptValue& callFrames, const String& callFrameId, RefPtr<JSONObject>* result);
    void getStepInPositions(ErrorString*, const ScriptValue& callFrames, const String& callFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location> >& positions);
    void setVariableValue(ErrorString*, const ScriptValue& callFrames, const String* callFrameIdOpt, const String* functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr);
    void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>* result);
    void getGeneratorObjectDetails(ErrorString*, const String& functionId, RefPtr<TypeBuilder::Debugger::GeneratorObjectDetails>* result);
    void getCollectionEntries(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CollectionEntry> >* result);
    void getProperties(ErrorString*, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>* result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>*);
    void getInternalProperties(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>* result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>*);
    Node* nodeForObjectId(const String& objectId);
    EventTarget* eventTargetForObjectId(const String& objectId);
    void releaseObject(const String& objectId);

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame> > wrapCallFrames(const ScriptValue&, int asyncOrdinal);

    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapObject(const ScriptValue&, const String& groupName, bool generatePreview = false) const;
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapTable(const ScriptValue& table, const ScriptValue& columns) const;
    PassRefPtr<TypeBuilder::Runtime::RemoteObject> wrapNode(Node*, const String& groupName);
    ScriptValue findObjectById(const String& objectId) const;

    String objectIdToObjectGroupName(const String& objectId) const;
    void releaseObjectGroup(const String&);

    void setCustomObjectFormatterEnabled(bool);

private:
    friend InjectedScript InjectedScriptManager::injectedScriptFor(ScriptState*);
    using InspectedStateAccessCheck = bool (*)(ScriptState*);
    InjectedScript(ScriptValue, InspectedStateAccessCheck, PassRefPtr<InjectedScriptNative>);

    ScriptValue nodeAsScriptValue(Node*);
    void initialize(ScriptValue, InspectedStateAccessCheck);
    bool canAccessInspectedWindow() const;
    const ScriptValue& injectedScriptObject() const;
    ScriptValue callFunctionWithEvalEnabled(ScriptFunctionCall&, bool& hadException) const;
    void makeCall(ScriptFunctionCall&, RefPtr<JSONValue>* result);
    void makeEvalCall(ErrorString*, ScriptFunctionCall&, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* = 0);
    void makeCallWithExceptionDetails(ScriptFunctionCall&, RefPtr<JSONValue>* result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>*);

    ScriptValue m_injectedScriptObject;
    InspectedStateAccessCheck m_inspectedStateAccessCheck;
    RefPtr<InjectedScriptNative> m_native;
};

} // namespace blink

#endif
