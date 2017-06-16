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

#include "pysideqmlregistertype.h"

// shiboken
#include <shiboken.h>

// pyside
#include <pyside.h>
#include <pysideproperty.h>

// auto generated headers
#include "pyside2_qtcore_python.h"
#include "pyside2_qtqml_python.h"

#ifndef PYSIDE_MAX_QML_TYPES
// Maximum number of different Qt QML types the user can export to QML using
// qmlRegisterType. This limit exists because the QML engine instantiates objects
// by calling a function with one argument (a void* pointer where the object should
// be created), and thus does not allow us to choose which object to create. Thus
// we create a C++ factory function for each new registered type at compile time.
#define PYSIDE_MAX_QML_TYPES 50
#endif

// Forward declarations.
static void propListMetaCall(PySideProperty* pp, PyObject* self, QMetaObject::Call call,
                             void **args);

// All registered python types and their creation functions.
static PyObject* pyTypes[PYSIDE_MAX_QML_TYPES];
static void (*createFuncs[PYSIDE_MAX_QML_TYPES])(void*);

// Mutex used to avoid race condition on PySide::nextQObjectMemoryAddr.
static QMutex nextQmlElementMutex;

template<int N>
struct ElementFactoryBase
{
    static void createInto(void* memory)
    {
        QMutexLocker locker(&nextQmlElementMutex);
        PySide::setNextQObjectMemoryAddr(memory);
        Shiboken::GilState state;
        PyObject* obj = PyObject_CallObject(pyTypes[N], 0);
        if (!obj || PyErr_Occurred())
            PyErr_Print();
        PySide::setNextQObjectMemoryAddr(0);
    }
};

template<int N>
struct ElementFactory : ElementFactoryBase<N>
{
    static void init()
    {
        createFuncs[N] = &ElementFactoryBase<N>::createInto;
        ElementFactory<N-1>::init();
    }
};

template<>
struct  ElementFactory<0> : ElementFactoryBase<0>
{
    static void init()
    {
        createFuncs[0] = &ElementFactoryBase<0>::createInto;
    }
};

int PySide::qmlRegisterType(PyObject *pyObj, const char *uri, int versionMajor,
                            int versionMinor, const char *qmlName)
{
    using namespace Shiboken;

    static PyTypeObject *qobjectType = Shiboken::Conversions::getPythonTypeObject("QObject*");
    assert(qobjectType);
    static int nextType = 0;

    if (nextType >= PYSIDE_MAX_QML_TYPES) {
        PyErr_Format(PyExc_TypeError, "You can only export %d custom QML types to QML.",
                     PYSIDE_MAX_QML_TYPES);
        return -1;
    }

    PyTypeObject *pyObjType = reinterpret_cast<PyTypeObject *>(pyObj);
    if (!PySequence_Contains(pyObjType->tp_mro, reinterpret_cast<PyObject *>(qobjectType))) {
        PyErr_Format(PyExc_TypeError, "A type inherited from %s expected, got %s.",
                     qobjectType->tp_name, pyObjType->tp_name);
        return -1;
    }

    QMetaObject *metaObject = reinterpret_cast<QMetaObject *>(
                ObjectType::getTypeUserData(reinterpret_cast<SbkObjectType *>(pyObj)));
    Q_ASSERT(metaObject);

    QQmlPrivate::RegisterType type;
    type.version = 0;

    // Allow registering Qt Quick items.
    bool registered = false;
#ifdef PYSIDE_QML_SUPPORT
    QuickRegisterItemFunction quickRegisterItemFunction = getQuickRegisterItemFunction();
    if (quickRegisterItemFunction) {
        registered = quickRegisterItemFunction(pyObj, uri, versionMajor, versionMinor,
                                               qmlName, &type);
    }
#endif

    // Register as simple QObject rather than Qt Quick item.
    if (!registered) {
        // Incref the type object, don't worry about decref'ing it because
        // there's no way to unregister a QML type.
        Py_INCREF(pyObj);

        pyTypes[nextType] = pyObj;

        // FIXME: Fix this to assign new type ids each time.
        type.typeId = qMetaTypeId<QObject*>();
        type.listId = qMetaTypeId<QQmlListProperty<QObject> >();
        type.attachedPropertiesFunction = QQmlPrivate::attachedPropertiesFunc<QObject>();
        type.attachedPropertiesMetaObject = QQmlPrivate::attachedPropertiesMetaObject<QObject>();

        type.parserStatusCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlParserStatus>::cast();
        type.valueSourceCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlPropertyValueSource>::cast();
        type.valueInterceptorCast =
                QQmlPrivate::StaticCastSelector<QObject, QQmlPropertyValueInterceptor>::cast();

        int objectSize = static_cast<int>(PySide::getSizeOfQObject(
                                              reinterpret_cast<SbkObjectType *>(pyObj)));
        type.objectSize = objectSize;
        type.create = createFuncs[nextType];
        type.uri = uri;
        type.versionMajor = versionMajor;
        type.versionMinor = versionMinor;
        type.elementName = qmlName;
        type.metaObject = metaObject;

        type.extensionObjectCreate = 0;
        type.extensionMetaObject = 0;
        type.customParser = 0;
        ++nextType;
    }

    int qmlTypeId = QQmlPrivate::qmlregister(QQmlPrivate::TypeRegistration, &type);
    if (qmlTypeId == -1) {
        PyErr_Format(PyExc_TypeError, "QML meta type registration of \"%s\" failed.",
                     qmlName);
    }
    return qmlTypeId;
}

extern "C"
{

// This is the user data we store in the property.
struct QmlListProperty
{
    PyTypeObject* type;
    PyObject* append;
    PyObject* at;
    PyObject* clear;
    PyObject* count;
};

static int propListTpInit(PyObject* self, PyObject* args, PyObject* kwds)
{
    static const char *kwlist[] = {"type", "append", "at", "clear", "count", 0};
    PySideProperty* pySelf = reinterpret_cast<PySideProperty*>(self);
    QmlListProperty* data = new QmlListProperty;
    memset(data, 0, sizeof(QmlListProperty));

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "OO|OOO:QtQml.ListProperty", (char**) kwlist,
                                     &data->type,
                                     &data->append,
                                     &data->at,
                                     &data->clear,
                                     &data->count)) {
        return 0;
    }
    PySide::Property::setMetaCallHandler(pySelf, &propListMetaCall);
    PySide::Property::setTypeName(pySelf, "QQmlListProperty<QObject>");
    PySide::Property::setUserData(pySelf, data);

    return 1;
}

void propListTpFree(void* self)
{
    PySideProperty* pySelf = reinterpret_cast<PySideProperty*>(self);
    delete reinterpret_cast<QmlListProperty*>(PySide::Property::userData(pySelf));
    // calls base type constructor
    Py_TYPE(pySelf)->tp_base->tp_free(self);
}

PyTypeObject PropertyListType = {
    PyVarObject_HEAD_INIT(0, 0)
    "ListProperty",            /*tp_name*/
    sizeof(PySideProperty),    /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /*tp_flags*/
    0,                         /*tp_doc */
    0,                         /*tp_traverse */
    0,                         /*tp_clear */
    0,                         /*tp_richcompare */
    0,                         /*tp_weaklistoffset */
    0,                         /*tp_iter */
    0,                         /*tp_iternext */
    0,                         /*tp_methods */
    0,                         /*tp_members */
    0,                         /*tp_getset */
    &PySidePropertyType,       /*tp_base */
    0,                         /*tp_dict */
    0,                         /*tp_descr_get */
    0,                         /*tp_descr_set */
    0,                         /*tp_dictoffset */
    propListTpInit,            /*tp_init */
    0,                         /*tp_alloc */
    0,                         /*tp_new */
    propListTpFree,            /*tp_free */
    0,                         /*tp_is_gc */
    0,                         /*tp_bases */
    0,                         /*tp_mro */
    0,                         /*tp_cache */
    0,                         /*tp_subclasses */
    0,                         /*tp_weaklist */
    0,                         /*tp_del */
};

} // extern "C"

// Implementation of QQmlListProperty<T>::AppendFunction callback
void propListAppender(QQmlListProperty<QObject> *propList, QObject *item)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(2));
    PyTuple_SET_ITEM(args, 0, Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], propList->object));
    PyTuple_SET_ITEM(args, 1, Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], item));

    QmlListProperty* data = reinterpret_cast<QmlListProperty*>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->append, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// Implementation of QQmlListProperty<T>::CountFunction callback
int propListCount(QQmlListProperty<QObject> *propList)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(1));
    PyTuple_SET_ITEM(args, 0, Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], propList->object));

    QmlListProperty* data = reinterpret_cast<QmlListProperty*>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->count, args));

    // Check return type
    int cppResult = 0;
    PythonToCppFunc pythonToCpp;
    if (PyErr_Occurred())
        PyErr_Print();
    else if ((pythonToCpp = Shiboken::Conversions::isPythonToCppConvertible(Shiboken::Conversions::PrimitiveTypeConverter<int>(), retVal)))
        pythonToCpp(retVal, &cppResult);
    return cppResult;
}

// Implementation of QQmlListProperty<T>::AtFunction callback
QObject *propListAt(QQmlListProperty<QObject> *propList, int index)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(2));
    PyTuple_SET_ITEM(args, 0, Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], propList->object));
    PyTuple_SET_ITEM(args, 1, Shiboken::Conversions::copyToPython(Shiboken::Conversions::PrimitiveTypeConverter<int>(), &index));

    QmlListProperty* data = reinterpret_cast<QmlListProperty*>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->at, args));

    QObject *result = 0;
    if (PyErr_Occurred())
        PyErr_Print();
    else if (PyType_IsSubtype(Py_TYPE(retVal), data->type))
        Shiboken::Conversions::pythonToCppPointer((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], retVal, &result);
    return result;
}

// Implementation of QQmlListProperty<T>::ClearFunction callback
void propListClear(QQmlListProperty<QObject> * propList)
{
    Shiboken::GilState state;

    Shiboken::AutoDecRef args(PyTuple_New(1));
    PyTuple_SET_ITEM(args, 0, Shiboken::Conversions::pointerToPython((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], propList->object));

    QmlListProperty* data = reinterpret_cast<QmlListProperty*>(propList->data);
    Shiboken::AutoDecRef retVal(PyObject_CallObject(data->clear, args));

    if (PyErr_Occurred())
        PyErr_Print();
}

// qt_metacall specialization for ListProperties
static void propListMetaCall(PySideProperty* pp, PyObject* self, QMetaObject::Call call, void** args)
{
    if (call != QMetaObject::ReadProperty)
        return;

    QmlListProperty* data = reinterpret_cast<QmlListProperty*>(PySide::Property::userData(pp));
    QObject* qobj;
    Shiboken::Conversions::pythonToCppPointer((SbkObjectType*)SbkPySide2_QtCoreTypes[SBK_QOBJECT_IDX], self, &qobj);
    QQmlListProperty<QObject> declProp(qobj, data, &propListAppender, &propListCount, &propListAt, &propListClear);

    // Copy the data to the memory location requested by the meta call
    void* v = args[0];
    *reinterpret_cast<QQmlListProperty<QObject> *>(v) = declProp;
}

// VolatileBool (volatile bool) type definition.

static PyObject *
QtQml_VolatileBoolObject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static const char *kwlist[] = {"x", 0};
    PyObject *x = Py_False;
    long ok;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O:bool", const_cast<char **>(kwlist), &x))
        return Q_NULLPTR;
    ok = PyObject_IsTrue(x);
    if (ok < 0)
        return Q_NULLPTR;

    QtQml_VolatileBoolObject *self
            = reinterpret_cast<QtQml_VolatileBoolObject *>(type->tp_alloc(type, 0));

    if (self != Q_NULLPTR)
        self->flag = ok;

    return reinterpret_cast<PyObject *>(self);
}

static PyObject *
QtQml_VolatileBoolObject_get(QtQml_VolatileBoolObject *self)
{
    if (self->flag)
        return Py_True;
    return Py_False;
}

static PyObject *
QtQml_VolatileBoolObject_set(QtQml_VolatileBoolObject *self, PyObject *args)
{
    PyObject *value = Py_False;
    long ok;

    if (!PyArg_ParseTuple(args, "O:bool", &value)) {
        return Q_NULLPTR;
    }

    ok = PyObject_IsTrue(value);
    if (ok < 0) {
        PyErr_SetString(PyExc_TypeError, "Not a boolean value.");
        return Q_NULLPTR;
    }

    if (ok > 0)
        self->flag = true;
    else
        self->flag = false;

    Py_RETURN_NONE;
}

static PyMethodDef QtQml_VolatileBoolObject_methods[] = {
    {"get", reinterpret_cast<PyCFunction>(QtQml_VolatileBoolObject_get), METH_NOARGS,
     "B.get() -> Bool. Returns the value of the volatile boolean"
    },
    {"set", reinterpret_cast<PyCFunction>(QtQml_VolatileBoolObject_set), METH_VARARGS,
     "B.set(a) -> None. Sets the value of the volatile boolean"
    },
    {Q_NULLPTR}  /* Sentinel */
};

static PyObject *
QtQml_VolatileBoolObject_repr(QtQml_VolatileBoolObject *self)
{
    PyObject *s;

    if (self->flag)
        s = PyBytes_FromFormat("%s(True)",
                                Py_TYPE(self)->tp_name);
    else
        s = PyBytes_FromFormat("%s(False)",
                                Py_TYPE(self)->tp_name);
    Py_XINCREF(s);
    return s;
}

static PyObject *
QtQml_VolatileBoolObject_str(QtQml_VolatileBoolObject *self)
{
    PyObject *s;

    if (self->flag)
        s = PyBytes_FromFormat("%s(True) -> %p",
                                Py_TYPE(self)->tp_name, &(self->flag));
    else
        s = PyBytes_FromFormat("%s(False) -> %p",
                                Py_TYPE(self)->tp_name, &(self->flag));
    Py_XINCREF(s);
    return s;
}

PyTypeObject QtQml_VolatileBoolType = {
    PyVarObject_HEAD_INIT(Q_NULLPTR, 0)                         /*ob_size*/
    "VolatileBool",                                             /*tp_name*/
    sizeof(QtQml_VolatileBoolObject),                           /*tp_basicsize*/
    0,                                                          /*tp_itemsize*/
    0,                                                          /*tp_dealloc*/
    0,                                                          /*tp_print*/
    0,                                                          /*tp_getattr*/
    0,                                                          /*tp_setattr*/
    0,                                                          /*tp_compare*/
    reinterpret_cast<reprfunc>(QtQml_VolatileBoolObject_repr),  /*tp_repr*/
    0,                                                          /*tp_as_number*/
    0,                                                          /*tp_as_sequence*/
    0,                                                          /*tp_as_mapping*/
    0,                                                          /*tp_hash */
    0,                                                          /*tp_call*/
    reinterpret_cast<reprfunc>(QtQml_VolatileBoolObject_str),   /*tp_str*/
    0,                                                          /*tp_getattro*/
    0,                                                          /*tp_setattro*/
    0,                                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,                                         /*tp_flags*/
    "VolatileBool objects contain a C++ volatile bool",         /* tp_doc */
    0,                                                          /* tp_traverse */
    0,                                                          /* tp_clear */
    0,                                                          /* tp_richcompare */
    0,                                                          /* tp_weaklistoffset */
    0,                                                          /* tp_iter */
    0,                                                          /* tp_iternext */
    QtQml_VolatileBoolObject_methods,                           /* tp_methods */
    0,                                                          /* tp_members */
    0,                                                          /* tp_getset */
    0,                                                          /* tp_base */
    0,                                                          /* tp_dict */
    0,                                                          /* tp_descr_get */
    0,                                                          /* tp_descr_set */
    0,                                                          /* tp_dictoffset */
    0,                                                          /* tp_init */
    0,                                                          /* tp_alloc */
    QtQml_VolatileBoolObject_new,                               /* tp_new */
    0,                                                          /* tp_free */
    0,                                                          /* tp_is_gc */
    0,                                                          /* tp_bases */
    0,                                                          /* tp_mro */
    0,                                                          /* tp_cache */
    0,                                                          /* tp_subclasses */
    0,                                                          /* tp_weaklist */
    0,                                                          /* tp_del */
    0,                                                          /* tp_version_tag */
#if PY_MAJOR_VERSION > 3 || PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4
    0                                                           /* tp_finalize */
#endif
};

void PySide::initQmlSupport(PyObject* module)
{
    ElementFactory<PYSIDE_MAX_QML_TYPES - 1>::init();

    // Export QmlListProperty type
    if (PyType_Ready(&PropertyListType) < 0) {
        qWarning() << "Error initializing PropertyList type.";
        return;
    }

    Py_INCREF(reinterpret_cast<PyObject *>(&PropertyListType));
    PyModule_AddObject(module, PropertyListType.tp_name,
                       reinterpret_cast<PyObject *>(&PropertyListType));

    if (PyType_Ready(&QtQml_VolatileBoolType) < 0) {
        qWarning() << "Error initializing VolatileBool type.";
        return;
    }

    Py_INCREF(&QtQml_VolatileBoolType);
    PyModule_AddObject(module, QtQml_VolatileBoolType.tp_name,
                       reinterpret_cast<PyObject *>(&QtQml_VolatileBoolType));
}
