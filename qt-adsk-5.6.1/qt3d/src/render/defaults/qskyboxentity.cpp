/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qskyboxentity.h"
#include "qskyboxentity_p.h"

#include <Qt3DCore/qtransform.h>
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qcullface.h>
#include <Qt3DRender/qdepthtest.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qcuboidmesh.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/qshaderprogram.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {

QSkyboxEntityPrivate::QSkyboxEntityPrivate()
    : QEntityPrivate()
    , m_effect(new QEffect())
    , m_material(new QMaterial())
    , m_skyboxTexture(new QTextureCubeMap())
    , m_gl3Shader(new QShaderProgram())
    , m_gl2es2Shader(new QShaderProgram())
    , m_gl2Technique(new QTechnique())
    , m_es2Technique(new QTechnique())
    , m_gl3Technique(new QTechnique())
    , m_gl2RenderPass(new QRenderPass())
    , m_es2RenderPass(new QRenderPass())
    , m_gl3RenderPass(new QRenderPass())
    , m_mesh(new QCuboidMesh())
    , m_transform(new Qt3DCore::QTransform())
    , m_textureParameter(new QParameter(QStringLiteral("skyboxTexture"), m_skyboxTexture))
    , m_posXImage(new QTextureImage())
    , m_posYImage(new QTextureImage())
    , m_posZImage(new QTextureImage())
    , m_negXImage(new QTextureImage())
    , m_negYImage(new QTextureImage())
    , m_negZImage(new QTextureImage())
    , m_extension(QStringLiteral(".png"))
{
}

/*!
 * \internal
 */
void QSkyboxEntityPrivate::init()
{
    m_gl3Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/skybox.vert"))));
    m_gl3Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/gl3/skybox.frag"))));
    m_gl2es2Shader->setVertexShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/skybox.vert"))));
    m_gl2es2Shader->setFragmentShaderCode(QShaderProgram::loadSource(QUrl(QStringLiteral("qrc:/shaders/es2/skybox.frag"))));

    m_gl3Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_gl3Technique->graphicsApiFilter()->setMajorVersion(3);
    m_gl3Technique->graphicsApiFilter()->setMajorVersion(1);
    m_gl3Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::CoreProfile);

    m_gl2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGL);
    m_gl2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_gl2Technique->graphicsApiFilter()->setMajorVersion(0);
    m_gl2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_es2Technique->graphicsApiFilter()->setApi(QGraphicsApiFilter::OpenGLES);
    m_es2Technique->graphicsApiFilter()->setMajorVersion(2);
    m_es2Technique->graphicsApiFilter()->setMajorVersion(0);
    m_es2Technique->graphicsApiFilter()->setProfile(QGraphicsApiFilter::NoProfile);

    m_gl3RenderPass->setShaderProgram(m_gl3Shader);
    m_gl2RenderPass->setShaderProgram(m_gl2es2Shader);
    m_es2RenderPass->setShaderProgram(m_gl2es2Shader);

    QCullFace *cullFront = new QCullFace();
    cullFront->setMode(QCullFace::Front);
    QDepthTest *depthTest = new QDepthTest();
    depthTest->setFunc(QDepthTest::LessOrEqual);

    m_gl3RenderPass->addRenderState(cullFront);
    m_gl3RenderPass->addRenderState(depthTest);
    m_gl2RenderPass->addRenderState(cullFront);
    m_gl2RenderPass->addRenderState(depthTest);
    m_es2RenderPass->addRenderState(cullFront);
    m_es2RenderPass->addRenderState(depthTest);

    m_gl3Technique->addPass(m_gl3RenderPass);
    m_gl2Technique->addPass(m_gl2RenderPass);
    m_es2Technique->addPass(m_es2RenderPass);

    m_effect->addTechnique(m_gl3Technique);
    m_effect->addTechnique(m_gl2Technique);
    m_effect->addTechnique(m_es2Technique);

    m_material->setEffect(m_effect);
    m_material->addParameter(m_textureParameter);

    m_mesh->setXYMeshResolution(QSize(2, 2));
    m_mesh->setXZMeshResolution(QSize(2, 2));
    m_mesh->setYZMeshResolution(QSize(2, 2));

    m_posXImage->setCubeMapFace(QTextureCubeMap::CubeMapPositiveX);
    m_posYImage->setCubeMapFace(QTextureCubeMap::CubeMapPositiveY);
    m_posZImage->setCubeMapFace(QTextureCubeMap::CubeMapPositiveZ);
    m_negXImage->setCubeMapFace(QTextureCubeMap::CubeMapNegativeX);
    m_negYImage->setCubeMapFace(QTextureCubeMap::CubeMapNegativeY);
    m_negZImage->setCubeMapFace(QTextureCubeMap::CubeMapNegativeZ);

    m_skyboxTexture->setMagnificationFilter(QTextureCubeMap::Linear);
    m_skyboxTexture->setMinificationFilter(QTextureCubeMap::Linear);
    m_skyboxTexture->setGenerateMipMaps(false);
    m_skyboxTexture->setWrapMode(QTextureWrapMode(QTextureWrapMode::ClampToEdge));

    m_skyboxTexture->addTextureImage(m_posXImage);
    m_skyboxTexture->addTextureImage(m_posYImage);
    m_skyboxTexture->addTextureImage(m_posZImage);
    m_skyboxTexture->addTextureImage(m_negXImage);
    m_skyboxTexture->addTextureImage(m_negYImage);
    m_skyboxTexture->addTextureImage(m_negZImage);

    q_func()->addComponent(m_mesh);
    q_func()->addComponent(m_material);
    q_func()->addComponent(m_transform);
}

/*!
 * \internal
 */
void QSkyboxEntityPrivate::reloadTexture()
{
    m_posXImage->setSource(QUrl(m_baseName + QStringLiteral("_posx") + m_extension));
    m_posYImage->setSource(QUrl(m_baseName + QStringLiteral("_posy") + m_extension));
    m_posZImage->setSource(QUrl(m_baseName + QStringLiteral("_posz") + m_extension));
    m_negXImage->setSource(QUrl(m_baseName + QStringLiteral("_negx") + m_extension));
    m_negYImage->setSource(QUrl(m_baseName + QStringLiteral("_negy") + m_extension));
    m_negZImage->setSource(QUrl(m_baseName + QStringLiteral("_negz") + m_extension));
}

/*!
 * \class Qt3DRender::QSkyboxEntity
 * \inmodule Qt3DRender
 *
 * \brief Qt3DRender::QSkyboxEntity is a convenience Qt3DCore::QEntity subclass that can
 * be used to insert a skybox in a 3D scene.
 *
 * By specifying a base name and an extension, Qt3DCore::QSkyboxEntity
 * will take care of building a TextureCubeMap to be rendered at runtime. The
 * images in the source directory should match the pattern:
 * \b base name + * "_posx|_posy|_posz|_negx|_negy|_negz" + extension
 *
 * By default the extension defaults to .png.
 *
 * \note Please note that you shouldn't try to render skybox with an
 * orthographic projection.
 *
 * \since 5.5
 */

/*!
 * Constructs a new Qt3DCore::QSkyboxEntity object with \a parent as parent.
 */
QSkyboxEntity::QSkyboxEntity(QNode *parent)
    : QEntity(*new QSkyboxEntityPrivate, parent)
{
    d_func()->init();
}

QSkyboxEntity::~QSkyboxEntity()
{
    QNode::cleanup();
}

/*!
 * Sets the base name to \a baseName.
 */
void QSkyboxEntity::setBaseName(const QString &baseName)
{
    Q_D(QSkyboxEntity);
    if (baseName != d->m_baseName) {
        d->m_baseName = baseName;
        emit sourceDirectoryChanged(baseName);
        d->reloadTexture();
    }
}
/*!
 * Returns the base name.
 */
QString QSkyboxEntity::baseName() const
{
    Q_D(const QSkyboxEntity);
    return d->m_baseName;
}

/*!
 * Sets the extension to \a extension.
 */
void QSkyboxEntity::setExtension(const QString &extension)
{
    Q_D(QSkyboxEntity);
    if (extension != d->m_extension) {
        d->m_extension = extension;
        emit extensionChanged(extension);
        d->reloadTexture();
    }
}

/*!
 * Returns the extension
 */
QString QSkyboxEntity::extension() const
{
    Q_D(const QSkyboxEntity);
    return d->m_extension;
}

/*!
 * Sets the camera position to \a cameraPosition.
 */
void QSkyboxEntity::setCameraPosition(const QVector3D &cameraPosition)
{
    Q_D(QSkyboxEntity);
    if (cameraPosition != d->m_position) {
        d->m_position = cameraPosition;
        d->m_transform->setTranslation(d->m_position);
        emit cameraPositionChanged(cameraPosition);
    }
}

/*!
 * Returns the camera postion.
 */
QVector3D QSkyboxEntity::cameraPosition() const
{
    Q_D(const QSkyboxEntity);
    return d->m_position;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
