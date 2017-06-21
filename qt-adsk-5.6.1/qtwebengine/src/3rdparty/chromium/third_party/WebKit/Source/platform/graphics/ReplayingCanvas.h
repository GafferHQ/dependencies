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

#ifndef ReplayingCanvas_h
#define ReplayingCanvas_h

#include "platform/graphics/InterceptingCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

class ReplayingCanvas;

template<> class CanvasInterceptor<ReplayingCanvas> : protected InterceptingCanvasBase::CanvasInterceptorBase<ReplayingCanvas> {
public:
    CanvasInterceptor(InterceptingCanvasBase* canvas) : InterceptingCanvasBase::CanvasInterceptorBase<ReplayingCanvas>(canvas) { }
    ~CanvasInterceptor();
};

class ReplayingCanvas : public InterceptingCanvas<ReplayingCanvas>, public SkPicture::AbortCallback {
public:
    ReplayingCanvas(SkBitmap, unsigned fromStep, unsigned toStep);

    bool abort() override;
    SkCanvas::SaveLayerStrategy willSaveLayer(const SkRect* bounds, const SkPaint*, SaveFlags) override;
    void onDrawPicture(const SkPicture*, const SkMatrix*, const SkPaint*) override;

private:
    friend class CanvasInterceptor<ReplayingCanvas>;

    void updateInRange();

    unsigned m_fromStep;
    unsigned m_toStep;
    bool m_abortDrawing;
};

} // namespace blink

#endif // ReplayingCanvas_h
