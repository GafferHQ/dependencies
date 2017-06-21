// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorLanguage_h
#define NavigatorLanguage_h

#include "wtf/text/AtomicString.h"

namespace blink {

class NavigatorLanguage {
public:
    NavigatorLanguage();

    AtomicString language();
    virtual Vector<String> languages() = 0;
    bool hasLanguagesChanged();
    void setLanguagesChanged();

private:
    bool m_languagesChanged;
};

} // namespace blink

#endif // NavigatorLanguage_h
