/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenGL module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qglgradientcache_p.h"
#include <private/qdrawhelper_p.h>
#include <private/qgl_p.h>
#include <QtCore/qmutex.h>

QT_BEGIN_NAMESPACE

class QGL2GradientCacheWrapper
{
public:
    QGL2GradientCache *cacheForContext(const QGLContext *context) {
        QMutexLocker lock(&m_mutex);
        return m_resource.value(context);
    }

private:
    QGLContextGroupResource<QGL2GradientCache> m_resource;
    QMutex m_mutex;
};

Q_GLOBAL_STATIC(QGL2GradientCacheWrapper, qt_gradient_caches)

QGL2GradientCache *QGL2GradientCache::cacheForContext(const QGLContext *context)
{
    return qt_gradient_caches()->cacheForContext(context);
}

void QGL2GradientCache::cleanCache()
{
    QMutexLocker lock(&m_mutex);
    QGLGradientColorTableHash::const_iterator it = cache.constBegin();
    for (; it != cache.constEnd(); ++it) {
        const CacheInfo &cache_info = it.value();
        glDeleteTextures(1, &cache_info.texId);
    }
    cache.clear();
}

GLuint QGL2GradientCache::getBuffer(const QGradient &gradient, qreal opacity)
{
    QMutexLocker lock(&m_mutex);
    quint64 hash_val = 0;

    QGradientStops stops = gradient.stops();
    for (int i = 0; i < stops.size() && i <= 2; i++)
        hash_val += stops[i].second.rgba();

    QGLGradientColorTableHash::const_iterator it = cache.constFind(hash_val);

    if (it == cache.constEnd())
        return addCacheElement(hash_val, gradient, opacity);
    else {
        do {
            const CacheInfo &cache_info = it.value();
            if (cache_info.stops == stops && cache_info.opacity == opacity
                && cache_info.interpolationMode == gradient.interpolationMode())
            {
                return cache_info.texId;
            }
            ++it;
        } while (it != cache.constEnd() && it.key() == hash_val);
        // an exact match for these stops and opacity was not found, create new cache
        return addCacheElement(hash_val, gradient, opacity);
    }
}


GLuint QGL2GradientCache::addCacheElement(quint64 hash_val, const QGradient &gradient, qreal opacity)
{
    if (cache.size() == maxCacheSize()) {
        int elem_to_remove = qrand() % maxCacheSize();
        quint64 key = cache.keys()[elem_to_remove];

        // need to call glDeleteTextures on each removed cache entry:
        QGLGradientColorTableHash::const_iterator it = cache.constFind(key);
        do {
            glDeleteTextures(1, &it.value().texId);
        } while (++it != cache.constEnd() && it.key() == key);
        cache.remove(key); // may remove more than 1, but OK
    }

    CacheInfo cache_entry(gradient.stops(), opacity, gradient.interpolationMode());
    uint buffer[1024];
    generateGradientColorTable(gradient, buffer, paletteSize(), opacity);
    glGenTextures(1, &cache_entry.texId);
    glBindTexture(GL_TEXTURE_2D, cache_entry.texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, paletteSize(), 1,
                    0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    return cache.insert(hash_val, cache_entry).value().texId;
}


// GL's expects pixels in RGBA (when using GL_RGBA), bin-endian (ABGR on x86).
// Qt always stores in ARGB reguardless of the byte-order the mancine uses.
static inline uint qtToGlColor(uint c)
{
    uint o;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    o = (c & 0xff00ff00)  // alpha & green already in the right place
        | ((c >> 16) & 0x000000ff) // red
        | ((c << 16) & 0x00ff0000); // blue
#else //Q_BIG_ENDIAN
    o = (c << 8)
        | ((c >> 24) & 0x000000ff);
#endif // Q_BYTE_ORDER
    return o;
}

//TODO: Let GL generate the texture using an FBO
void QGL2GradientCache::generateGradientColorTable(const QGradient& gradient, uint *colorTable, int size, qreal opacity) const
{
    int pos = 0;
    QGradientStops s = gradient.stops();
    QVector<uint> colors(s.size());

    for (int i = 0; i < s.size(); ++i)
        colors[i] = s[i].second.rgba(); // Qt LIES! It returns ARGB (on little-endian AND on big-endian)

    bool colorInterpolation = (gradient.interpolationMode() == QGradient::ColorInterpolation);

    uint alpha = qRound(opacity * 256);
    uint current_color = ARGB_COMBINE_ALPHA(colors[0], alpha);
    qreal incr = 1.0 / qreal(size);
    qreal fpos = 1.5 * incr;
    colorTable[pos++] = qtToGlColor(PREMUL(current_color));

    while (fpos <= s.first().first) {
        colorTable[pos] = colorTable[pos - 1];
        pos++;
        fpos += incr;
    }

    if (colorInterpolation)
        current_color = PREMUL(current_color);

    for (int i = 0; i < s.size() - 1; ++i) {
        qreal delta = 1/(s[i+1].first - s[i].first);
        uint next_color = ARGB_COMBINE_ALPHA(colors[i+1], alpha);
        if (colorInterpolation)
            next_color = PREMUL(next_color);

        while (fpos < s[i+1].first && pos < size) {
            int dist = int(256 * ((fpos - s[i].first) * delta));
            int idist = 256 - dist;
            if (colorInterpolation)
                colorTable[pos] = qtToGlColor(INTERPOLATE_PIXEL_256(current_color, idist, next_color, dist));
            else
                colorTable[pos] = qtToGlColor(PREMUL(INTERPOLATE_PIXEL_256(current_color, idist, next_color, dist)));
            ++pos;
            fpos += incr;
        }
        current_color = next_color;
    }

    Q_ASSERT(s.size() > 0);

    uint last_color = qtToGlColor(PREMUL(ARGB_COMBINE_ALPHA(colors[s.size() - 1], alpha)));
    for (;pos < size; ++pos)
        colorTable[pos] = last_color;

    // Make sure the last color stop is represented at the end of the table
    colorTable[size-1] = last_color;
}

QT_END_NAMESPACE
