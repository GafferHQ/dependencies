/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrRenderTargetPriv_DEFINED
#define GrRenderTargetPriv_DEFINED

#include "GrRenderTarget.h"

/** Class that adds methods to GrRenderTarget that are only intended for use internal to Skia.
    This class is purely a privileged window into GrRenderTarget. It should never have additional
    data members or virtual methods. */
class GrRenderTargetPriv {
public:
    /**
     * GrStencilAttachment is not part of the public API.
     */
    GrStencilAttachment* getStencilAttachment() const { return fRenderTarget->fStencilAttachment; }

    /**
     * If this render target already has a stencil buffer, return it. Otherwise attempt to attach
     * one.
     */
    GrStencilAttachment* attachStencilAttachment() const;

    void didAttachStencilAttachment(GrStencilAttachment*);

private:
    explicit GrRenderTargetPriv(GrRenderTarget* renderTarget) : fRenderTarget(renderTarget) {}
    GrRenderTargetPriv(const GrRenderTargetPriv&) {} // unimpl
    GrRenderTargetPriv& operator=(const GrRenderTargetPriv&); // unimpl

    // No taking addresses of this type.
    const GrRenderTargetPriv* operator&() const;
    GrRenderTargetPriv* operator&();

    GrRenderTarget* fRenderTarget;
        
    friend class GrRenderTarget; // to construct/copy this type.
};

inline GrRenderTargetPriv GrRenderTarget::renderTargetPriv() { return GrRenderTargetPriv(this); }

inline const GrRenderTargetPriv GrRenderTarget::renderTargetPriv () const {
    return GrRenderTargetPriv(const_cast<GrRenderTarget*>(this));
}

#endif
