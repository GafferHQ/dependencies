/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "textureblitter.h"

#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES           0x8D65
#endif

QT_BEGIN_NAMESPACE

TextureBlitter::TextureBlitter()
    : m_shaderProgram(new QOpenGLShaderProgram)
    , m_shaderProgramExternal(new QOpenGLShaderProgram)
    , m_currentProgram(0)
    , m_currentTarget(GL_TEXTURE_2D)
{
    static const char *textureVertexProgram =
            "uniform highp mat4 matrix;\n"
            "attribute highp vec3 vertexCoordEntry;\n"
            "attribute highp vec2 textureCoordEntry;\n"
            "varying highp vec2 textureCoord;\n"
            "void main() {\n"
            "   textureCoord = textureCoordEntry;\n"
            "   gl_Position = matrix * vec4(vertexCoordEntry, 1);\n"
            "}\n";

    static const char *textureFragmentProgram =
            "uniform sampler2D texture;\n"
            "varying highp vec2 textureCoord;\n"
            "void main() {\n"
            "   gl_FragColor = texture2D(texture, textureCoord);\n"
            "}\n";

    static const char *textureFragmentProgramExternal =
        "#extension GL_OES_EGL_image_external : require\n"
        "uniform samplerExternalOES texture;\n"
        "varying highp vec2 textureCoord;\n"
        "void main() {\n"
        "   gl_FragColor = texture2D(texture, textureCoord);\n"
        "}\n";

    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, textureVertexProgram);
    m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, textureFragmentProgram);
    m_shaderProgram->link();

    m_shaderProgramExternal->addShaderFromSourceCode(QOpenGLShader::Vertex, textureVertexProgram);
    m_shaderProgramExternal->addShaderFromSourceCode(QOpenGLShader::Fragment, textureFragmentProgramExternal);
    m_shaderProgramExternal->link();
}

TextureBlitter::~TextureBlitter()
{
    delete m_shaderProgram;
    delete m_shaderProgramExternal;
}

void TextureBlitter::bind(quint32 target)
{
    m_currentTarget = target;
    switch (target) {
    case GL_TEXTURE_2D:
        m_currentProgram = m_shaderProgram;
        break;
    case GL_TEXTURE_EXTERNAL_OES:
        m_currentProgram = m_shaderProgramExternal;
        break;
    default:
        qFatal("INVALID TARGET TYPE %d", target);
        break;
    }

    m_currentProgram->bind();

    m_vertexCoordEntry = m_shaderProgram->attributeLocation("vertexCoordEntry");
    m_textureCoordEntry = m_shaderProgram->attributeLocation("textureCoordEntry");
    m_matrixLocation = m_shaderProgram->uniformLocation("matrix");

    //Enable transparent windows
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TextureBlitter::release()
{
    m_currentProgram->release();
}

void TextureBlitter::drawTexture(int textureId, const QRectF &targetRect, const QSize &targetSize, int depth, bool targethasInvertedY, bool sourceHasInvertedY)
{

    glViewport(0,0,targetSize.width(),targetSize.height());
    GLfloat zValue = depth / 1000.0f;
    //Set Texture and Vertex coordinates
    const GLfloat textureCoordinates[] = {
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };

    GLfloat x1 = targetRect.left();
    GLfloat x2 = targetRect.right();
    GLfloat y1, y2;
    if (targethasInvertedY) {
        if (sourceHasInvertedY) {
            y1 = targetRect.top();
            y2 = targetRect.bottom();
        } else {
            y1 = targetRect.bottom();
            y2 = targetRect.top();
        }
    } else {
        if (sourceHasInvertedY) {
            y1 = targetSize.height() - targetRect.top();
            y2 = targetSize.height() - targetRect.bottom();
        } else {
            y1 = targetSize.height() - targetRect.bottom();
            y2 = targetSize.height() - targetRect.top();
        }
    }

    const GLfloat vertexCoordinates[] = {
        GLfloat(x1), GLfloat(y1), zValue,
        GLfloat(x2), GLfloat(y1), zValue,
        GLfloat(x2), GLfloat(y2), zValue,
        GLfloat(x1), GLfloat(y2), zValue
    };

    //Set matrix to transfrom geometry values into gl coordinate space.
    m_transformMatrix.setToIdentity();
    m_transformMatrix.scale( 2.0f / targetSize.width(), 2.0f / targetSize.height() );
    m_transformMatrix.translate(-targetSize.width() / 2.0f, -targetSize.height() / 2.0f);

    //attach the data!
    QOpenGLContext *currentContext = QOpenGLContext::currentContext();
    currentContext->functions()->glEnableVertexAttribArray(m_vertexCoordEntry);
    currentContext->functions()->glEnableVertexAttribArray(m_textureCoordEntry);

    currentContext->functions()->glVertexAttribPointer(m_vertexCoordEntry, 3, GL_FLOAT, GL_FALSE, 0, vertexCoordinates);
    currentContext->functions()->glVertexAttribPointer(m_textureCoordEntry, 2, GL_FLOAT, GL_FALSE, 0, textureCoordinates);

    m_currentProgram->setUniformValue(m_matrixLocation, m_transformMatrix);

    glBindTexture(m_currentTarget, textureId);

    glTexParameterf(m_currentTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(m_currentTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindTexture(m_currentTarget, 0);

    currentContext->functions()->glDisableVertexAttribArray(m_vertexCoordEntry);
    currentContext->functions()->glDisableVertexAttribArray(m_textureCoordEntry);
}

QT_END_NAMESPACE
