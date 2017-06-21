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
#include "core/inspector/ScriptCallFrame.h"

#include "platform/TracedValue.h"

namespace blink {

ScriptCallFrame::ScriptCallFrame()
    : m_functionName("undefined")
    , m_scriptId("")
    , m_scriptName("undefined")
    , m_lineNumber(0)
    , m_column(0)
{
}

ScriptCallFrame::ScriptCallFrame(const String& functionName, const String& scriptId, const String& scriptName, unsigned lineNumber, unsigned column)
    : m_functionName(functionName)
    , m_scriptId(scriptId)
    , m_scriptName(scriptName)
    , m_lineNumber(lineNumber)
    , m_column(column)
{
}

ScriptCallFrame::~ScriptCallFrame()
{
}

// buildInspectorObject() and toTracedValue() should set the same fields.
// If either of them is modified, the other should be also modified.
PassRefPtr<TypeBuilder::Console::CallFrame> ScriptCallFrame::buildInspectorObject() const
{
    return TypeBuilder::Console::CallFrame::create()
        .setFunctionName(m_functionName)
        .setScriptId(m_scriptId)
        .setUrl(m_scriptName)
        .setLineNumber(m_lineNumber)
        .setColumnNumber(m_column)
        .release();
}

void ScriptCallFrame::toTracedValue(TracedValue* value) const
{
    value->beginDictionary();
    value->setString("functionName", m_functionName);
    value->setString("scriptId", m_scriptId);
    value->setString("url", m_scriptName);
    value->setInteger("lineNumber", m_lineNumber);
    value->setInteger("columnNumber", m_column);
    value->endDictionary();
}

} // namespace blink
