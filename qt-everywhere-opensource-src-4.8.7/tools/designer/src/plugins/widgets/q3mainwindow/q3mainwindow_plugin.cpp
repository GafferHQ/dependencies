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

#include "q3mainwindow_plugin.h"
#include "q3mainwindow_container.h"

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QExtensionManager>

#include <QtCore/qplugin.h>
#include <Qt3Support/Q3MainWindow>

QT_BEGIN_NAMESPACE

Q3MainWindowPlugin::Q3MainWindowPlugin(const QIcon &icon, QObject *parent)
    : QObject(parent), m_initialized(false), m_icon(icon)
{}

QString Q3MainWindowPlugin::name() const
{ return QLatin1String("Q3MainWindow"); }

QString Q3MainWindowPlugin::group() const
{ return QLatin1String("[invisible]"); }

QString Q3MainWindowPlugin::toolTip() const
{ return QString(); }

QString Q3MainWindowPlugin::whatsThis() const
{ return QString(); }

QString Q3MainWindowPlugin::includeFile() const
{ return QLatin1String("q3mainwindow.h"); }

QIcon Q3MainWindowPlugin::icon() const
{ return m_icon; }

bool Q3MainWindowPlugin::isContainer() const
{ return true; }

QWidget *Q3MainWindowPlugin::createWidget(QWidget *parent)
{ return new Q3MainWindow(parent); }

bool Q3MainWindowPlugin::isInitialized() const
{ return m_initialized; }

void Q3MainWindowPlugin::initialize(QDesignerFormEditorInterface *core)
{
    Q_UNUSED(core);

    if (m_initialized)
        return;

    m_initialized = true;
    QExtensionManager *mgr = core->extensionManager();
    mgr->registerExtensions(new Q3MainWindowContainerFactory(mgr), Q_TYPEID(QDesignerContainerExtension));
}

QString Q3MainWindowPlugin::codeTemplate() const
{ return QString(); }

QString Q3MainWindowPlugin::domXml() const
{
    return QLatin1String("\
<ui language=\"c++\">\
    <widget class=\"Q3MainWindow\" name=\"mainWindow\">\
        <property name=\"geometry\">\
            <rect>\
                <x>0</x>\
                <y>0</y>\
                <width>100</width>\
                <height>80</height>\
            </rect>\
        </property>\
        <widget class=\"QWidget\" name=\"centralWidget\" />\
    </widget>\
</ui>");
}


QT_END_NAMESPACE
