/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FrameConsole_h
#define FrameConsole_h

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/frame/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;
class DocumentLoader;
class ResourceError;
class ResourceResponse;
class ScriptCallStack;
class WorkerGlobalScopeProxy;

// FrameConsole takes per-frame console messages and routes them up through the FrameHost to the ChromeClient and Inspector.
// It's meant as an abstraction around ChromeClient calls and the way that Blink core/ can add messages to the console.
class CORE_EXPORT FrameConsole final : public NoBaseWillBeGarbageCollected<FrameConsole> {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(FrameConsole);
public:
    static PassOwnPtrWillBeRawPtr<FrameConsole> create(LocalFrame& frame)
    {
        return adoptPtrWillBeNoop(new FrameConsole(frame));
    }

    void addMessage(PassRefPtrWillBeRawPtr<ConsoleMessage>);
    void adoptWorkerMessagesAfterTermination(WorkerGlobalScopeProxy*);

    void reportResourceResponseReceived(DocumentLoader*, unsigned long requestIdentifier, const ResourceResponse&);

    static String formatStackTraceString(const String& originalMessage, PassRefPtrWillBeRawPtr<ScriptCallStack>);

    static void mute();
    static void unmute();

    void clearMessages();

    void didFailLoading(unsigned long requestIdentifier, const ResourceError&);

    DECLARE_TRACE();

private:
    explicit FrameConsole(LocalFrame&);

    LocalFrame& frame() const
    {
        ASSERT(m_frame);
        return *m_frame;
    }

    ConsoleMessageStorage* messageStorage();

    RawPtrWillBeMember<LocalFrame> m_frame;
};

} // namespace blink

#endif // FrameConsole_h
