/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "RTCI420Frame.h"

#include "talk/media/base/videoframe.h"
#include "webrtc/base/scoped_ptr.h"

@implementation RTCI420Frame {
  rtc::scoped_ptr<cricket::VideoFrame> _videoFrame;
}

- (NSUInteger)width {
  return _videoFrame->GetWidth();
}

- (NSUInteger)height {
  return _videoFrame->GetHeight();
}

- (NSUInteger)chromaWidth {
  return _videoFrame->GetChromaWidth();
}

- (NSUInteger)chromaHeight {
  return _videoFrame->GetChromaHeight();
}

- (NSUInteger)chromaSize {
  return _videoFrame->GetChromaSize();
}

- (const uint8_t*)yPlane {
  const cricket::VideoFrame* const_frame = _videoFrame.get();
  return const_frame->GetYPlane();
}

- (const uint8_t*)uPlane {
  const cricket::VideoFrame* const_frame = _videoFrame.get();
  return const_frame->GetUPlane();
}

- (const uint8_t*)vPlane {
  const cricket::VideoFrame* const_frame = _videoFrame.get();
  return const_frame->GetVPlane();
}

- (NSInteger)yPitch {
  return _videoFrame->GetYPitch();
}

- (NSInteger)uPitch {
  return _videoFrame->GetUPitch();
}

- (NSInteger)vPitch {
  return _videoFrame->GetVPitch();
}

- (BOOL)makeExclusive {
  return _videoFrame->MakeExclusive();
}

@end

@implementation RTCI420Frame (Internal)

- (instancetype)initWithVideoFrame:(cricket::VideoFrame*)videoFrame {
  if (self = [super init]) {
    // Keep a shallow copy of the video frame. The underlying frame buffer is
    // not copied.
    _videoFrame.reset(videoFrame->Copy());
  }
  return self;
}

@end
