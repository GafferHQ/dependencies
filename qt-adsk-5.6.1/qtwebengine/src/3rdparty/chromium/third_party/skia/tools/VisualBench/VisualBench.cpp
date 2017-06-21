/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#include "VisualBench.h"

#include "ProcStats.h"
#include "SkApplication.h"
#include "SkCanvas.h"
#include "SkCommandLineFlags.h"
#include "SkForceLinking.h"
#include "SkGraphics.h"
#include "SkGr.h"
#include "SkImageDecoder.h"
#include "SkOSFile.h"
#include "SkStream.h"
#include "Stats.h"
#include "gl/GrGLInterface.h"

__SK_FORCE_IMAGE_DECODER_LINKING;

// Between samples we reset context
// Between frames we swap buffers
// Between flushes we call flush on GrContext

DEFINE_int32(gpuFrameLag, 5, "Overestimate of maximum number of frames GPU allows to lag.");
DEFINE_int32(samples, 10, "Number of times to time each skp.");
DEFINE_int32(frames, 5, "Number of frames of each skp to render per sample.");
DEFINE_double(flushMs, 20, "Target flush time in millseconds.");
DEFINE_double(loopMs, 5, "Target loop time in millseconds.");
DEFINE_int32(msaa, 0, "Number of msaa samples.");
DEFINE_bool2(fullscreen, f, true, "Run fullscreen.");
DEFINE_bool2(verbose, v, false, "enable verbose output from the test driver.");

static SkString humanize(double ms) {
    if (FLAGS_verbose) {
        return SkStringPrintf("%llu", (uint64_t)(ms*1e6));
    }
    return HumanizeMs(ms);
}

#define HUMANIZE(time) humanize(time).c_str()

VisualBench::VisualBench(void* hwnd, int argc, char** argv)
    : INHERITED(hwnd)
    , fCurrentSample(0)
    , fCurrentFrame(0)
    , fFlushes(1)
    , fLoops(1)
    , fState(kPreWarmLoops_State)
    , fBenchmark(NULL) {
    SkCommandLineFlags::Parse(argc, argv);

    this->setTitle();
    this->setupBackend();

    fBenchmarkStream.reset(SkNEW(VisualBenchmarkStream));

    // Print header
    SkDebugf("curr/maxrss\tloops\tflushes\tmin\tmedian\tmean\tmax\tstddev\tbench\n");
}

VisualBench::~VisualBench() {
    INHERITED::detach();
}

void VisualBench::setTitle() {
    SkString title("VisualBench");
    INHERITED::setTitle(title.c_str());
}

SkSurface* VisualBench::createSurface() {
    SkSurfaceProps props(INHERITED::getSurfaceProps());
    return SkSurface::NewRenderTargetDirect(fRenderTarget, &props);
}

bool VisualBench::setupBackend() {
    this->setColorType(kRGBA_8888_SkColorType);
    this->setVisibleP(true);
    this->setClipToBounds(false);

    if (FLAGS_fullscreen) {
        if (!this->makeFullscreen()) {
            SkDebugf("Could not go fullscreen!");
        }
    }
    if (!this->attach(kNativeGL_BackEndType, FLAGS_msaa, &fAttachmentInfo)) {
        SkDebugf("Not possible to create backend.\n");
        INHERITED::detach();
        return false;
    }

    this->setVsync(false);
    this->resetContext();
    return true;
}

void VisualBench::resetContext() {
    fInterface.reset(GrGLCreateNativeInterface());
    SkASSERT(fInterface);

    // setup contexts
    fContext.reset(GrContext::Create(kOpenGL_GrBackend, (GrBackendContext)fInterface.get()));
    SkASSERT(fContext);

    // setup rendertargets
    this->setupRenderTarget();
}

void VisualBench::setupRenderTarget() {
    if (fContext) {
        fRenderTarget.reset(this->renderTarget(fAttachmentInfo, fInterface, fContext));
    }
}

inline void VisualBench::renderFrame(SkCanvas* canvas) {
    for (int flush = 0; flush < fFlushes; flush++) {
        fBenchmark->draw(fLoops, canvas);
        canvas->flush();
    }
    INHERITED::present();
}

void VisualBench::printStats() {
    const SkTArray<double>& measurements = fRecords.back().fMeasurements;
    const char* shortName = fBenchmark->getUniqueName();
    if (FLAGS_verbose) {
        for (int i = 0; i < measurements.count(); i++) {
            SkDebugf("%s  ", HUMANIZE(measurements[i]));
        }
        SkDebugf("%s\n", shortName);
    } else {
        SkASSERT(measurements.count());
        Stats stats(measurements);
        const double stdDevPercent = 100 * sqrt(stats.var) / stats.mean;
        SkDebugf("%4d/%-4dMB\t%d\t%d\t%s\t%s\t%s\t%s\t%.0f%%\t%s\n",
                 sk_tools::getCurrResidentSetSizeMB(),
                 sk_tools::getMaxResidentSetSizeMB(),
                 fLoops,
                 fFlushes,
                 HUMANIZE(stats.min),
                 HUMANIZE(stats.median),
                 HUMANIZE(stats.mean),
                 HUMANIZE(stats.max),
                 stdDevPercent,
                 shortName);
    }
}

bool VisualBench::advanceRecordIfNecessary(SkCanvas* canvas) {
    if (fBenchmark) {
        return true;
    }

    while ((fBenchmark = fBenchmarkStream->next()) &&
           (SkCommandLineFlags::ShouldSkip(FLAGS_match, fBenchmark->getUniqueName()) ||
            !fBenchmark->isSuitableFor(Benchmark::kGPU_Backend))) {}

    if (!fBenchmark) {
        return false;
    }

    canvas->clear(0xffffffff);
    fBenchmark->preDraw();
    fBenchmark->perCanvasPreDraw(canvas);
    fRecords.push_back();
    return true;
}

void VisualBench::preWarm(State nextState) {
    if (fCurrentFrame >= FLAGS_gpuFrameLag) {
        // we currently time across all frames to make sure we capture all GPU work
        fState = nextState;
        fCurrentFrame = 0;
        fTimer.start();
    } else {
        fCurrentFrame++;
    }
}

void VisualBench::draw(SkCanvas* canvas) {
    if (!this->advanceRecordIfNecessary(canvas)) {
        this->closeWindow();
        return;
    }
    this->renderFrame(canvas);
    switch (fState) {
        case kPreWarmLoops_State: {
            this->preWarm(kTuneLoops_State);
            break;
        }
        case kTuneLoops_State: {
            if (1 << 30 == fLoops) {
                // We're about to wrap.  Something's wrong with the bench.
                SkDebugf("InnerLoops wrapped\n");
                fLoops = 0;
            } else {
                fTimer.end();
                double elapsed = fTimer.fWall;
                if (elapsed > FLAGS_loopMs) {
                    fState = kPreWarmTiming_State;

                    // Scale back the number of loops
                    fLoops = (int)ceil(fLoops * FLAGS_loopMs / elapsed);
                    fFlushes = (int)ceil(FLAGS_flushMs / elapsed);
                } else {
                    fState = kPreWarmLoops_State;
                    fLoops *= 2;
                }

                fCurrentFrame = 0;
                fTimer = WallTimer();
                this->resetContext();
            }
            break;
        }
        case kPreWarmTiming_State: {
            this->preWarm(kTiming_State);
            break;
        }
        case kTiming_State: {
            if (fCurrentFrame >= FLAGS_frames) {
                fTimer.end();
                fRecords.back().fMeasurements.push_back(
                        fTimer.fWall / (FLAGS_frames * fLoops * fFlushes));
                if (fCurrentSample++ >= FLAGS_samples) {
                    fState = kPreWarmLoops_State;
                    this->printStats();
                    fBenchmark->perCanvasPostDraw(canvas);
                    fBenchmark = NULL;
                    fCurrentSample = 0;
                    fFlushes = 1;
                    fLoops = 1;
                } else {
                    fState = kPreWarmTiming_State;
                }
                fTimer = WallTimer();
                this->resetContext();
                fCurrentFrame = 0;
            } else {
                fCurrentFrame++;
            }
            break;
        }
    }

    // Invalidate the window to force a redraw. Poor man's animation mechanism.
    this->inval(NULL);
}

void VisualBench::onSizeChange() {
    this->setupRenderTarget();
}

bool VisualBench::onHandleChar(SkUnichar unichar) {
    return true;
}

// Externally declared entry points
void application_init() {
    SkGraphics::Init();
    SkEvent::Init();
}

void application_term() {
    SkEvent::Term();
    SkGraphics::Term();
}

SkOSWindow* create_sk_window(void* hwnd, int argc, char** argv) {
    return new VisualBench(hwnd, argc, argv);
}

