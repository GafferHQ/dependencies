/****************************************************************************
**
** Copyright (C) 2014 BlackBerry Ltd.
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickopenglinfo_p.h"
#include "qopenglcontext.h"
#include "qquickwindow.h"
#include "qquickitem.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype OpenGLInfo
    \instantiates QQuickOpenGLInfo
    \inqmlmodule QtQuick
    \ingroup qtquick-effects
    \since 5.4
    \brief Provides information about the used OpenGL version

    The OpenGLInfo attached type provides information about the OpenGL
    version being used to render the surface of the attachee item.

    If the attachee item is not currently associated with any graphical
    surface, the properties are set to the values of the default surface
    format. When it becomes associated with a surface, all properties
    will update.

    \sa ShaderEffect
 */
QQuickOpenGLInfo::QQuickOpenGLInfo(QQuickItem *item)
    : QObject(item)
    , m_window(0)
    , m_majorVersion(2)
    , m_minorVersion(0)
    , m_profile(NoProfile)
    , m_renderableType(Unspecified)
{
    connect(item, SIGNAL(windowChanged(QQuickWindow*)), this, SLOT(setWindow(QQuickWindow*)));
    setWindow(item->window());
}

/*!
    \qmlproperty int QtQuick::OpenGLInfo::majorVersion

    This property holds the major OpenGL version.

    The default version is \c 2.0.

    \sa minorVersion, profile
 */
int QQuickOpenGLInfo::majorVersion() const
{
    return m_majorVersion;
}

/*!
    \qmlproperty int QtQuick::OpenGLInfo::minorVersion

    This property holds the minor OpenGL version.

    The default version is \c 2.0.

    \sa majorVersion, profile
 */
int QQuickOpenGLInfo::minorVersion() const
{
    return m_minorVersion;
}

/*!
    \qmlproperty enumeration QtQuick::OpenGLInfo::profile

    This property holds the configured OpenGL context profile.

    The possible values are:
    \list
    \li OpenGLInfo.NoProfile (default) - OpenGL version is lower than 3.2.
    \li OpenGLInfo.CoreProfile - Functionality deprecated in OpenGL version 3.0 is not available.
    \li OpenGLInfo.CompatibilityProfile - Functionality from earlier OpenGL versions is available.
    \endlist

    Reusable QML components will typically use this property in bindings in order to
    choose between core and non core profile compatible shader sources.

    \sa majorVersion, minorVersion
 */
QQuickOpenGLInfo::ContextProfile QQuickOpenGLInfo::profile() const
{
    return m_profile;
}

/*!
    \qmlproperty enumeration QtQuick::OpenGLInfo::renderableType

    This property holds the renderable type.

    The possible values are:
    \list
    \li OpenGLInfo.Unspecified (default) - Unspecified rendering method
    \li OpenGLInfo.OpenGL - Desktop OpenGL rendering
    \li OpenGLInfo.OpenGLES - OpenGL ES rendering
    \endlist
 */
QQuickOpenGLInfo::RenderableType QQuickOpenGLInfo::renderableType() const
{
    return m_renderableType;
}

QQuickOpenGLInfo *QQuickOpenGLInfo::qmlAttachedProperties(QObject *object)
{
    if (QQuickItem *item = qobject_cast<QQuickItem *>(object))
        return new QQuickOpenGLInfo(item);
    return 0;
}

void QQuickOpenGLInfo::updateFormat()
{
    QOpenGLContext *context = 0;
    if (m_window)
        context = m_window->openglContext();
    QSurfaceFormat format = context ? context->format() : QSurfaceFormat::defaultFormat();

    if (m_majorVersion != format.majorVersion()) {
        m_majorVersion = format.majorVersion();
        emit majorVersionChanged();
    }

    if (m_minorVersion != format.minorVersion()) {
        m_minorVersion = format.minorVersion();
        emit minorVersionChanged();
    }

    ContextProfile profile = static_cast<ContextProfile>(format.profile());
    if (m_profile != profile) {
        m_profile = profile;
        emit profileChanged();
    }

    RenderableType renderableType = static_cast<RenderableType>(format.renderableType());
    if (m_renderableType != renderableType) {
        m_renderableType = renderableType;
        emit renderableTypeChanged();
    }
}

void QQuickOpenGLInfo::setWindow(QQuickWindow *window)
{
    if (m_window != window) {
        if (m_window) {
            disconnect(m_window, SIGNAL(sceneGraphInitialized()), this, SLOT(updateFormat()));
            disconnect(m_window, SIGNAL(sceneGraphInvalidated()), this, SLOT(updateFormat()));
        }
        if (window) {
            connect(window, SIGNAL(sceneGraphInitialized()), this, SLOT(updateFormat()));
            connect(window, SIGNAL(sceneGraphInvalidated()), this, SLOT(updateFormat()));
        }
        m_window = window;
    }
    updateFormat();
}

QT_END_NAMESPACE
