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

#ifndef Partitions_h
#define Partitions_h

#include "wtf/PartitionAlloc.h"
#include "wtf/WTF.h"
#include "wtf/WTFExport.h"

namespace WTF {

class WTF_EXPORT Partitions {
public:
    static void initialize();
    // TODO(bashi): Remove this function and make initialize() take
    // HistogramEnumerationFunction when we can make sure that WTF::initialize()
    // is called before using this class.
    static void setHistogramEnumeration(HistogramEnumerationFunction);
    static void shutdown();
    ALWAYS_INLINE static PartitionRootGeneric* bufferPartition()
    {
        // TODO(haraken): This check is needed because some call sites allocate
        // Blink things before WTF::initialize(). We should fix those call sites
        // and remove the check.
        if (UNLIKELY(!s_initialized))
            initialize();
        return m_bufferAllocator.root();
    }

    ALWAYS_INLINE static PartitionRootGeneric* fastMallocPartition()
    {
        // TODO(haraken): This check is needed because some call sites allocate
        // Blink things before WTF::initialize(). We should fix those call sites
        // and remove the check.
        if (UNLIKELY(!s_initialized))
            initialize();
        return m_fastMallocAllocator.root();
    }

    ALWAYS_INLINE static PartitionRoot* objectModelPartition()
    {
        ASSERT(s_initialized);
        return m_objectModelAllocator.root();
    }
    ALWAYS_INLINE static PartitionRoot* layoutPartition()
    {
        ASSERT(s_initialized);
        return m_layoutAllocator.root();
    }

    static size_t currentDOMMemoryUsage()
    {
        ASSERT(s_initialized);
        return m_objectModelAllocator.root()->totalSizeOfCommittedPages;
    }

    static size_t totalSizeOfCommittedPages()
    {
        size_t totalSize = 0;
        totalSize += m_fastMallocAllocator.root()->totalSizeOfCommittedPages;
        totalSize += m_bufferAllocator.root()->totalSizeOfCommittedPages;
        totalSize += m_objectModelAllocator.root()->totalSizeOfCommittedPages;
        totalSize += m_layoutAllocator.root()->totalSizeOfCommittedPages;
        return totalSize;
    }

    static void decommitFreeableMemory();

    static void reportMemoryUsageHistogram();

    static void dumpMemoryStats(PartitionStatsDumper*);

private:
    static int s_initializationLock;
    static bool s_initialized;
    static PartitionAllocatorGeneric m_fastMallocAllocator;
    static PartitionAllocatorGeneric m_bufferAllocator;
    static SizeSpecificPartitionAllocator<3328> m_objectModelAllocator;
    static SizeSpecificPartitionAllocator<1024> m_layoutAllocator;
    static HistogramEnumerationFunction m_histogramEnumeration;
};

} // namespace WTF

#endif // Partitions_h
