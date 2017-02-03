/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "previewactiongroup.h"

#include <deviceprofile_p.h>
#include <shared_settings_p.h>

#include <QtGui/QStyleFactory>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

enum { MaxDeviceActions = 20 };

namespace qdesigner_internal {

PreviewActionGroup::PreviewActionGroup(QDesignerFormEditorInterface *core, QObject *parent) :
    QActionGroup(parent),
    m_core(core)
{
    /* Create a list of up to MaxDeviceActions invisible actions to be
     * populated with device profiles (actiondata: index) followed by the
     * standard style actions (actiondata: style name). */
    connect(this, SIGNAL(triggered(QAction*)), this, SLOT(slotTriggered(QAction*)));
    setExclusive(true);

    const QString objNamePostfix = QLatin1String("_action");
    // Create invisible actions for devices. Set index as action data.
    QString objNamePrefix = QLatin1String("__qt_designer_device_");
    for (int i = 0; i < MaxDeviceActions; i++) {
        QAction *a = new QAction(this);
        QString objName = objNamePrefix;
        objName += QString::number(i);
        objName += objNamePostfix;
        a->setObjectName(objName);
        a->setVisible(false);
        a->setData(i);
        addAction(a);
    }
    // Create separator at index MaxDeviceActions
    QAction *sep = new QAction(this);
    sep->setObjectName(QLatin1String("__qt_designer_deviceseparator"));
    sep->setSeparator(true);
    sep->setVisible(false);
    addAction(sep);
    // Populate devices
    updateDeviceProfiles();

    // Add style actions
    const QStringList styles = QStyleFactory::keys();
    const QStringList::const_iterator cend = styles.constEnd();
    // Make sure ObjectName  is unique in case toolbar solution is used.
    objNamePrefix = QLatin1String("__qt_designer_style_");
    // Create styles. Set style name string as action data.
    for (QStringList::const_iterator it = styles.constBegin(); it !=  cend ;++it) {
        QAction *a = new QAction(tr("%1 Style").arg(*it), this);
        QString objName = objNamePrefix;
        objName += *it;
        objName += objNamePostfix;
        a->setObjectName(objName);
        a->setData(*it);
        addAction(a);
    }
}

void PreviewActionGroup::updateDeviceProfiles()
{
    typedef QList<DeviceProfile> DeviceProfileList;
    typedef QList<QAction *> ActionList;

    const QDesignerSharedSettings settings(m_core);
    const DeviceProfileList profiles = settings.deviceProfiles();
    const ActionList al = actions();
    // Separator?
    const bool hasProfiles = !profiles.empty();
    al.at(MaxDeviceActions)->setVisible(hasProfiles);
    int index = 0;
    if (hasProfiles) {
        // Make actions visible
        const int maxIndex = qMin(static_cast<int>(MaxDeviceActions), profiles.size());
        for (; index < maxIndex; index++) {
            const QString name = profiles.at(index).name();
            al.at(index)->setText(name);
            al.at(index)->setVisible(true);
        }
    }
    // Hide rest
    for ( ; index < MaxDeviceActions; index++)
        al.at(index)->setVisible(false);
}

void PreviewActionGroup::slotTriggered(QAction *a)
{
    // Device or style according to data.
    const QVariant data = a->data();
    switch (data.type()) {
    case QVariant::String:
        emit preview(data.toString(), -1);
        break;
    case QVariant::Int:
        emit preview(QString(), data.toInt());
        break;
    default:
        break;
    }
}

}

QT_END_NAMESPACE
