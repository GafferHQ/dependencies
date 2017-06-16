//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#include "libANGLE/angletypes.h"
#include "libANGLE/Program.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"

namespace gl
{

bool operator==(const Rectangle &a, const Rectangle &b)
{
    return a.x == b.x &&
           a.y == b.y &&
           a.width == b.width &&
           a.height == b.height;
}

bool operator!=(const Rectangle &a, const Rectangle &b)
{
    return !(a == b);
}

SamplerState::SamplerState()
    : minFilter(GL_NEAREST_MIPMAP_LINEAR),
      magFilter(GL_LINEAR),
      wrapS(GL_REPEAT),
      wrapT(GL_REPEAT),
      wrapR(GL_REPEAT),
      maxAnisotropy(1.0f),
      baseLevel(0),
      maxLevel(1000),
      minLod(-1000.0f),
      maxLod(1000.0f),
      compareMode(GL_NONE),
      compareFunc(GL_LEQUAL),
      swizzleRed(GL_RED),
      swizzleGreen(GL_GREEN),
      swizzleBlue(GL_BLUE),
      swizzleAlpha(GL_ALPHA)
{}

bool SamplerState::swizzleRequired() const
{
    return swizzleRed != GL_RED || swizzleGreen != GL_GREEN ||
           swizzleBlue != GL_BLUE || swizzleAlpha != GL_ALPHA;
}

bool SamplerState::operator==(const SamplerState &other) const
{
    return minFilter == other.minFilter &&
           magFilter == other.magFilter &&
           wrapS == other.wrapS &&
           wrapT == other.wrapT &&
           wrapR == other.wrapR &&
           maxAnisotropy == other.maxAnisotropy &&
           baseLevel == other.baseLevel &&
           maxLevel == other.maxLevel &&
           minLod == other.minLod &&
           maxLod == other.maxLod &&
           compareMode == other.compareMode &&
           compareFunc == other.compareFunc &&
           swizzleRed == other.swizzleRed &&
           swizzleGreen == other.swizzleGreen &&
           swizzleBlue == other.swizzleBlue &&
           swizzleAlpha == other.swizzleAlpha;
}

bool SamplerState::operator!=(const SamplerState &other) const
{
    return !(*this == other);
}

static void MinMax(int a, int b, int *minimum, int *maximum)
{
    if (a < b)
    {
        *minimum = a;
        *maximum = b;
    }
    else
    {
        *minimum = b;
        *maximum = a;
    }
}

bool ClipRectangle(const Rectangle &source, const Rectangle &clip, Rectangle *intersection)
{
    int minSourceX, maxSourceX, minSourceY, maxSourceY;
    MinMax(source.x, source.x + source.width, &minSourceX, &maxSourceX);
    MinMax(source.y, source.y + source.height, &minSourceY, &maxSourceY);

    int minClipX, maxClipX, minClipY, maxClipY;
    MinMax(clip.x, clip.x + clip.width, &minClipX, &maxClipX);
    MinMax(clip.y, clip.y + clip.height, &minClipY, &maxClipY);

    if (minSourceX >= maxClipX || maxSourceX <= minClipX || minSourceY >= maxClipY || maxSourceY <= minClipY)
    {
        if (intersection)
        {
            intersection->x = minSourceX;
            intersection->y = maxSourceY;
            intersection->width = maxSourceX - minSourceX;
            intersection->height = maxSourceY - minSourceY;
        }

        return false;
    }
    else
    {
        if (intersection)
        {
            intersection->x = std::max(minSourceX, minClipX);
            intersection->y = std::max(minSourceY, minClipY);
            intersection->width  = std::min(maxSourceX, maxClipX) - std::max(minSourceX, minClipX);
            intersection->height = std::min(maxSourceY, maxClipY) - std::max(minSourceY, minClipY);
        }

        return true;
    }
}

bool Box::operator==(const Box &other) const
{
    return (x == other.x && y == other.y && z == other.z &&
            width == other.width && height == other.height && depth == other.depth);
}

bool Box::operator!=(const Box &other) const
{
    return !(*this == other);
}

}
