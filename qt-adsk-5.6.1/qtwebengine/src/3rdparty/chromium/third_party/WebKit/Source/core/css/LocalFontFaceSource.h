// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LocalFontFaceSource_h
#define LocalFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class LocalFontFaceSource final : public CSSFontFaceSource {
public:
    LocalFontFaceSource(const String& fontName) : m_fontName(fontName) { }
    virtual bool isLocal() const override { return true; }
    virtual bool isLocalFontAvailable(const FontDescription&) override;

private:
    virtual PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override;

    class LocalFontHistograms {
    public:
        LocalFontHistograms() : m_reported(false) { }
        void record(bool loadSuccess);
    private:
        bool m_reported;
    };

    AtomicString m_fontName;
    LocalFontHistograms m_histograms;
};

} // namespace blink

#endif
