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

#ifndef WebLeakDetector_h
#define WebLeakDetector_h

#include "WebFrame.h"
#include "public/platform/WebCommon.h"

namespace blink {

class WebLeakDetectorClient {
public:
    struct Result {
        unsigned numberOfLiveAudioNodes;
        unsigned numberOfLiveDocuments;
        unsigned numberOfLiveNodes;
        // FIXME: Deprecated, remove once chrome is updated.
        unsigned numberOfLiveRenderObjects;
        unsigned numberOfLiveLayoutObjects;
        unsigned numberOfLiveResources;
        unsigned numberOfLiveActiveDOMObjects;
        unsigned numberOfLiveScriptPromises;
        unsigned numberOfLiveFrames;
        unsigned numberOfLiveV8PerContextData;
    };

    virtual void onLeakDetectionComplete(const Result&) = 0;
};

class WebLeakDetector {
public:
    virtual ~WebLeakDetector() { }

    BLINK_EXPORT static WebLeakDetector* create(WebLeakDetectorClient*);

    // Cleans up the DOM objects and counts them. |WebLeakDetectorClient::onLeakDetectionComplete()| is called when done.
    // This is supposed to be used for detecting DOM-object leaks.
    virtual void collectGarbageAndGetDOMCounts(WebLocalFrame*) = 0;
};

} // namespace blink

#endif // WebLeakDetector_h
