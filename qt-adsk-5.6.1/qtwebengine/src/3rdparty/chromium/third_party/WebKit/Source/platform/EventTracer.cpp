/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "platform/EventTracer.h"

#include "public/platform/Platform.h"
#include "public/platform/WebConvertableToTraceFormat.h"
#include "wtf/Assertions.h"
#include <stdio.h>

namespace blink {

static_assert(sizeof(Platform::TraceEventHandle) == sizeof(TraceEvent::TraceEventHandle), "TraceEventHandle types must be compatible");
static_assert(sizeof(Platform::TraceEventAPIAtomicWord) == sizeof(TraceEvent::TraceEventAPIAtomicWord), "TraceEventAPIAtomicWord types must be compatible");
static_assert(sizeof(TraceEvent::TraceEventAPIAtomicWord) == sizeof(const char*), "TraceEventAPIAtomicWord must be pointer-sized.");

// The dummy variable is needed to avoid a crash when someone updates the state variables
// before EventTracer::initialize() is called.
TraceEvent::TraceEventAPIAtomicWord dummyTraceSamplingState = 0;
TraceEvent::TraceEventAPIAtomicWord* traceSamplingState[3] = {&dummyTraceSamplingState, &dummyTraceSamplingState, &dummyTraceSamplingState };

void EventTracer::initialize()
{
    // current() might not exist in unit tests.
    if (!Platform::current())
        return;

    traceSamplingState[0] = Platform::current()->getTraceSamplingState(0);
    // FIXME: traceSamplingState[0] can be 0 in split-dll build. http://crbug.com/256965
    if (!traceSamplingState[0])
        traceSamplingState[0] = &dummyTraceSamplingState;
    traceSamplingState[1] = Platform::current()->getTraceSamplingState(1);
    if (!traceSamplingState[1])
        traceSamplingState[1] = &dummyTraceSamplingState;
    traceSamplingState[2] = Platform::current()->getTraceSamplingState(2);
    if (!traceSamplingState[2])
        traceSamplingState[2] = &dummyTraceSamplingState;
}

const unsigned char* EventTracer::getTraceCategoryEnabledFlag(const char* categoryName)
{
    static const char* dummyCategoryEnabledFlag = "*";
    // current() might not exist in unit tests.
    if (!Platform::current())
        return reinterpret_cast<const unsigned char*>(dummyCategoryEnabledFlag);

    return Platform::current()->getTraceCategoryEnabledFlag(categoryName);
}

TraceEvent::TraceEventHandle EventTracer::addTraceEvent(char phase, const unsigned char* categoryEnabledFlag,
    const char* name, unsigned long long id, double timestamp,
    int numArgs, const char* argNames[], const unsigned char argTypes[],
    const unsigned long long argValues[],
    PassRefPtr<TraceEvent::ConvertableToTraceFormat> convertableValue1,
    PassRefPtr<TraceEvent::ConvertableToTraceFormat> convertableValue2,
    unsigned char flags)
{
    WebConvertableToTraceFormat webConvertableValues[2];
    webConvertableValues[0] = WebConvertableToTraceFormat(convertableValue1);
    webConvertableValues[1] = WebConvertableToTraceFormat(convertableValue2);
    return Platform::current()->addTraceEvent(phase, categoryEnabledFlag, name, id, timestamp, numArgs, argNames, argTypes, argValues, webConvertableValues, flags);
}

TraceEvent::TraceEventHandle EventTracer::addTraceEvent(char phase, const unsigned char* categoryEnabledFlag,
    const char* name, unsigned long long id, double timestamp,
    int numArgs, const char** argNames, const unsigned char* argTypes,
    const unsigned long long* argValues, unsigned char flags)
{
    return Platform::current()->addTraceEvent(phase, categoryEnabledFlag, name, id, timestamp, numArgs, argNames, argTypes, argValues, 0, flags);
}

void EventTracer::updateTraceEventDuration(const unsigned char* categoryEnabledFlag, const char* name, TraceEvent::TraceEventHandle handle)
{
    Platform::current()->updateTraceEventDuration(categoryEnabledFlag, name, handle);
}

} // namespace blink
