/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef MAC_DEPLOMYMENT_SHARED_H
#define MAC_DEPLOMYMENT_SHARED_H

#include <QString>
#include <QStringList>
#include <QDebug>
#include <QSet>
#include <QVersionNumber>

extern int logLevel;
#define LogError()      if (logLevel < 0) {} else qDebug() << "ERROR:"
#define LogWarning()    if (logLevel < 1) {} else qDebug() << "WARNING:"
#define LogNormal()     if (logLevel < 2) {} else qDebug() << "Log:"
#define LogDebug()      if (logLevel < 3) {} else qDebug() << "Log:"

extern bool runStripEnabled;

class FrameworkInfo
{
public:
    bool isDylib;
    QString frameworkDirectory;
    QString frameworkName;
    QString frameworkPath;
    QString binaryDirectory;
    QString binaryName;
    QString binaryPath;
    QString rpathUsed;
    QString version;
    QString installName;
    QString deployedInstallName;
    QString sourceFilePath;
    QString frameworkDestinationDirectory;
    QString binaryDestinationDirectory;
};

class DylibInfo
{
public:
    QString binaryPath;
    QVersionNumber currentVersion;
    QVersionNumber compatibilityVersion;
};

class OtoolInfo
{
public:
    QString installName;
    QString binaryPath;
    QVersionNumber currentVersion;
    QVersionNumber compatibilityVersion;
    QList<DylibInfo> dependencies;
};

bool operator==(const FrameworkInfo &a, const FrameworkInfo &b);
QDebug operator<<(QDebug debug, const FrameworkInfo &info);

class ApplicationBundleInfo
{
    public:
    QString path;
    QString binaryPath;
    QStringList libraryPaths;
};

class DeploymentInfo
{
public:
    QString qtPath;
    QString pluginPath;
    QStringList deployedFrameworks;
    QSet<QString> rpathsUsed;
    bool useLoaderPath;
};

inline QDebug operator<<(QDebug debug, const ApplicationBundleInfo &info);

void changeQtFrameworks(const QString appPath, const QString &qtPath, bool useDebugLibs);
void changeQtFrameworks(const QList<FrameworkInfo> frameworks, const QStringList &binaryPaths, const QString &qtPath);

OtoolInfo findDependencyInfo(const QString &binaryPath);
FrameworkInfo parseOtoolLibraryLine(const QString &line, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs);
QString findAppBinary(const QString &appBundlePath);
QList<FrameworkInfo> getQtFrameworks(const QString &path, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs);
QList<FrameworkInfo> getQtFrameworks(const QStringList &otoolLines, const QString &appBundlePath, const QSet<QString> &rpaths, bool useDebugLibs);
QString copyFramework(const FrameworkInfo &framework, const QString path);
DeploymentInfo deployQtFrameworks(const QString &appBundlePath, const QStringList &additionalExecutables, bool useDebugLibs);
DeploymentInfo deployQtFrameworks(QList<FrameworkInfo> frameworks,const QString &bundlePath, const QStringList &binaryPaths, bool useDebugLibs, bool useLoaderPath);
void createQtConf(const QString &appBundlePath);
void deployPlugins(const QString &appBundlePath, DeploymentInfo deploymentInfo, bool useDebugLibs);
bool deployQmlImports(const QString &appBundlePath, DeploymentInfo deploymentInfo, QStringList &qmlDirs);
void changeIdentification(const QString &id, const QString &binaryPath);
void changeInstallName(const QString &oldName, const QString &newName, const QString &binaryPath);
void runStrip(const QString &binaryPath);
void stripAppBinary(const QString &bundlePath);
QString findAppBinary(const QString &appBundlePath);
QStringList findAppFrameworkNames(const QString &appBundlePath);
QStringList findAppFrameworkPaths(const QString &appBundlePath);
void codesignFile(const QString &identity, const QString &filePath);
QSet<QString> codesignBundle(const QString &identity,
                             const QString &appBundlePath,
                             QList<QString> additionalBinariesContainingRpaths);
void codesign(const QString &identity, const QString &appBundlePath);
void createDiskImage(const QString &appBundlePath);


#endif
