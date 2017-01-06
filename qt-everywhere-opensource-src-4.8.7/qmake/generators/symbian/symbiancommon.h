/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qmake application of the Qt Toolkit.
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

#ifndef SYMBIANCOMMON_H
#define SYMBIANCOMMON_H

#include <project.h>
#include <makefile.h>
#include "initprojectdeploy_symbian.h"

#define PRINT_FILE_CREATE_ERROR(filename) fprintf(stderr, "Error: Could not create '%s'\n", qPrintable(filename));

class SymbianLocalization
{
public:
    QString qtLanguageCode;
    QString symbianLanguageCode;
    QString shortCaption;
    QString longCaption;
    QString pkgDisplayName;
    QString installerPkgDisplayName;
};

typedef QList<SymbianLocalization> SymbianLocalizationList;
typedef QListIterator<SymbianLocalization> SymbianLocalizationListIterator;

class SymbianCommonGenerator
{
public:
    enum TargetType {
        TypeExe,
        TypeDll,
        TypeLib,
        TypePlugin,
        TypeSubdirs
    };


    SymbianCommonGenerator(MakefileGenerator *generator);

    virtual void init();

protected:

    QString removePathSeparators(QString &file);
    void removeSpecialCharacters(QString& str);
    void generatePkgFile(const QString &iconFile,
                         bool epocBuild,
                         const SymbianLocalizationList &symbianLocalizationList);
    bool containsStartWithItem(const QChar &c, const QStringList& src);

    void writeRegRssFile(QMap<QString, QStringList> &useritems);
    void writeRegRssList(QTextStream &t, QStringList &userList,
                         const QString &listTag,
                         const QString &listItem);
    void writeRssFile(QString &numberOfIcons, QString &iconfile);
    void writeLocFile(const SymbianLocalizationList &symbianLocalizationList);
    void readRssRules(QString &numberOfIcons,
                      QString &iconFile,
                      QMap<QString, QStringList> &userRssRules);

    void writeCustomDefFile();

    void parseTsFiles(SymbianLocalizationList *symbianLocalizationList);
    void fillQt2SymbianLocalizationList(SymbianLocalizationList *symbianLocalizationList);

    void parsePreRules(const QString &deploymentVariable,
                       const QString &variableSuffix,
                       QStringList *rawRuleList,
                       QStringList *languageRuleList,
                       QStringList *headerRuleList,
                       QStringList *vendorRuleList);
    void parsePostRules(const QString &deploymentVariable,
                        const QString &variableSuffix,
                        QStringList *rawRuleList);
    bool parseTsContent(const QString &tsFilename, SymbianLocalization *loc);
    QString generatePkgNameForHeader(const SymbianLocalizationList &symbianLocalizationList,
                                     const QString &defaultName,
                                     bool isForSmartInstaller);
    void addLocalizedResourcesToDeployment(const QString &deploymentFilesVar,
                                           const SymbianLocalizationList &symbianLocalizationList);
    QString generateLocFileName();


protected:
    MakefileGenerator *generator;

    QStringList generatedFiles;
    QStringList generatedDirs;
    QString fixedTarget;
    QString privateDirUid;
    QString uid3;
    TargetType targetType;
};

#endif // SYMBIANCOMMON_H
