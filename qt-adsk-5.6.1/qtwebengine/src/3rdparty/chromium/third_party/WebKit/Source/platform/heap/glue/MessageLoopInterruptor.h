/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef MessageLoopInterruptor_h
#define MessageLoopInterruptor_h

#include "platform/heap/ThreadState.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class MessageLoopInterruptor : public ThreadState::Interruptor {
public:
    explicit MessageLoopInterruptor(WebThread* thread) : m_thread(thread) { }

    void requestInterrupt() override
    {
        // GCTask has an empty run() method. Its only purpose is to guarantee
        // that MessageLoop will have a task to process which will result
        // in PendingGCRunner::didProcessTask being executed.
        m_thread->postTask(FROM_HERE, new GCTask);
    }

private:
    class GCTask : public WebThread::Task {
    public:
        virtual ~GCTask() { }

        void run() override
        {
            // Don't do anything here because we don't know if this is
            // a nested event loop or not. PendingGCRunner::didProcessTask
            // will enter correct safepoint for us.
            // We are not calling onInterrupted() because that always
            // conservatively enters safepoint with pointers on stack.
        }
    };

    WebThread* m_thread;
};

} // namespace blink

#endif
