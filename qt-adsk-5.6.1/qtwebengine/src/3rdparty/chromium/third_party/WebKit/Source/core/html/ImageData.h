/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef ImageData_h
#define ImageData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT ImageData final : public GarbageCollectedFinalized<ImageData>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ImageData* create(const IntSize&);
    static ImageData* create(const IntSize&, PassRefPtr<DOMUint8ClampedArray>);
    static ImageData* create(unsigned width, unsigned height, ExceptionState&);
    static ImageData* create(DOMUint8ClampedArray*, unsigned width, ExceptionState&);
    static ImageData* create(DOMUint8ClampedArray*, unsigned width, unsigned height, ExceptionState&);

    IntSize size() const { return m_size; }
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    const DOMUint8ClampedArray* data() const { return m_data.get(); }
    DOMUint8ClampedArray* data() { return m_data.get(); }

    DEFINE_INLINE_TRACE() { }

    void dispose();

    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) override WARN_UNUSED_RETURN;

private:
    explicit ImageData(const IntSize&);
    ImageData(const IntSize&, PassRefPtr<DOMUint8ClampedArray>);

    static bool validateConstructorArguments(DOMUint8ClampedArray*, unsigned width, unsigned&, ExceptionState&);

    IntSize m_size;
    RefPtr<DOMUint8ClampedArray> m_data;
};

} // namespace blink

#endif // ImageData_h
