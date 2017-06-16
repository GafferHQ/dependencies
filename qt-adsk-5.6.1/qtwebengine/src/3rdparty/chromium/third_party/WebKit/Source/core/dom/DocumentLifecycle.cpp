/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/dom/DocumentLifecycle.h"

#include "wtf/Assertions.h"

namespace blink {

static DocumentLifecycle::DeprecatedTransition* s_deprecatedTransitionStack = 0;

DocumentLifecycle::Scope::Scope(DocumentLifecycle& lifecycle, State finalState)
    : m_lifecycle(lifecycle)
    , m_finalState(finalState)
{
}

DocumentLifecycle::Scope::~Scope()
{
    m_lifecycle.advanceTo(m_finalState);
}

DocumentLifecycle::DeprecatedTransition::DeprecatedTransition(State from, State to)
    : m_previous(s_deprecatedTransitionStack)
    , m_from(from)
    , m_to(to)
{
    s_deprecatedTransitionStack = this;
}

DocumentLifecycle::DeprecatedTransition::~DeprecatedTransition()
{
    s_deprecatedTransitionStack = m_previous;
}

DocumentLifecycle::DocumentLifecycle()
    : m_state(Uninitialized)
    , m_detachCount(0)
{
}

DocumentLifecycle::~DocumentLifecycle()
{
}

#if ENABLE(ASSERT)

bool DocumentLifecycle::canAdvanceTo(State state) const
{
    // We can stop from anywhere.
    if (state == Stopping)
        return true;

    switch (m_state) {
    case Uninitialized:
        return state == Inactive;
    case Inactive:
        if (state == StyleClean)
            return true;
        if (state == Disposed)
            return true;
        break;
    case VisualUpdatePending:
        if (state == InPreLayout)
            return true;
        if (state == InStyleRecalc)
            return true;
        if (state == InPerformLayout)
            return true;
        break;
    case InStyleRecalc:
        return state == StyleClean;
    case StyleClean:
        // We can synchronously recalc style.
        if (state == InStyleRecalc)
            return true;
        // We can notify layout objects that subtrees changed.
        if (state == InLayoutSubtreeChange)
            return true;
        // We can synchronously perform layout.
        if (state == InPreLayout)
            return true;
        if (state == InPerformLayout)
            return true;
        // We can redundant arrive in the style clean state.
        if (state == StyleClean)
            return true;
        if (state == LayoutClean)
            return true;
        if (state == InCompositingUpdate)
            return true;
        break;
    case InLayoutSubtreeChange:
        return state == LayoutSubtreeChangeClean;
    case LayoutSubtreeChangeClean:
        // We can synchronously recalc style.
        if (state == InStyleRecalc)
            return true;
        // We can synchronously perform layout.
        if (state == InPreLayout)
            return true;
        if (state == InPerformLayout)
            return true;
        // Can move back to style clean.
        if (state == StyleClean)
            return true;
        if (state == LayoutClean)
            return true;
        if (state == InCompositingUpdate)
            return true;
        break;
    case InPreLayout:
        if (state == InStyleRecalc)
            return true;
        if (state == StyleClean)
            return true;
        if (state == InPreLayout)
            return true;
        break;
    case InPerformLayout:
        return state == AfterPerformLayout;
    case AfterPerformLayout:
        // We can synchronously recompute layout in AfterPerformLayout.
        // FIXME: Ideally, we would unnest this recursion into a loop.
        if (state == InPreLayout)
            return true;
        if (state == LayoutClean)
            return true;
        break;
    case LayoutClean:
        // We can synchronously recalc style.
        if (state == InStyleRecalc)
            return true;
        // We can synchronously perform layout.
        if (state == InPreLayout)
            return true;
        if (state == InPerformLayout)
            return true;
        // We can redundant arrive in the layout clean state. This situation
        // can happen when we call layout recursively and we unwind the stack.
        if (state == LayoutClean)
            return true;
        if (state == StyleClean)
            return true;
        if (state == InCompositingUpdate)
            return true;
        break;
    case InCompositingUpdate:
        return state == CompositingClean;
    case CompositingClean:
        if (state == InStyleRecalc)
            return true;
        if (state == InPreLayout)
            return true;
        if (state == InCompositingUpdate)
            return true;
        if (state == InPaintInvalidation)
            return true;
        break;
    case InPaintInvalidation:
        return state == PaintInvalidationClean;
    case PaintInvalidationClean:
        if (state == InStyleRecalc)
            return true;
        if (state == InPreLayout)
            return true;
        if (state == InCompositingUpdate)
            return true;
        break;
    case Stopping:
        return state == Stopped;
    case Stopped:
        return state == Disposed;
    case Disposed:
        // FIXME: We can dispose a document multiple times. This seems wrong.
        // See https://code.google.com/p/chromium/issues/detail?id=301668.
        return state == Disposed;
    }
    return false;
}

bool DocumentLifecycle::canRewindTo(State state) const
{
    // This transition is bogus, but we've whitelisted it anyway.
    if (s_deprecatedTransitionStack && m_state == s_deprecatedTransitionStack->from() && state == s_deprecatedTransitionStack->to())
        return true;
    return m_state == StyleClean || m_state == LayoutSubtreeChangeClean || m_state == AfterPerformLayout || m_state == LayoutClean || m_state == CompositingClean || m_state == PaintInvalidationClean;
}

#endif

void DocumentLifecycle::advanceTo(State state)
{
    ASSERT(canAdvanceTo(state));
    m_state = state;
}

void DocumentLifecycle::ensureStateAtMost(State state)
{
    ASSERT(state == VisualUpdatePending || state == StyleClean || state == LayoutClean);
    if (m_state <= state)
        return;
    ASSERT(canRewindTo(state));
    m_state = state;
}

}
