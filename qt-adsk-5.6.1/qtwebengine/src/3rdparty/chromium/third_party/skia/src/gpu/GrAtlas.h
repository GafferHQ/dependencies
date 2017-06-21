
/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrAtlas_DEFINED
#define GrAtlas_DEFINED


#include "SkTDArray.h"
#include "GrTexture.h"
#include "SkPoint.h"
#include "SkTInternalLList.h"

class GrGpu;
class GrRectanizer;
class GrAtlas;

// The backing GrTexture for a set of GrAtlases is broken into a spatial grid of GrPlots. When
// a GrAtlas needs space on the texture, it requests a GrPlot. Each GrAtlas can claim one
// or more GrPlots. The GrPlots keep track of subimage placement via their GrRectanizer. Once a
// GrPlot is "full" (i.e. there is no room for the new subimage according to the GrRectanizer), the
// GrAtlas can request a new GrPlot via GrAtlas::addToAtlas().
//
// If all GrPlots are allocated, the replacement strategy is up to the client. The drawToken is
// available to ensure that all draw calls are finished for that particular GrPlot.
// GrAtlas::removeUnusedPlots() will free up any finished plots for a given GrAtlas.

class GrPlot {
public:
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(GrPlot);

    // This returns a plot ID unique to each plot in a given GrAtlas. They are
    // consecutive and start at 0.
    int id() const { return fID; }

    GrTexture* texture() const { return fTexture; }

    bool addSubImage(int width, int height, const void*, SkIPoint16*);

    void resetRects();

private:
    GrPlot();
    ~GrPlot(); // does not try to delete the fNext field
    void init(GrAtlas* atlas, int id, int offX, int offY, int width, int height, size_t bpp,
              bool batchUploads);

    int                     fID;
    unsigned char*          fPlotData;
    GrTexture*              fTexture;
    GrRectanizer*           fRects;
    GrAtlas*                fAtlas;
    SkIPoint16              fOffset;        // the offset of the plot in the backing texture
    size_t                  fBytesPerPixel;
    SkIRect                 fDirtyRect;
    bool                    fDirty;
    bool                    fBatchUploads;

    friend class GrAtlas;
};

typedef SkTInternalLList<GrPlot> GrPlotList;

class GrAtlas {
public:
    // This class allows each client to independently track the GrPlots in
    // which its data is stored.
    class ClientPlotUsage {
    public:
        bool isEmpty() const { return 0 == fPlots.count(); }

#ifdef SK_DEBUG
        bool contains(const GrPlot* plot) const { 
            return fPlots.contains(const_cast<GrPlot*>(plot)); 
        }
#endif

    private:
        SkTDArray<GrPlot*> fPlots;

        friend class GrAtlas;
    };

    GrAtlas(GrGpu*, GrPixelConfig, GrSurfaceFlags flags, 
            const SkISize& backingTextureSize,
            int numPlotsX, int numPlotsY, bool batchUploads);
    ~GrAtlas();

    // Adds a width x height subimage to the atlas. Upon success it returns 
    // the containing GrPlot and absolute location in the backing texture. 
    // NULL is returned if the subimage cannot fit in the atlas.
    // If provided, the image data will either be immediately uploaded or
    // written to the CPU-side backing bitmap.
    GrPlot* addToAtlas(ClientPlotUsage*, int width, int height, const void* image, SkIPoint16* loc);

    // remove reference to this plot
    static void RemovePlot(ClientPlotUsage* usage, const GrPlot* plot);

    GrTexture* getTexture() const {
        return fTexture;
    }

    enum IterOrder {
        kLRUFirst_IterOrder,
        kMRUFirst_IterOrder
    };

    typedef GrPlotList::Iter PlotIter;
    GrPlot* iterInit(PlotIter* iter, IterOrder order) {
        return iter->init(fPlotList, kLRUFirst_IterOrder == order
                                                       ? GrPlotList::Iter::kTail_IterStart
                                                       : GrPlotList::Iter::kHead_IterStart);
    }

private:
    void makeMRU(GrPlot* plot);

    GrGpu*         fGpu;
    GrPixelConfig  fPixelConfig;
    GrSurfaceFlags fFlags;
    GrTexture*     fTexture;
    SkISize        fBackingTextureSize;
    int            fNumPlotsX;
    int            fNumPlotsY;
    bool           fBatchUploads;

    // allocated array of GrPlots
    GrPlot*       fPlotArray;
    // LRU list of GrPlots (MRU at head - LRU at tail)
    GrPlotList    fPlotList;
};

#endif
