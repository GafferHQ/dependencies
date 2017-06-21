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
#include "core/animation/InertEffect.h"

#include "core/animation/Interpolation.h"

namespace blink {

PassRefPtrWillBeRawPtr<InertEffect> InertEffect::create(PassRefPtrWillBeRawPtr<EffectModel> effect, const Timing& timing, bool paused, double inheritedTime)
{
    return adoptRefWillBeNoop(new InertEffect(effect, timing, paused, inheritedTime));
}

InertEffect::InertEffect(PassRefPtrWillBeRawPtr<EffectModel> model, const Timing& timing, bool paused, double inheritedTime)
    : AnimationEffect(timing)
    , m_model(model)
    , m_paused(paused)
    , m_inheritedTime(inheritedTime)
{
}

void InertEffect::sample(OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>>& result)
{
    updateInheritedTime(m_inheritedTime, TimingUpdateOnDemand);
    if (!isInEffect()) {
        result.clear();
        return;
    }

    double iteration = currentIteration();
    ASSERT(iteration >= 0);
    // FIXME: Handle iteration values which overflow int.
    return m_model->sample(static_cast<int>(iteration), timeFraction(), iterationDuration(), result);
}

double InertEffect::calculateTimeToEffectChange(bool, double, double) const
{
    return std::numeric_limits<double>::infinity();
}

DEFINE_TRACE(InertEffect)
{
    visitor->trace(m_model);
    AnimationEffect::trace(visitor);
}

} // namespace blink
