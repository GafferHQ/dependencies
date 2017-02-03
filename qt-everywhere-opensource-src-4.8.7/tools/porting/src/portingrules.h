/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qt3to4 porting application of the Qt Toolkit.
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

#ifndef PORTINGRULES_H
#define PORTINGRULES_H

#include "qtsimplexml.h"
#include "tokenreplacements.h"
#include <QList>
#include <QPair>
#include <QHash>
#include <QSet>
#include <QStringList>

QT_BEGIN_NAMESPACE

class RuleDescription
{
public:
    explicit RuleDescription(QtSimpleXml &replacementRule) {
        qt3 = replacementRule[QLatin1String("Qt3")].text();
        qt4 = replacementRule[QLatin1String("Qt4")].text();
        ruleType = replacementRule.attribute(QLatin1String("Type"));
    }
    QString qt3;
    QString qt4;
    QString ruleType;
    bool operator==(const RuleDescription &other) const
    {
        return (qt3 == other.qt3 && qt4 == other.qt4 && ruleType == other.ruleType);
    }
};

class PortingRules
{
public:
    static void createInstance(QString xmlFilePath);
    static PortingRules *instance();
    static void deleteInstance();

    enum QtVersion{Qt3, Qt4};
    PortingRules(QString xmlFilePath);
    QList<TokenReplacement*> getTokenReplacementRules();
    QStringList getHeaderList(QtVersion qtVersion);
    QHash<QByteArray, QByteArray> getNeededHeaders();
    QStringList getInheritsQt();
    QHash<QByteArray, QByteArray> getClassLibraryList();
    QHash<QByteArray, QByteArray> getHeaderReplacements();
private:
    static PortingRules *theInstance;

    QList<TokenReplacement*> tokenRules;
    QStringList qt3Headers;
    QStringList qt4Headers;
    QHash<QByteArray, QByteArray> neededHeaders;
    QStringList inheritsQtClass;
    QList<RuleDescription> disabledRules;
    QHash<QByteArray, QByteArray> classLibraryList;
    QHash<QByteArray, QByteArray> headerReplacements;


    void parseXml(const QString fileName);
    void checkScopeAddRule(/*const */QtSimpleXml &currentRule);
    QtSimpleXml *loadXml(const QString fileName) const ;
    QString resolveFileName(const QString currentFileName,
                            const QString includeFileName) const;
    bool isReplacementRule(const QString ruleType) const;
    void disableRule(QtSimpleXml &replacementRule);
    bool isRuleDisabled(QtSimpleXml &replacementRule) const;
    void addLogWarning(const QString text) const;
    void addLogError(const QString text) const;
};

QT_END_NAMESPACE

#endif // PORTINGRULES_H
