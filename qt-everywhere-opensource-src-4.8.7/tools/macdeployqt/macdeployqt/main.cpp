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
#include "../shared/shared.h"
#include <qdir.h>

int main(int argc, char **argv)
{
    QString appBundlePath;
    if (argc > 1)
        appBundlePath = QString::fromLocal8Bit(argv[1]);

    if (argc < 2 || appBundlePath.startsWith("-")) {
        qDebug() << "Usage: macdeployqt app-bundle [options]";
        qDebug() << "";
        qDebug() << "Options:";
        qDebug() << "   -verbose=<0-3>     : 0 = no output, 1 = error/warning (default), 2 = normal, 3 = debug";
        qDebug() << "   -no-plugins        : Skip plugin deployment";
        qDebug() << "   -dmg               : Create a .dmg disk image";
        qDebug() << "   -no-strip          : Don't run 'strip' on the binaries";
        qDebug() << "   -use-debug-libs    : Deploy with debug versions of frameworks and plugins (implies -no-strip)";
        qDebug() << "   -executable=<path> : Let the given executable use the deployed frameworks too";
        qDebug() << "";
        qDebug() << "macdeployqt takes an application bundle as input and makes it";
        qDebug() << "self-contained by copying in the Qt frameworks and plugins that";
        qDebug() << "the application uses.";
        qDebug() << "";
        qDebug() << "Plugins related to a framework are copied in with the";
        qDebug() << "framework. The accessibilty, image formats, and text codec";
        qDebug() << "plugins are always copied, unless \"-no-plugins\" is specified.";
        qDebug() << "";
        qDebug() << "See the \"Deploying an Application on Qt/Mac\" topic in the";
        qDebug() << "documentation for more information about deployment on Mac OS X.";

        return 0;
    }

    if (appBundlePath.endsWith("/"))
        appBundlePath.chop(1);

    if (QDir().exists(appBundlePath) == false) {
        qDebug() << "Error: Could not find app bundle" << appBundlePath;
        return 0;
    }

    bool plugins = true;
    bool dmg = false;
    bool useDebugLibs = false;
    extern bool runStripEnabled;
    QStringList additionalExecutables;

    for (int i = 2; i < argc; ++i) {
        QByteArray argument = QByteArray(argv[i]);
        if (argument == QByteArray("-no-plugins")) {
            LogDebug() << "Argument found:" << argument;
            plugins = false;
        } else if (argument == QByteArray("-dmg")) {
            LogDebug() << "Argument found:" << argument;
            dmg = true;
        } else if (argument == QByteArray("-no-strip")) {
            LogDebug() << "Argument found:" << argument;
            runStripEnabled = false;
        } else if (argument == QByteArray("-use-debug-libs")) {
            LogDebug() << "Argument found:" << argument;
            useDebugLibs = true;
            runStripEnabled = false;
        } else if (argument.startsWith(QByteArray("-verbose"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            bool ok = false;
            int number = argument.mid(index+1).toInt(&ok);
            if (!ok)
                LogError() << "Could not parse verbose level";
            else
                logLevel = number;
        } else if (argument.startsWith(QByteArray("-executable"))) {
            LogDebug() << "Argument found:" << argument;
            int index = argument.indexOf("=");
            if (index < 0 || index >= argument.size())
                LogError() << "Missing executable path";
            else
                additionalExecutables << argument.mid(index+1);
        } else if (argument.startsWith("-")) {
            LogError() << "Unknown argument" << argument << "\n";
            return 0;
        }
     }

    DeploymentInfo deploymentInfo  = deployQtFrameworks(appBundlePath, additionalExecutables, useDebugLibs);

    if (plugins) {
        if (deploymentInfo.qtPath.isEmpty())
            deploymentInfo.pluginPath = "/Developer/Applications/Qt/plugins"; // Assume binary package.
        else
            deploymentInfo.pluginPath = deploymentInfo.qtPath + "/plugins";

        LogNormal();
        deployPlugins(appBundlePath, deploymentInfo, useDebugLibs);
        createQtConf(appBundlePath);
    }

    if (dmg) {
        LogNormal();
        createDiskImage(appBundlePath);
    }
}

