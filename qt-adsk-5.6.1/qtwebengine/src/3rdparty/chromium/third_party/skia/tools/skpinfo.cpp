/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCommandLineFlags.h"
#include "SkPicture.h"
#include "SkPictureData.h"
#include "SkStream.h"

DEFINE_string2(input, i, "", "skp on which to report");
DEFINE_bool2(version, v, true, "version");
DEFINE_bool2(cullRect, c, true, "cullRect");
DEFINE_bool2(flags, f, true, "flags");
DEFINE_bool2(tags, t, true, "tags");
DEFINE_bool2(quiet, q, false, "quiet");

// This tool can print simple information about an SKP but its main use
// is just to check if an SKP has been truncated during the recording
// process.
// return codes:
static const int kSuccess = 0;
static const int kTruncatedFile = 1;
static const int kNotAnSKP = 2;
static const int kInvalidTag = 3;
static const int kMissingInput = 4;
static const int kIOError = 5;

int tool_main(int argc, char** argv);
int tool_main(int argc, char** argv) {
    SkCommandLineFlags::SetUsage("Prints information about an skp file");
    SkCommandLineFlags::Parse(argc, argv);

    if (FLAGS_input.count() != 1) {
        if (!FLAGS_quiet) {
            SkDebugf("Missing input file\n");
        }
        return kMissingInput;
    }

    SkFILEStream stream(FLAGS_input[0]);
    if (!stream.isValid()) {
        if (!FLAGS_quiet) {
            SkDebugf("Couldn't open file\n");
        }
        return kIOError;
    }

    size_t totStreamSize = stream.getLength();

    SkPictInfo info;
    if (!SkPicture::InternalOnly_StreamIsSKP(&stream, &info)) {
        return kNotAnSKP;
    }

    if (FLAGS_version && !FLAGS_quiet) {
        SkDebugf("Version: %d\n", info.fVersion);
    }
    if (FLAGS_cullRect && !FLAGS_quiet) {
        SkDebugf("Cull Rect: %f,%f,%f,%f\n",
                 info.fCullRect.fLeft, info.fCullRect.fTop,
                 info.fCullRect.fRight, info.fCullRect.fBottom);
    }
    if (FLAGS_flags && !FLAGS_quiet) {
        SkDebugf("Flags: 0x%x\n", info.fFlags);
    }

    if (!stream.readBool()) {
        // If we read true there's a picture playback object flattened
        // in the file; if false, there isn't a playback, so we're done
        // reading the file.
        return kSuccess;
    }

    for (;;) {
        uint32_t tag = stream.readU32();
        if (SK_PICT_EOF_TAG == tag) {
            break;
        }

        uint32_t chunkSize = stream.readU32();
        size_t curPos = stream.getPosition();

        // "move" doesn't error out when seeking beyond the end of file
        // so we need a preemptive check here.
        if (curPos+chunkSize > totStreamSize) {
            if (!FLAGS_quiet) {
                SkDebugf("truncated file\n");
            }
            return kTruncatedFile;
        }

        // Not all the tags store the chunk size (in bytes). Three
        // of them store tag-specific size information (e.g., number of
        // fonts) instead. This forces us to early exit when those
        // chunks are encountered.
        switch (tag) {
        case SK_PICT_READER_TAG:
            if (FLAGS_tags && !FLAGS_quiet) {
                SkDebugf("SK_PICT_READER_TAG %d\n", chunkSize);
            }
            break;
        case SK_PICT_FACTORY_TAG:
            if (FLAGS_tags && !FLAGS_quiet) {
                SkDebugf("SK_PICT_FACTORY_TAG %d\n", chunkSize);
            }
            break;
        case SK_PICT_TYPEFACE_TAG:
            if (FLAGS_tags && !FLAGS_quiet) {
                SkDebugf("SK_PICT_TYPEFACE_TAG %d\n", chunkSize);
                SkDebugf("Exiting early due to format limitations\n");
            }
            return kSuccess;       // TODO: need to store size in bytes
            break;
        case SK_PICT_PICTURE_TAG:
            if (FLAGS_tags && !FLAGS_quiet) {
                SkDebugf("SK_PICT_PICTURE_TAG %d\n", chunkSize);
                SkDebugf("Exiting early due to format limitations\n");
            }
            return kSuccess;       // TODO: need to store size in bytes
            break;
        case SK_PICT_BUFFER_SIZE_TAG:
            if (FLAGS_tags && !FLAGS_quiet) {
                SkDebugf("SK_PICT_BUFFER_SIZE_TAG %d\n", chunkSize);
            }
            break;
        default:
            if (!FLAGS_quiet) {
                SkDebugf("Unknown tag %d\n", chunkSize);
            }
            return kInvalidTag;
        }

        if (!stream.move(chunkSize)) {
            if (!FLAGS_quiet) {
                SkDebugf("seek error\n");
            }
            return kTruncatedFile;
        }
    }

    return kSuccess;
}

#if !defined SK_BUILD_FOR_IOS
int main(int argc, char * const argv[]) {
    return tool_main(argc, (char**) argv);
}
#endif
