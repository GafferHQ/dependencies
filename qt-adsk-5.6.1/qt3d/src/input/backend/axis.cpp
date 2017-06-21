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

#include "axis_p.h"
#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/qaxisinput.h>
#include <Qt3DCore/qscenepropertychange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

namespace Input {

Axis::Axis()
    : Qt3DCore::QBackendNode()
    , m_enabled(false)
    , m_axisValue(0.0f)
{
}

void Axis::updateFromPeer(Qt3DCore::QNode *peer)
{
    QAxis *axis = static_cast<QAxis *>(peer);
    m_enabled = axis->isEnabled();
    m_name = axis->name();
    Q_FOREACH (QAxisInput *input, axis->inputs())
        m_inputs.push_back(input->id());
}

void Axis::cleanup()
{
    m_enabled = false;
    m_inputs.clear();
    m_name.clear();
    m_axisValue = 0.0f;
}

void Axis::setAxisValue(float axisValue)
{
    m_axisValue = axisValue;
}

void Axis::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    Qt3DCore::QScenePropertyChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QScenePropertyChange>(e);
    if (e->type() == Qt3DCore::NodeUpdated) {
        if (propertyChange->propertyName() == QByteArrayLiteral("enabled")) {
            m_enabled = propertyChange->value().toBool();
        } else if (propertyChange->propertyName() == QByteArrayLiteral("name")) {
            m_name = propertyChange->value().toString();
        }
    } else if (e->type() == Qt3DCore::NodeAdded) {
        if (propertyChange->propertyName() == QByteArrayLiteral("input"))
            m_inputs.push_back(propertyChange->value().value<Qt3DCore::QNodeId>());
    } else if (e->type() == Qt3DCore::NodeRemoved) {
        if (propertyChange->propertyName() == QByteArrayLiteral("input"))
            m_inputs.removeOne(propertyChange->value().value<Qt3DCore::QNodeId>());
    }
}

} // namespace Input

} // namespace Qt3DInput

QT_END_NAMESPACE
