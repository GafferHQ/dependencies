//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer11.cpp: Implements the Framebuffer11 class.

#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"

#include "common/debug.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Clear11.h"
#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Texture.h"

namespace rx
{

Framebuffer11::Framebuffer11(const gl::Framebuffer::Data &data, Renderer11 *renderer)
    : FramebufferD3D(data, renderer),
      mRenderer(renderer)
{
    ASSERT(mRenderer != nullptr);
}

Framebuffer11::~Framebuffer11()
{
}

static gl::Error InvalidateAttachmentSwizzles(const gl::FramebufferAttachment *attachment)
{
    if (attachment && attachment->type() == GL_TEXTURE)
    {
        gl::Texture *texture = attachment->getTexture();

        TextureD3D *textureD3D = GetImplAs<TextureD3D>(texture);

        TextureStorage *texStorage = NULL;
        gl::Error error = textureD3D->getNativeTexture(&texStorage);
        if (error.isError())
        {
            return error;
        }

        if (texStorage)
        {
            TextureStorage11 *texStorage11 = TextureStorage11::makeTextureStorage11(texStorage);
            ASSERT(texStorage11);

            texStorage11->invalidateSwizzleCacheLevel(attachment->mipLevel());
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error Framebuffer11::invalidateSwizzles() const
{
    for (auto it = mData.mColorAttachments.cbegin(); it != mData.mColorAttachments.cend(); ++it)
    {
        gl::FramebufferAttachment *colorAttachment = *it;
        gl::Error error = InvalidateAttachmentSwizzles(colorAttachment);
        if (error.isError())
        {
            return error;
        }
    }

    gl::Error error = InvalidateAttachmentSwizzles(mData.mDepthAttachment);
    if (error.isError())
    {
        return error;
    }

    error = InvalidateAttachmentSwizzles(mData.mStencilAttachment);
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error Framebuffer11::clear(const gl::State &state, const ClearParameters &clearParams)
{
    Clear11 *clearer = mRenderer->getClearer();
    gl::Error error = clearer->clearFramebuffer(clearParams, mData);
    if (error.isError())
    {
        return error;
    }

    error = invalidateSwizzles();
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

static gl::Error getRenderTargetResource(const gl::FramebufferAttachment *colorbuffer, unsigned int *subresourceIndexOut,
                                         ID3D11Texture2D **texture2DOut)
{
    ASSERT(colorbuffer);

    RenderTarget11 *renderTarget = NULL;
    gl::Error error = d3d11::GetAttachmentRenderTarget(colorbuffer, &renderTarget);
    if (error.isError())
    {
        return error;
    }

    ID3D11Resource *renderTargetResource = renderTarget->getTexture();
    ASSERT(renderTargetResource);

    *subresourceIndexOut = renderTarget->getSubresourceIndex();
    *texture2DOut = d3d11::DynamicCastComObject<ID3D11Texture2D>(renderTargetResource);

    if (!(*texture2DOut))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to query the ID3D11Texture2D from a RenderTarget");
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error Framebuffer11::readPixels(const gl::Rectangle &area, GLenum format, GLenum type, size_t outputPitch, const gl::PixelPackState &pack, uint8_t *pixels) const
{
    ID3D11Texture2D *colorBufferTexture = NULL;
    unsigned int subresourceIndex = 0;

    const gl::FramebufferAttachment *colorbuffer = mData.getReadAttachment();
    ASSERT(colorbuffer);

    gl::Error error = getRenderTargetResource(colorbuffer, &subresourceIndex, &colorBufferTexture);
    if (error.isError())
    {
        return error;
    }

    gl::Buffer *packBuffer = pack.pixelBuffer.get();
    if (packBuffer != NULL)
    {
        Buffer11 *packBufferStorage = Buffer11::makeBuffer11(packBuffer->getImplementation());
        PackPixelsParams packParams(area, format, type, outputPitch, pack, reinterpret_cast<ptrdiff_t>(pixels));

        error = packBufferStorage->packPixels(colorBufferTexture, subresourceIndex, packParams);
        if (error.isError())
        {
            SafeRelease(colorBufferTexture);
            return error;
        }

        packBuffer->getIndexRangeCache()->clear();
    }
    else
    {
        error = mRenderer->readTextureData(colorBufferTexture, subresourceIndex, area, format, type, outputPitch, pack, pixels);
        if (error.isError())
        {
            SafeRelease(colorBufferTexture);
            return error;
        }
    }

    SafeRelease(colorBufferTexture);

    return gl::Error(GL_NO_ERROR);
}

gl::Error Framebuffer11::blit(const gl::Rectangle &sourceArea, const gl::Rectangle &destArea, const gl::Rectangle *scissor,
                              bool blitRenderTarget, bool blitDepth, bool blitStencil, GLenum filter,
                              const gl::Framebuffer *sourceFramebuffer)
{
    if (blitRenderTarget)
    {
        const gl::FramebufferAttachment *readBuffer = sourceFramebuffer->getReadColorbuffer();
        ASSERT(readBuffer);

        RenderTargetD3D *readRenderTarget = NULL;
        gl::Error error = GetAttachmentRenderTarget(readBuffer, &readRenderTarget);
        if (error.isError())
        {
            return error;
        }
        ASSERT(readRenderTarget);

        for (size_t colorAttachment = 0; colorAttachment < mData.mColorAttachments.size(); colorAttachment++)
        {
            if (mData.mColorAttachments[colorAttachment] != nullptr &&
                mData.mDrawBufferStates[colorAttachment] != GL_NONE)
            {
                const gl::FramebufferAttachment *drawBuffer = mData.mColorAttachments[colorAttachment];

                RenderTargetD3D *drawRenderTarget = NULL;
                error = GetAttachmentRenderTarget(drawBuffer, &drawRenderTarget);
                if (error.isError())
                {
                    return error;
                }
                ASSERT(drawRenderTarget);

                error = mRenderer->blitRenderbufferRect(sourceArea, destArea, readRenderTarget, drawRenderTarget,
                                                        filter, scissor, blitRenderTarget, false, false);
                if (error.isError())
                {
                    return error;
                }
            }
        }
    }

    if (blitDepth || blitStencil)
    {
        gl::FramebufferAttachment *readBuffer = sourceFramebuffer->getDepthOrStencilbuffer();
        ASSERT(readBuffer);

        RenderTargetD3D *readRenderTarget = NULL;
        gl::Error error = GetAttachmentRenderTarget(readBuffer, &readRenderTarget);
        if (error.isError())
        {
            return error;
        }
        ASSERT(readRenderTarget);

        const gl::FramebufferAttachment *drawBuffer = mData.getDepthOrStencilAttachment();
        ASSERT(drawBuffer);

        RenderTargetD3D *drawRenderTarget = NULL;
        error = GetAttachmentRenderTarget(drawBuffer, &drawRenderTarget);
        if (error.isError())
        {
            return error;
        }
        ASSERT(drawRenderTarget);

        error = mRenderer->blitRenderbufferRect(sourceArea, destArea, readRenderTarget, drawRenderTarget, filter, scissor,
                                                false, blitDepth, blitStencil);
        if (error.isError())
        {
            return error;
        }
    }

    gl::Error error = invalidateSwizzles();
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

GLenum Framebuffer11::getRenderTargetImplementationFormat(RenderTargetD3D *renderTarget) const
{
    RenderTarget11 *renderTarget11 = RenderTarget11::makeRenderTarget11(renderTarget);
    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(renderTarget11->getDXGIFormat());
    return dxgiFormatInfo.internalFormat;
}

}
