/****************************************************************************
**
** Copyright (C) 2012 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
#ifndef TESTS_AUTO_CORELIB_TOOLS_MOC_CXX11_FINAL_CLASSES_H
#define TESTS_AUTO_CORELIB_TOOLS_MOC_CXX11_FINAL_CLASSES_H

#include <QtCore/QObject>

#ifndef Q_MOC_RUN // hide from moc
# define final
# define sealed
# define EXPORT_MACRO
# define EXPORT_MACRO2(X,Y,Z)
#endif

class FinalTestClassQt Q_DECL_FINAL : public QObject
{
    Q_OBJECT
public:
    explicit FinalTestClassQt(QObject *parent = 0)
        : QObject(parent) {}
};


class EXPORT_MACRO ExportedFinalTestClassQt Q_DECL_FINAL : public QObject
{
    Q_OBJECT
public:
    explicit ExportedFinalTestClassQt(QObject *parent = 0)
        : QObject(parent) {}
};

class EXPORT_MACRO2(X,Y,Z) ExportedFinalTestClassQtX Q_DECL_FINAL : public QObject
{
    Q_OBJECT
public:
    explicit ExportedFinalTestClassQtX(QObject *parent = 0)
        : QObject(parent) {}
};

class FinalTestClassCpp11 final : public QObject
{
    Q_OBJECT
public:
    explicit FinalTestClassCpp11(QObject *parent = 0)
        : QObject(parent) {}
};

class EXPORT_MACRO ExportedFinalTestClassCpp11 final : public QObject
{
    Q_OBJECT
public:
    explicit ExportedFinalTestClassCpp11(QObject *parent = 0)
        : QObject(parent) {}
};

class EXPORT_MACRO2(X,Y,Z) ExportedFinalTestClassCpp11X final : public QObject
{
    Q_OBJECT
public:
    explicit ExportedFinalTestClassCpp11X(QObject *parent = 0)
        : QObject(parent) {}
};

class SealedTestClass sealed : public QObject
{
    Q_OBJECT
public:
    explicit SealedTestClass(QObject *parent = 0)
        : QObject(parent) {}
};

class EXPORT_MACRO ExportedSealedTestClass sealed : public QObject
{
    Q_OBJECT
public:
    explicit ExportedSealedTestClass(QObject *parent = 0)
        : QObject(parent) {}
};

class EXPORT_MACRO2(X,Y,Z) ExportedSealedTestClassX sealed : public QObject
{
    Q_OBJECT
public:
    explicit ExportedSealedTestClassX(QObject *parent = 0)
        : QObject(parent) {}
};

#ifndef Q_MOC_RUN
# undef final
# undef sealed
# undef EXPORT_MACRO
# undef EXPORT_MACRO2
#endif

#endif // TESTS_AUTO_CORELIB_TOOLS_MOC_CXX11_FINAL_CLASSES_H
