/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/frame/Console.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ScriptArguments.h"
#include "platform/TraceEvent.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {

ConsoleBase::~ConsoleBase()
{
}

void ConsoleBase::debug(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, DebugMessageLevel, scriptState, arguments);
}

void ConsoleBase::error(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, ErrorMessageLevel, scriptState, arguments);
}

void ConsoleBase::info(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, InfoMessageLevel, scriptState, arguments);
}

void ConsoleBase::log(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, LogMessageLevel, scriptState, arguments);
}

void ConsoleBase::warn(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(LogMessageType, WarningMessageLevel, scriptState, arguments);
}

void ConsoleBase::dir(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(DirMessageType, LogMessageLevel, scriptState, arguments);
}

void ConsoleBase::dirxml(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(DirXMLMessageType, LogMessageLevel, scriptState, arguments);
}

void ConsoleBase::table(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(TableMessageType, LogMessageLevel, scriptState, arguments);
}

void ConsoleBase::clear(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(ClearMessageType, LogMessageLevel, scriptState, arguments, true);
}

void ConsoleBase::trace(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(TraceMessageType, LogMessageLevel, scriptState, arguments, true, true);
}

void ConsoleBase::assertCondition(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments, bool condition)
{
    if (condition)
        return;

    internalAddMessage(AssertMessageType, ErrorMessageLevel, scriptState, arguments, true);
}

void ConsoleBase::count(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    RefPtrWillBeRawPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    // Follow Firebug's behavior of counting with null and undefined title in
    // the same bucket as no argument
    String title;
    arguments->getFirstArgumentAsString(title);
    String identifier = title.isEmpty() ? String(lastCaller.sourceURL() + ':' + String::number(lastCaller.lineNumber()))
        : String(title + '@');

    HashCountedSet<String>::AddResult result = m_counts.add(identifier);
    String message = title + ": " + String::number(result.storedValue->value);

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, DebugMessageLevel, message);
    consoleMessage->setType(CountMessageType);
    consoleMessage->setScriptState(scriptState);
    consoleMessage->setCallStack(callStack.release());
    reportMessageToConsole(consoleMessage.release());
}

void ConsoleBase::markTimeline(const String& title)
{
    timeStamp(title);
}

void ConsoleBase::profile(const String& title)
{
    InspectorInstrumentation::consoleProfile(context(), title);
}

void ConsoleBase::profileEnd(const String& title)
{
    InspectorInstrumentation::consoleProfileEnd(context(), title);
}

void ConsoleBase::time(const String& title)
{
    TRACE_EVENT_COPY_ASYNC_BEGIN0("blink.console", title.utf8().data(), this);

    if (title.isNull())
        return;

    m_times.add(title, monotonicallyIncreasingTime());
}

void ConsoleBase::timeEnd(ScriptState* scriptState, const String& title)
{
    TRACE_EVENT_COPY_ASYNC_END0("blink.console", title.utf8().data(), this);

    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    HashMap<String, double>::iterator it = m_times.find(title);
    if (it == m_times.end())
        return;

    double startTime = it->value;
    m_times.remove(it);

    double elapsed = monotonicallyIncreasingTime() - startTime;
    String message = title + String::format(": %.3fms", elapsed * 1000);

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, DebugMessageLevel, message);
    consoleMessage->setType(TimeEndMessageType);
    consoleMessage->setScriptState(scriptState);
    consoleMessage->setCallStack(createScriptCallStackForConsole(1));
    reportMessageToConsole(consoleMessage.release());
}

void ConsoleBase::timeStamp(const String& title)
{
    TRACE_EVENT_INSTANT1("devtools.timeline", "TimeStamp", TRACE_EVENT_SCOPE_THREAD, "data", InspectorTimeStampEvent::data(context(), title));
}

static String formatTimelineTitle(const String& title)
{
    return String::format("Timeline '%s'", title.utf8().data());
}

void ConsoleBase::timeline(ScriptState* scriptState, const String& title)
{
    TRACE_EVENT_COPY_ASYNC_BEGIN0("blink.console", formatTimelineTitle(title).utf8().data(), this);
}

void ConsoleBase::timelineEnd(ScriptState* scriptState, const String& title)
{
    TRACE_EVENT_COPY_ASYNC_END0("blink.console", formatTimelineTitle(title).utf8().data(), this);
}

void ConsoleBase::group(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(StartGroupMessageType, LogMessageLevel, scriptState, arguments, true);
}

void ConsoleBase::groupCollapsed(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    internalAddMessage(StartGroupCollapsedMessageType, LogMessageLevel, scriptState, arguments, true);
}

void ConsoleBase::groupEnd()
{
    internalAddMessage(EndGroupMessageType, LogMessageLevel, nullptr, nullptr, true);
}

void ConsoleBase::internalAddMessage(MessageType type, MessageLevel level, ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> scriptArguments, bool acceptNoArguments, bool printTrace)
{
    RefPtrWillBeRawPtr<ScriptArguments> arguments = scriptArguments;
    if (!acceptNoArguments && (!arguments || !arguments->argumentCount()))
        return;

    if (scriptState && !scriptState->contextIsValid())
        arguments.clear();
    String message;
    if (arguments)
        arguments->getFirstArgumentAsString(message);
    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, level, message);
    consoleMessage->setType(type);
    consoleMessage->setScriptState(scriptState);
    consoleMessage->setScriptArguments(arguments);

    size_t stackSize = printTrace ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    consoleMessage->setCallStack(createScriptCallStackForConsole(stackSize));
    reportMessageToConsole(consoleMessage.release());
}

} // namespace blink
