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

#include "tools.h"
#include "environment.h"

#include <qdir.h>
#include <qfile.h>
#include <qbytearray.h>
#include <qstringlist.h>

#include <iostream>

std::ostream &operator<<(std::ostream &s, const QString &val);
using namespace std;

void Tools::checkLicense(QMap<QString,QString> &dictionary,
                         const QString &sourcePath, const QString &buildPath)
{
    QString tpLicense = sourcePath + "/LICENSE.PREVIEW.COMMERCIAL";
    if (QFile::exists(tpLicense)) {
        dictionary["EDITION"] = "Preview";
        dictionary["LICENSE FILE"] = tpLicense;
        return;
    }

    dictionary["LICHECK"] = "licheck.exe";

    const QString licenseChecker =
        QDir::toNativeSeparators(sourcePath + "/bin/licheck.exe");

    if (QFile::exists(licenseChecker)) {
        const QString qMakeSpec =
            QDir::toNativeSeparators(dictionary.value("QMAKESPEC"));
        const QString xQMakeSpec =
            QDir::toNativeSeparators(dictionary.value("XQMAKESPEC"));

        QString command = QString("%1 %2 %3 %4 %5 %6")
            .arg(licenseChecker,
                 dictionary.value("LICENSE_CONFIRMED", "no"),
                 QDir::toNativeSeparators(sourcePath),
                 QDir::toNativeSeparators(buildPath),
                 qMakeSpec, xQMakeSpec);

        int returnValue = 0;
        QString licheckOutput = Environment::execute(command, &returnValue);

        if (returnValue) {
            dictionary["DONE"] = "error";
        } else {
            foreach (const QString &var, licheckOutput.split('\n'))
                dictionary[var.section('=', 0, 0).toUpper()] = var.section('=', 1, 1);
        }
    } else {
        cout << endl << "Error: Could not find licheck.exe" << endl
             << "Try re-installing." << endl << endl;
        dictionary["DONE"] = "error";
    }
}

