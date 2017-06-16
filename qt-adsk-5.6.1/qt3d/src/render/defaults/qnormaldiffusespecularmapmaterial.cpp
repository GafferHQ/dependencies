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

#include "qnormaldiffusespecularmapmaterial.h"
#include "qnormaldiffusespecularmapmaterial_p.h"
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

QNormalDiffuseSpecularMapMaterialPrivate::QNormalDiffuseSpecularMapMaterialPrivate()
    : QMaterialPrivate()
    , m_normalDiffuseSpecularEffect(new QEffect())
    , m_diffuseTexture(new QTexture2D())
    , m_normalTexture(new QTexture2D())
    , m_specularTexture(new QTexture2D())
    , m_ambientParameter(new QParameter(QStringLiteral("ka"), QColor::fromRgbF(0.05f, 0.05f, 0.05f, 1.0f)))
    , m_diffuseParameter(new QParameter(QStringLiteral("diffuseTexture"), m_diffuseTexture))
    , m_normalParameter(new QParameter(QStringLiteral("normalTexture"), m_normalTexture))
    , m_specularParameter(new QParameter(QStringLiteral("specularTexture"), m_specularTexture))
    , m_shininessParameter(new QParameter(QStringLiteral("shininess"), 150.0f))
    , m_textureScaleParameter(new QParameter(QStringLiteral("texCoordScale"), 1.0f))
    , m_normalDiffuseSpecularGL3Technique(new QTechnique())
    , m_normalDiffuseSpecularGL2Technique(new QTechnique())
    , m_normalDiffuseSpecularES2Technique(new QTechnique())
    , m_normalDiffuseSpecularGL3RenderPass(new QRenderPass())
    , m_normalDiffuseSpecularGL2RenderPass(new QRenderPass())
    , m_normalDiffuseSpecularES2RenderPass(new QRenderPass())
    , m_normalDiffuseSpecularGL3Shader(new QShaderProgram())
    , m_normalDiffuseSpecularGL2ES2Shader(new QShaderProgram())
{
    m_diffuseTexture->setMagnificationFilter(QAbstractTextureProvider::Linear);
    m_diffuseTexture->setMinificationFilter(QAbstractTextureProvider::LinearMipMapLinear);
    m_diffuseTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_diffuseTexture->setGenerateMipMaps(true);
    m_diffuseTexture->setMaximumAnisotropy(16.0f);

    m_normalTexture->setMagnificationFilter(QAbstractTextureProvider::Linear);
    m_normalTexture->setMinificationFilter(QAbstractTextureProvider::Linear);
    m_normalTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_normalTexture->setMaximumAnisotropy(16.0f);

    m_specularTexture->setMagnificationFilter(QAbstractTextureProvider::Linear);
    m_specularTexture->setMinificationFilter(QAbstractTextureProvider::LinearMipMapLinear);
    m_specularTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::Repeat));
    m_specularTexture->setGenerateMipMaps(true);
    m_specularTexture->setMaximumAnisotropy(16.0f);
}

void QNormalDiffuseSpecularMapMaterialPrivate::init()
{
    connect(m_ambientParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleAmbientChanged);
    connect(m_diffuseParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleDiffuseChanged);
    connect(m_normalParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleNormalChanged);
    connect(m_specularParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleSpecularChanged);
    connect(m_shininessParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleShininessChanged);
    connect(m_textureScaleParameter, &Qt3DRender::QParameter::valueChanged,
            this, &QNormalDiffuseSpecularMapMaterialPrivate::handleTextureScaleChanged);

    m_normalDiffuseSpecularGL3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/normaldiffusemap.vert"))));
    m_normalDiffuseSpecularGL3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/normaldiffusespecularmap.frag"))));
    m_normalDiffuseSpecularGL2ES2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusemap.vert"))));
    m_normalDiffuseSpecularGL2ES2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/normaldiffusespecularmap.frag"))));

    m_normalDiffuseSpecularGL3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_normalDiffuseSpecularGL3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_normalDiffuseSpecularGL3Technique->graphicsApiFilter()->setMinorVersion(1);
    m_normalDiffuseSpecularGL3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_normalDiffuseSpecularGL2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_normalDiffuseSpecularGL2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_normalDiffuseSpecularGL2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_normalDiffuseSpecularGL2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_normalDiffuseSpecularES2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_normalDiffuseSpecularES2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_normalDiffuseSpecularES2Technique->graphicsApiFilter()->setMinorVersion(0);
    m_normalDiffuseSpecularES2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_normalDiffuseSpecularGL3RenderPass->setShaderProgram(m_normalDiffuseSpecularGL3Shader);
    m_normalDiffuseSpecularGL2RenderPass->setShaderProgram(m_normalDiffuseSpecularGL2ES2Shader);
    m_normalDiffuseSpecularES2RenderPass->setShaderProgram(m_normalDiffuseSpecularGL2ES2Shader);

    m_normalDiffuseSpecularGL3Technique->addPass(m_normalDiffuseSpecularGL3RenderPass);
    m_normalDiffuseSpecularGL2Technique->addPass(m_normalDiffuseSpecularGL2RenderPass);
    m_normalDiffuseSpecularES2Technique->addPass(m_normalDiffuseSpecularES2RenderPass);

    m_normalDiffuseSpecularEffect->addTechnique(m_normalDiffuseSpecularGL3Technique);
    m_normalDiffuseSpecularEffect->addTechnique(m_normalDiffuseSpecularGL2Technique);
    m_normalDiffuseSpecularEffect->addTechnique(m_normalDiffuseSpecularES2Technique);

    m_normalDiffuseSpecularEffect->addParameter(m_ambientParameter);
    m_normalDiffuseSpecularEffect->addParameter(m_diffuseParameter);
    m_normalDiffuseSpecularEffect->addParameter(m_normalParameter);
    m_normalDiffuseSpecularEffect->addParameter(m_specularParameter);
    m_normalDiffuseSpecularEffect->addParameter(m_shininessParameter);
    m_normalDiffuseSpecularEffect->addParameter(m_textureScaleParameter);

    q_func()->setEffect(m_normalDiffuseSpecularEffect);
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleAmbientChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->ambientChanged(var.value<QColor>());
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleDiffuseChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->diffuseChanged(var.value<QAbstractTextureProvider *>());
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleNormalChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->normalChanged(var.value<QAbstractTextureProvider *>());
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleSpecularChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->specularChanged(var.value<QAbstractTextureProvider *>());
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleShininessChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->shininessChanged(var.toFloat());
}

void QNormalDiffuseSpecularMapMaterialPrivate::handleTextureScaleChanged(const QVariant &var)
{
    Q_Q(QNormalDiffuseSpecularMapMaterial);
    emit q->textureScaleChanged(var.toFloat());
}

/*!
    \class Qt3DRender::QNormalDiffuseSpecularMapMaterial
    \brief The QNormalDiffuseSpecularMapMaterial provides a default implementation of the phong lighting and bump effect where the diffuse and specular light components
    are read from texture maps and the normals of the mesh being rendered from a normal texture map.
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
    Constructs a new Qt3DRender::QNormalDiffuseSpecularMapMaterial instance with parent object \a parent.
*/
QNormalDiffuseSpecularMapMaterial::QNormalDiffuseSpecularMapMaterial(QNode *parent)
    : QMaterial(*new QNormalDiffuseSpecularMapMaterialPrivate, parent)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->init();
}

/*! \internal */
QNormalDiffuseSpecularMapMaterial::QNormalDiffuseSpecularMapMaterial(QNormalDiffuseSpecularMapMaterialPrivate &dd, QNode *parent)
    : QMaterial(dd, parent)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->init();
}

/*!
    Destroys the Qt3DRender::QNormalDiffuseSpecularMapMaterial instance.
*/
QNormalDiffuseSpecularMapMaterial::~QNormalDiffuseSpecularMapMaterial()
{
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::ambient

    Holds the current ambient color.
*/
QColor QNormalDiffuseSpecularMapMaterial::ambient() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_ambientParameter->value().value<QColor>();
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::diffuse

    Holds the current diffuse map texture.

    By default, the diffuse texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTextureProvider *QNormalDiffuseSpecularMapMaterial::diffuse() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_diffuseParameter->value().value<QAbstractTextureProvider *>();
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::normal

    Holds the current normal map texture.

    By default, the normal texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTextureProvider *QNormalDiffuseSpecularMapMaterial::normal() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_normalParameter->value().value<QAbstractTextureProvider *>();
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::specular

    Holds the current specular map texture.

    By default, the specular texture has the following properties:

    \list
        \li Linear minification and magnification filters
        \li Linear mipmap with mipmapping enabled
        \li Repeat wrap mode
        \li Maximum anisotropy of 16.0
    \endlist
*/
QAbstractTextureProvider *QNormalDiffuseSpecularMapMaterial::specular() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_specularParameter->value().value<QAbstractTextureProvider *>();
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::shininess

    Holds the current shininess as a float value.
*/
float QNormalDiffuseSpecularMapMaterial::shininess() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_shininessParameter->value().toFloat();
}

/*!
    \property Qt3DRender::QNormalDiffuseSpecularMapMaterial::textureScale

    Holds the current texture scale as a float value.
*/
float QNormalDiffuseSpecularMapMaterial::textureScale() const
{
    Q_D(const QNormalDiffuseSpecularMapMaterial);
    return d->m_textureScaleParameter->value().toFloat();
}

void QNormalDiffuseSpecularMapMaterial::setAmbient(const QColor &ambient)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_ambientParameter->setValue(ambient);
}

void QNormalDiffuseSpecularMapMaterial::setDiffuse(QAbstractTextureProvider *diffuse)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_diffuseParameter->setValue(QVariant::fromValue(diffuse));
}

void QNormalDiffuseSpecularMapMaterial::setNormal(QAbstractTextureProvider *normal)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_normalParameter->setValue(QVariant::fromValue(normal));
}

void QNormalDiffuseSpecularMapMaterial::setSpecular(QAbstractTextureProvider *specular)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_specularParameter->setValue(QVariant::fromValue(specular));
}

void QNormalDiffuseSpecularMapMaterial::setShininess(float shininess)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_shininessParameter->setValue(shininess);
}

void QNormalDiffuseSpecularMapMaterial::setTextureScale(float textureScale)
{
    Q_D(QNormalDiffuseSpecularMapMaterial);
    d->m_textureScaleParameter->setValue(textureScale);
}

} // namespace Qt3DRender

QT_END_NAMESPACE
