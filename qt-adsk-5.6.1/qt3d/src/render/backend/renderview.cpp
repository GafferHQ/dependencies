/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "renderview_p.h"
#include <Qt3DRender/qmaterial.h>
#include <Qt3DRender/qrenderaspect.h>
#include <Qt3DRender/qrendertarget.h>
#include <Qt3DRender/qlight.h>
#include <Qt3DRender/private/sphere_p.h>

#include <Qt3DRender/private/cameraselectornode_p.h>
#include <Qt3DRender/private/framegraphnode_p.h>
#include <Qt3DRender/private/layerfilternode_p.h>
#include <Qt3DRender/private/qparameter_p.h>
#include <Qt3DRender/private/cameralens_p.h>
#include <Qt3DRender/private/rendercommand_p.h>
#include <Qt3DRender/private/effect_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/layer_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/private/renderpassfilternode_p.h>
#include <Qt3DRender/private/renderpass_p.h>
#include <Qt3DRender/private/geometryrenderer_p.h>
#include <Qt3DRender/private/renderstateset_p.h>
#include <Qt3DRender/private/techniquefilternode_p.h>
#include <Qt3DRender/private/viewportnode_p.h>

#include <Qt3DRender/qparametermapping.h>

#include <Qt3DCore/qentity.h>

#include <QtGui/qsurface.h>

#include <algorithm>

#include <QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

static const int MAX_LIGHTS = 8;

namespace  {

// TODO: Should we treat lack of layer data as implicitly meaning that an
// entity is in all layers?
bool isEntityInLayers(const Entity *entity, const QStringList &layers)
{
    if (layers.isEmpty())
        return true;

    QList<Layer *> renderLayers = entity->renderComponents<Layer>();
    Q_FOREACH (Layer *layer, renderLayers) {
        if (layer->isEnabled())
            Q_FOREACH (const QString &layerName, layer->layers())
                if (layers.contains(layerName))
                    return true;
    }
    return false;
}

bool isEntityFrustumCulled(const Entity *entity, const Plane *planes)
{
    const Sphere *s = entity->worldBoundingVolumeWithChildren();
    for (int i = 0; i < 6; ++i) {
        if (QVector3D::dotProduct(s->center(), planes[i].normal) + planes[i].d < -s->radius())
            return true;
    }
    return false;
}

void destroyUniformValue(const QUniformValue *value, Qt3DCore::QFrameAllocator *allocator)
{
    QUniformValue *v = const_cast<QUniformValue *>(value);
    if (v->isTexture())
        allocator->deallocate(static_cast<TextureUniform *>(v));
    else
        allocator->deallocate(v);
}

} // anonymouse namespace

RenderView::StandardUniformsPFuncsHash RenderView::ms_standardUniformSetters = RenderView::initializeStandardUniformSetters();
QStringList RenderView::ms_standardAttributesNames = RenderView::initializeStandardAttributeNames();

RenderView::StandardUniformsPFuncsHash RenderView::initializeStandardUniformSetters()
{
    RenderView::StandardUniformsPFuncsHash setters;

    setters.insert(QStringLiteral("modelMatrix"), &RenderView::modelMatrix);
    setters.insert(QStringLiteral("viewMatrix"), &RenderView::viewMatrix);
    setters.insert(QStringLiteral("projectionMatrix"), &RenderView::projectionMatrix);
    setters.insert(QStringLiteral("modelView"), &RenderView::modelViewMatrix);
    setters.insert(QStringLiteral("modelViewProjection"), &RenderView::modelViewProjectionMatrix);
    setters.insert(QStringLiteral("mvp"), &RenderView::modelViewProjectionMatrix);
    setters.insert(QStringLiteral("inverseModelMatrix"), &RenderView::inverseModelMatrix);
    setters.insert(QStringLiteral("inverViewMatrix"), &RenderView::inverseViewMatrix);
    setters.insert(QStringLiteral("inverseProjectionMatrix"), &RenderView::inverseProjectionMatrix);
    setters.insert(QStringLiteral("inverseModelView"), &RenderView::inverseModelViewMatrix);
    setters.insert(QStringLiteral("inverseModelViewProjection"), &RenderView::inverseModelViewProjectionMatrix);
    setters.insert(QStringLiteral("modelNormalMatrix"), &RenderView::modelNormalMatrix);
    setters.insert(QStringLiteral("modelViewNormal"), &RenderView::modelViewNormalMatrix);
    setters.insert(QStringLiteral("viewportMatrix"), &RenderView::viewportMatrix);
    setters.insert(QStringLiteral("inverseViewportMatrix"), &RenderView::inverseViewportMatrix);
    setters.insert(QStringLiteral("time"), &RenderView::time);
    setters.insert(QStringLiteral("eyePosition"), &RenderView::eyePosition);

    return setters;
}

QStringList RenderView::initializeStandardAttributeNames()
{
    QStringList attributesNames;

    attributesNames << QAttribute::defaultPositionAttributeName();
    attributesNames << QAttribute::defaultTextureCoordinateAttributeName();
    attributesNames << QAttribute::defaultNormalAttributeName();
    attributesNames << QAttribute::defaultColorAttributeName();
    attributesNames << QAttribute::defaultTangentAttributeName();

    return attributesNames;
}

QUniformValue *RenderView::modelMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(model, m_allocator);
}

QUniformValue *RenderView::viewMatrix(const QMatrix4x4 &) const
{
    return QUniformValue::fromVariant(*m_data->m_viewMatrix, m_allocator);
}

QUniformValue *RenderView::projectionMatrix(const QMatrix4x4 &) const
{
    return QUniformValue::fromVariant(m_data->m_renderCamera->projection(), m_allocator);
}

QUniformValue *RenderView::modelViewMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(*m_data->m_viewMatrix * model, m_allocator);
}

QUniformValue *RenderView::modelViewProjectionMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(*m_data->m_viewProjectionMatrix * model, m_allocator);
}

QUniformValue *RenderView::inverseModelMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(model.inverted(), m_allocator);
}

QUniformValue *RenderView::inverseViewMatrix(const QMatrix4x4 &) const
{
    return QUniformValue::fromVariant(m_data->m_viewMatrix->inverted(), m_allocator);
}

QUniformValue *RenderView::inverseProjectionMatrix(const QMatrix4x4 &) const
{
    QMatrix4x4 projection;
    if (m_data->m_renderCamera)
        projection = m_data->m_renderCamera->projection();
    return QUniformValue::fromVariant(projection.inverted(), m_allocator);
}

QUniformValue *RenderView::inverseModelViewMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant((*m_data->m_viewMatrix * model).inverted(), m_allocator);
}

QUniformValue *RenderView::inverseModelViewProjectionMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant((*m_data->m_viewProjectionMatrix * model).inverted(0), m_allocator);
}

QUniformValue *RenderView::modelNormalMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(QVariant::fromValue(model.normalMatrix()), m_allocator);
}

QUniformValue *RenderView::modelViewNormalMatrix(const QMatrix4x4 &model) const
{
    return QUniformValue::fromVariant(QVariant::fromValue((*m_data->m_viewMatrix * model).normalMatrix()), m_allocator);
}

// TODO: Move this somewhere global where GraphicsContext::setViewport() can use it too
static QRectF resolveViewport(const QRectF &fractionalViewport, const QSize &surfaceSize)
{
    return QRectF(fractionalViewport.x() * surfaceSize.width(),
                  (1.0 - fractionalViewport.y() - fractionalViewport.height()) * surfaceSize.height(),
                  fractionalViewport.width() * surfaceSize.width(),
                  fractionalViewport.height() * surfaceSize.height());
}

QUniformValue *RenderView::viewportMatrix(const QMatrix4x4 &model) const
{
    // TODO: Can we avoid having to pass the model matrix in to these functions?
    Q_UNUSED(model);
    QMatrix4x4 viewportMatrix;
    viewportMatrix.viewport(resolveViewport(*m_viewport, m_surfaceSize));
    return QUniformValue::fromVariant(QVariant::fromValue(viewportMatrix), m_allocator);

}

QUniformValue *RenderView::inverseViewportMatrix(const QMatrix4x4 &model) const
{
    Q_UNUSED(model);
    QMatrix4x4 viewportMatrix;
    viewportMatrix.viewport(resolveViewport(*m_viewport, m_surfaceSize));
    QMatrix4x4 inverseViewportMatrix = viewportMatrix.inverted();
    return QUniformValue::fromVariant(QVariant::fromValue(inverseViewportMatrix), m_allocator);

}

QUniformValue *RenderView::time(const QMatrix4x4 &model) const
{
    Q_UNUSED(model);
    qint64 time = m_renderer->time();
    float t = time / 1000000000.0f;
    return QUniformValue::fromVariant(QVariant(t), m_allocator);
}

QUniformValue *RenderView::eyePosition(const QMatrix4x4 &model) const
{
    Q_UNUSED(model);
    return QUniformValue::fromVariant(QVariant::fromValue(m_data->m_eyePos), m_allocator);
}

RenderView::RenderView()
    : m_renderer(Q_NULLPTR)
    , m_allocator(Q_NULLPTR)
    , m_data(Q_NULLPTR)
    , m_clearColor(Q_NULLPTR)
    , m_viewport(Q_NULLPTR)
    , m_clearBuffer(QClearBuffer::None)
    , m_stateSet(Q_NULLPTR)
    , m_noDraw(false)
    , m_frustumCulling(false)
{
}

RenderView::~RenderView()
{
    if (m_allocator == Q_NULLPTR) // Mainly needed for unit tests
        return;

    Q_FOREACH (RenderCommand *command, m_commands) {
        // Deallocate all uniform values of the QUniformPack of each RenderCommand
        const QHash<QString, const QUniformValue* > uniforms = command->m_uniforms.uniforms();
        const QHash<QString, const QUniformValue* >::const_iterator end = uniforms.constEnd();
        QHash<QString, const QUniformValue* >::const_iterator it = uniforms.constBegin();

        for (; it != end; ++it)
            destroyUniformValue(it.value(), m_allocator);

        if (command->m_stateSet != Q_NULLPTR) // We do not delete the RenderState as that is stored statically
            m_allocator->deallocate<RenderStateSet>(command->m_stateSet);

        // Deallocate RenderCommand
        m_allocator->deallocate<RenderCommand>(command);
    }

    // Deallocate viewMatrix/viewProjectionMatrix
    m_allocator->deallocate<QMatrix4x4>(m_data->m_viewMatrix);
    m_allocator->deallocate<QMatrix4x4>(m_data->m_viewProjectionMatrix);
    // Deallocate viewport rect
    m_allocator->deallocate<QRectF>(m_viewport);
    // Deallocate clearColor
    m_allocator->deallocate<QColor>(m_clearColor);
    // Deallocate m_data
    m_allocator->deallocate<InnerData>(m_data);
    // Deallocate m_stateSet
    if (m_stateSet)
        m_allocator->deallocate<RenderStateSet>(m_stateSet);
}

// We need to overload the delete operator so that when the Renderer deletes the list of RenderViews, each RenderView
// can clear itself with the frame allocator and deletes its RenderViews
void RenderView::operator delete(void *ptr)
{
    RenderView *rView = static_cast<RenderView *>(ptr);
    if (rView != Q_NULLPTR && rView->m_allocator != Q_NULLPTR)
        rView->m_allocator->deallocateRawMemory(rView, sizeof(RenderView));
}

// Since placement new is used we need a matching operator delete, at least MSVC complains otherwise
void RenderView::operator delete(void *ptr, void *)
{
    RenderView *rView = static_cast<RenderView *>(ptr);
    if (rView != Q_NULLPTR && rView->m_allocator != Q_NULLPTR)
        rView->m_allocator->deallocateRawMemory(rView, sizeof(RenderView));
}

void RenderView::sort()
{
    // Compares the bitsetKey of the RenderCommands
    // Key[Depth | StateCost | Shader]
    std::sort(m_commands.begin(), m_commands.end(), compareCommands);

    // Minimize uniform changes
    int i = 0;
    while (i < m_commands.size()) {
        int j = i;

        // Advance while commands share the same shader
        while (i < m_commands.size() && m_commands[j]->m_shaderDna == m_commands[i]->m_shaderDna)
            ++i;

        if (i - j > 0) { // Several commands have the same shader, so we minimize uniform changes
            QHash<QString, const QUniformValue *> cachedUniforms = m_commands[j++]->m_uniforms.uniforms();

            while (j < i) {
                QHash<QString, const QUniformValue *> &uniforms = m_commands[j]->m_uniforms.m_uniforms;
                QHash<QString, const QUniformValue *>::iterator it = uniforms.begin();

                while (it != uniforms.end()) {
                    bool found = false;
                    // We are comparing the values:
                    // - raw uniform values
                    // - the texture Node id if the uniform represents a texture
                    // since all textures are assigned texture units before the RenderCommands
                    // sharing the same material (shader) are rendered, we can't have the case
                    // where two uniforms, referencing the same texture eventually have 2 different
                    // texture unit values
                    if (cachedUniforms.contains(it.key())) {
                        const QUniformValue *refValue = cachedUniforms.value(it.key());
                        if (*const_cast<QUniformValue *>(refValue) == *it.value()) {
                            destroyUniformValue(it.value(), m_allocator);
                            it = uniforms.erase(it);
                            found = true;
                        }
                    }
                    if (!found) {
                        cachedUniforms.insert(it.key(), it.value());
                        ++it;
                    }
                }
                ++j;
            }
        }
        ++i;
    }
}

void RenderView::setRenderer(Renderer *renderer)
{
    m_renderer = renderer;
    m_manager = renderer->nodeManagers();
    m_data->m_uniformBlockBuilder.shaderDataManager = m_manager->shaderDataManager();
}

void RenderView::gatherLights(Entity *node)
{
    const QList<Light *> lights = node->renderComponents<Light>();
    if (!lights.isEmpty())
        m_lightSources.append(LightSource(node, lights));

    // Traverse children
    Q_FOREACH (Entity *child, node->children())
        gatherLights(child);
}

class LightSourceCompare
{
public:
    LightSourceCompare(Entity *node) { p = node->worldBoundingVolume()->center(); }
    bool operator()(const RenderView::LightSource &a, const RenderView::LightSource &b) const {
        const float distA = p.distanceToPoint(a.entity->worldBoundingVolume()->center());
        const float distB = p.distanceToPoint(b.entity->worldBoundingVolume()->center());
        return distA < distB;
    }

private:
    QVector3D p;
};

// Tries to order renderCommand by shader so as to minimize shader changes
void RenderView::buildRenderCommands(Entity *node, const Plane *planes)
{
    // Skip branches that are not enabled
    if (!node->isEnabled())
        return;

    if (m_frustumCulling && isEntityFrustumCulled(node, planes))
        return;

    // Build renderCommand for current node
    if (isEntityInLayers(node, m_data->m_layers)) {
        GeometryRenderer *geometryRenderer = Q_NULLPTR;
        if (node->componentHandle<GeometryRenderer, 16>() != HGeometryRenderer()
                && (geometryRenderer = node->renderComponent<GeometryRenderer>()) != Q_NULLPTR
                && geometryRenderer->isEnabled()) {

            // There is a geometry renderer
            if (geometryRenderer != Q_NULLPTR && !geometryRenderer->geometryId().isNull()) {
                // TO DO: Perform culling here
                // As shaders may be deforming, transforming the mesh
                // We might want to make that optional or dependent on an explicit bounding box item

                // Find the material, effect, technique and set of render passes to use
                Material *material = Q_NULLPTR;
                Effect *effect = Q_NULLPTR;
                if ((material = node->renderComponent<Material>()) != Q_NULLPTR && material->isEnabled())
                    effect = m_renderer->nodeManagers()->effectManager()->lookupResource(material->effect());
                Technique *technique = findTechniqueForEffect(m_renderer, this, effect);

                ParameterInfoList parameters;
                // Order set:
                // 1 Pass Filter
                // 2 Technique Filter
                // 3 Material
                // 4 Effect
                // 5 Technique
                // 6 RenderPass

                // Add Parameters define in techniqueFilter and passFilter
                // passFilter have priority over techniqueFilter
                if (m_data->m_passFilter)
                    parametersFromParametersProvider(&parameters, m_renderer->nodeManagers()->parameterManager(),
                                                     m_data->m_passFilter);
                if (m_data->m_techniqueFilter)
                    parametersFromParametersProvider(&parameters, m_renderer->nodeManagers()->parameterManager(),
                                                     m_data->m_techniqueFilter);

                RenderRenderPassList passes;
                if (technique) {
                    passes = findRenderPassesForTechnique(m_manager, this, technique);
                } else {
                    material = m_manager->data<Material, MaterialManager>(m_renderer->defaultMaterialHandle());
                    effect = m_manager->data<Effect, EffectManager>(m_renderer->defaultEffectHandle());
                    technique = m_manager->data<Technique, TechniqueManager>(m_renderer->defaultTechniqueHandle());
                    passes << m_manager->data<RenderPass, RenderPassManager>(m_renderer->defaultRenderPassHandle());
                }

                // Get the parameters for our selected rendering setup (override what was defined in the technique/pass filter)
                parametersFromMaterialEffectTechnique(&parameters, m_manager->parameterManager(), material, effect, technique);

                // 1 RenderCommand per RenderPass pass on an Entity with a Mesh
                m_commands.reserve(m_commands.size() + passes.size());
                Q_FOREACH (RenderPass *pass, passes) {

                    // Add the RenderPass Parameters
                    ParameterInfoList globalParameters = parameters;
                    parametersFromParametersProvider(&globalParameters, m_manager->parameterManager(), pass);

                    RenderCommand *command = m_allocator->allocate<RenderCommand>();
                    command->m_depth = m_data->m_eyePos.distanceToPoint(node->worldBoundingVolume()->center());

                    command->m_geometry = m_manager->lookupHandle<Geometry, GeometryManager, HGeometry>(geometryRenderer->geometryId());
                    command->m_geometryRenderer = node->componentHandle<GeometryRenderer, 16>();
                    command->m_instancesCount = 0;
                    command->m_stateSet = Q_NULLPTR;
                    command->m_changeCost = 0;
                    // For RenderPass based states we use the globally set RenderState
                    // if no renderstates are defined as part of the pass. That means:
                    // RenderPass { renderStates: [] } will use the states defined by
                    // StateSet in the FrameGraph
                    if (!pass->renderStates().isEmpty())
                        command->m_stateSet = buildRenderStateSet(pass->renderStates(), m_allocator);
                    if (command->m_stateSet != Q_NULLPTR) {
                        // Merge per pass stateset with global stateset
                        // so that the local stateset only overrides
                        if (m_stateSet != Q_NULLPTR)
                            command->m_stateSet->merge(m_stateSet);
                        command->m_changeCost = m_renderer->defaultRenderState()->changeCost(command->m_stateSet);
                    }

                    // Pick which lights to take in to account.
                    // For now decide based on the distance by taking the MAX_LIGHTS closest lights.
                    // Replace with more sophisticated mechanisms later.
                    std::sort(m_lightSources.begin(), m_lightSources.end(), LightSourceCompare(node));
                    QVector<LightSource> activeLightSources; // NB! the total number of lights here may still exceed MAX_LIGHTS
                    activeLightSources.reserve(m_lightSources.count());
                    int lightCount = 0;
                    for (int i = 0; i < m_lightSources.count() && lightCount < MAX_LIGHTS; ++i) {
                        activeLightSources.append(m_lightSources[i]);
                        lightCount += m_lightSources[i].lights.count();
                    }

                    setShaderAndUniforms(command, pass, globalParameters, *(node->worldTransform()), activeLightSources);

                    buildSortingKey(command);
                    m_commands.append(command);
                }
            }
        }
    }

    // Traverse children
    Q_FOREACH (Entity *child, node->children())
        buildRenderCommands(child, planes);
}

const AttachmentPack &RenderView::attachmentPack() const
{
    return m_attachmentPack;
}

void RenderView::setUniformValue(QUniformPack &uniformPack, const QString &name, const QVariant &value)
{
    if (const QUniformValue *val = uniformPack.uniform(name))
        destroyUniformValue(val, m_allocator);

    Texture *tex = Q_NULLPTR;

    if ((tex = value.value<Texture *>()) != Q_NULLPTR) {
        uniformPack.setTexture(name, tex->peerUuid());
        TextureUniform *texUniform = m_allocator->allocate<TextureUniform>();
        texUniform->setTextureId(tex->peerUuid());
        uniformPack.setUniform(name, texUniform);
    } else {
        uniformPack.setUniform(name, QUniformValue::fromVariant(value, m_allocator));
    }
}

void RenderView::setStandardUniformValue(QUniformPack &uniformPack, const QString &glslName, const QString &name, const QMatrix4x4 &worldTransform)
{
    if (const QUniformValue *val = uniformPack.uniform(glslName))
        destroyUniformValue(val, m_allocator);

    uniformPack.setUniform(glslName, (this->*ms_standardUniformSetters[name])(worldTransform));
}

void RenderView::setUniformBlockValue(QUniformPack &uniformPack, Shader *shader, const ShaderUniformBlock &block, const QVariant &value)
{
    ShaderData *shaderData = Q_NULLPTR;
    if ((shaderData = value.value<ShaderData *>())) {
        // UBO are indexed by <ShaderId, ShaderDataId> so that a same QShaderData can be used among different shaders
        // while still making sure that if they have a different layout everything will still work
        // If two shaders define the same block with the exact same layout, in that case the UBO could be shared
        // but how do we know that ? We'll need to compare ShaderUniformBlocks

        // For now a UBO is unique to a Shader and a ShaderData
        // later we might want to make them shareable across Shaders but
        // that will require checking that all Shaders have the same layout for a given
        // uniform block
        ShaderDataShaderUboKey uboKey(shaderData->peerUuid(),
                                      shader->peerUuid());

        BlockToUBO uniformBlockUBO;
        uniformBlockUBO.m_blockIndex = block.m_index;
        uniformBlockUBO.m_shaderDataID = shaderData->peerUuid();
        bool uboNeedsUpdate = false;

        // build UBO at uboId if not created before
        if (!m_manager->uboManager()->contains(uboKey)) {
            m_manager->uboManager()->getOrCreateResource(uboKey);
            uboNeedsUpdate = true;
        }

        // If shaderData  has been updated (property has changed or one of the nested properties has changed)
        // foreach property defined in the QShaderData, we try to fill the value of the corresponding active uniform(s)
        // for all the updated properties (all the properties if the UBO was just created)
        if (shaderData->updateViewTransform(*m_data->m_viewMatrix) || uboNeedsUpdate) {
            // Clear previous values remaining in the hash
            m_data->m_uniformBlockBuilder.activeUniformNamesToValue.clear();
            // Update only update properties if uboNeedsUpdate is true, otherwise update the whole block
            m_data->m_uniformBlockBuilder.updatedPropertiesOnly = uboNeedsUpdate;
            // Retrieve names and description of each active uniforms in the uniform block
            m_data->m_uniformBlockBuilder.uniforms = shader->activeUniformsForBlock(block.m_index);
            // Builds the name-value map for the block
            m_data->m_uniformBlockBuilder.buildActiveUniformNameValueMapStructHelper(shaderData, block.m_name);
            if (!uboNeedsUpdate)
                shaderData->markDirty();
            // copy the name-value map into the BlockToUBO
            uniformBlockUBO.m_updatedProperties = m_data->m_uniformBlockBuilder.activeUniformNamesToValue;
            uboNeedsUpdate = true;
        }

        uniformBlockUBO.m_needsUpdate = uboNeedsUpdate;
        uniformPack.setUniformBuffer(uniformBlockUBO);
    }
}

void RenderView::setDefaultUniformBlockShaderDataValue(QUniformPack &uniformPack, Shader *shader, ShaderData *shaderData, const QString &structName)
{
    m_data->m_uniformBlockBuilder.activeUniformNamesToValue.clear();

    // updates transformed properties;
    shaderData->updateViewTransform(*m_data->m_viewMatrix);
    // Force to update the whole block
    m_data->m_uniformBlockBuilder.updatedPropertiesOnly = false;
    // Retrieve names and description of each active uniforms in the uniform block
    m_data->m_uniformBlockBuilder.uniforms = shader->activeUniformsForBlock(-1);
    // Build name-value map for the block
    m_data->m_uniformBlockBuilder.buildActiveUniformNameValueMapStructHelper(shaderData, structName);
    // Set uniform values for each entrie of the block name-value map
    QHash<QString, QVariant>::const_iterator activeValuesIt = m_data->m_uniformBlockBuilder.activeUniformNamesToValue.constBegin();
    const QHash<QString, QVariant>::const_iterator activeValuesEnd = m_data->m_uniformBlockBuilder.activeUniformNamesToValue.constEnd();

    while (activeValuesIt != activeValuesEnd) {
        setUniformValue(uniformPack, activeValuesIt.key(), activeValuesIt.value());
        ++activeValuesIt;
    }
}

void RenderView::buildSortingKey(RenderCommand *command)
{
    // Build a bitset key depending on the SortingCriterion
    int sortCount = m_data->m_sortingCriteria.count();

    // If sortCount == 0, no sorting is applied

    // Handle at most 4 filters at once
    for (int i = 0; i < sortCount && i < 4; i++) {
        SortCriterion *sC = m_manager->lookupResource<SortCriterion, SortCriterionManager>(m_data->m_sortingCriteria[i]);

        switch (sC->sortType()) {
        case QSortCriterion::StateChangeCost:
            command->m_sortingType.sorts[i] = command->m_changeCost; // State change cost
            break;
        case QSortCriterion::BackToFront:
            command->m_sortBackToFront = true; // Depth value
            break;
        case QSortCriterion::Material:
            command->m_sortingType.sorts[i] = command->m_shaderDna; // Material
            break;
        default:
            Q_UNREACHABLE();
        }
    }
}

void RenderView::setShaderAndUniforms(RenderCommand *command, RenderPass *rPass, ParameterInfoList &parameters, const QMatrix4x4 &worldTransform,
                                      const QVector<LightSource> &activeLightSources)
{
    // The VAO Handle is set directly in the renderer thread so as to avoid having to use a mutex here
    // Set shader, technique, and effect by basically doing :
    // ShaderProgramManager[MaterialManager[frontentEntity->id()]->Effect->Techniques[TechniqueFilter->name]->RenderPasses[RenderPassFilter->name]];
    // The Renderer knows that if one of those is null, a default material / technique / effect as to be used

    // Find all RenderPasses (in order) matching values set in the RenderPassFilter
    // Get list of parameters for the Material, Effect, and Technique
    // For each ParameterBinder in the RenderPass -> create a QUniformPack
    // Once that works, improve that to try and minimize QUniformPack updates

    if (rPass != Q_NULLPTR) {
        // Index Shader by Shader UUID
        command->m_shader = m_manager->lookupHandle<Shader, ShaderManager, HShader>(rPass->shaderProgram());
        Shader *shader = Q_NULLPTR;
        if ((shader = m_manager->data<Shader, ShaderManager>(command->m_shader)) != Q_NULLPTR) {
            command->m_shaderDna = shader->dna();

            // Builds the QUniformPack, sets shader standard uniforms and store attributes name / glname bindings
            // If a parameter is defined and not found in the bindings it is assumed to be a binding of Uniform type with the glsl name
            // equals to the parameter name
            const QVector<QString> &uniformNames = shader->uniformsNames();
            const QVector<QString> &attributeNames = shader->attributesNames();
            const QVector<QString> &uniformBlockNames = shader->uniformBlockNames();

            // Set fragData Name and index
            // Later on we might want to relink the shader if attachments have changed
            // But for now we set them once and for all
            QHash<QString, int> fragOutputs;
            if (!m_renderTarget.isNull() && !shader->isLoaded()) {
                Q_FOREACH (const Attachment &att, m_attachmentPack.attachments()) {
                    if (att.m_type <= QRenderAttachment::ColorAttachment15)
                        fragOutputs.insert(att.m_name, att.m_type);
                }
            }

            if (!uniformNames.isEmpty() || !attributeNames.isEmpty()) {

                // Set default standard uniforms without bindings
                Q_FOREACH (const QString &uniformName, uniformNames) {
                    if (ms_standardUniformSetters.contains(uniformName))
                        setStandardUniformValue(command->m_uniforms, uniformName, uniformName, worldTransform);
                }

                // Set default attributes
                Q_FOREACH (const QString &attributeName, attributeNames) {
                    if (ms_standardAttributesNames.contains(attributeName))
                        command->m_parameterAttributeToShaderNames.insert(attributeName, attributeName);
                }

                // Set uniforms and attributes explicitly binded
                Q_FOREACH (const ParameterMapping &binding, rPass->bindings()) {
                    ParameterInfoList::iterator it = findParamInfo(&parameters, binding.parameterName());
                    if (it == parameters.end()) {
                        if (binding.bindingType() == QParameterMapping::Attribute
                                && attributeNames.contains(binding.shaderVariableName())) {
                            command->m_parameterAttributeToShaderNames.insert(binding.parameterName(), binding.shaderVariableName());
                        } else if (binding.bindingType() == QParameterMapping::StandardUniform
                                   && uniformNames.contains(binding.shaderVariableName())
                                   && ms_standardUniformSetters.contains(binding.parameterName())) {
                            setStandardUniformValue(command->m_uniforms, binding.shaderVariableName(), binding.parameterName(), worldTransform);
                        } else if (binding.bindingType() == QParameterMapping::FragmentOutput
                                   && fragOutputs.contains(binding.parameterName())) {
                            fragOutputs.insert(binding.shaderVariableName(), fragOutputs.take(binding.parameterName()));
                        } else {
                            qCWarning(Render::Backend) << Q_FUNC_INFO << "Trying to bind a Parameter that hasn't been defined " << binding.parameterName();
                        }
                    } else {
                        setUniformValue(command->m_uniforms, binding.shaderVariableName(), it->value);
                        parameters.erase(it);
                    }
                }

                // Parameters remaining could be
                // -> uniform scalar / vector
                // -> uniform struct / arrays
                // -> uniform block / array (4.3)

                if ((!uniformNames.isEmpty() || !uniformBlockNames.isEmpty()) && !parameters.isEmpty()) {
                    ParameterInfoList::iterator it = parameters.begin();
                    while (it != parameters.end()) {
                        if (uniformNames.contains(it->name)) { // Parameter is a regular uniform
                            setUniformValue(command->m_uniforms, it->name, it->value);
                            it = parameters.erase(it);
                        } else if (uniformBlockNames.indexOf(it->name) != -1) { // Parameter is a uniform block
                            const ShaderUniformBlock &block = shader->uniformBlock(it->name);
                            setUniformBlockValue(command->m_uniforms, shader, block, it->value);
                            it = parameters.erase(it);
                        } else {
                            const QVariant &v = it->value;
                            ShaderData *shaderData = Q_NULLPTR;
                            if ((shaderData = v.value<ShaderData *>()) != Q_NULLPTR) {
                                // Try to check if we have a struct or array matching a QShaderData parameter
                                setDefaultUniformBlockShaderDataValue(command->m_uniforms, shader, shaderData, it->name);
                                it = parameters.erase(it);
                            } else {
                                // Else param unused by current shader
                                ++it;
                            }
                        }
                    }
                }

                // Lights
                const QString LIGHT_COUNT_NAME = QStringLiteral("lightCount");
                const QString LIGHT_POSITION_NAME = QStringLiteral(".position");

                int lightIdx = 0;
                Q_FOREACH (const LightSource &lightSource, activeLightSources) {
                    if (lightIdx == MAX_LIGHTS)
                        break;
                    Entity *lightEntity = lightSource.entity;
                    const QVector3D worldPos = lightEntity->worldBoundingVolume()->center();
                    Q_FOREACH (Light *light, lightSource.lights) {
                        if (lightIdx == MAX_LIGHTS)
                            break;
                        const QString structName = QStringLiteral("lights[") + QString::number(lightIdx) + QLatin1Char(']');
                        setUniformValue(command->m_uniforms, structName + LIGHT_POSITION_NAME, worldPos);
                        setDefaultUniformBlockShaderDataValue(command->m_uniforms, shader, light, structName);
                        ++lightIdx;
                    }
                }

                if (uniformNames.contains(LIGHT_COUNT_NAME))
                    setUniformValue(command->m_uniforms, LIGHT_COUNT_NAME, qMax(1, lightIdx));

                if (activeLightSources.isEmpty()) {
                    setUniformValue(command->m_uniforms, QStringLiteral("lights[0].type"), int(QLight::DirectionalLight));
                    setUniformValue(command->m_uniforms, QStringLiteral("lights[0].direction"), QVector3D(0.0f, -1.0f, -1.0f));
                    setUniformValue(command->m_uniforms, QStringLiteral("lights[0].color"), QVector3D(1.0f, 1.0f, 1.0f));
                    setUniformValue(command->m_uniforms, QStringLiteral("lights[0].intensity"), 1.0f);
                }
            }
            // Set frag outputs in the shaders if hash not empty
            if (!fragOutputs.isEmpty())
                shader->setFragOutputs(fragOutputs);
        }
    }
    else {
        qCWarning(Render::Backend) << Q_FUNC_INFO << "Using default effect as none was provided";
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
