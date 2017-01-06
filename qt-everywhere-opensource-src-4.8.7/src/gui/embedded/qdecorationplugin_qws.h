/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QDECORATIONPLUGIN_QWS_H
#define QDECORATIONPLUGIN_QWS_H

#include <QtCore/qplugin.h>
#include <QtCore/qfactoryinterface.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QDecoration;

struct Q_GUI_EXPORT QDecorationFactoryInterface : public QFactoryInterface
{
    virtual QDecoration *create(const QString &key) = 0;
};

#define QDecorationFactoryInterface_iid "com.trolltech.Qt.QDecorationFactoryInterface"
Q_DECLARE_INTERFACE(QDecorationFactoryInterface, QDecorationFactoryInterface_iid)

class Q_GUI_EXPORT QDecorationPlugin : public QObject, public QDecorationFactoryInterface
{
    Q_OBJECT
    Q_INTERFACES(QDecorationFactoryInterface:QFactoryInterface)
        public:
    explicit QDecorationPlugin(QObject *parent = 0);
    ~QDecorationPlugin();

    virtual QStringList keys() const = 0;
    virtual QDecoration *create(const QString &key) = 0;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QDECORATIONPLUGIN_QWS_H
