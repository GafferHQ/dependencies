/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ArchiveResourceCollection_h
#define ArchiveResourceCollection_h

#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ArchiveResource;
class KURL;
class MHTMLArchive;

class PLATFORM_EXPORT ArchiveResourceCollection final : public NoBaseWillBeGarbageCollectedFinalized<ArchiveResourceCollection> {
    WTF_MAKE_NONCOPYABLE(ArchiveResourceCollection); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(ArchiveResourceCollection);
public:
    static PassOwnPtrWillBeRawPtr<ArchiveResourceCollection> create()
    {
        return adoptPtrWillBeNoop(new ArchiveResourceCollection);
    }

    ~ArchiveResourceCollection();

    void addResource(PassRefPtrWillBeRawPtr<ArchiveResource>);
    void addAllResources(MHTMLArchive*);

    ArchiveResource* archiveResourceForURL(const KURL&);
    PassRefPtrWillBeRawPtr<MHTMLArchive> popSubframeArchive(const String& frameName, const KURL&);

    DECLARE_TRACE();

private:
    ArchiveResourceCollection();

    WillBeHeapHashMap<String, RefPtrWillBeMember<ArchiveResource>> m_subresources;
    WillBeHeapHashMap<String, RefPtrWillBeMember<MHTMLArchive>> m_subframes;
};

}

#endif
