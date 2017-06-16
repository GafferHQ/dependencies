/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Command line tool for speech intelligibility enhancement. Provides for
//  running and testing intelligibility_enhancer as an independent process.
//  Use --help for options.
//

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_audio/real_fourier.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::complex;

namespace webrtc {

using webrtc::RealFourier;
using webrtc::IntelligibilityEnhancer;

DEFINE_int32(clear_type,
             webrtc::intelligibility::VarianceArray::kStepInfinite,
             "Variance algorithm for clear data.");
DEFINE_double(clear_alpha, 0.9, "Variance decay factor for clear data.");
DEFINE_int32(clear_window,
             475,
             "Window size for windowed variance for clear data.");
DEFINE_int32(sample_rate,
             16000,
             "Audio sample rate used in the input and output files.");
DEFINE_int32(ana_rate,
             800,
             "Analysis rate; gains recalculated every N blocks.");
DEFINE_int32(
    var_rate,
    2,
    "Variance clear rate; history is forgotten every N gain recalculations.");
DEFINE_double(gain_limit, 1000.0, "Maximum gain change in one block.");

DEFINE_string(clear_file, "speech.wav", "Input file with clear speech.");
DEFINE_string(noise_file, "noise.wav", "Input file with noise data.");
DEFINE_string(out_file,
              "proc_enhanced.wav",
              "Enhanced output. Use '-' to "
              "play through aplay immediately.");

// Constant IntelligibilityEnhancer constructor parameters.
const int kErbResolution = 2;
const int kNumChannels = 1;

// void function for gtest
void void_main(int argc, char* argv[]) {
  google::SetUsageMessage(
      "\n\nVariance algorithm types are:\n"
      "  0 - infinite/normal,\n"
      "  1 - exponentially decaying,\n"
      "  2 - rolling window.\n"
      "\nInput files must be little-endian 16-bit signed raw PCM.\n");
  google::ParseCommandLineFlags(&argc, &argv, true);

  size_t samples;        // Number of samples in input PCM file
  size_t fragment_size;  // Number of samples to process at a time
                         // to simulate APM stream processing

  // Load settings and wav input.

  fragment_size = FLAGS_sample_rate / 100;  // Mirror real time APM chunk size.
                                            // Duplicates chunk_length_ in
                                            // IntelligibilityEnhancer.

  struct stat in_stat, noise_stat;
  ASSERT_EQ(stat(FLAGS_clear_file.c_str(), &in_stat), 0)
      << "Empty speech file.";
  ASSERT_EQ(stat(FLAGS_noise_file.c_str(), &noise_stat), 0)
      << "Empty noise file.";

  samples = std::min(in_stat.st_size, noise_stat.st_size) / 2;

  WavReader in_file(FLAGS_clear_file);
  std::vector<float> in_fpcm(samples);
  in_file.ReadSamples(samples, &in_fpcm[0]);

  WavReader noise_file(FLAGS_noise_file);
  std::vector<float> noise_fpcm(samples);
  noise_file.ReadSamples(samples, &noise_fpcm[0]);

  // Run intelligibility enhancement.

  IntelligibilityEnhancer enh(
      kErbResolution, FLAGS_sample_rate, kNumChannels, FLAGS_clear_type,
      static_cast<float>(FLAGS_clear_alpha), FLAGS_clear_window, FLAGS_ana_rate,
      FLAGS_var_rate, FLAGS_gain_limit);

  // Slice the input into smaller chunks, as the APM would do, and feed them
  // through the enhancer.
  float* clear_cursor = &in_fpcm[0];
  float* noise_cursor = &noise_fpcm[0];

  for (size_t i = 0; i < samples; i += fragment_size) {
    enh.ProcessCaptureAudio(&noise_cursor);
    enh.ProcessRenderAudio(&clear_cursor);
    clear_cursor += fragment_size;
    noise_cursor += fragment_size;
  }

  if (FLAGS_out_file.compare("-") == 0) {
    const std::string temp_out_filename =
        test::TempFilename(test::WorkingDir(), "temp_wav_file");
    {
      WavWriter out_file(temp_out_filename, FLAGS_sample_rate, kNumChannels);
      out_file.WriteSamples(&in_fpcm[0], samples);
    }
    system(("aplay " + temp_out_filename).c_str());
    system(("rm " + temp_out_filename).c_str());
  } else {
    WavWriter out_file(FLAGS_out_file, FLAGS_sample_rate, kNumChannels);
    out_file.WriteSamples(&in_fpcm[0], samples);
  }
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::void_main(argc, argv);
  return 0;
}
