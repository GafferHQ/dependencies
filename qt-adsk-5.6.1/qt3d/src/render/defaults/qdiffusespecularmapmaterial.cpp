/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdiffusespecularmapmaterial.h"
#include "qdiffusespecularmapmaterial_p.h"

#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qshaderprogram.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <QUrl>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

QDiffuseSpecularMapMaterialPrivate::QDiffuseSpecularMapMaterialPrivate()
    : QMaterialPrivate()
    , m_diffuseSpecularMapEffect(new QEffect())
    , m_diffuseTexture(new QTexture2D())
    , m_specularTexture(new QTexture2D())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("diffuseTexture"), m_diffuseTexture))
    , m_specularParameter(new QParameter(QStringLiteral("specularTexture"), m_specularTexture))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_textureScaleParameter(new QParameter(QStringLiteral("texCoordScale"), 1.0f))
    , m_diffuseSpecularMapGL3Technique(new QTechnique())
    , m_diffuseSpecularMapGL2Technique(new QTechnique())
    , m_diffuseSpecularMapES2Technique(new QTechnique())
    , m_diffuseSpecularMapGL3RenderPass(new QRenderPass())
    , m_diffuseSpecularMapGL2RenderPass(new QRenderPass())
    , m_diffuseSpecularMapES2RenderPass(new QRenderPass())
    , m_diffuseSpecularMapGL3Shader(new QShaderProgram())
    , m_diffuseSpecularMapGL2ES2Shader(new QShaderProgram())
{
    m_diffuseTexture->setMagnificationFilter(QAbstractTextureProvider::Linear);
    m_diffuseTexture->setMinificationFilter(QAbstractTextureProvider::LinearMipMapLinear);
    m_diffuseTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_diffuseTexture->setGenerateMipMaps(true);
    m_diffuseTexture->setMaximumAnisotropy(16.0f);

    m_specularTexture->setMagnificationFilter(QAbstractTextureProvider::Linear);
    m_specularTexture->setMinificationFilter(QAbstractTextureProvider::LinearMipMapLinear);
    m_specularTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_specularTexture->setGenerateMipMaps(true);
    m_specularTexture->setMaximumAnisotropy(16.0f);
}

void QDiffuseSpecularMapMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseSpecularMapMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseSpecularMapMaterialPrivate::handleDiffuseChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseSpecularMapMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseSpecularMapMaterialPrivate::handleShininessChanged);
    connect(m_textureScaleParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QDiffuseSpecularMapMaterialPrivate::handleTextureScaleChanged);

    m_diffuseSpecularMapGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/diffusemap.vert"))));
    m_diffuseSpecularMapGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/diffusespecularmap.frag"))));
    m_diffuseSpecularMapGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/diffusemap.vert"))));
    m_diffuseSpecularMapGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/diffusespecularmap.frag"))));

    m_diffuseSpecularMapGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_diffuseSpecularMapGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_diffuseSpecularMapGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_diffuseSpecularMapGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_diffuseSpecularMapGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_diffuseSpecularMapGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_diffuseSpecularMapGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_diffuseSpecularMapGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_diffuseSpecularMapES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_diffuseSpecularMapES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_diffuseSpecularMapES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_diffuseSpecularMapES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_diffuseSpecularMapGL3RenderPass->setShaderProgram(m_diffuseSpecularMapGL3Shader);
    m_diffuseSpecularMapGL2RenderPass->setShaderProgram(m_diffuseSpecularMapGL2ES2Shader);
    m_diffuseSpecularMapES2RenderPass->setShaderProgram(m_diffuseSpecularMapGL2ES2Shader);

    m_diffuseSpecularMapGL3Technique->addPass(m_diffuseSpecularMapGL3RenderPass);
    m_diffuseSpecularMapGL2Technique->addPass(m_diffuseSpecularMapGL2RenderPass);
    m_diffuseSpecularMapES2Technique->addPass(m_diffuseSpecularMapES2RenderPass);

    m_diffuseSpecularMapEffect->addTechnique(m_diffuseSpecularMapGL3Technique);
    m_diffuseSpecularMapEffect->addTechnique(m_diffuseSpecularMapGL2Technique);
    m_diffuseSpecularMapEffect->addTechnique(m_diffuseSpecularMapES2Technique);

    m_diffuseSpecularMapEffect->addParameter(m_ambientParameter);
    m_diffuseSpecularMapEffect->addParameter(m_diffuseParameter);
    m_diffuseSpecularMapEffect->addParameter(m_specularParameter);
    m_diffuseSpecularMapEffect->addParameter(m_shininessParameter);
    m_diffuseSpecularMapEffect->addParameter(m_textureScaleParameter);

    q_func()->setEffect(m_diffuseSpecularMapEffect);
}

void QDiffuseSpecularMapMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QDiffuseSpecularMapMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QDiffuseSpecularMapMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QDiffuseSpecularMapMaterial);
    emit q->diffuseChanged(var.value<QAbstractTextureProvider *>());
}

void QDiffuseSpecularMapMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QDiffuseSpecularMapMaterial);
    emit q->specularChanged(var.value<QAbstractTextureProvider *>());
}

void QDiffuseSpecularMapMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QDiffuseSpecularMapMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QDiffuseSpecularMapMaterialPrivate::handleTextureScaleChanged(const QVariant &var)
{
    Q_Q(QDiffuseSpecularMapMaterial);
    emit q->textureScaleChanged(var.toFloat());
}

/*!
    \class Qt3DRender::QDiffuseSpecularMapMaterial
    \brief The QDiffuseSpecularMapMaterial provides a default implementation of the phong lighting and bump effect where the diffuse and specular light components
    are read from texture maps.
    \inmodule Qt3DRender
    \since 5.5

    The specular lighting effect is based on the combination of 3 lighting components ambient, diffuse and specular.
    The relative strengths of these components is controlled by means of their reflectivity coefficients which are modelled as RGB triplets:

    \list
    \li Ambient is the color that is emitted by an object without any other light source.
    \li Diffuse is the color that is emitted for rought surface reflections with the lights.
    \li Specular is the color emitted for shiny surface reflections with the lights.
    \li The shininess of a surface is controlled by a float property.
    \endlist

    This material uses an effect with a single render pass approach and performs per fragment lighting.
    Techniques are provided for OpenGL 2, OpenGL 3 or above as well as OpenGL ES 2.
*/

/*!
    Constructs a new Qt3DRender::QDiffuseSpecularMapMaterial instance with parent object \a parent.
*/
QDiffuseSpecularMapMaterial::QDiffuseSpecularMapMaterial(QNode *parent)
    : QMaterial(*new QDiffuseSpecularMapMaterialPrivate, parent)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->init();
}

/*!
    Destroys the QDiffuseSpecularMapMaterial instance.
*/
QDiffuseSpecularMapMaterial::~QDiffuseSpecularMapMaterial()
{
}

/*!
    \property Qt3DRender::QDiffuseSpecularMapMaterial::ambient

    Holds the current ambient color.
*/
QColor QDiffuseSpecularMapMaterial::ambient() const
{
    Q_D(const QDiffuseSpecularMapMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property Qt3DRender::QDiffuseSpecularMapMaterial::diffuse

    Holds the current diffuse map texture.

    By default, the diffuse texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTextureProvider *QDiffuseSpecularMapMaterial::diffuse() const
{
    Q_D(const QDiffuseSpecularMapMaterial);
    return d->m_diffuseParameter->value().value<QAbstractTextureProvider *>();
}

/*!
    \property Qt3DRender::QDiffuseSpecularMapMaterial::specular

    Holds the current specular map texture.

    By default, the specular texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTextureProvider *QDiffuseSpecularMapMaterial::specular() const
{
    Q_D(const QDiffuseSpecularMapMaterial);
    return d->m_specularParameter->value().value<QAbstractTextureProvider *>();
}

/*!
    \property Qt3DRender::QDiffuseSpecularMapMaterial::shininess

    Holds the current shininess as a float value.
*/
float QDiffuseSpecularMapMaterial::shininess() const
{
    Q_D(const QDiffuseSpecularMapMaterial);
    return d->m_shininessParameter->value().toFloat();
}

/*!
    \property Qt3DRender::QDiffuseSpecularMapMaterial::textureScale

    Holds the current texture scale as a float value.
*/
float QDiffuseSpecularMapMaterial::textureScale() const
{
    Q_D(const QDiffuseSpecularMapMaterial);
    return d->m_textureScaleParameter->value().toFloat();
}

void QDiffuseSpecularMapMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QDiffuseSpecularMapMaterial::setDiffuse(QAbstractTextureProvider *diffuse)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->m_diffuseParameter->setValue(QVariant::fromValue(diffuse));
}

void QDiffuseSpecularMapMaterial::setSpecular(QAbstractTextureProvider *specular)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->m_specularParameter->setValue(QVariant::fromValue(specular));
}

void QDiffuseSpecularMapMaterial::setShininess(float shininess)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QDiffuseSpecularMapMaterial::setTextureScale(float textureScale)
{
    Q_D(QDiffuseSpecularMapMaterial);
    d->m_textureScaleParameter->setValue(textureScale);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
