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

#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DInput/QAbstractPhysicalDevice>

class TestDevice : public Qt3DInput::QAbstractPhysicalDevice
{
    Q_OBJECT
public:
    explicit TestDevice(Qt3DCore::QNode *parent = Q_NULLPTR)
        : Qt3DInput::QAbstractPhysicalDevice(parent)
    {}

    ~TestDevice()
    {
        QNode::cleanup();
    }

    int axisCount() const Q_DECL_FINAL { return 0; }
    int buttonCount() const Q_DECL_FINAL { return 0; }
    QStringList axisNames() const Q_DECL_FINAL { return QStringList(); }
    QStringList buttonNames() const Q_DECL_FINAL { return QStringList(); }
    int axisIdentifier(const QString &name) const Q_DECL_FINAL { Q_UNUSED(name) return 0; }
    int buttonIdentifier(const QString &name) const Q_DECL_FINAL { Q_UNUSED(name) return 0; }

protected:
    void copy(const Qt3DCore::QNode *ref) Q_DECL_FINAL
    {
        QAbstractPhysicalDevice::copy(ref);
    }

private:
    QT3D_CLONEABLE(TestDevice)
};
