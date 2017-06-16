// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <cmath>
#include <string>
#include <vector>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gl_helper_readback_support.h"
#include "content/common/gpu/client/gl_helper_scaling.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gl/gl_implementation.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace content {

using blink::WebGLId;
using blink::WebGraphicsContext3D;
using gpu_blink::WebGraphicsContext3DInProcessCommandBufferImpl;

content::GLHelper::ScalerQuality kQualities[] = {
    content::GLHelper::SCALER_QUALITY_BEST,
    content::GLHelper::SCALER_QUALITY_GOOD,
    content::GLHelper::SCALER_QUALITY_FAST, };

const char* kQualityNames[] = {"best", "good", "fast", };

class GLHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    WebGraphicsContext3D::Attributes attributes;
    bool lose_context_when_out_of_memory = false;
    context_ =
        WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
            attributes, lose_context_when_out_of_memory);
    context_->InitializeOnCurrentThread();
    context_support_ = context_->GetContextSupport();
    helper_.reset(
        new content::GLHelper(context_->GetGLInterface(), context_support_));
    helper_scaling_.reset(new content::GLHelperScaling(
        context_->GetGLInterface(), helper_.get()));
  }

  void TearDown() override {
    helper_scaling_.reset(NULL);
    helper_.reset(NULL);
    context_.reset(NULL);
  }

  void StartTracing(const std::string& filter) {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig(filter,
                                       base::trace_event::RECORD_UNTIL_FULL),
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  static void TraceDataCB(
      const base::Callback<void()>& callback,
      std::string* output,
      const scoped_refptr<base::RefCountedString>& json_events_str,
      bool has_more_events) {
    if (output->size() > 1 && !json_events_str->data().empty()) {
      output->append(",");
    }
    output->append(json_events_str->data());
    if (!has_more_events) {
      callback.Run();
    }
  }

  // End tracing, return tracing data in a simple map
  // of event name->counts.
  void EndTracing(std::map<std::string, int>* event_counts) {
    std::string json_data = "[";
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    base::RunLoop run_loop;
    base::trace_event::TraceLog::GetInstance()->Flush(
        base::Bind(&GLHelperTest::TraceDataCB,
                   run_loop.QuitClosure(),
                   base::Unretained(&json_data)));
    run_loop.Run();
    json_data.append("]");

    std::string error_msg;
    scoped_ptr<base::Value> trace_data(
        base::JSONReader::DeprecatedReadAndReturnError(json_data, 0, NULL,
                                                       &error_msg));
    CHECK(trace_data)
        << "JSON parsing failed (" << error_msg << ") JSON data:" << std::endl
        << json_data;

    base::ListValue* list;
    CHECK(trace_data->GetAsList(&list));
    for (size_t i = 0; i < list->GetSize(); i++) {
      base::Value* item = NULL;
      if (list->Get(i, &item)) {
        base::DictionaryValue* dict;
        CHECK(item->GetAsDictionary(&dict));
        std::string name;
        CHECK(dict->GetString("name", &name));
        std::string trace_type;
        CHECK(dict->GetString("ph", &trace_type));
        // Count all except END traces, as they come in BEGIN/END pairs.
        if (trace_type != "E")
          (*event_counts)[name]++;
        VLOG(1) << "trace name: " << name;
      }
    }
  }

  // Bicubic filter kernel function.
  static float Bicubic(float x) {
    const float a = -0.5;
    x = std::abs(x);
    float x2 = x * x;
    float x3 = x2 * x;
    if (x <= 1) {
      return (a + 2) * x3 - (a + 3) * x2 + 1;
    } else if (x < 2) {
      return a * x3 - 5 * a * x2 + 8 * a * x - 4 * a;
    } else {
      return 0.0f;
    }
  }

  // Look up a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  int Channel(SkBitmap* pixels, int x, int y, int c) {
    if (pixels->bytesPerPixel() == 4) {
      uint32* data =
          pixels->getAddr32(std::max(0, std::min(x, pixels->width() - 1)),
                            std::max(0, std::min(y, pixels->height() - 1)));
      return (*data) >> (c * 8) & 0xff;
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      return *pixels->getAddr8(std::max(0, std::min(x, pixels->width() - 1)),
                               std::max(0, std::min(y, pixels->height() - 1)));
    }
  }

  // Set a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  void SetChannel(SkBitmap* pixels, int x, int y, int c, int v) {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LT(x, pixels->width());
    DCHECK_LT(y, pixels->height());
    if (pixels->bytesPerPixel() == 4) {
      uint32* data = pixels->getAddr32(x, y);
      v = std::max(0, std::min(v, 255));
      *data = (*data & ~(0xffu << (c * 8))) | (v << (c * 8));
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      uint8* data = pixels->getAddr8(x, y);
      v = std::max(0, std::min(v, 255));
      *data = v;
    }
  }

  // Print all the R, G, B or A values from an SkBitmap in a
  // human-readable format.
  void PrintChannel(SkBitmap* pixels, int c) {
    for (int y = 0; y < pixels->height(); y++) {
      std::string formatted;
      for (int x = 0; x < pixels->width(); x++) {
        formatted.append(base::StringPrintf("%3d, ", Channel(pixels, x, y, c)));
      }
      LOG(ERROR) << formatted;
    }
  }

  // Print out the individual steps of a scaler pipeline.
  std::string PrintStages(
      const std::vector<GLHelperScaling::ScalerStage>& scaler_stages) {
    std::string ret;
    for (size_t i = 0; i < scaler_stages.size(); i++) {
      ret.append(base::StringPrintf("%dx%d -> %dx%d ",
                                    scaler_stages[i].src_size.width(),
                                    scaler_stages[i].src_size.height(),
                                    scaler_stages[i].dst_size.width(),
                                    scaler_stages[i].dst_size.height()));
      bool xy_matters = false;
      switch (scaler_stages[i].shader) {
        case GLHelperScaling::SHADER_BILINEAR:
          ret.append("bilinear");
          break;
        case GLHelperScaling::SHADER_BILINEAR2:
          ret.append("bilinear2");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR3:
          ret.append("bilinear3");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR4:
          ret.append("bilinear4");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BILINEAR2X2:
          ret.append("bilinear2x2");
          break;
        case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
          ret.append("bicubic upscale");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_BICUBIC_HALF_1D:
          ret.append("bicubic 1/2");
          xy_matters = true;
          break;
        case GLHelperScaling::SHADER_PLANAR:
          ret.append("planar");
          break;
        case GLHelperScaling::SHADER_YUV_MRT_PASS1:
          ret.append("rgb2yuv pass 1");
          break;
        case GLHelperScaling::SHADER_YUV_MRT_PASS2:
          ret.append("rgb2yuv pass 2");
          break;
      }

      if (xy_matters) {
        if (scaler_stages[i].scale_x) {
          ret.append(" X");
        } else {
          ret.append(" Y");
        }
      }
      ret.append("\n");
    }
    return ret;
  }

  bool CheckScale(double scale, int samples, bool already_scaled) {
    // 1:1 is valid if there is one sample.
    if (samples == 1 && scale == 1.0) {
      return true;
    }
    // Is it an exact down-scale (50%, 25%, etc.?)
    if (scale == 2.0 * samples) {
      return true;
    }
    // Upscales, only valid if we haven't already scaled in this dimension.
    if (!already_scaled) {
      // Is it a valid bilinear upscale?
      if (samples == 1 && scale <= 1.0) {
        return true;
      }
      // Multi-sample upscale-downscale combination?
      if (scale > samples / 2.0 && scale < samples) {
        return true;
      }
    }
    return false;
  }

  // Make sure that the stages of the scaler pipeline are sane.
  void ValidateScalerStages(
      content::GLHelper::ScalerQuality quality,
      const std::vector<GLHelperScaling::ScalerStage>& scaler_stages,
      const gfx::Size& dst_size,
      const std::string& message) {
    bool previous_error = HasFailure();
    // First, check that the input size for each stage is equal to
    // the output size of the previous stage.
    for (size_t i = 1; i < scaler_stages.size(); i++) {
      EXPECT_EQ(scaler_stages[i - 1].dst_size.width(),
                scaler_stages[i].src_size.width());
      EXPECT_EQ(scaler_stages[i - 1].dst_size.height(),
                scaler_stages[i].src_size.height());
      EXPECT_EQ(scaler_stages[i].src_subrect.x(), 0);
      EXPECT_EQ(scaler_stages[i].src_subrect.y(), 0);
      EXPECT_EQ(scaler_stages[i].src_subrect.width(),
                scaler_stages[i].src_size.width());
      EXPECT_EQ(scaler_stages[i].src_subrect.height(),
                scaler_stages[i].src_size.height());
    }

    // Check the output size matches the destination of the last stage
    EXPECT_EQ(scaler_stages[scaler_stages.size() - 1].dst_size.width(),
              dst_size.width());
    EXPECT_EQ(scaler_stages[scaler_stages.size() - 1].dst_size.height(),
              dst_size.height());

    // Used to verify that up-scales are not attempted after some
    // other scale.
    bool scaled_x = false;
    bool scaled_y = false;

    for (size_t i = 0; i < scaler_stages.size(); i++) {
      // Note: 2.0 means scaling down by 50%
      double x_scale =
          static_cast<double>(scaler_stages[i].src_subrect.width()) /
          static_cast<double>(scaler_stages[i].dst_size.width());
      double y_scale =
          static_cast<double>(scaler_stages[i].src_subrect.height()) /
          static_cast<double>(scaler_stages[i].dst_size.height());

      int x_samples = 0;
      int y_samples = 0;

      // Codify valid scale operations.
      switch (scaler_stages[i].shader) {
        case GLHelperScaling::SHADER_PLANAR:
        case GLHelperScaling::SHADER_YUV_MRT_PASS1:
        case GLHelperScaling::SHADER_YUV_MRT_PASS2:
          EXPECT_TRUE(false) << "Invalid shader.";
          break;

        case GLHelperScaling::SHADER_BILINEAR:
          if (quality != content::GLHelper::SCALER_QUALITY_FAST) {
            x_samples = 1;
            y_samples = 1;
          }
          break;
        case GLHelperScaling::SHADER_BILINEAR2:
          x_samples = 2;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR3:
          x_samples = 3;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR4:
          x_samples = 4;
          y_samples = 1;
          break;
        case GLHelperScaling::SHADER_BILINEAR2X2:
          x_samples = 2;
          y_samples = 2;
          break;
        case GLHelperScaling::SHADER_BICUBIC_UPSCALE:
          if (scaler_stages[i].scale_x) {
            EXPECT_LT(x_scale, 1.0);
            EXPECT_EQ(y_scale, 1.0);
          } else {
            EXPECT_EQ(x_scale, 1.0);
            EXPECT_LT(y_scale, 1.0);
          }
          break;
        case GLHelperScaling::SHADER_BICUBIC_HALF_1D:
          if (scaler_stages[i].scale_x) {
            EXPECT_EQ(x_scale, 2.0);
            EXPECT_EQ(y_scale, 1.0);
          } else {
            EXPECT_EQ(x_scale, 1.0);
            EXPECT_EQ(y_scale, 2.0);
          }
          break;
      }

      if (!scaler_stages[i].scale_x) {
        std::swap(x_samples, y_samples);
      }

      if (x_samples) {
        EXPECT_TRUE(CheckScale(x_scale, x_samples, scaled_x))
            << "x_scale = " << x_scale;
      }
      if (y_samples) {
        EXPECT_TRUE(CheckScale(y_scale, y_samples, scaled_y))
            << "y_scale = " << y_scale;
      }

      if (x_scale != 1.0) {
        scaled_x = true;
      }
      if (y_scale != 1.0) {
        scaled_y = true;
      }
    }

    if (HasFailure() && !previous_error) {
      LOG(ERROR) << "Invalid scaler stages: " << message;
      LOG(ERROR) << "Scaler stages:";
      LOG(ERROR) << PrintStages(scaler_stages);
    }
  }

  // Compares two bitmaps taking color types into account. Checks whether each
  // component of each pixel is no more than |maxdiff| apart. If bitmaps are not
  // similar enough, prints out |truth|, |other|, |source|, |scaler_stages|
  // and |message|.
  void Compare(SkBitmap* truth,
               SkBitmap* other,
               int maxdiff,
               SkBitmap* source,
               const std::vector<GLHelperScaling::ScalerStage>& scaler_stages,
               std::string message) {
    EXPECT_EQ(truth->width(), other->width());
    EXPECT_EQ(truth->height(), other->height());
    bool swizzle = (truth->colorType() == kRGBA_8888_SkColorType &&
                    other->colorType() == kBGRA_8888_SkColorType) ||
                   (truth->colorType() == kBGRA_8888_SkColorType &&
                    other->colorType() == kRGBA_8888_SkColorType);
    EXPECT_TRUE(swizzle || truth->colorType() == other->colorType());
    int bpp = truth->bytesPerPixel();
    for (int x = 0; x < truth->width(); x++) {
      for (int y = 0; y < truth->height(); y++) {
        for (int c = 0; c < bpp; c++) {
          int a = Channel(truth, x, y, c);
          // swizzle when comparing if needed
          int b = swizzle && (c == 0 || c == 2)
                      ? Channel(other, x, y, (c + 2) & 2)
                      : Channel(other, x, y, c);
          EXPECT_NEAR(a, b, maxdiff) << " x=" << x << " y=" << y << " c=" << c
                                     << " " << message;
          if (std::abs(a - b) > maxdiff) {
            LOG(ERROR) << "-------expected--------";
            for (int i = 0; i < bpp; i++) {
              LOG(ERROR) << "Channel " << i << ":";
              PrintChannel(truth, i);
            }
            LOG(ERROR) << "-------actual--------";
            for (int i = 0; i < bpp; i++) {
              LOG(ERROR) << "Channel " << i << ":";
              PrintChannel(other, i);
            }
            if (source) {
              LOG(ERROR) << "-------original--------";
              for (int i = 0; i < source->bytesPerPixel(); i++) {
                LOG(ERROR) << "Channel " << i << ":";
                PrintChannel(source, i);
              }
            }
            LOG(ERROR) << "-----Scaler stages------";
            LOG(ERROR) << PrintStages(scaler_stages);
            return;
          }
        }
      }
    }
  }

  // Get a single R, G, B or A value as a float.
  float ChannelAsFloat(SkBitmap* pixels, int x, int y, int c) {
    return Channel(pixels, x, y, c) / 255.0;
  }

  // Works like a GL_LINEAR lookup on an SkBitmap.
  float Bilinear(SkBitmap* pixels, float x, float y, int c) {
    x -= 0.5;
    y -= 0.5;
    int base_x = static_cast<int>(floorf(x));
    int base_y = static_cast<int>(floorf(y));
    x -= base_x;
    y -= base_y;
    return (ChannelAsFloat(pixels, base_x, base_y, c) * (1 - x) * (1 - y) +
            ChannelAsFloat(pixels, base_x + 1, base_y, c) * x * (1 - y) +
            ChannelAsFloat(pixels, base_x, base_y + 1, c) * (1 - x) * y +
            ChannelAsFloat(pixels, base_x + 1, base_y + 1, c) * x * y);
  }

  // Encodes an RGBA bitmap to grayscale.
  // Reference implementation for
  // GLHelper::CopyToTextureImpl::EncodeTextureAsGrayscale.
  void EncodeToGrayscaleSlow(SkBitmap* input, SkBitmap* output) {
    const float kRGBtoGrayscaleColorWeights[3] = {0.213f, 0.715f, 0.072f};
    CHECK_EQ(kAlpha_8_SkColorType, output->colorType());
    CHECK_EQ(input->width(), output->width());
    CHECK_EQ(input->height(), output->height());
    CHECK_EQ(input->colorType(), kRGBA_8888_SkColorType);

    for (int dst_y = 0; dst_y < output->height(); dst_y++) {
      for (int dst_x = 0; dst_x < output->width(); dst_x++) {
        float c0 = ChannelAsFloat(input, dst_x, dst_y, 0);
        float c1 = ChannelAsFloat(input, dst_x, dst_y, 1);
        float c2 = ChannelAsFloat(input, dst_x, dst_y, 2);
        float value = c0 * kRGBtoGrayscaleColorWeights[0] +
                      c1 * kRGBtoGrayscaleColorWeights[1] +
                      c2 * kRGBtoGrayscaleColorWeights[2];
        SetChannel(
            output, dst_x, dst_y, 0, static_cast<int>(value * 255.0f + 0.5f));
      }
    }
  }

  // Very slow bicubic / bilinear scaler for reference.
  void ScaleSlow(SkBitmap* input,
                 SkBitmap* output,
                 content::GLHelper::ScalerQuality quality) {
    float xscale = static_cast<float>(input->width()) / output->width();
    float yscale = static_cast<float>(input->height()) / output->height();
    float clamped_xscale = xscale < 1.0 ? 1.0 : 1.0 / xscale;
    float clamped_yscale = yscale < 1.0 ? 1.0 : 1.0 / yscale;
    for (int dst_y = 0; dst_y < output->height(); dst_y++) {
      for (int dst_x = 0; dst_x < output->width(); dst_x++) {
        for (int channel = 0; channel < 4; channel++) {
          float dst_x_in_src = (dst_x + 0.5f) * xscale;
          float dst_y_in_src = (dst_y + 0.5f) * yscale;

          float value = 0.0f;
          float sum = 0.0f;
          switch (quality) {
            case content::GLHelper::SCALER_QUALITY_BEST:
              for (int src_y = -10; src_y < input->height() + 10; ++src_y) {
                float coeff_y =
                    Bicubic((src_y + 0.5f - dst_y_in_src) * clamped_yscale);
                if (coeff_y == 0.0f) {
                  continue;
                }
                for (int src_x = -10; src_x < input->width() + 10; ++src_x) {
                  float coeff =
                      coeff_y *
                      Bicubic((src_x + 0.5f - dst_x_in_src) * clamped_xscale);
                  if (coeff == 0.0f) {
                    continue;
                  }
                  sum += coeff;
                  float c = ChannelAsFloat(input, src_x, src_y, channel);
                  value += c * coeff;
                }
              }
              break;

            case content::GLHelper::SCALER_QUALITY_GOOD: {
              int xshift = 0, yshift = 0;
              while ((output->width() << xshift) < input->width()) {
                xshift++;
              }
              while ((output->height() << yshift) < input->height()) {
                yshift++;
              }
              int xmag = 1 << xshift;
              int ymag = 1 << yshift;
              if (xmag == 4 && output->width() * 3 >= input->width()) {
                xmag = 3;
              }
              if (ymag == 4 && output->height() * 3 >= input->height()) {
                ymag = 3;
              }
              for (int x = 0; x < xmag; x++) {
                for (int y = 0; y < ymag; y++) {
                  value += Bilinear(input,
                                    (dst_x * xmag + x + 0.5) * xscale / xmag,
                                    (dst_y * ymag + y + 0.5) * yscale / ymag,
                                    channel);
                  sum += 1.0;
                }
              }
              break;
            }

            case content::GLHelper::SCALER_QUALITY_FAST:
              value = Bilinear(input, dst_x_in_src, dst_y_in_src, channel);
              sum = 1.0;
          }
          value /= sum;
          SetChannel(output,
                     dst_x,
                     dst_y,
                     channel,
                     static_cast<int>(value * 255.0f + 0.5f));
        }
      }
    }
  }

  void FlipSKBitmap(SkBitmap* bitmap) {
    int bpp = bitmap->bytesPerPixel();
    DCHECK(bpp == 4 || bpp == 1);
    int top_line = 0;
    int bottom_line = bitmap->height() - 1;
    while (top_line < bottom_line) {
      for (int x = 0; x < bitmap->width(); x++) {
        bpp == 4 ? std::swap(*bitmap->getAddr32(x, top_line),
                             *bitmap->getAddr32(x, bottom_line))
                 : std::swap(*bitmap->getAddr8(x, top_line),
                             *bitmap->getAddr8(x, bottom_line));
      }
      top_line++;
      bottom_line--;
    }
  }

  // Swaps red and blue channels in each pixel in a 32-bit bitmap.
  void SwizzleSKBitmap(SkBitmap* bitmap) {
    int bpp = bitmap->bytesPerPixel();
    DCHECK(bpp == 4);
    for (int y = 0; y < bitmap->height(); y++) {
      for (int x = 0; x < bitmap->width(); x++) {
        // Swap channels 0 and 2 (red and blue)
        int c0 = Channel(bitmap, x, y, 0);
        int c2 = Channel(bitmap, x, y, 2);
        SetChannel(bitmap, x, y, 2, c0);
        SetChannel(bitmap, x, y, 0, c2);
      }
    }
  }

  // gl_helper scales recursively, so we'll need to do that
  // in the reference implementation too.
  void ScaleSlowRecursive(SkBitmap* input,
                          SkBitmap* output,
                          content::GLHelper::ScalerQuality quality) {
    if (quality == content::GLHelper::SCALER_QUALITY_FAST ||
        quality == content::GLHelper::SCALER_QUALITY_GOOD) {
      ScaleSlow(input, output, quality);
      return;
    }

    float xscale = static_cast<float>(output->width()) / input->width();

    // This corresponds to all the operations we can do directly.
    float yscale = static_cast<float>(output->height()) / input->height();
    if ((xscale == 1.0f && yscale == 1.0f) ||
        (xscale == 0.5f && yscale == 1.0f) ||
        (xscale == 1.0f && yscale == 0.5f) ||
        (xscale >= 1.0f && yscale == 1.0f) ||
        (xscale == 1.0f && yscale >= 1.0f)) {
      ScaleSlow(input, output, quality);
      return;
    }

    // Now we break the problem down into smaller pieces, using the
    // operations available.
    int xtmp = input->width();
    int ytmp = input->height();

    if (output->height() != input->height()) {
      ytmp = output->height();
      while (ytmp < input->height() && ytmp * 2 != input->height()) {
        ytmp += ytmp;
      }
    } else {
      xtmp = output->width();
      while (xtmp < input->width() && xtmp * 2 != input->width()) {
        xtmp += xtmp;
      }
    }

    SkBitmap tmp;
    tmp.allocN32Pixels(xtmp, ytmp);

    ScaleSlowRecursive(input, &tmp, quality);
    ScaleSlowRecursive(&tmp, output, quality);
  }

  // Creates an RGBA SkBitmap
  scoped_ptr<SkBitmap> CreateTestBitmap(int width,
                                        int height,
                                        int test_pattern) {
    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    bitmap->allocPixels(SkImageInfo::Make(
        width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));

    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        switch (test_pattern) {
          case 0:  // Smooth test pattern
            SetChannel(bitmap.get(), x, y, 0, x * 10);
            SetChannel(bitmap.get(), x, y, 0, y == 0 ? x * 50 : x * 10);
            SetChannel(bitmap.get(), x, y, 1, y * 10);
            SetChannel(bitmap.get(), x, y, 2, (x + y) * 10);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
          case 1:  // Small blocks
            SetChannel(bitmap.get(), x, y, 0, x & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 1, y & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 2, (x + y) & 1 ? 255 : 0);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
          case 2:  // Medium blocks
            SetChannel(bitmap.get(), x, y, 0, 10 + x / 2 * 50);
            SetChannel(bitmap.get(), x, y, 1, 10 + y / 3 * 50);
            SetChannel(bitmap.get(), x, y, 2, (x + y) / 5 * 50 + 5);
            SetChannel(bitmap.get(), x, y, 3, 255);
            break;
        }
      }
    }
    return bitmap.Pass();
  }

  // Binds texture and framebuffer and loads the bitmap pixels into the texture.
  void BindTextureAndFrameBuffer(WebGLId texture,
                                 WebGLId framebuffer,
                                 SkBitmap* bitmap,
                                 int width,
                                 int height) {
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    context_->bindTexture(GL_TEXTURE_2D, texture);
    context_->texImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         width,
                         height,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         bitmap->getPixels());
  }

  // Create a test image, transform it using
  // GLHelper::CropScaleReadbackAndCleanTexture and a reference implementation
  // and compare the results.
  void TestCropScaleReadbackAndCleanTexture(int xsize,
                                            int ysize,
                                            int scaled_xsize,
                                            int scaled_ysize,
                                            int test_pattern,
                                            SkColorType out_color_type,
                                            bool swizzle,
                                            size_t quality_index) {
    DCHECK(out_color_type == kAlpha_8_SkColorType ||
           out_color_type == kRGBA_8888_SkColorType ||
           out_color_type == kBGRA_8888_SkColorType);
    WebGLId src_texture = context_->createTexture();
    WebGLId framebuffer = context_->createFramebuffer();
    scoped_ptr<SkBitmap> input_pixels =
        CreateTestBitmap(xsize, ysize, test_pattern).Pass();
    BindTextureAndFrameBuffer(
        src_texture, framebuffer, input_pixels.get(), xsize, ysize);

    std::string message = base::StringPrintf(
        "input size: %dx%d "
        "output size: %dx%d "
        "pattern: %d , quality: %s, "
        "out_color_type: %d",
        xsize,
        ysize,
        scaled_xsize,
        scaled_ysize,
        test_pattern,
        kQualityNames[quality_index],
        out_color_type);

    // Transform the bitmap using GLHelper::CropScaleReadbackAndCleanTexture.
    SkBitmap output_pixels;
    output_pixels.allocPixels(SkImageInfo::Make(
        scaled_xsize, scaled_ysize, out_color_type, kPremul_SkAlphaType));
    base::RunLoop run_loop;
    gfx::Size encoded_texture_size;
    helper_->CropScaleReadbackAndCleanTexture(
        src_texture,
        gfx::Size(xsize, ysize),
        gfx::Rect(xsize, ysize),
        gfx::Size(scaled_xsize, scaled_ysize),
        static_cast<unsigned char*>(output_pixels.getPixels()),
        out_color_type,
        base::Bind(&callcallback, run_loop.QuitClosure()),
        kQualities[quality_index]);
    run_loop.Run();
    // CropScaleReadbackAndCleanTexture flips the pixels. Flip them back.
    FlipSKBitmap(&output_pixels);

    // If the bitmap shouldn't have changed - compare against input.
    if (xsize == scaled_xsize && ysize == scaled_ysize &&
        out_color_type != kAlpha_8_SkColorType) {
      const std::vector<GLHelperScaling::ScalerStage> dummy_stages;
      Compare(input_pixels.get(),
              &output_pixels,
              0,
              NULL,
              dummy_stages,
              message + " comparing against input");
      return;
    }

    // Now transform the bitmap using the reference implementation.
    SkBitmap scaled_pixels;
    scaled_pixels.allocPixels(SkImageInfo::Make(scaled_xsize,
                                                scaled_ysize,
                                                kRGBA_8888_SkColorType,
                                                kPremul_SkAlphaType));
    SkBitmap truth_pixels;
    // Step 1: Scale
    ScaleSlowRecursive(
        input_pixels.get(), &scaled_pixels, kQualities[quality_index]);
    // Step 2: Encode to grayscale if needed.
    if (out_color_type == kAlpha_8_SkColorType) {
      truth_pixels.allocPixels(SkImageInfo::Make(
          scaled_xsize, scaled_ysize, out_color_type, kPremul_SkAlphaType));
      EncodeToGrayscaleSlow(&scaled_pixels, &truth_pixels);
    } else {
      truth_pixels = scaled_pixels;
    }

    // Now compare the results.
    SkAutoLockPixels lock_input(truth_pixels);
    const std::vector<GLHelperScaling::ScalerStage> dummy_stages;
    Compare(&truth_pixels,
            &output_pixels,
            2,
            input_pixels.get(),
            dummy_stages,
            message + " comparing against transformed/scaled");

    context_->deleteTexture(src_texture);
    context_->deleteFramebuffer(framebuffer);
  }

  // Scaling test: Create a test image, scale it using GLHelperScaling
  // and a reference implementation and compare the results.
  void TestScale(int xsize,
                 int ysize,
                 int scaled_xsize,
                 int scaled_ysize,
                 int test_pattern,
                 size_t quality_index,
                 bool flip) {
    WebGLId src_texture = context_->createTexture();
    WebGLId framebuffer = context_->createFramebuffer();
    scoped_ptr<SkBitmap> input_pixels =
        CreateTestBitmap(xsize, ysize, test_pattern).Pass();
    BindTextureAndFrameBuffer(
        src_texture, framebuffer, input_pixels.get(), xsize, ysize);

    std::string message = base::StringPrintf(
        "input size: %dx%d "
        "output size: %dx%d "
        "pattern: %d quality: %s",
        xsize,
        ysize,
        scaled_xsize,
        scaled_ysize,
        test_pattern,
        kQualityNames[quality_index]);

    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(kQualities[quality_index],
                                         gfx::Size(xsize, ysize),
                                         gfx::Rect(0, 0, xsize, ysize),
                                         gfx::Size(scaled_xsize, scaled_ysize),
                                         flip,
                                         false,
                                         &stages);
    ValidateScalerStages(kQualities[quality_index],
                         stages,
                         gfx::Size(scaled_xsize, scaled_ysize),
                         message);

    WebGLId dst_texture =
        helper_->CopyAndScaleTexture(src_texture,
                                     gfx::Size(xsize, ysize),
                                     gfx::Size(scaled_xsize, scaled_ysize),
                                     flip,
                                     kQualities[quality_index]);

    SkBitmap output_pixels;
    output_pixels.allocPixels(SkImageInfo::Make(scaled_xsize,
                                                scaled_ysize,
                                                kRGBA_8888_SkColorType,
                                                kPremul_SkAlphaType));

    helper_->ReadbackTextureSync(
        dst_texture,
        gfx::Rect(0, 0, scaled_xsize, scaled_ysize),
        static_cast<unsigned char*>(output_pixels.getPixels()),
        kRGBA_8888_SkColorType);
    if (flip) {
      // Flip the pixels back.
      FlipSKBitmap(&output_pixels);
    }

    // If the bitmap shouldn't have changed - compare against input.
    if (xsize == scaled_xsize && ysize == scaled_ysize) {
      Compare(input_pixels.get(),
              &output_pixels,
              0,
              NULL,
              stages,
              message + " comparing against input");
      return;
    }

    // Now scale the bitmap using the reference implementation.
    SkBitmap truth_pixels;
    truth_pixels.allocPixels(SkImageInfo::Make(scaled_xsize,
                                               scaled_ysize,
                                               kRGBA_8888_SkColorType,
                                               kPremul_SkAlphaType));
    ScaleSlowRecursive(
        input_pixels.get(), &truth_pixels, kQualities[quality_index]);
    Compare(&truth_pixels,
            &output_pixels,
            2,
            input_pixels.get(),
            stages,
            message + " comparing against scaled");

    context_->deleteTexture(src_texture);
    context_->deleteTexture(dst_texture);
    context_->deleteFramebuffer(framebuffer);
  }

  // Create a scaling pipeline and check that it is made up of
  // valid scaling operations.
  void TestScalerPipeline(size_t quality,
                          int xsize,
                          int ysize,
                          int dst_xsize,
                          int dst_ysize) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(kQualities[quality],
                                         gfx::Size(xsize, ysize),
                                         gfx::Rect(0, 0, xsize, ysize),
                                         gfx::Size(dst_xsize, dst_ysize),
                                         false,
                                         false,
                                         &stages);
    ValidateScalerStages(kQualities[quality],
                         stages,
                         gfx::Size(dst_xsize, dst_ysize),
                         base::StringPrintf(
                             "input size: %dx%d "
                             "output size: %dx%d "
                             "quality: %s",
                             xsize,
                             ysize,
                             dst_xsize,
                             dst_ysize,
                             kQualityNames[quality]));
  }

  // Create a scaling pipeline and make sure that the steps
  // are exactly the steps we expect.
  void CheckPipeline(content::GLHelper::ScalerQuality quality,
                     int xsize,
                     int ysize,
                     int dst_xsize,
                     int dst_ysize,
                     const std::string& description) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ComputeScalerStages(quality,
                                         gfx::Size(xsize, ysize),
                                         gfx::Rect(0, 0, xsize, ysize),
                                         gfx::Size(dst_xsize, dst_ysize),
                                         false,
                                         false,
                                         &stages);
    ValidateScalerStages(content::GLHelper::SCALER_QUALITY_GOOD,
                         stages,
                         gfx::Size(dst_xsize, dst_ysize),
                         "");
    EXPECT_EQ(PrintStages(stages), description);
  }

  // Note: Left/Right means Top/Bottom when used for Y dimension.
  enum Margin {
    MarginLeft,
    MarginMiddle,
    MarginRight,
    MarginInvalid,
  };

  static Margin NextMargin(Margin m) {
    switch (m) {
      case MarginLeft:
        return MarginMiddle;
      case MarginMiddle:
        return MarginRight;
      case MarginRight:
        return MarginInvalid;
      default:
        return MarginInvalid;
    }
  }

  int compute_margin(int insize, int outsize, Margin m) {
    int available = outsize - insize;
    switch (m) {
      default:
        EXPECT_TRUE(false) << "This should not happen.";
        return 0;
      case MarginLeft:
        return 0;
      case MarginMiddle:
        return (available / 2) & ~1;
      case MarginRight:
        return available;
    }
  }

  // Convert 0.0 - 1.0 to 0 - 255
  int float_to_byte(float v) {
    int ret = static_cast<int>(floorf(v * 255.0f + 0.5f));
    if (ret < 0) {
      return 0;
    }
    if (ret > 255) {
      return 255;
    }
    return ret;
  }

  static void callcallback(const base::Callback<void()>& callback,
                           bool result) {
    callback.Run();
  }

  void PrintPlane(unsigned char* plane, int xsize, int stride, int ysize) {
    for (int y = 0; y < ysize; y++) {
      std::string formatted;
      for (int x = 0; x < xsize; x++) {
        formatted.append(base::StringPrintf("%3d, ", plane[y * stride + x]));
      }
      LOG(ERROR) << formatted << "   (" << (plane + y * stride) << ")";
    }
  }

  // Compare two planes make sure that each component of each pixel
  // is no more than |maxdiff| apart.
  void ComparePlane(unsigned char* truth,
                    int truth_stride,
                    unsigned char* other,
                    int other_stride,
                    int maxdiff,
                    int xsize,
                    int ysize,
                    SkBitmap* source,
                    std::string message) {
    for (int x = 0; x < xsize; x++) {
      for (int y = 0; y < ysize; y++) {
        int a = other[y * other_stride + x];
        int b = truth[y * truth_stride + x];
        EXPECT_NEAR(a, b, maxdiff) << " x=" << x << " y=" << y << " "
                                   << message;
        if (std::abs(a - b) > maxdiff) {
          LOG(ERROR) << "-------expected--------";
          PrintPlane(truth, xsize, truth_stride, ysize);
          LOG(ERROR) << "-------actual--------";
          PrintPlane(other, xsize, other_stride, ysize);
          if (source) {
            LOG(ERROR) << "-------before yuv conversion: red--------";
            PrintChannel(source, 0);
            LOG(ERROR) << "-------before yuv conversion: green------";
            PrintChannel(source, 1);
            LOG(ERROR) << "-------before yuv conversion: blue-------";
            PrintChannel(source, 2);
          }
          return;
        }
      }
    }
  }

  void DrawGridToBitmap(int w, int h,
                        SkColor background_color,
                        SkColor grid_color,
                        int grid_pitch,
                        int grid_width,
                        SkBitmap& bmp) {
    ASSERT_GT(grid_pitch, 0);
    ASSERT_GT(grid_width, 0);
    ASSERT_NE(background_color, grid_color);

    for (int y = 0; y < h; ++y) {
      bool y_on_grid = ((y % grid_pitch) < grid_width);

      for (int x = 0; x < w; ++x) {
        bool on_grid = (y_on_grid || ((x % grid_pitch) < grid_width));

        if (bmp.colorType() == kRGBA_8888_SkColorType ||
            bmp.colorType() == kBGRA_8888_SkColorType) {
          *bmp.getAddr32(x, y) = (on_grid ? grid_color : background_color);
        } else if (bmp.colorType() == kRGB_565_SkColorType) {
          *bmp.getAddr16(x, y) = (on_grid ? grid_color : background_color);
        }
      }
    }
  }

  void DrawCheckerToBitmap(int w, int h,
                           SkColor color1, SkColor color2,
                           int rect_w, int rect_h,
                           SkBitmap& bmp) {
    ASSERT_GT(rect_w, 0);
    ASSERT_GT(rect_h, 0);
    ASSERT_NE(color1, color2);

    for (int y = 0; y < h; ++y) {
      bool y_bit = (((y / rect_h) & 0x1) == 0);

      for (int x = 0; x < w; ++x) {
        bool x_bit = (((x / rect_w) & 0x1) == 0);

        bool use_color2 = (x_bit != y_bit);  // xor
        if (bmp.colorType() == kRGBA_8888_SkColorType ||
            bmp.colorType() == kBGRA_8888_SkColorType) {
          *bmp.getAddr32(x, y) = (use_color2 ? color2 : color1);
        } else if (bmp.colorType() == kRGB_565_SkColorType) {
          *bmp.getAddr16(x, y) = (use_color2 ? color2 : color1);
        }
      }
    }
  }

  bool ColorComponentsClose(SkColor component1,
                            SkColor component2,
                            SkColorType color_type) {
    int c1 = static_cast<int>(component1);
    int c2 = static_cast<int>(component2);
    bool result = false;
    switch (color_type) {
      case kRGBA_8888_SkColorType:
      case kBGRA_8888_SkColorType:
        result = (std::abs(c1 - c2) == 0);
        break;
      case kRGB_565_SkColorType:
        result = (std::abs(c1 - c2) <= 7);
        break;
      default:
        break;
    }
    return result;
  }

  bool ColorsClose(SkColor color1, SkColor color2, SkColorType color_type) {
    bool red = ColorComponentsClose(SkColorGetR(color1),
                                    SkColorGetR(color2), color_type);
    bool green = ColorComponentsClose(SkColorGetG(color1),
                                        SkColorGetG(color2), color_type);
    bool blue = ColorComponentsClose(SkColorGetB(color1),
                                     SkColorGetB(color2), color_type);
    bool alpha = ColorComponentsClose(SkColorGetA(color1),
                                      SkColorGetA(color2), color_type);
    if (color_type == kRGB_565_SkColorType) {
      return red && blue && green;
    }
    return red && blue && green && alpha;
  }

  bool IsEqual(const SkBitmap& bmp1, const SkBitmap& bmp2) {
    if (bmp1.isNull() && bmp2.isNull())
      return true;
    if (bmp1.width() != bmp2.width() ||
        bmp1.height() != bmp2.height()) {
        LOG(ERROR) << "Bitmap geometry check failure";
        return false;
    }
    if (bmp1.colorType() != bmp2.colorType())
      return false;

    SkAutoLockPixels lock1(bmp1);
    SkAutoLockPixels lock2(bmp2);
    if (!bmp1.getPixels() || !bmp2.getPixels()) {
      LOG(ERROR) << "Empty Bitmap!";
      return false;
    }
    for (int y = 0; y < bmp1.height(); ++y) {
      for (int x = 0; x < bmp1.width(); ++x) {
        if (!ColorsClose(bmp1.getColor(x,y),
                         bmp2.getColor(x,y),
                         bmp1.colorType())) {
          LOG(ERROR) << "Bitmap color comparision failure";
          return false;
        }
      }
    }
    return true;
  }

  void BindAndAttachTextureWithPixels(GLuint src_texture,
                                      SkColorType color_type,
                                      const gfx::Size& src_size,
                                      const SkBitmap& input_pixels) {
    context_->bindTexture(GL_TEXTURE_2D, src_texture);
    GLenum format = 0;
    switch (color_type) {
      case kBGRA_8888_SkColorType:
        format = GL_BGRA_EXT;
        break;
      case kRGBA_8888_SkColorType:
        format = GL_RGBA;
        break;
      case kRGB_565_SkColorType:
        format = GL_RGB;
        break;
      default:
        NOTREACHED();
    }
    GLenum type = (color_type == kRGB_565_SkColorType) ?
                  GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;
    context_->texImage2D(GL_TEXTURE_2D,
                         0,
                         format,
                         src_size.width(),
                         src_size.height(),
                         0,
                         format,
                         type,
                         input_pixels.getPixels());
  }

  void ReadBackTexture(GLuint src_texture,
                       const gfx::Size& src_size,
                       unsigned char* pixels,
                       SkColorType color_type,
                       bool async) {
    if (async) {
      base::RunLoop run_loop;
      helper_->ReadbackTextureAsync(src_texture,
                                    src_size,
                                    pixels,
                                    color_type,
                                    base::Bind(&callcallback,
                                               run_loop.QuitClosure()));
      run_loop.Run();
    } else {
      helper_->ReadbackTextureSync(src_texture,
                                   gfx::Rect(src_size),
                                   pixels,
                                   color_type);
    }
  }
  // Test basic format readback.
  bool TestTextureFormatReadback(const gfx::Size& src_size,
                         SkColorType color_type,
                         bool async) {
    SkImageInfo info =
        SkImageInfo::Make(src_size.width(),
                          src_size.height(),
                          color_type,
                          kPremul_SkAlphaType);
    if (!helper_->IsReadbackConfigSupported(color_type)) {
      LOG(INFO) << "Skipping test format not supported" << color_type;
      return true;
    }
    WebGLId src_texture = context_->createTexture();
    SkBitmap input_pixels;
    input_pixels.allocPixels(info);
    // Test Pattern-1, Fill with Plain color pattern.
    // Erase the input bitmap with red color.
    input_pixels.eraseColor(SK_ColorRED);
    BindAndAttachTextureWithPixels(src_texture,
                                   color_type,
                                   src_size,
                                   input_pixels);
    SkBitmap output_pixels;
    output_pixels.allocPixels(info);
    // Initialize the output bitmap with Green color.
    // When the readback is over output bitmap should have the red color.
    output_pixels.eraseColor(SK_ColorGREEN);
    uint8* pixels = static_cast<uint8*>(output_pixels.getPixels());
    ReadBackTexture(src_texture, src_size, pixels, color_type, async);
    bool result = IsEqual(input_pixels, output_pixels);
    if (!result) {
      LOG(ERROR) << "Bitmap comparision failure Pattern-1";
      return false;
    }
    const int rect_w = 10, rect_h = 4, src_grid_pitch = 10, src_grid_width = 4;
    const SkColor color1 = SK_ColorRED, color2 = SK_ColorBLUE;
    // Test Pattern-2, Fill with Grid Pattern.
    DrawGridToBitmap(src_size.width(), src_size.height(),
                   color2, color1,
                   src_grid_pitch, src_grid_width,
                   input_pixels);
    BindAndAttachTextureWithPixels(src_texture,
                                   color_type,
                                   src_size,
                                   input_pixels);
    ReadBackTexture(src_texture, src_size, pixels, color_type, async);
    result = IsEqual(input_pixels, output_pixels);
    if (!result) {
      LOG(ERROR) << "Bitmap comparision failure Pattern-2";
      return false;
    }
    // Test Pattern-3, Fill with CheckerBoard Pattern.
    DrawCheckerToBitmap(src_size.width(),
                    src_size.height(),
                    color1,
                    color2, rect_w, rect_h, input_pixels);
    BindAndAttachTextureWithPixels(src_texture,
                                   color_type,
                                   src_size,
                                   input_pixels);
    ReadBackTexture(src_texture, src_size, pixels, color_type, async);
    result = IsEqual(input_pixels, output_pixels);
    if (!result) {
      LOG(ERROR) << "Bitmap comparision failure Pattern-3";
      return false;
    }
    context_->deleteTexture(src_texture);
    if (HasFailure()) {
      return false;
    }
    return true;
  }

  // YUV readback test. Create a test pattern, convert to YUV
  // with reference implementation and compare to what gl_helper
  // returns.
  void TestYUVReadback(int xsize,
                       int ysize,
                       int output_xsize,
                       int output_ysize,
                       int xmargin,
                       int ymargin,
                       int test_pattern,
                       bool flip,
                       bool use_mrt,
                       content::GLHelper::ScalerQuality quality) {
    WebGLId src_texture = context_->createTexture();
    SkBitmap input_pixels;
    input_pixels.allocN32Pixels(xsize, ysize);

    for (int x = 0; x < xsize; ++x) {
      for (int y = 0; y < ysize; ++y) {
        switch (test_pattern) {
          case 0:  // Smooth test pattern
            SetChannel(&input_pixels, x, y, 0, x * 10);
            SetChannel(&input_pixels, x, y, 1, y * 10);
            SetChannel(&input_pixels, x, y, 2, (x + y) * 10);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
          case 1:  // Small blocks
            SetChannel(&input_pixels, x, y, 0, x & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 1, y & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 2, (x + y) & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
          case 2:  // Medium blocks
            SetChannel(&input_pixels, x, y, 0, 10 + x / 2 * 50);
            SetChannel(&input_pixels, x, y, 1, 10 + y / 3 * 50);
            SetChannel(&input_pixels, x, y, 2, (x + y) / 5 * 50 + 5);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
        }
      }
    }

    context_->bindTexture(GL_TEXTURE_2D, src_texture);
    context_->texImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         xsize,
                         ysize,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         input_pixels.getPixels());

    gpu::Mailbox mailbox;
    context_->genMailboxCHROMIUM(mailbox.name);
    EXPECT_FALSE(mailbox.IsZero());
    context_->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint32 sync_point = context_->insertSyncPoint();

    std::string message = base::StringPrintf(
        "input size: %dx%d "
        "output size: %dx%d "
        "margin: %dx%d "
        "pattern: %d %s %s",
        xsize,
        ysize,
        output_xsize,
        output_ysize,
        xmargin,
        ymargin,
        test_pattern,
        flip ? "flip" : "noflip",
        flip ? "mrt" : "nomrt");
    scoped_ptr<ReadbackYUVInterface> yuv_reader(
        helper_->CreateReadbackPipelineYUV(
            quality,
            gfx::Size(xsize, ysize),
            gfx::Rect(0, 0, xsize, ysize),
            gfx::Size(xsize, ysize),
            flip,
            use_mrt));

    scoped_refptr<media::VideoFrame> output_frame =
        media::VideoFrame::CreateFrame(
            media::VideoFrame::YV12,
            // The coded size of the output frame is rounded up to the next
            // 16-byte boundary.  This tests that the readback is being
            // positioned inside the frame's visible region, and not dependent
            // on its coded size.
            gfx::Size((output_xsize + 15) & ~15, (output_ysize + 15) & ~15),
            gfx::Rect(0, 0, output_xsize, output_ysize),
            gfx::Size(output_xsize, output_ysize),
            base::TimeDelta::FromSeconds(0));
    scoped_refptr<media::VideoFrame> truth_frame =
        media::VideoFrame::CreateFrame(
            media::VideoFrame::YV12,
            gfx::Size(output_xsize, output_ysize),
            gfx::Rect(0, 0, output_xsize, output_ysize),
            gfx::Size(output_xsize, output_ysize),
            base::TimeDelta::FromSeconds(0));

    base::RunLoop run_loop;
    yuv_reader->ReadbackYUV(mailbox,
                            sync_point,
                            output_frame.get(),
                            gfx::Point(xmargin, ymargin),
                            base::Bind(&callcallback, run_loop.QuitClosure()));
    run_loop.Run();

    if (flip) {
      FlipSKBitmap(&input_pixels);
    }

    unsigned char* Y = truth_frame->visible_data(media::VideoFrame::kYPlane);
    unsigned char* U = truth_frame->visible_data(media::VideoFrame::kUPlane);
    unsigned char* V = truth_frame->visible_data(media::VideoFrame::kVPlane);
    int32 y_stride = truth_frame->stride(media::VideoFrame::kYPlane);
    int32 u_stride = truth_frame->stride(media::VideoFrame::kUPlane);
    int32 v_stride = truth_frame->stride(media::VideoFrame::kVPlane);
    memset(Y, 0x00, y_stride * output_ysize);
    memset(U, 0x80, u_stride * output_ysize / 2);
    memset(V, 0x80, v_stride * output_ysize / 2);

    const float kRGBtoYColorWeights[] = {0.257f, 0.504f, 0.098f, 0.0625f};
    const float kRGBtoUColorWeights[] = {-0.148f, -0.291f, 0.439f, 0.5f};
    const float kRGBtoVColorWeights[] = {0.439f, -0.368f, -0.071f, 0.5f};

    for (int y = 0; y < ysize; y++) {
      for (int x = 0; x < xsize; x++) {
        Y[(y + ymargin) * y_stride + x + xmargin] = float_to_byte(
            ChannelAsFloat(&input_pixels, x, y, 0) * kRGBtoYColorWeights[0] +
            ChannelAsFloat(&input_pixels, x, y, 1) * kRGBtoYColorWeights[1] +
            ChannelAsFloat(&input_pixels, x, y, 2) * kRGBtoYColorWeights[2] +
            kRGBtoYColorWeights[3]);
      }
    }

    for (int y = 0; y < ysize / 2; y++) {
      for (int x = 0; x < xsize / 2; x++) {
        U[(y + ymargin / 2) * u_stride + x + xmargin / 2] =
            float_to_byte(Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 0) *
                              kRGBtoUColorWeights[0] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 1) *
                              kRGBtoUColorWeights[1] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 2) *
                              kRGBtoUColorWeights[2] +
                          kRGBtoUColorWeights[3]);
        V[(y + ymargin / 2) * v_stride + x + xmargin / 2] =
            float_to_byte(Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 0) *
                              kRGBtoVColorWeights[0] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 1) *
                              kRGBtoVColorWeights[1] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 2) *
                              kRGBtoVColorWeights[2] +
                          kRGBtoVColorWeights[3]);
      }
    }

    ComparePlane(Y,
                 y_stride,
                 output_frame->visible_data(media::VideoFrame::kYPlane),
                 output_frame->stride(media::VideoFrame::kYPlane),
                 2,
                 output_xsize,
                 output_ysize,
                 &input_pixels,
                 message + " Y plane");
    ComparePlane(U,
                 u_stride,
                 output_frame->visible_data(media::VideoFrame::kUPlane),
                 output_frame->stride(media::VideoFrame::kUPlane),
                 2,
                 output_xsize / 2,
                 output_ysize / 2,
                 &input_pixels,
                 message + " U plane");
    ComparePlane(V,
                 v_stride,
                 output_frame->visible_data(media::VideoFrame::kVPlane),
                 output_frame->stride(media::VideoFrame::kVPlane),
                 2,
                 output_xsize / 2,
                 output_ysize / 2,
                 &input_pixels,
                 message + " V plane");

    context_->deleteTexture(src_texture);
  }

  void TestAddOps(int src, int dst, bool scale_x, bool allow3) {
    std::deque<GLHelperScaling::ScaleOp> ops;
    GLHelperScaling::ScaleOp::AddOps(src, dst, scale_x, allow3, &ops);
    // Scale factor 3 is a special case.
    // It is currently only allowed by itself.
    if (allow3 && dst * 3 >= src && dst * 2 < src) {
      EXPECT_EQ(ops[0].scale_factor, 3);
      EXPECT_EQ(ops.size(), 1U);
      EXPECT_EQ(ops[0].scale_x, scale_x);
      EXPECT_EQ(ops[0].scale_size, dst);
      return;
    }

    for (size_t i = 0; i < ops.size(); i++) {
      EXPECT_EQ(ops[i].scale_x, scale_x);
      if (i == 0) {
        // Only the first op is allowed to be a scale up.
        // (Scaling up *after* scaling down would make it fuzzy.)
        EXPECT_TRUE(ops[0].scale_factor == 0 || ops[0].scale_factor == 2);
      } else {
        // All other operations must be 50% downscales.
        EXPECT_EQ(ops[i].scale_factor, 2);
      }
    }
    // Check that the scale factors make sense and add up.
    int tmp = dst;
    for (int i = static_cast<int>(ops.size() - 1); i >= 0; i--) {
      EXPECT_EQ(tmp, ops[i].scale_size);
      if (ops[i].scale_factor == 0) {
        EXPECT_EQ(i, 0);
        EXPECT_GT(tmp, src);
        tmp = src;
      } else {
        tmp *= ops[i].scale_factor;
      }
    }
    EXPECT_EQ(tmp, src);
  }

  void CheckPipeline2(int xsize,
                      int ysize,
                      int dst_xsize,
                      int dst_ysize,
                      const std::string& description) {
    std::vector<GLHelperScaling::ScalerStage> stages;
    helper_scaling_->ConvertScalerOpsToScalerStages(
        content::GLHelper::SCALER_QUALITY_GOOD,
        gfx::Size(xsize, ysize),
        gfx::Rect(0, 0, xsize, ysize),
        gfx::Size(dst_xsize, dst_ysize),
        false,
        false,
        &x_ops_,
        &y_ops_,
        &stages);
    EXPECT_EQ(x_ops_.size(), 0U);
    EXPECT_EQ(y_ops_.size(), 0U);
    ValidateScalerStages(content::GLHelper::SCALER_QUALITY_GOOD,
                         stages,
                         gfx::Size(dst_xsize, dst_ysize),
                         "");
    EXPECT_EQ(PrintStages(stages), description);
  }

  void CheckOptimizationsTest() {
    // Basic upscale. X and Y should be combined into one pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 2000));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 2000));
    CheckPipeline2(1024, 768, 2000, 2000, "1024x768 -> 2000x2000 bilinear\n");

    // X scaled 1/2, Y upscaled, should still be one pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 512));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 2000));
    CheckPipeline2(1024, 768, 512, 2000, "1024x768 -> 512x2000 bilinear\n");

    // X upscaled, Y scaled 1/2, one bilinear pass
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 2000));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 384));
    CheckPipeline2(1024, 768, 2000, 384, "1024x768 -> 2000x384 bilinear\n");

    // X scaled 1/2, Y scaled 1/2, one bilinear pass
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 512));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 384));
    CheckPipeline2(1024, 768, 512, 384, "1024x768 -> 512x384 bilinear\n");

    // X scaled 1/2, Y scaled to 60%, one bilinear2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 50));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100, 100, 50, 60, "100x100 -> 50x60 bilinear2 Y\n");

    // X scaled to 60%, Y scaled 1/2, one bilinear2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 50));
    CheckPipeline2(100, 100, 60, 50, "100x100 -> 60x50 bilinear2 X\n");

    // X scaled to 60%, Y scaled 60%, one bilinear2x2 pass.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100, 100, 60, 60, "100x100 -> 60x60 bilinear2x2\n");

    // X scaled to 40%, Y scaled 40%, two bilinear3 passes.
    x_ops_.push_back(GLHelperScaling::ScaleOp(3, true, 40));
    y_ops_.push_back(GLHelperScaling::ScaleOp(3, false, 40));
    CheckPipeline2(100,
                   100,
                   40,
                   40,
                   "100x100 -> 100x40 bilinear3 Y\n"
                   "100x40 -> 40x40 bilinear3 X\n");

    // X scaled to 60%, Y scaled 40%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(3, false, 40));
    CheckPipeline2(100,
                   100,
                   60,
                   40,
                   "100x100 -> 100x40 bilinear3 Y\n"
                   "100x40 -> 60x40 bilinear2 X\n");

    // X scaled to 40%, Y scaled 60%
    x_ops_.push_back(GLHelperScaling::ScaleOp(3, true, 40));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    CheckPipeline2(100,
                   100,
                   40,
                   60,
                   "100x100 -> 100x60 bilinear2 Y\n"
                   "100x60 -> 40x60 bilinear3 X\n");

    // X scaled to 30%, Y scaled 30%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 120));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 60));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 30));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100,
                   100,
                   30,
                   30,
                   "100x100 -> 100x30 bilinear4 Y\n"
                   "100x30 -> 30x30 bilinear4 X\n");

    // X scaled to 50%, Y scaled 30%
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 50));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100, 100, 50, 30, "100x100 -> 50x30 bilinear4 Y\n");

    // X scaled to 150%, Y scaled 30%
    // Note that we avoid combinding X and Y passes
    // as that would probably be LESS efficient here.
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 150));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 120));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 60));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 30));
    CheckPipeline2(100,
                   100,
                   150,
                   30,
                   "100x100 -> 100x30 bilinear4 Y\n"
                   "100x30 -> 150x30 bilinear\n");

    // X scaled to 1%, Y scaled 1%
    x_ops_.push_back(GLHelperScaling::ScaleOp(0, true, 128));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 64));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 32));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 16));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 8));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 4));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 2));
    x_ops_.push_back(GLHelperScaling::ScaleOp(2, true, 1));
    y_ops_.push_back(GLHelperScaling::ScaleOp(0, false, 128));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 64));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 32));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 16));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 8));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 4));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 2));
    y_ops_.push_back(GLHelperScaling::ScaleOp(2, false, 1));
    CheckPipeline2(100,
                   100,
                   1,
                   1,
                   "100x100 -> 100x32 bilinear4 Y\n"
                   "100x32 -> 100x4 bilinear4 Y\n"
                   "100x4 -> 64x1 bilinear2x2\n"
                   "64x1 -> 8x1 bilinear4 X\n"
                   "8x1 -> 1x1 bilinear4 X\n");
  }

  scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> context_;
  gpu::ContextSupport* context_support_;
  scoped_ptr<content::GLHelper> helper_;
  scoped_ptr<content::GLHelperScaling> helper_scaling_;
  std::deque<GLHelperScaling::ScaleOp> x_ops_, y_ops_;
};

class GLHelperPixelTest : public GLHelperTest {
 private:
  gfx::DisableNullDrawGLBindings enable_pixel_output_;
};

TEST_F(GLHelperTest, RGBASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kRGBA_8888_SkColorType,
                                          false);
  EXPECT_EQ(result, true);
}


TEST_F(GLHelperTest, BGRASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kBGRA_8888_SkColorType,
                                          false);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperTest, RGB565SyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kRGB_565_SkColorType,
                                          false);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperTest, RGBAASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kRGBA_8888_SkColorType,
                                          true);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperTest, BGRAASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kBGRA_8888_SkColorType,
                                          true);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperTest, RGB565ASyncReadbackTest) {
  const int kTestSize = 64;
  bool result = TestTextureFormatReadback(gfx::Size(kTestSize,kTestSize),
                                          kRGB_565_SkColorType,
                                          true);
  EXPECT_EQ(result, true);
}

TEST_F(GLHelperPixelTest, YUVReadbackOptTest) {
  // This test uses the cb_command tracing events to detect how many
  // scaling passes are actually performed by the YUV readback pipeline.
  StartTracing(TRACE_DISABLED_BY_DEFAULT("cb_command"));

  TestYUVReadback(800,
                  400,
                  800,
                  400,
                  0,
                  0,
                  1,
                  false,
                  true,
                  content::GLHelper::SCALER_QUALITY_FAST);

  std::map<std::string, int> event_counts;
  EndTracing(&event_counts);
  int draw_buffer_calls = event_counts["kDrawBuffersEXTImmediate"];
  int draw_arrays_calls = event_counts["kDrawArrays"];
  VLOG(1) << "Draw buffer calls: " << draw_buffer_calls;
  VLOG(1) << "DrawArrays calls: " << draw_arrays_calls;

  if (draw_buffer_calls) {
    // When using MRT, the YUV readback code should only
    // execute two draw arrays, and scaling should be integrated
    // into those two calls since we are using the FAST scalign
    // quality.
    EXPECT_EQ(2, draw_arrays_calls);
  } else {
    // When not using MRT, there are three passes for the YUV,
    // and one for the scaling.
    EXPECT_EQ(4, draw_arrays_calls);
  }
}

TEST_F(GLHelperPixelTest, YUVReadbackTest) {
  int sizes[] = {2, 4, 14};
  for (int flip = 0; flip <= 1; flip++) {
    for (int use_mrt = 0; use_mrt <= 1; use_mrt++) {
      for (unsigned int x = 0; x < arraysize(sizes); x++) {
        for (unsigned int y = 0; y < arraysize(sizes); y++) {
          for (unsigned int ox = x; ox < arraysize(sizes); ox++) {
            for (unsigned int oy = y; oy < arraysize(sizes); oy++) {
              // If output is a subsection of the destination frame, (letterbox)
              // then try different variations of where the subsection goes.
              for (Margin xm = x < ox ? MarginLeft : MarginRight;
                   xm <= MarginRight;
                   xm = NextMargin(xm)) {
                for (Margin ym = y < oy ? MarginLeft : MarginRight;
                     ym <= MarginRight;
                     ym = NextMargin(ym)) {
                  for (int pattern = 0; pattern < 3; pattern++) {
                    TestYUVReadback(sizes[x],
                                    sizes[y],
                                    sizes[ox],
                                    sizes[oy],
                                    compute_margin(sizes[x], sizes[ox], xm),
                                    compute_margin(sizes[y], sizes[oy], ym),
                                    pattern,
                                    flip == 1,
                                    use_mrt == 1,
                                    content::GLHelper::SCALER_QUALITY_GOOD);
                    if (HasFailure()) {
                      return;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

// Per pixel tests, all sizes are small so that we can print
// out the generated bitmaps.
TEST_F(GLHelperPixelTest, ScaleTest) {
  int sizes[] = {3, 6, 16};
  for (int flip = 0; flip <= 1; flip++) {
    for (size_t q_index = 0; q_index < arraysize(kQualities); q_index++) {
      for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
          for (int dst_x = 0; dst_x < 3; dst_x++) {
            for (int dst_y = 0; dst_y < 3; dst_y++) {
              for (int pattern = 0; pattern < 3; pattern++) {
                TestScale(sizes[x],
                          sizes[y],
                          sizes[dst_x],
                          sizes[dst_y],
                          pattern,
                          q_index,
                          flip == 1);
                if (HasFailure()) {
                  return;
                }
              }
            }
          }
        }
      }
    }
  }
}

// Per pixel tests, all sizes are small so that we can print
// out the generated bitmaps.
TEST_F(GLHelperPixelTest, CropScaleReadbackAndCleanTextureTest) {
  const int kSizes[] = {3, 6, 16};
  const SkColorType kColorTypes[] = {
      kAlpha_8_SkColorType, kRGBA_8888_SkColorType, kBGRA_8888_SkColorType};
  for (size_t color_type = 0; color_type < arraysize(kColorTypes);
       color_type++) {
    // Test BEST and FAST qualities, skip GOOD
    for (size_t q_index = 0; q_index < arraysize(kQualities); q_index += 2) {
      for (size_t x = 0; x < arraysize(kSizes); x++) {
        for (size_t y = 0; y < arraysize(kSizes); y++) {
          for (size_t dst_x = 0; dst_x < arraysize(kSizes); dst_x++) {
            for (size_t dst_y = 0; dst_y < arraysize(kSizes); dst_y++) {
              for (int pattern = 0; pattern < 3; pattern++) {
                TestCropScaleReadbackAndCleanTexture(kSizes[x],
                                                     kSizes[y],
                                                     kSizes[dst_x],
                                                     kSizes[dst_y],
                                                     pattern,
                                                     kColorTypes[color_type],
                                                     false,
                                                     q_index);
                if (HasFailure())
                  return;
              }
            }
          }
        }
      }
    }
  }
}

// Validate that all scaling generates valid pipelines.
TEST_F(GLHelperTest, ValidateScalerPipelines) {
  int sizes[] = {7, 99, 128, 256, 512, 719, 720, 721, 1920, 2011, 3217, 4096};
  for (size_t q = 0; q < arraysize(kQualities); q++) {
    for (size_t x = 0; x < arraysize(sizes); x++) {
      for (size_t y = 0; y < arraysize(sizes); y++) {
        for (size_t dst_x = 0; dst_x < arraysize(sizes); dst_x++) {
          for (size_t dst_y = 0; dst_y < arraysize(sizes); dst_y++) {
            TestScalerPipeline(
                q, sizes[x], sizes[y], sizes[dst_x], sizes[dst_y]);
            if (HasFailure()) {
              return;
            }
          }
        }
      }
    }
  }
}

// Make sure we don't create overly complicated pipelines
// for a few common use cases.
TEST_F(GLHelperTest, CheckSpecificPipelines) {
  // Upscale should be single pass.
  CheckPipeline(content::GLHelper::SCALER_QUALITY_GOOD,
                1024,
                700,
                1280,
                720,
                "1024x700 -> 1280x720 bilinear\n");
  // Slight downscale should use BILINEAR2X2.
  CheckPipeline(content::GLHelper::SCALER_QUALITY_GOOD,
                1280,
                720,
                1024,
                700,
                "1280x720 -> 1024x700 bilinear2x2\n");
  // Most common tab capture pipeline on the Pixel.
  // Should be using two BILINEAR3 passes.
  CheckPipeline(content::GLHelper::SCALER_QUALITY_GOOD,
                2560,
                1476,
                1249,
                720,
                "2560x1476 -> 2560x720 bilinear3 Y\n"
                "2560x720 -> 1249x720 bilinear3 X\n");
}

TEST_F(GLHelperTest, ScalerOpTest) {
  for (int allow3 = 0; allow3 <= 1; allow3++) {
    for (int dst = 1; dst < 2049; dst += 1 + (dst >> 3)) {
      for (int src = 1; src < 2049; src++) {
        TestAddOps(src, dst, allow3 == 1, (src & 1) == 1);
        if (HasFailure()) {
          LOG(ERROR) << "Failed for src=" << src << " dst=" << dst
                     << " allow3=" << allow3;
          return;
        }
      }
    }
  }
}

TEST_F(GLHelperTest, CheckOptimizations) {
  // Test in baseclass since it is friends with GLHelperScaling
  CheckOptimizationsTest();
}

}  // namespace content

namespace {

int RunHelper(base::TestSuite* test_suite) {
  content::UnitTestTestSuite runner(test_suite);
  base::MessageLoopForIO message_loop;
  return runner.Run();
}

}  // namespace

// These tests needs to run against a proper GL environment, so we
// need to set it up before we can run the tests.
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::TestSuite* suite = new content::ContentTestSuite(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  return base::LaunchUnitTestsSerially(
    argc,
    argv,
    base::Bind(&RunHelper, base::Unretained(suite)));
}
