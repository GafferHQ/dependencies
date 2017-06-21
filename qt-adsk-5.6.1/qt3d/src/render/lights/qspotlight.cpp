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

#include "qspotlight.h"
#include "qspotlight_p.h"
#include <Qt3DCore/qscenepropertychange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {


/*
  Expected Shader struct

  \code

  struct SpotLight
  {
   vec3 position;
   vec3 direction;
   vec4 color;
   float intensity;
   float cutOffAngle;
  };

  uniform SpotLight spotLights[10];

  \endcode
 */

QSpotLightPrivate::QSpotLightPrivate()
    : QPointLightPrivate(QLight::SpotLight)
    , m_cutOffAngle(45.0f)
    , m_direction(0.0f, -1.0f, 0.0f)
{
}

/*!
  \class Qt3DRender::QSpotLight
  \inmodule Qt3DRender
  \since 5.5

 */

/*!
    \qmltype SpotLight
    \instantiates Qt3DRender::QSpotLight
    \inherits AbstractLight
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief For OpenGL ...
*/

/*! \fn void Qt3DRender::QSpotLight::copy(const Qt3DCore::QNode *ref)
  Copies the \a ref instance into this one.
 */

void QSpotLight::copy(const QNode *ref)
{
    const QSpotLight *light = static_cast<const QSpotLight*>(ref);
    d_func()->m_direction = light->d_func()->m_direction;
    d_func()->m_cutOffAngle = light->d_func()->m_cutOffAngle;
    QLight::copy(ref);
}


/*!
  \fn Qt3DRender::QSpotLight::QSpotLight(Qt3DCore::QNode *parent)
  Constructs a new QSpotLight with the specified \a parent.
 */
QSpotLight::QSpotLight(QNode *parent)
    : QPointLight(*new QSpotLightPrivate, parent)
{
}

/*! \internal */
QSpotLight::QSpotLight(QSpotLightPrivate &dd, QNode *parent)
    : QPointLight(dd, parent)
{
}

/*!
  \qmlproperty vector3d Qt3D.Render::SpotLight::direction

*/

/*!
  \property Qt3DRender::QSpotLight::direction

 */

    QVector3D QSpotLight::direction() const
{
    Q_D(const QSpotLight);
    return d->m_direction;
}


/*!
  \qmlproperty float Qt3D.Render::SpotLight::cutOffAngle

*/

/*!
  \property Qt3DRender::QSpotLight::cutOffAngle

 */
float QSpotLight::cutOffAngle() const
{
    Q_D(const QSpotLight);
    return d->m_cutOffAngle;
}

void QSpotLight::setDirection(const QVector3D &direction)
{
    Q_D(QSpotLight);
    if (direction != d->m_direction) {
        d->m_direction = direction;
        emit directionChanged(direction);
    }
}

void QSpotLight::setCutOffAngle(float cutOffAngle)
{
    Q_D(QSpotLight);
    if (d->m_cutOffAngle != cutOffAngle) {
        d->m_cutOffAngle = cutOffAngle;
        emit cutOffAngleChanged(cutOffAngle);
    }
}

} // namespace Qt3DRender

QT_END_NAMESPACE
