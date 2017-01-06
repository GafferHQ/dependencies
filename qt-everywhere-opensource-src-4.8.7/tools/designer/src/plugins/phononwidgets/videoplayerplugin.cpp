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

#include "videoplayerplugin.h"
#include "videoplayertaskmenu.h"

#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QExtensionManager>
#include <QtDesigner/QDesignerFormEditorInterface>

#include <QtCore/qplugin.h>
#include <phonon/videoplayer.h>

static const char *toolTipC = "Phonon Video Player";

QT_BEGIN_NAMESPACE

VideoPlayerPlugin::VideoPlayerPlugin(const QString &group, QObject *parent) :
    QObject(parent),
    m_group(group),
    m_initialized(false)
{
}

QString VideoPlayerPlugin::name() const
{
    return QLatin1String("Phonon::VideoPlayer");
}

QString VideoPlayerPlugin::group() const
{
    return m_group;
}

QString VideoPlayerPlugin::toolTip() const
{
    return tr(toolTipC);
}

QString VideoPlayerPlugin::whatsThis() const
{
    return tr(toolTipC);
}

QString VideoPlayerPlugin::includeFile() const
{
    return QLatin1String("<phonon/videoplayer.h>");
}

QIcon VideoPlayerPlugin::icon() const
{
    return QIcon(QLatin1String(":/trolltech/phononwidgets/images/videoplayer.png"));
}

bool VideoPlayerPlugin::isContainer() const
{
    return false;
}

QWidget *VideoPlayerPlugin::createWidget(QWidget *parent)
{
    return new Phonon::VideoPlayer(Phonon::NoCategory, parent);
}

bool VideoPlayerPlugin::isInitialized() const
{
    return m_initialized;
}

void VideoPlayerPlugin::initialize(QDesignerFormEditorInterface * core)
{
    if (m_initialized)
        return;

    QExtensionManager *mgr = core->extensionManager();
    VideoPlayerTaskMenuFactory::registerExtension(mgr, Q_TYPEID(QDesignerTaskMenuExtension));
    m_initialized = true;
}

QString VideoPlayerPlugin::domXml() const
{
    return QLatin1String("\
    <ui language=\"c++\">\
        <widget class=\"Phonon::VideoPlayer\" name=\"videoPlayer\">\
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

QT_END_NAMESPACE
