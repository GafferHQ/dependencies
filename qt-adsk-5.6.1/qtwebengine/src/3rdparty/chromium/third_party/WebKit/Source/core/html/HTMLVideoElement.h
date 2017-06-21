/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLVideoElement_h
#define HTMLVideoElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/graphics/GraphicsTypes3D.h"

class SkPaint;

namespace blink {
class WebGraphicsContext3D;
class ExceptionState;
class GraphicsContext;

// GL types as defined in OpenGL ES 2.0 header file gl2.h from khronos.org.
// That header cannot be included directly due to a conflict with NPAPI headers.
// See crbug.com/328085.
typedef unsigned GLenum;
typedef int GC3Dint;

class CORE_EXPORT HTMLVideoElement final : public HTMLMediaElement, public CanvasImageSource {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLVideoElement> create(Document&);
    DECLARE_VIRTUAL_TRACE();

    unsigned videoWidth() const;
    unsigned videoHeight() const;

    // Fullscreen
    void webkitEnterFullscreen(ExceptionState&);
    void webkitExitFullscreen();
    bool webkitSupportsFullscreen();
    bool webkitDisplayingFullscreen();

    // Statistics
    unsigned webkitDecodedFrameCount() const;
    unsigned webkitDroppedFrameCount() const;

    // Used by canvas to gain raw pixel access
    void paintCurrentFrame(SkCanvas*, const IntRect&, const SkPaint*) const;

    // Used by WebGL to do GPU-GPU textures copy if possible.
    bool copyVideoTextureToPlatformTexture(WebGraphicsContext3D*, Platform3DObject texture, GLenum internalFormat, GLenum type, bool premultiplyAlpha, bool flipY);

    bool shouldDisplayPosterImage() const { return displayMode() == Poster; }

    KURL posterImageURL() const;

    bool hasAvailableVideoFrame() const;

    // FIXME: Remove this when WebMediaPlayerClientImpl::loadInternal does not depend on it.
    KURL mediaPlayerPosterURL() override;

    // CanvasImageSource implementation
    PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const override;
    bool isVideoElement() const override { return true; }
    bool wouldTaintOrigin(SecurityOrigin*) const override;
    FloatSize elementSize() const override;
    const KURL& sourceURL() const override { return currentSrc(); }

    bool isHTMLVideoElement() const override { return true; }

private:
    HTMLVideoElement(Document&);

    bool layoutObjectIsNeeded(const ComputedStyle&) override;
    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    void attach(const AttachContext& = AttachContext()) override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;
    bool hasVideo() const override { return webMediaPlayer() && webMediaPlayer()->hasVideo(); }
    bool supportsFullscreen() const;
    bool isURLAttribute(const Attribute&) const override;
    const AtomicString imageSourceURL() const override;

    void updateDisplayState() override;
    void didMoveToNewDocument(Document& oldDocument) override;
    void setDisplayMode(DisplayMode) override;

    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;

    AtomicString m_defaultPosterURL;
};

} // namespace blink

#endif // HTMLVideoElement_h
