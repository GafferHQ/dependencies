/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarativemultimediaglobal_p.h"

#include <qcamerainfo.h>
#include <qjsengine.h>

QT_BEGIN_NAMESPACE

/*!
\qmltype QtMultimedia
\inqmlmodule QtMultimedia
\ingroup multimedia_qml
\since QtMultimedia 5.4
\brief Provides a global object with useful functions from Qt Multimedia.

The \c QtMultimedia object is a global object with utility functions and properties.

It is not instantiable; to use it, call the members of the global \c QtMultimedia object directly.
For example:

\qml
Camera {
    deviceId: QtMultimedia.defaultCamera.deviceId
}
\endqml

*/

/*!
    \qmlproperty object QtMultimedia::QtMultimedia::defaultCamera
    \readonly

    The \c defaultCamera object provides information about the default camera on the system.

    Its properties are \c deviceId, \c displayName, \c position and \c orientation. See
    \l{QtMultimedia::QtMultimedia::availableCameras}{availableCameras} for a description of each
    of them.

    If there is no default camera, \c defaultCamera.deviceId will contain an empty string.

    \note This property is static; it is not updated if the system's default camera changes after the
    application started.
*/

/*!
    \qmlproperty list<object> QtMultimedia::QtMultimedia::availableCameras
    \readonly

    This property provides information about the cameras available on the system.

    Each object in the list has the following properties:

    \table
    \row
    \li \c deviceId
    \li
    This read-only property holds the unique identifier of the camera.

    You can choose which device to use with a \l Camera object by setting its
    \l{Camera::deviceId}{deviceId} property to this value.

    \row
    \li \c displayName
    \li
    This read-only property holds the human-readable name of the camera.
    You can use this property to display the name of the camera in a user interface.

    \row
    \li \c position
    \li
    This read-only property holds the physical position of the camera on the hardware system.
    Please see \l{Camera::position}{Camera.position} for more information.

    \row
    \li \c orientation
    \li
    This read-only property holds the physical orientation of the camera sensor.
    Please see \l{Camera::orientation}{Camera.orientation} for more information.

    \endtable

    \note This property is static; it is not updated when cameras are added or removed from
    the system, like USB cameras on a desktop platform.

    The following example shows how to display a list of available cameras. The user can change
    the active camera by selecting one of the items in the list.

    \qml
    Item {

        Camera {
            id: camera
        }

        VideoOutput {
            anchors.fill: parent
            source: camera
        }

        ListView {
            anchors.fill: parent

            model: QtMultimedia.availableCameras
            delegate: Text {
                text: modelData.displayName

                MouseArea {
                    anchors.fill: parent
                    onClicked: camera.deviceId = modelData.deviceId
                }
            }
        }
    }

    \endqml
*/

static QJSValue cameraInfoToJSValue(QJSEngine *jsEngine, const QCameraInfo &camera)
{
    QJSValue o = jsEngine->newObject();
    o.setProperty(QStringLiteral("deviceId"), camera.deviceName());
    o.setProperty(QStringLiteral("displayName"), camera.description());
    o.setProperty(QStringLiteral("position"), int(camera.position()));
    o.setProperty(QStringLiteral("orientation"), camera.orientation());
    return o;
}

QDeclarativeMultimediaGlobal::QDeclarativeMultimediaGlobal(QJSEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

QJSValue QDeclarativeMultimediaGlobal::defaultCamera() const
{
    return cameraInfoToJSValue(m_engine, QCameraInfo::defaultCamera());
}

QJSValue QDeclarativeMultimediaGlobal::availableCameras() const
{
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    QJSValue availableCameras = m_engine->newArray(cameras.count());
    for (int i = 0; i < cameras.count(); ++i)
        availableCameras.setProperty(i, cameraInfoToJSValue(m_engine, cameras.at(i)));
    return availableCameras;
}

QT_END_NAMESPACE
