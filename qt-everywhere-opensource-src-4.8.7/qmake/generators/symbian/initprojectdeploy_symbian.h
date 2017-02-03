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

#ifndef INITPROJECTDEPLOYSYMBIAN_H
#define INITPROJECTDEPLOYSYMBIAN_H

#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <option.h>
#include <qdir.h>
#include <qfile.h>
#include <stdlib.h>

#define PLUGIN_STUB_DIR "qmakepluginstubs"
#define ROM_DEPLOYMENT_PLATFORM "rom"
#define EMULATOR_DEPLOYMENT_PLATFORM "emulator"

struct CopyItem
{
    CopyItem(const QString& f, const QString& t)
        : from(f) , to(t) { }
    CopyItem(const QString& f, const QString& t, const QStringList& l)
        : from(f) , to(t), flags(l) { }
    QString from;
    QString to;
    QStringList flags;
};
typedef QList<CopyItem> DeploymentList;

extern QString generate_uid(const QString& target);
extern QString generate_test_uid(const QString& target);

extern void initProjectDeploySymbian(QMakeProject* project,
                              DeploymentList &deploymentList,
                              const QString &testPath,
                              bool deployBinaries,
                              bool epocBuild,
                              const QString &platform,
                              const QString &build,
                              QStringList& generatedDirs,
                              QStringList& generatedFiles);

#endif // INITPROJECTDEPLOYSYMBIAN_H
