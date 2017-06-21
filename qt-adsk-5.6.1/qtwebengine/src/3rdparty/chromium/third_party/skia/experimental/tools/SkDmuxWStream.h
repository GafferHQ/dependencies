/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkDmuxWStream_DEFINED
#define SkDmuxWStream_DEFINED

#include "SkStream.h"
#include "SkTDArray.h"

/**
 *  A SkWStream Demultiplexer.  If initialized with
 *      SkDmuxWStream dmuxWStream(NULL, 0);
 *  then it becomes a /dev/null.
 */
class SkDmuxWStream : public SkWStream {
public:
    SkDmuxWStream(SkWStream* const streamArray[], size_t count);
    ~SkDmuxWStream();
    virtual bool write(const void* buffer, size_t size) override;
    virtual void newline() override;
    virtual void flush() override;
    virtual size_t bytesWritten() const override;

private:
    SkTDArray<SkWStream*> fWStreams;
    size_t fBytesWritten;
};

#endif  // SkDmuxWStream_DEFINED
