/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenVG module of the Qt Toolkit.
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

#include "qgltexturepool_p.h"
#include "qpixmapdata_gl_p.h"
#include "qgl_p.h"

QT_BEGIN_NAMESPACE

Q_OPENGL_EXPORT extern QGLWidget* qt_gl_share_widget();

static QGLTexturePool *qt_gl_texture_pool = 0;

class QGLTexturePoolPrivate
{
public:
    QGLTexturePoolPrivate() : lruFirst(0), lruLast(0) {}

    QGLTexture *lruFirst;
    QGLTexture *lruLast;
};

QGLTexturePool::QGLTexturePool()
    : d_ptr(new QGLTexturePoolPrivate())
{
}

QGLTexturePool::~QGLTexturePool()
{
}

QGLTexturePool *QGLTexturePool::instance()
{
    if (!qt_gl_texture_pool)
        qt_gl_texture_pool = new QGLTexturePool();
    return qt_gl_texture_pool;
}

GLuint QGLTexturePool::createTexture(GLenum target,
                                            GLint level,
                                            GLint internalformat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            QGLTexture *texture)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(target, tex);
    do {
        glTexImage2D(target, level, internalformat, width, height, 0, format, type, 0);
        GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            if (texture)
                moveToHeadOfLRU(texture);
            return tex;
        } else if (error != GL_OUT_OF_MEMORY) {
            qWarning("QGLTexturePool: cannot create temporary texture because of invalid params");
            return 0;
        }
    } while (reclaimSpace(internalformat, width, height, format, type, texture));
    qWarning("QGLTexturePool: cannot reclaim sufficient space for a %dx%d texture",
             width, height);
    return 0;
}

bool QGLTexturePool::createPermanentTexture(GLuint tex,
                                            GLenum target,
                                            GLint level,
                                            GLint internalformat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            const GLvoid *data)
{
    glBindTexture(target, tex);
    do {
        glTexImage2D(target, level, internalformat, width, height, 0, format, type, data);

        GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            return true;
        } else if (error != GL_OUT_OF_MEMORY) {
            qWarning("QGLTexturePool: cannot create permanent texture because of invalid params");
            return false;
        }
    } while (reclaimSpace(internalformat, width, height, format, type, 0));
    qWarning("QGLTexturePool: cannot reclaim sufficient space for a %dx%d texture",
             width, height);
    return 0;
}

void QGLTexturePool::useTexture(QGLTexture *texture)
{
    moveToHeadOfLRU(texture);
    texture->inTexturePool = true;
}

void QGLTexturePool::detachTexture(QGLTexture *texture)
{
    removeFromLRU(texture);
    texture->inTexturePool = false;
}

bool QGLTexturePool::reclaimSpace(GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    QGLTexture *texture)
{
    Q_UNUSED(internalformat);   // For future use in picking the best texture to eject.
    Q_UNUSED(width);
    Q_UNUSED(height);
    Q_UNUSED(format);
    Q_UNUSED(type);

    bool succeeded = false;
    bool wasInLRU = false;
    if (texture) {
        wasInLRU = texture->inLRU;
        moveToHeadOfLRU(texture);
    }

    QGLTexture *lrutexture = textureLRU();
    if (lrutexture && lrutexture != texture) {
        if (lrutexture->boundPixmap)
            lrutexture->boundPixmap->reclaimTexture();
        else
            QGLTextureCache::instance()->remove(lrutexture->boundKey);
        succeeded = true;
    }

    if (texture && !wasInLRU)
        removeFromLRU(texture);

    return succeeded;
}

void QGLTexturePool::hibernate()
{
    Q_D(QGLTexturePool);
    QGLTexture *texture = d->lruLast;
    while (texture) {
        QGLTexture *prevLRU = texture->prevLRU;
        texture->inTexturePool = false;
        texture->inLRU = false;
        texture->nextLRU = 0;
        texture->prevLRU = 0;
        if (texture->boundPixmap)
            texture->boundPixmap->hibernate();
        else
            QGLTextureCache::instance()->remove(texture->boundKey);
        texture = prevLRU;
    }
    d->lruFirst = 0;
    d->lruLast = 0;
}

void QGLTexturePool::moveToHeadOfLRU(QGLTexture *texture)
{
    Q_D(QGLTexturePool);
    if (texture->inLRU) {
        if (!texture->prevLRU)
            return;     // Already at the head of the list.
        removeFromLRU(texture);
    }
    texture->inLRU = true;
    texture->nextLRU = d->lruFirst;
    texture->prevLRU = 0;
    if (d->lruFirst)
        d->lruFirst->prevLRU = texture;
    else
        d->lruLast = texture;
    d->lruFirst = texture;
}

void QGLTexturePool::removeFromLRU(QGLTexture *texture)
{
    Q_D(QGLTexturePool);
    if (!texture->inLRU)
        return;
    if (texture->nextLRU)
        texture->nextLRU->prevLRU = texture->prevLRU;
    else
        d->lruLast = texture->prevLRU;
    if (texture->prevLRU)
        texture->prevLRU->nextLRU = texture->nextLRU;
    else
        d->lruFirst = texture->nextLRU;
    texture->inLRU = false;
}

QGLTexture *QGLTexturePool::textureLRU()
{
    Q_D(QGLTexturePool);
    return d->lruLast;
}

QT_END_NAMESPACE
