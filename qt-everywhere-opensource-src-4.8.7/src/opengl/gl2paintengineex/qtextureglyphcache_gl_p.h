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

#ifndef QTEXTUREGLYPHCACHE_GL_P_H
#define QTEXTUREGLYPHCACHE_GL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <private/qtextureglyphcache_p.h>
#include <private/qgl_p.h>
#include <qglshaderprogram.h>
#include <qglframebufferobject.h>

// #define QT_GL_TEXTURE_GLYPH_CACHE_DEBUG

QT_BEGIN_NAMESPACE

class QGL2PaintEngineExPrivate;

struct QGLGlyphTexture
{
    QGLGlyphTexture(const QGLContext *ctx)
        : m_fbo(0)
        , m_width(0)
        , m_height(0)
    {
        if (ctx && QGLFramebufferObject::hasOpenGLFramebufferObjects() && !ctx->d_ptr->workaround_brokenFBOReadBack)
            glGenFramebuffers(1, &m_fbo);

#ifdef QT_GL_TEXTURE_GLYPH_CACHE_DEBUG
        qDebug(" -> QGLGlyphTexture() %p for context %p.", this, ctx);
#endif
    }

    ~QGLGlyphTexture() {
        const QGLContext *ctx = QGLContext::currentContext();
#ifdef QT_GL_TEXTURE_GLYPH_CACHE_DEBUG
        qDebug("~QGLGlyphTexture() %p for context %p.", this, ctx);
#endif
        // At this point, the context group is made current, so it's safe to
        // release resources without a makeCurrent() call
        if (ctx) {
            if (m_fbo)
                glDeleteFramebuffers(1, &m_fbo);
            if (m_width || m_height)
                glDeleteTextures(1, &m_texture);
        }
    }

    GLuint m_texture;
    GLuint m_fbo;
    int m_width;
    int m_height;
};

class Q_OPENGL_EXPORT QGLTextureGlyphCache : public QImageTextureGlyphCache, public QGLContextGroupResourceBase
{
public:
    QGLTextureGlyphCache(const QGLContext *context, QFontEngineGlyphCache::Type type, const QTransform &matrix);
    ~QGLTextureGlyphCache();

    virtual void createTextureData(int width, int height);
    virtual void resizeTextureData(int width, int height);
    virtual void fillTexture(const Coord &c, glyph_t glyph, QFixed subPixelPosition);
    virtual int glyphPadding() const;
    virtual int maxTextureWidth() const;
    virtual int maxTextureHeight() const;

    inline GLuint texture() const {
        QGLTextureGlyphCache *that = const_cast<QGLTextureGlyphCache *>(this);
        QGLGlyphTexture *glyphTexture = that->m_textureResource.value(ctx);
        return glyphTexture ? glyphTexture->m_texture : 0;
    }

    inline int width() const {
        QGLTextureGlyphCache *that = const_cast<QGLTextureGlyphCache *>(this);
        QGLGlyphTexture *glyphTexture = that->m_textureResource.value(ctx);
        return glyphTexture ? glyphTexture->m_width : 0;
    }
    inline int height() const {
        QGLTextureGlyphCache *that = const_cast<QGLTextureGlyphCache *>(this);
        QGLGlyphTexture *glyphTexture = that->m_textureResource.value(ctx);
        return glyphTexture ? glyphTexture->m_height : 0;
    }

    inline void setPaintEnginePrivate(QGL2PaintEngineExPrivate *p) { pex = p; }

    void setContext(const QGLContext *context);
    inline const QGLContext *context() const { return ctx; }

    inline int serialNumber() const { return m_serialNumber; }

    enum FilterMode {
        Nearest,
        Linear
    };
    FilterMode filterMode() const { return m_filterMode; }
    void setFilterMode(FilterMode m) { m_filterMode = m; }

    void clear();

    void contextDeleted(const QGLContext *context) {
        if (ctx == context)
            ctx = 0;
    }
    void freeResource(void *) { ctx = 0; }

private:
    QGLContextGroupResource<QGLGlyphTexture> m_textureResource;

    const QGLContext *ctx;
    QGL2PaintEngineExPrivate *pex;
    QGLShaderProgram *m_blitProgram;
    FilterMode m_filterMode;

    GLfloat m_vertexCoordinateArray[8];
    GLfloat m_textureCoordinateArray[8];

    int m_serialNumber;
};

QT_END_NAMESPACE

#endif

