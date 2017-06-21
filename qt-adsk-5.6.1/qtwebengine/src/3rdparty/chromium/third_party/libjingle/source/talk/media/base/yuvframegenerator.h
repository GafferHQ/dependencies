/*
 * libjingle
 * Copyright 2010 Google Inc.
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

// Generates YUV420 frames with a "landscape with striped crosshair" in the
// Y-plane, plus a horizontal gradient in the U-plane and a vertical one in the
// V-plane. This makes for a nice mix of colours that is suited for both
// catching visual errors and making sure e.g. YUV->RGB/BGR conversion looks
// the same on different platforms.
// There is also a solid box bouncing around in the Y-plane, and two differently
// coloured lines bouncing horizontally and vertically in the U and V plane.
// This helps illustrating how the frame boundary goes, and can aid as a quite
// handy visual help for noticing e.g. packet loss if the frames are encoded
// and sent over the network.

#ifndef TALK_MEDIA_BASE_YUVFRAMEGENERATOR_H_
#define TALK_MEDIA_BASE_YUVFRAMEGENERATOR_H_

#include "webrtc/base/basictypes.h"

namespace cricket {

class YuvFrameGenerator {
 public:
  // Constructs a frame-generator that produces frames of size |width|x|height|.
  // If |enable_barcode| is specified, barcodes can be included in the frames
  // when calling |GenerateNextFrame(uint8*, uint32)|. If |enable_barcode| is
  // |true| then |width|x|height| should be at least 160x100; otherwise this
  // constructor will abort.
  YuvFrameGenerator(int width, int height, bool enable_barcode);
  ~YuvFrameGenerator();

  int GetFrameSize() { return frame_data_size_; }

  // Generate the next frame and return it in the provided |frame_buffer|. If
  // barcode_value is not |nullptr| the value referred by it will be encoded
  // into a barcode in the frame.  The value should in the range:
  // [0..9,999,999]. If the value exceeds this range or barcodes were not
  // requested in the constructor, this function will abort.
  void GenerateNextFrame(uint8* frame_buffer, int32 barcode_value);

  int GetHeight() { return height_; }
  int GetWidth() { return width_; }

  // Fetch the bounds of the barcode from the generator. The barcode will
  // always be at this location. This function will abort if barcodes were not
  // requested in the constructor.
  void GetBarcodeBounds(int* top, int* left, int* width, int* height);

 private:
  void DrawLandscape(uint8 *p, int w, int h);
  void DrawGradientX(uint8 *p, int w, int h);
  void DrawGradientY(uint8 *p, int w, int h);
  void DrawMovingLineX(uint8 *p, int w, int h, int n);
  void DrawMovingLineY(uint8 *p, int w, int h, int n);
  void DrawBouncingCube(uint8 *p, int w, int h, int n);

  void DrawBarcode(uint32 value);
  int DrawSideGuardBars(int x, int y, int height);
  int DrawMiddleGuardBars(int x, int y, int height);
  int DrawEanEncodedDigit(int digit, int x, int y, int height, bool r_code);
  void DrawBlockRectangle(uint8* p, int x_start, int y_start,
                          int width, int height, int pitch, uint8 value);

 private:
  int width_;
  int height_;
  int frame_index_;
  int frame_data_size_;
  uint8* y_data_;
  uint8* u_data_;
  uint8* v_data_;

  int barcode_start_x_;
  int barcode_start_y_;

  DISALLOW_COPY_AND_ASSIGN(YuvFrameGenerator);
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_YUVFRAMEGENERATOR_H_
