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

#include "qdeclarativeview_plugin.h"

#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>

#include <QtCore/qplugin.h>
#include <QtDeclarative/QDeclarativeView>

static const char toolTipC[] = "QtDeclarative view widget";

QT_BEGIN_NAMESPACE

QDeclarativeViewPlugin::QDeclarativeViewPlugin(QObject *parent) :
    QObject(parent),
    m_initialized(false)
{
}

QString QDeclarativeViewPlugin::name() const
{
    return QLatin1String("QDeclarativeView");
}

QString QDeclarativeViewPlugin::group() const
{
    return QLatin1String("Display Widgets");
}

QString QDeclarativeViewPlugin::toolTip() const
{
    return tr(toolTipC);
}

QString QDeclarativeViewPlugin::whatsThis() const
{
    return tr(toolTipC);
}

QString QDeclarativeViewPlugin::includeFile() const
{
    return QLatin1String("QtDeclarative/QDeclarativeView");
}

QIcon QDeclarativeViewPlugin::icon() const
{
    return QIcon();
}

bool QDeclarativeViewPlugin::isContainer() const
{
    return false;
}

QWidget *QDeclarativeViewPlugin::createWidget(QWidget *parent)
{
    return new QDeclarativeView(parent);
}

bool QDeclarativeViewPlugin::isInitialized() const
{
    return m_initialized;
}

void QDeclarativeViewPlugin::initialize(QDesignerFormEditorInterface * /*core*/)
{
    if (m_initialized)
        return;

    m_initialized = true;
}

QString QDeclarativeViewPlugin::domXml() const
{
    return QLatin1String("\
    <ui language=\"c++\">\
        <widget class=\"QDeclarativeView\" name=\"declarativeView\">\
            <property name=\"geometry\">\
                <rect>\
                    <x>0</x>\
                    <y>0</y>\
                    <width>300</width>\
                    <height>200</height>\
                </rect>\
            </property>\
        </widget>\
    </ui>");
}

Q_EXPORT_PLUGIN2(customwidgetplugin, QDeclarativeViewPlugin)

QT_END_NAMESPACE
