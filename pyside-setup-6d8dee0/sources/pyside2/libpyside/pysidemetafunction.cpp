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
#include <sbkpython.h>
#include "pysidemetafunction.h"
#include "pysidemetafunction_p.h"

#include <shiboken.h>
#include <QObject>
#include <QMetaMethod>
#include <QDebug>

extern "C"
{

struct PySideMetaFunctionPrivate
{
    QObject* qobject;
    int methodIndex;
};

//methods
static void         functionFree(void*);
static PyObject*    functionCall(PyObject*, PyObject*, PyObject*);

PyTypeObject PySideMetaFunctionType = {
    PyVarObject_HEAD_INIT(0, 0)
    /*tp_name*/             "PySide.MetaFunction",
    /*tp_basicsize*/        sizeof(PySideMetaFunction),
    /*tp_itemsize*/         0,
    /*tp_dealloc*/          0,
    /*tp_print*/            0,
    /*tp_getattr*/          0,
    /*tp_setattr*/          0,
    /*tp_compare*/          0,
    /*tp_repr*/             0,
    /*tp_as_number*/        0,
    /*tp_as_sequence*/      0,
    /*tp_as_mapping*/       0,
    /*tp_hash*/             0,
    /*tp_call*/             functionCall,
    /*tp_str*/              0,
    /*tp_getattro*/         0,
    /*tp_setattro*/         0,
    /*tp_as_buffer*/        0,
    /*tp_flags*/            Py_TPFLAGS_DEFAULT,
    /*tp_doc*/              "MetaFunction",
    /*tp_traverse*/         0,
    /*tp_clear*/            0,
    /*tp_richcompare*/      0,
    /*tp_weaklistoffset*/   0,
    /*tp_iter*/             0,
    /*tp_iternext*/         0,
    /*tp_methods*/          0,
    /*tp_members*/          0,
    /*tp_getset*/           0,
    /*tp_base*/             0,
    /*tp_dict*/             0,
    /*tp_descr_get*/        0,
    /*tp_descr_set*/        0,
    /*tp_dictoffset*/       0,
    /*tp_init*/             0,
    /*tp_alloc*/            0,
    /*tp_new*/              PyType_GenericNew,
    /*tp_free*/             functionFree,
    /*tp_is_gc*/            0,
    /*tp_bases*/            0,
    /*tp_mro*/              0,
    /*tp_cache*/            0,
    /*tp_subclasses*/       0,
    /*tp_weaklist*/         0,
    /*tp_del*/              0,
    /*tp_version_tag*/      0
};

void functionFree(void *self)
{
    PySideMetaFunction* function = reinterpret_cast<PySideMetaFunction*>(self);
    delete function->d;
}

PyObject *functionCall(PyObject *self, PyObject *args, PyObject * /* kw */)
{
    PySideMetaFunction* function = reinterpret_cast<PySideMetaFunction*>(self);

    PyObject* retVal;
    if (!PySide::MetaFunction::call(function->d->qobject, function->d->methodIndex, args, &retVal))
        return 0;
    return retVal;
}

} // extern "C"

namespace PySide { namespace MetaFunction {

void init(PyObject* module)
{
    if (PyType_Ready(&PySideMetaFunctionType) < 0)
        return;

    PyModule_AddObject(module, "MetaFunction", reinterpret_cast<PyObject *>(&PySideMetaFunctionType));
}

PySideMetaFunction* newObject(QObject* source, int methodIndex)
{
    if (methodIndex >= source->metaObject()->methodCount())
        return 0;

    QMetaMethod method = source->metaObject()->method(methodIndex);
    if ((method.methodType() == QMetaMethod::Slot) ||
        (method.methodType() == QMetaMethod::Method)) {
        PySideMetaFunction* function = PyObject_New(PySideMetaFunction, &PySideMetaFunctionType);
        function->d = new PySideMetaFunctionPrivate();
        function->d->qobject = source;
        function->d->methodIndex = methodIndex;
        return function;
    }
    return 0;
}

bool call(QObject* self, int methodIndex, PyObject* args, PyObject** retVal)
{

    QMetaMethod method = self->metaObject()->method(methodIndex);
    QList<QByteArray> argTypes = method.parameterTypes();

    // args given plus return type
    Shiboken::AutoDecRef sequence(PySequence_Fast(args, 0));
    int numArgs = PySequence_Fast_GET_SIZE(sequence.object()) + 1;

    if (numArgs - 1 > argTypes.count()) {
        PyErr_Format(PyExc_TypeError, "%s only accepts %d argument(s), %d given!",
                     method.methodSignature().constData(),
                     argTypes.count(), numArgs - 1);
        return false;
    }

    if (numArgs - 1 < argTypes.count()) {
        PyErr_Format(PyExc_TypeError, "%s needs %d argument(s), %d given!",
                     method.methodSignature().constData(),
                     argTypes.count(), numArgs - 1);
        return false;
    }

    QVariant* methValues = new QVariant[numArgs];
    void** methArgs = new void*[numArgs];

    // Prepare room for return type
    const char* returnType = method.typeName();
    if (returnType && std::strcmp("void", returnType))
        argTypes.prepend(returnType);
    else
        argTypes.prepend(QByteArray());

    int i;
    for (i = 0; i < numArgs; ++i) {
        const QByteArray& typeName = argTypes[i];
        // This must happen only when the method hasn't return type.
        if (typeName.isEmpty()) {
            methArgs[i] = 0;
            continue;
        }

        Shiboken::Conversions::SpecificConverter converter(typeName);
        if (converter) {
            int typeId = QMetaType::type(typeName);
            if (!Shiboken::Conversions::pythonTypeIsObjectType(converter)) {
                if (!typeId) {
                    PyErr_Format(PyExc_TypeError, "Value types used on meta functions (including signals) need to be "
                                                  "registered on meta type: %s", typeName.data());
                    break;
                }
                methValues[i] = QVariant(typeId, static_cast<const void *>(0));
            }
            methArgs[i] = methValues[i].data();
            if (i == 0) // Don't do this for return type
                continue;
            if (typeId == QVariant::String) {
                QString tmp;
                converter.toCpp(PySequence_Fast_GET_ITEM(sequence.object(), i - 1), &tmp);
                methValues[i] = tmp;
            } else {
                converter.toCpp(PySequence_Fast_GET_ITEM(sequence.object(), i - 1), methArgs[i]);
            }
        } else {
            PyErr_Format(PyExc_TypeError, "Unknown type used to call meta function (that may be a signal): %s", argTypes[i].constData());
            break;
        }
    }

    bool ok = i == numArgs;
    if (ok) {
        Py_BEGIN_ALLOW_THREADS
        QMetaObject::metacall(self, QMetaObject::InvokeMetaMethod, method.methodIndex(), methArgs);
        Py_END_ALLOW_THREADS

        if (retVal) {
            if (methArgs[0]) {
                static SbkConverter* qVariantTypeConverter = Shiboken::Conversions::getConverter("QVariant");
                Q_ASSERT(qVariantTypeConverter);
                *retVal = Shiboken::Conversions::copyToPython(qVariantTypeConverter, &methValues[0]);
            } else {
                *retVal = Py_None;
                Py_INCREF(*retVal);
            }
        }
    }

    delete[] methArgs;
    delete[] methValues;

    return ok;
}


} //namespace MetaFunction
} //namespace PySide

