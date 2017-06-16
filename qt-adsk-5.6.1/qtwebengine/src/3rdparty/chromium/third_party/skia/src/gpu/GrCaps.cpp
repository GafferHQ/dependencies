
/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrCaps.h"
#include "GrContextOptions.h"

GrShaderCaps::GrShaderCaps() {
    fShaderDerivativeSupport = false;
    fGeometryShaderSupport = false;
    fPathRenderingSupport = false;
    fDstReadInShaderSupport = false;
    fDualSourceBlendingSupport = false;
    fMixedSamplesSupport = false;
    fShaderPrecisionVaries = false;
}

static const char* shader_type_to_string(GrShaderType type) {
    switch (type) {
    case kVertex_GrShaderType:
        return "vertex";
    case kGeometry_GrShaderType:
        return "geometry";
    case kFragment_GrShaderType:
        return "fragment";
    }
    return "";
}

static const char* precision_to_string(GrSLPrecision p) {
    switch (p) {
    case kLow_GrSLPrecision:
        return "low";
    case kMedium_GrSLPrecision:
        return "medium";
    case kHigh_GrSLPrecision:
        return "high";
    }
    return "";
}

SkString GrShaderCaps::dump() const {
    SkString r;
    static const char* gNY[] = { "NO", "YES" };
    r.appendf("Shader Derivative Support          : %s\n", gNY[fShaderDerivativeSupport]);
    r.appendf("Geometry Shader Support            : %s\n", gNY[fGeometryShaderSupport]);
    r.appendf("Path Rendering Support             : %s\n", gNY[fPathRenderingSupport]);
    r.appendf("Dst Read In Shader Support         : %s\n", gNY[fDstReadInShaderSupport]);
    r.appendf("Dual Source Blending Support       : %s\n", gNY[fDualSourceBlendingSupport]);
    r.appendf("Mixed Samples Support              : %s\n", gNY[fMixedSamplesSupport]);

    r.appendf("Shader Float Precisions (varies: %s):\n", gNY[fShaderPrecisionVaries]);

    for (int s = 0; s < kGrShaderTypeCount; ++s) {
        GrShaderType shaderType = static_cast<GrShaderType>(s);
        r.appendf("\t%s:\n", shader_type_to_string(shaderType));
        for (int p = 0; p < kGrSLPrecisionCount; ++p) {
            if (fFloatPrecisions[s][p].supported()) {
                GrSLPrecision precision = static_cast<GrSLPrecision>(p);
                r.appendf("\t\t%s: log_low: %d log_high: %d bits: %d\n",
                    precision_to_string(precision),
                    fFloatPrecisions[s][p].fLogRangeLow,
                    fFloatPrecisions[s][p].fLogRangeHigh,
                    fFloatPrecisions[s][p].fBits);
            }
        }
    }

    return r;
}

void GrShaderCaps::applyOptionsOverrides(const GrContextOptions& options) {
    fDualSourceBlendingSupport = fDualSourceBlendingSupport && !options.fSuppressDualSourceBlending;
}

///////////////////////////////////////////////////////////////////////////////

GrCaps::GrCaps(const GrContextOptions& options) {
    fMipMapSupport = false;
    fNPOTTextureTileSupport = false;
    fTwoSidedStencilSupport = false;
    fStencilWrapOpsSupport = false;
    fDiscardRenderTargetSupport = false;
    fReuseScratchTextures = true;
    fReuseScratchBuffers = true;
    fGpuTracingSupport = false;
    fCompressedTexSubImageSupport = false;
    fOversizedStencilSupport = false;
    fTextureBarrierSupport = false;
    fSupportsInstancedDraws = false;

    fUseDrawInsteadOfClear = false;

    fBlendEquationSupport = kBasic_BlendEquationSupport;
    fAdvBlendEqBlacklist = 0;

    fMapBufferFlags = kNone_MapFlags;

    fMaxRenderTargetSize = 0;
    fMaxTextureSize = 0;
    fMinTextureSize = 0;
    fMaxSampleCount = 0;

    memset(fConfigRenderSupport, 0, sizeof(fConfigRenderSupport));
    memset(fConfigTextureSupport, 0, sizeof(fConfigTextureSupport));

    fSupressPrints = options.fSuppressPrints;
    fDrawPathMasksToCompressedTextureSupport = options.fDrawPathToCompressedTexture;
    fGeometryBufferMapThreshold = options.fGeometryBufferMapThreshold;
    fUseDrawInsteadOfPartialRenderTargetWrite = options.fUseDrawInsteadOfPartialRenderTargetWrite;
}

void GrCaps::applyOptionsOverrides(const GrContextOptions& options) {
    fMaxTextureSize = SkTMin(fMaxTextureSize, options.fMaxTextureSizeOverride);
    fMinTextureSize = SkTMax(fMinTextureSize, options.fMinTextureSizeOverride);
}

static SkString map_flags_to_string(uint32_t flags) {
    SkString str;
    if (GrCaps::kNone_MapFlags == flags) {
        str = "none";
    } else {
        SkASSERT(GrCaps::kCanMap_MapFlag & flags);
        SkDEBUGCODE(flags &= ~GrCaps::kCanMap_MapFlag);
        str = "can_map";

        if (GrCaps::kSubset_MapFlag & flags) {
            str.append(" partial");
        } else {
            str.append(" full");
        }
        SkDEBUGCODE(flags &= ~GrCaps::kSubset_MapFlag);
    }
    SkASSERT(0 == flags); // Make sure we handled all the flags.
    return str;
}

SkString GrCaps::dump() const {
    SkString r;
    static const char* gNY[] = {"NO", "YES"};
    r.appendf("MIP Map Support                    : %s\n", gNY[fMipMapSupport]);
    r.appendf("NPOT Texture Tile Support          : %s\n", gNY[fNPOTTextureTileSupport]);
    r.appendf("Two Sided Stencil Support          : %s\n", gNY[fTwoSidedStencilSupport]);
    r.appendf("Stencil Wrap Ops  Support          : %s\n", gNY[fStencilWrapOpsSupport]);
    r.appendf("Discard Render Target Support      : %s\n", gNY[fDiscardRenderTargetSupport]);
    r.appendf("Reuse Scratch Textures             : %s\n", gNY[fReuseScratchTextures]);
    r.appendf("Reuse Scratch Buffers              : %s\n", gNY[fReuseScratchBuffers]);
    r.appendf("Gpu Tracing Support                : %s\n", gNY[fGpuTracingSupport]);
    r.appendf("Compressed Update Support          : %s\n", gNY[fCompressedTexSubImageSupport]);
    r.appendf("Oversized Stencil Support          : %s\n", gNY[fOversizedStencilSupport]);
    r.appendf("Texture Barrier Support            : %s\n", gNY[fTextureBarrierSupport]);
    r.appendf("Supports instanced draws           : %s\n", gNY[fSupportsInstancedDraws]);
    r.appendf("Draw Instead of Clear [workaround] : %s\n", gNY[fUseDrawInsteadOfClear]);
    r.appendf("Draw Instead of TexSubImage [workaround] : %s\n",
              gNY[fUseDrawInsteadOfPartialRenderTargetWrite]);
    if (this->advancedBlendEquationSupport()) {
        r.appendf("Advanced Blend Equation Blacklist  : 0x%x\n", fAdvBlendEqBlacklist);
    }

    r.appendf("Max Texture Size                   : %d\n", fMaxTextureSize);
    r.appendf("Min Texture Size                   : %d\n", fMinTextureSize);
    r.appendf("Max Render Target Size             : %d\n", fMaxRenderTargetSize);
    r.appendf("Max Sample Count                   : %d\n", fMaxSampleCount);

    static const char* kBlendEquationSupportNames[] = {
        "Basic",
        "Advanced",
        "Advanced Coherent",
    };
    GR_STATIC_ASSERT(0 == kBasic_BlendEquationSupport);
    GR_STATIC_ASSERT(1 == kAdvanced_BlendEquationSupport);
    GR_STATIC_ASSERT(2 == kAdvancedCoherent_BlendEquationSupport);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kBlendEquationSupportNames) == kLast_BlendEquationSupport + 1);

    r.appendf("Blend Equation Support             : %s\n",
              kBlendEquationSupportNames[fBlendEquationSupport]);
    r.appendf("Map Buffer Support                 : %s\n",
              map_flags_to_string(fMapBufferFlags).c_str());

    static const char* kConfigNames[] = {
        "Unknown",  // kUnknown_GrPixelConfig
        "Alpha8",   // kAlpha_8_GrPixelConfig,
        "Index8",   // kIndex_8_GrPixelConfig,
        "RGB565",   // kRGB_565_GrPixelConfig,
        "RGBA444",  // kRGBA_4444_GrPixelConfig,
        "RGBA8888", // kRGBA_8888_GrPixelConfig,
        "BGRA8888", // kBGRA_8888_GrPixelConfig,
        "SRGBA8888",// kSRGBA_8888_GrPixelConfig,
        "ETC1",     // kETC1_GrPixelConfig,
        "LATC",     // kLATC_GrPixelConfig,
        "R11EAC",   // kR11_EAC_GrPixelConfig,
        "ASTC12x12",// kASTC_12x12_GrPixelConfig,
        "RGBAFloat",// kRGBA_float_GrPixelConfig
        "AlphaHalf",// kAlpha_half_GrPixelConfig
        "RGBAHalf", // kRGBA_half_GrPixelConfig
    };
    GR_STATIC_ASSERT(0  == kUnknown_GrPixelConfig);
    GR_STATIC_ASSERT(1  == kAlpha_8_GrPixelConfig);
    GR_STATIC_ASSERT(2  == kIndex_8_GrPixelConfig);
    GR_STATIC_ASSERT(3  == kRGB_565_GrPixelConfig);
    GR_STATIC_ASSERT(4  == kRGBA_4444_GrPixelConfig);
    GR_STATIC_ASSERT(5  == kRGBA_8888_GrPixelConfig);
    GR_STATIC_ASSERT(6  == kBGRA_8888_GrPixelConfig);
    GR_STATIC_ASSERT(7  == kSRGBA_8888_GrPixelConfig);
    GR_STATIC_ASSERT(8  == kETC1_GrPixelConfig);
    GR_STATIC_ASSERT(9  == kLATC_GrPixelConfig);
    GR_STATIC_ASSERT(10  == kR11_EAC_GrPixelConfig);
    GR_STATIC_ASSERT(11 == kASTC_12x12_GrPixelConfig);
    GR_STATIC_ASSERT(12 == kRGBA_float_GrPixelConfig);
    GR_STATIC_ASSERT(13 == kAlpha_half_GrPixelConfig);
    GR_STATIC_ASSERT(14 == kRGBA_half_GrPixelConfig);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kConfigNames) == kGrPixelConfigCnt);

    SkASSERT(!fConfigRenderSupport[kUnknown_GrPixelConfig][0]);
    SkASSERT(!fConfigRenderSupport[kUnknown_GrPixelConfig][1]);

    for (size_t i = 1; i < SK_ARRAY_COUNT(kConfigNames); ++i)  {
        r.appendf("%s is renderable: %s, with MSAA: %s\n",
                  kConfigNames[i],
                  gNY[fConfigRenderSupport[i][0]],
                  gNY[fConfigRenderSupport[i][1]]);
    }

    SkASSERT(!fConfigTextureSupport[kUnknown_GrPixelConfig]);

    for (size_t i = 1; i < SK_ARRAY_COUNT(kConfigNames); ++i)  {
        r.appendf("%s is uploadable to a texture: %s\n",
                  kConfigNames[i],
                  gNY[fConfigTextureSupport[i]]);
    }

    return r;
}
