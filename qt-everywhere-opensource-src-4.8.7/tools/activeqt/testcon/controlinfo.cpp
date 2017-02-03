/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "controlinfo.h"

#include <QtGui>

QT_BEGIN_NAMESPACE

ControlInfo::ControlInfo(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    listInfo->setColumnCount(2);
    listInfo->headerItem()->setText(0, tr("Item"));
    listInfo->headerItem()->setText(1, tr("Details"));
}

void ControlInfo::setControl(QWidget *activex)
{
    listInfo->clear();

    const QMetaObject *mo = activex->metaObject();
    QTreeWidgetItem *group = new QTreeWidgetItem(listInfo);
    group->setText(0, tr("Class Info"));
    group->setText(1, QString::number(mo->classInfoCount()));

    QTreeWidgetItem *item = 0;
    int i;
    int count;
    for (i = mo->classInfoOffset(); i < mo->classInfoCount(); ++i) {
	const QMetaClassInfo info = mo->classInfo(i);
	item = new QTreeWidgetItem(group);
        item->setText(0, QString::fromLatin1(info.name()));
        item->setText(1, QString::fromLatin1(info.value()));
    }
    group = new QTreeWidgetItem(listInfo);
    group->setText(0, tr("Signals"));

    count = 0;
    for (i = mo->methodOffset(); i < mo->methodCount(); ++i) {
	const QMetaMethod method = mo->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            ++count;
	    item = new QTreeWidgetItem(group);
            item->setText(0, QString::fromLatin1(method.signature()));
        }
    }
    group->setText(1, QString::number(count));

    group = new QTreeWidgetItem(listInfo);
    group->setText(0, tr("Slots"));

    count = 0;
    for (i = mo->methodOffset(); i < mo->methodCount(); ++i) {
	const QMetaMethod method = mo->method(i);
        if (method.methodType() == QMetaMethod::Slot) {
            ++count;
	    item = new QTreeWidgetItem(group);
            item->setText(0, QString::fromLatin1(method.signature()));
        }
    }
    group->setText(1, QString::number(count));

    group = new QTreeWidgetItem(listInfo);
    group->setText(0, tr("Properties"));

    count = 0;
    for (i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        ++count;
	const QMetaProperty property = mo->property(i);
	item = new QTreeWidgetItem(group);
        item->setText(0, QString::fromLatin1(property.name()));
        item->setText(1, QString::fromLatin1(property.typeName()));
        if (!property.isDesignable()) {
            item->setTextColor(0, Qt::gray);
            item->setTextColor(1, Qt::gray);
        }
    }
    group->setText(1, QString::number(count));
}

QT_END_NAMESPACE
