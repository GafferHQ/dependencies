/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of PySide2.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PYSIDEQMLREGISTERTYPE_H
#define PYSIDEQMLREGISTERTYPE_H

#include <Python.h>

struct SbkObjectType;

namespace PySide
{

extern void* nextQmlElementMemoryAddr;

/**
 * Init the QML support doing things like registering QtQml.ListProperty and create the necessary stuff for
 * qmlRegisterType.
 *
 * \param module QtQml python module
 */
void initQmlSupport(PyObject* module);

/**
 * PySide implementation of qmlRegisterType<T> function.
 *
 * \param pyObj Python type to be registered.
 * \param uri QML element uri.
 * \param versionMajor QML component major version.
 * \param versionMinor QML component minor version.
 * \param qmlName QML element name
 * \return the metatype id of the registered type.
 */
int qmlRegisterType(PyObject *pyObj, const char *uri, int versionMajor, int versionMinor,
                    const char *qmlName);
}

// Volatile Bool Ptr type definition.

typedef struct {
    PyObject_HEAD
    volatile bool flag;
} QtQml_VolatileBoolObject;

PyAPI_DATA(PyTypeObject) QtQml_VolatileBoolType;

#define VolatileBool_Check(op) (Py_TYPE(op) == &QtQml_VolatileBoolType)

#endif
