/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 BasysKom GmbH.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlvmemetaobject_p.h"


#include "qqml.h"
#include <private/qqmlrefcount_p.h>
#include "qqmlexpression.h"
#include "qqmlexpression_p.h"
#include "qqmlcontext_p.h"
#include "qqmlbinding_p.h"
#include "qqmlpropertyvalueinterceptor_p.h"

#include <private/qqmlglobal_p.h>

#include <private/qv4object_p.h>
#include <private/qv4variantobject_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4scopedvalue_p.h>

QT_BEGIN_NAMESPACE

QQmlVMEVariantQObjectPtr::QQmlVMEVariantQObjectPtr()
    : QQmlGuard<QObject>(0), m_target(0), m_index(-1)
{
}

QQmlVMEVariantQObjectPtr::~QQmlVMEVariantQObjectPtr()
{
}

void QQmlVMEVariantQObjectPtr::objectDestroyed(QObject *)
{
    if (!m_target || QQmlData::wasDeleted(m_target->object))
        return;

    if (m_index >= 0) {
        QV4::ExecutionEngine *v4 = m_target->properties.engine();
        if (v4) {
            QV4::Scope scope(v4);
            QV4::Scoped<QV4::MemberData> sp(scope, m_target->properties.value());
            if (sp)
                *(sp->data() + m_index) = QV4::Primitive::nullValue();
        }

        m_target->activate(m_target->object, m_target->methodOffset() + m_index, 0);
    }
}

void QQmlVMEVariantQObjectPtr::setGuardedValue(QObject *obj, QQmlVMEMetaObject *target, int index)
{
    m_target = target;
    m_index = index;
    setObject(obj);
}

class QQmlVMEMetaObjectEndpoint : public QQmlNotifierEndpoint
{
public:
    QQmlVMEMetaObjectEndpoint();
    void tryConnect();

    QFlagPointer<QQmlVMEMetaObject> metaObject;
};

QQmlVMEMetaObjectEndpoint::QQmlVMEMetaObjectEndpoint()
    : QQmlNotifierEndpoint(QQmlNotifierEndpoint::QQmlVMEMetaObjectEndpoint)
{
}

void QQmlVMEMetaObjectEndpoint_callback(QQmlNotifierEndpoint *e, void **)
{
    QQmlVMEMetaObjectEndpoint *vmee = static_cast<QQmlVMEMetaObjectEndpoint*>(e);
    vmee->tryConnect();
}

void QQmlVMEMetaObjectEndpoint::tryConnect()
{
    int aliasId = this - metaObject->aliasEndpoints;

    if (metaObject.flag()) {
        // This is actually notify
        int sigIdx = metaObject->methodOffset() + aliasId + metaObject->metaData->propertyCount;
        metaObject->activate(metaObject->object, sigIdx, 0);
    } else {
        QQmlVMEMetaData::AliasData *d = metaObject->metaData->aliasData() + aliasId;
        if (!d->isObjectAlias()) {
            QQmlContextData *ctxt = metaObject->ctxt;
            QObject *target = ctxt->idValues[d->contextIdx].data();
            if (!target)
                return;

            if (d->notifySignal != -1)
                connect(target, d->notifySignal, ctxt->engine);
        }

        metaObject.setFlag();
    }
}


QQmlInterceptorMetaObject::QQmlInterceptorMetaObject(QObject *obj, QQmlPropertyCache *cache)
    : object(obj),
      cache(cache),
      interceptors(0),
      hasAssignedMetaObjectData(false)
{
    QObjectPrivate *op = QObjectPrivate::get(obj);

    if (op->metaObject) {
        parent = op->metaObject;
        // Use the extra flag in QBiPointer to know if we can safely cast parent.asT1() to QQmlVMEMetaObject*
        parent.setFlagValue(QQmlData::get(obj)->hasVMEMetaObject);
    } else {
        parent = obj->metaObject();
    }

    op->metaObject = this;
    QQmlData::get(obj)->hasInterceptorMetaObject = true;
}

QQmlInterceptorMetaObject::~QQmlInterceptorMetaObject()
{

}

void QQmlInterceptorMetaObject::registerInterceptor(int index, int valueIndex, QQmlPropertyValueInterceptor *interceptor)
{
    interceptor->m_coreIndex = index;
    interceptor->m_valueTypeCoreIndex = valueIndex;
    interceptor->m_next = interceptors;
    interceptors = interceptor;
}

int QQmlInterceptorMetaObject::metaCall(QObject *o, QMetaObject::Call c, int id, void **a)
{
    Q_ASSERT(o == object);
    Q_UNUSED(o);

    if (intercept(c, id, a))
        return -1;
    return object->qt_metacall(c, id, a);
}

bool QQmlInterceptorMetaObject::intercept(QMetaObject::Call c, int id, void **a)
{
    if (c == QMetaObject::WriteProperty && interceptors &&
       !(*reinterpret_cast<int*>(a[3]) & QQmlPropertyPrivate::BypassInterceptor)) {

        for (QQmlPropertyValueInterceptor *vi = interceptors; vi; vi = vi->m_next) {
            if (vi->m_coreIndex != id)
                continue;

            int valueIndex = vi->m_valueTypeCoreIndex;
            int type = QQmlData::get(object)->propertyCache->property(id)->propType;

            if (type != QVariant::Invalid) {
                if (valueIndex != -1) {
                    QQmlValueType *valueType = QQmlValueTypeFactory::valueType(type);
                    Q_ASSERT(valueType);

                    //
                    // Consider the following case:
                    //  color c = { 0.1, 0.2, 0.3 }
                    //  interceptor exists on c.r
                    //  write { 0.2, 0.4, 0.6 }
                    //
                    // The interceptor may choose not to update the r component at this
                    // point (for example, a behavior that creates an animation). But we
                    // need to ensure that the g and b components are updated correctly.
                    //
                    // So we need to perform a full write where the value type is:
                    //    r = old value, g = new value, b = new value
                    //
                    // And then call the interceptor which may or may not write the
                    // new value to the r component.
                    //
                    // This will ensure that the other components don't contain stale data
                    // and any relevant signals are emitted.
                    //
                    // To achieve this:
                    //   (1) Store the new value type as a whole (needed due to
                    //       aliasing between a[0] and static storage in value type).
                    //   (2) Read the entire existing value type from object -> valueType temp.
                    //   (3) Read the previous value of the component being changed
                    //       from the valueType temp.
                    //   (4) Write the entire new value type into the temp.
                    //   (5) Overwrite the component being changed with the old value.
                    //   (6) Perform a full write to the value type (which may emit signals etc).
                    //   (7) Issue the interceptor call with the new component value.
                    //

                    QMetaProperty valueProp = valueType->metaObject()->property(valueIndex);
                    QVariant newValue(type, a[0]);

                    valueType->read(object, id);
                    QVariant prevComponentValue = valueProp.read(valueType);

                    valueType->setValue(newValue);
                    QVariant newComponentValue = valueProp.read(valueType);

                    // Don't apply the interceptor if the intercepted value has not changed
                    bool updated = false;
                    if (newComponentValue != prevComponentValue) {
                        valueProp.write(valueType, prevComponentValue);
                        valueType->write(object, id, QQmlPropertyPrivate::DontRemoveBinding | QQmlPropertyPrivate::BypassInterceptor);

                        vi->write(newComponentValue);
                        updated = true;
                    }

                    if (updated)
                        return true;
                } else {
                    vi->write(QVariant(type, a[0]));
                    return true;
                }
            }
        }
    }
    return false;
}


QAbstractDynamicMetaObject *QQmlInterceptorMetaObject::toDynamicMetaObject(QObject *o)
{
    if (!hasAssignedMetaObjectData) {
        *static_cast<QMetaObject *>(this) = *cache->createMetaObject();

        if (parent.isT1())
            this->d.superdata = parent.asT1()->toDynamicMetaObject(o);
        else
            this->d.superdata = parent.asT2();

        hasAssignedMetaObjectData = true;
    }

    return this;
}

QQmlVMEMetaObject::QQmlVMEMetaObject(QObject *obj,
                                     QQmlPropertyCache *cache,
                                     const QQmlVMEMetaData *meta)
    : QQmlInterceptorMetaObject(obj, cache),
      ctxt(QQmlData::get(obj, true)->outerContext), metaData(meta),
      aliasEndpoints(0),
      methods(0)
{
    cache->addref();

    QQmlData::get(obj)->hasVMEMetaObject = true;

    int qobject_type = qMetaTypeId<QObject*>();
    int variant_type = qMetaTypeId<QVariant>();
    // Need JS wrapper to ensure properties are marked.
    // ### FIXME: I hope that this can be removed once we have the proper scope chain
    // set up and the JS wrappers always exist.
    bool needsJSWrapper = (metaData->propertyCount > 0);

    // ### Optimize
    for (int ii = 0; ii < metaData->propertyCount; ++ii) {
        int t = (metaData->propertyData() + ii)->propertyType;
        if (t == qobject_type || t == variant_type) {
            needsJSWrapper = true;
            break;
        }
    }

    if (needsJSWrapper)
        ensureQObjectWrapper();
}

QQmlVMEMetaObject::~QQmlVMEMetaObject()
{
    if (parent.isT1()) parent.asT1()->objectDestroyed(object);
    delete [] aliasEndpoints;
    delete [] methods;

    qDeleteAll(varObjectGuards);

    cache->release();
}

QV4::MemberData *QQmlVMEMetaObject::propertiesAsMemberData()
{
    if (properties.isUndefined()) {
        if (properties.valueRef())
            // in some situations, the QObject wrapper (and associated data,
            // such as the varProperties array) will have been cleaned up, but the
            // QObject ptr will not yet have been deleted (eg, waiting on deleteLater).
            // In this situation, return 0.
            return 0;
        allocateProperties();
    }

    return static_cast<QV4::MemberData*>(properties.asManaged());
}

void QQmlVMEMetaObject::writeProperty(int id, int v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = QV4::Primitive::fromInt32(v);
}

void QQmlVMEMetaObject::writeProperty(int id, bool v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = QV4::Primitive::fromBoolean(v);
}

void QQmlVMEMetaObject::writeProperty(int id, double v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = QV4::Primitive::fromDouble(v);
}

void QQmlVMEMetaObject::writeProperty(int id, const QString& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newString(v);
}

void QQmlVMEMetaObject::writeProperty(int id, const QUrl& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, const QDate& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, const QDateTime& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, const QPointF& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, const QSizeF& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, const QRectF& v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = cache->engine->newVariantObject(QVariant::fromValue(v));
}

void QQmlVMEMetaObject::writeProperty(int id, QObject* v)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        *(md->data() + id) = QV4::QObjectWrapper::wrap(cache->engine, v);

    QQmlVMEVariantQObjectPtr *guard = getQObjectGuardForProperty(id);
    if (v && !guard) {
        guard = new QQmlVMEVariantQObjectPtr();
        varObjectGuards.append(guard);
    }
    if (guard)
        guard->setGuardedValue(v, this, id);
}

int QQmlVMEMetaObject::readPropertyAsInt(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return 0;

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    if (!sv->isInt32())
        return 0;
    return sv->integerValue();
}

bool QQmlVMEMetaObject::readPropertyAsBool(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return false;

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    if (!sv->isBoolean())
        return false;
    return sv->booleanValue();
}

double QQmlVMEMetaObject::readPropertyAsDouble(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return 0.0;

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    if (!sv->isDouble())
        return 0.0;
    return sv->doubleValue();
}

QString QQmlVMEMetaObject::readPropertyAsString(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QString();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    if (!sv->isString())
        return QString();
    return sv->stringValue()->toQString();
}

QUrl QQmlVMEMetaObject::readPropertyAsUrl(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QUrl();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::Url)
        return QUrl();
    return v->d()->data.value<QUrl>();
}

QDate QQmlVMEMetaObject::readPropertyAsDate(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QDate();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::Date)
        return QDate();
    return v->d()->data.value<QDate>();
}

QDateTime QQmlVMEMetaObject::readPropertyAsDateTime(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QDateTime();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::DateTime)
        return QDateTime();
    return v->d()->data.value<QDateTime>();
}

QSizeF QQmlVMEMetaObject::readPropertyAsSizeF(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QSizeF();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::SizeF)
        return QSizeF();
    return v->d()->data.value<QSizeF>();
}

QPointF QQmlVMEMetaObject::readPropertyAsPointF(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QPointF();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::PointF)
        return QPointF();
    return v->d()->data.value<QPointF>();
}

QObject* QQmlVMEMetaObject::readPropertyAsQObject(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return 0;

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::QObjectWrapper *wrapper = sv->as<QV4::QObjectWrapper>();
    if (!wrapper)
        return 0;
    return wrapper->object();
}

QList<QObject *> *QQmlVMEMetaObject::readPropertyAsList(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return 0;

    QV4::Scope scope(cache->engine);
    QV4::Scoped<QV4::VariantObject> v(scope, *(md->data() + id));
    if (!v || (int)v->d()->data.userType() != qMetaTypeId<QList<QObject *> >()) {
        QVariant variant(qVariantFromValue(QList<QObject*>()));
        v = cache->engine->newVariantObject(variant);
        *(md->data() + id) = v;
    }
    return static_cast<QList<QObject *> *>(v->d()->data.data());
}

QRectF QQmlVMEMetaObject::readPropertyAsRectF(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return QRectF();

    QV4::Scope scope(cache->engine);
    QV4::ScopedValue sv(scope, *(md->data() + id));
    const QV4::VariantObject *v = sv->as<QV4::VariantObject>();
    if (!v || v->d()->data.type() != QVariant::RectF)
        return QRectF();
    return v->d()->data.value<QRectF>();
}

int QQmlVMEMetaObject::metaCall(QObject *o, QMetaObject::Call c, int _id, void **a)
{
    Q_ASSERT(o == object);
    Q_UNUSED(o);

    int id = _id;

    if (intercept(c, _id, a))
        return -1;

    if (c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty) {
        if (id >= propOffset()) {
            id -= propOffset();

            if (id < metaData->propertyCount) {
               int t = (metaData->propertyData() + id)->propertyType;
                bool needActivate = false;

                if (t == QQmlVMEMetaData::VarPropertyType) {
                    // the context can be null if accessing var properties from cpp after re-parenting an item.
                    QQmlEnginePrivate *ep = (ctxt == 0 || ctxt->engine == 0) ? 0 : QQmlEnginePrivate::get(ctxt->engine);
                    QV8Engine *v8e = (ep == 0) ? 0 : ep->v8engine();
                    if (v8e) {
                        if (c == QMetaObject::ReadProperty) {
                            *reinterpret_cast<QVariant *>(a[0]) = readPropertyAsVariant(id);
                        } else if (c == QMetaObject::WriteProperty) {
                            writeProperty(id, *reinterpret_cast<QVariant *>(a[0]));
                        }
                    } else if (c == QMetaObject::ReadProperty) {
                        // if the context was disposed, we just return an invalid variant from read.
                        *reinterpret_cast<QVariant *>(a[0]) = QVariant();
                    }

                } else {

                    if (c == QMetaObject::ReadProperty) {
                        switch(t) {
                        case QVariant::Int:
                            *reinterpret_cast<int *>(a[0]) = readPropertyAsInt(id);
                            break;
                        case QVariant::Bool:
                            *reinterpret_cast<bool *>(a[0]) = readPropertyAsBool(id);
                            break;
                        case QVariant::Double:
                            *reinterpret_cast<double *>(a[0]) = readPropertyAsDouble(id);
                            break;
                        case QVariant::String:
                            *reinterpret_cast<QString *>(a[0]) = readPropertyAsString(id);
                            break;
                        case QVariant::Url:
                            *reinterpret_cast<QUrl *>(a[0]) = readPropertyAsUrl(id);
                            break;
                        case QVariant::Date:
                            *reinterpret_cast<QDate *>(a[0]) = readPropertyAsDate(id);
                            break;
                        case QVariant::DateTime:
                            *reinterpret_cast<QDateTime *>(a[0]) = readPropertyAsDateTime(id);
                            break;
                        case QVariant::RectF:
                            *reinterpret_cast<QRectF *>(a[0]) = readPropertyAsRectF(id);
                            break;
                        case QVariant::SizeF:
                            *reinterpret_cast<QSizeF *>(a[0]) = readPropertyAsSizeF(id);
                            break;
                        case QVariant::PointF:
                            *reinterpret_cast<QPointF *>(a[0]) = readPropertyAsPointF(id);
                            break;
                        case QMetaType::QObjectStar:
                            *reinterpret_cast<QObject **>(a[0]) = readPropertyAsQObject(id);
                            break;
                        case QMetaType::QVariant:
                            *reinterpret_cast<QVariant *>(a[0]) = readPropertyAsVariant(id);
                            break;
                        default:
                        {
                            if (t == qMetaTypeId<QQmlListProperty<QObject> >()) {
                                QList<QObject *> *list = readPropertyAsList(id);
                                QQmlListProperty<QObject> *p = static_cast<QQmlListProperty<QObject> *>(a[0]);
                                *p = QQmlListProperty<QObject>(object, list,
                                                                      list_append, list_count, list_at,
                                                                      list_clear);
                                p->dummy1 = this;
                                p->dummy2 = reinterpret_cast<void *>(quintptr(methodOffset() + id));
                            } else {
                                QV4::MemberData *md = propertiesAsMemberData();
                                if (md) {
                                    QVariant propertyAsVariant;
                                    if (QV4::VariantObject *v = (md->data() + id)->as<QV4::VariantObject>())
                                        propertyAsVariant = v->d()->data;
                                    QQml_valueTypeProvider()->readValueType(propertyAsVariant, a[0], t);
                                }
                            }
                            break;
                        }
                        }

                    } else if (c == QMetaObject::WriteProperty) {

                        switch(t) {
                        case QVariant::Int:
                            needActivate = *reinterpret_cast<int *>(a[0]) != readPropertyAsInt(id);
                            writeProperty(id, *reinterpret_cast<int *>(a[0]));
                            break;
                        case QVariant::Bool:
                            needActivate = *reinterpret_cast<bool *>(a[0]) != readPropertyAsBool(id);
                            writeProperty(id, *reinterpret_cast<bool *>(a[0]));
                            break;
                        case QVariant::Double:
                            needActivate = *reinterpret_cast<double *>(a[0]) != readPropertyAsDouble(id);
                            writeProperty(id, *reinterpret_cast<double *>(a[0]));
                            break;
                        case QVariant::String:
                            needActivate = *reinterpret_cast<QString *>(a[0]) != readPropertyAsString(id);
                            writeProperty(id, *reinterpret_cast<QString *>(a[0]));
                            break;
                        case QVariant::Url:
                            needActivate = *reinterpret_cast<QUrl *>(a[0]) != readPropertyAsUrl(id);
                            writeProperty(id, *reinterpret_cast<QUrl *>(a[0]));
                            break;
                        case QVariant::Date:
                            needActivate = *reinterpret_cast<QDate *>(a[0]) != readPropertyAsDate(id);
                            writeProperty(id, *reinterpret_cast<QDate *>(a[0]));
                            break;
                        case QVariant::DateTime:
                            needActivate = *reinterpret_cast<QDateTime *>(a[0]) != readPropertyAsDateTime(id);
                            writeProperty(id, *reinterpret_cast<QDateTime *>(a[0]));
                            break;
                        case QVariant::RectF:
                            needActivate = *reinterpret_cast<QRectF *>(a[0]) != readPropertyAsRectF(id);
                            writeProperty(id, *reinterpret_cast<QRectF *>(a[0]));
                            break;
                        case QVariant::SizeF:
                            needActivate = *reinterpret_cast<QSizeF *>(a[0]) != readPropertyAsSizeF(id);
                            writeProperty(id, *reinterpret_cast<QSizeF *>(a[0]));
                            break;
                        case QVariant::PointF:
                            needActivate = *reinterpret_cast<QPointF *>(a[0]) != readPropertyAsPointF(id);
                            writeProperty(id, *reinterpret_cast<QPointF *>(a[0]));
                            break;
                        case QMetaType::QObjectStar:
                            needActivate = *reinterpret_cast<QObject **>(a[0]) != readPropertyAsQObject(id);
                            writeProperty(id, *reinterpret_cast<QObject **>(a[0]));
                            break;
                        case QMetaType::QVariant:
                            writeProperty(id, *reinterpret_cast<QVariant *>(a[0]));
                            break;
                        default: {
                            QV4::MemberData *md = propertiesAsMemberData();
                            if (md) {
                                QV4::VariantObject *v = (md->data() + id)->as<QV4::VariantObject>();
                                if (!v) {
                                    *(md->data() + id) = cache->engine->newVariantObject(QVariant());
                                    v = (md->data() + id)->as<QV4::VariantObject>();
                                    QQml_valueTypeProvider()->initValueType(t, v->d()->data);
                                }
                                needActivate = !QQml_valueTypeProvider()->equalValueType(t, a[0], v->d()->data);
                                QQml_valueTypeProvider()->writeValueType(t, a[0], v->d()->data);
                            }
                            break;
                        }
                        }
                    }

                }

                if (c == QMetaObject::WriteProperty && needActivate) {
                    activate(object, methodOffset() + id, 0);
                }

                return -1;
            }

            id -= metaData->propertyCount;

            if (id < metaData->aliasCount) {

                QQmlVMEMetaData::AliasData *d = metaData->aliasData() + id;

                if (d->flags & QML_ALIAS_FLAG_PTR && c == QMetaObject::ReadProperty)
                        *reinterpret_cast<void **>(a[0]) = 0;

                if (!ctxt) return -1;

                QQmlContext *context = ctxt->asQQmlContext();
                QQmlContextPrivate *ctxtPriv = QQmlContextPrivate::get(context);

                QObject *target = ctxtPriv->data->idValues[d->contextIdx].data();
                if (!target)
                    return -1;

                connectAlias(id);

                if (d->isObjectAlias()) {
                    *reinterpret_cast<QObject **>(a[0]) = target;
                    return -1;
                }

                // Remove binding (if any) on write
                if(c == QMetaObject::WriteProperty) {
                    int flags = *reinterpret_cast<int*>(a[3]);
                    if (flags & QQmlPropertyPrivate::RemoveBindingOnAliasWrite) {
                        QQmlData *targetData = QQmlData::get(target);
                        if (targetData && targetData->hasBindingBit(d->propertyIndex()))
                            QQmlPropertyPrivate::removeBinding(target, d->propertyIdx);
                    }
                }

                if (d->isValueTypeAlias()) {
                    // Value type property
                    QQmlValueType *valueType = QQmlValueTypeFactory::valueType(d->valueType());
                    Q_ASSERT(valueType);

                    valueType->read(target, d->propertyIndex());
                    int rv = QMetaObject::metacall(valueType, c, d->valueTypeIndex(), a);

                    if (c == QMetaObject::WriteProperty)
                        valueType->write(target, d->propertyIndex(), 0x00);

                    return rv;

                } else {
                    return QMetaObject::metacall(target, c, d->propertyIndex(), a);
                }

            }
            return -1;

        }

    } else if(c == QMetaObject::InvokeMetaMethod) {

        if (id >= methodOffset()) {

            id -= methodOffset();
            int plainSignals = metaData->signalCount + metaData->propertyCount +
                               metaData->aliasCount;
            if (id < plainSignals) {
                activate(object, _id, a);
                return -1;
            }

            id -= plainSignals;

            if (id < metaData->methodCount) {
                if (!ctxt->engine)
                    return -1; // We can't run the method

                QQmlEnginePrivate *ep = QQmlEnginePrivate::get(ctxt->engine);
                ep->referenceScarceResources(); // "hold" scarce resources in memory during evaluation.
                QV4::Scope scope(ep->v4engine());


                QV4::ScopedFunctionObject function(scope, method(id));
                if (!function) {
                    // The function was not compiled.  There are some exceptional cases which the
                    // expression rewriter does not rewrite properly (e.g., \r-terminated lines
                    // are not rewritten correctly but this bug is deemed out-of-scope to fix for
                    // performance reasons; see QTBUG-24064) and thus compilation will have failed.
                    QQmlError e;
                    e.setDescription(QString(QLatin1String("Exception occurred during compilation of function: %1")).
                                     arg(QLatin1String(QMetaObject::method(_id).methodSignature().constData())));
                    ep->warning(e);
                    return -1; // The dynamic method with that id is not available.
                }

                QQmlVMEMetaData::MethodData *data = metaData->methodData() + id;

                QV4::ScopedCallData callData(scope, data->parameterCount);
                callData->thisObject = ep->v8engine()->global();

                for (int ii = 0; ii < data->parameterCount; ++ii)
                    callData->args[ii] = scope.engine->fromVariant(*(QVariant *)a[ii + 1]);

                QV4::ScopedValue result(scope);
                result = function->call(callData);
                if (scope.hasException()) {
                    QQmlError error = scope.engine->catchExceptionAsQmlError();
                    if (error.isValid())
                        ep->warning(error);
                    if (a[0]) *(QVariant *)a[0] = QVariant();
                } else {
                    if (a[0]) *(QVariant *)a[0] = scope.engine->toVariant(result, 0);
                }

                ep->dereferenceScarceResources(); // "release" scarce resources if top-level expression evaluation is complete.
                return -1;
            }
            return -1;
        }
    }

    if (parent.isT1())
        return parent.asT1()->metaCall(object, c, _id, a);
    else
        return object->qt_metacall(c, _id, a);
}

QV4::ReturnedValue QQmlVMEMetaObject::method(int index)
{
    if (!ctxt || !ctxt->isValid()) {
        qWarning("QQmlVMEMetaObject: Internal error - attempted to evaluate a function in an invalid context");
        return QV4::Encode::undefined();
    }

    if (!methods)
        return QV4::Encode::undefined();

    return methods[index].value();
}

QV4::ReturnedValue QQmlVMEMetaObject::readVarProperty(int id)
{
    Q_ASSERT((metaData->propertyData() + id)->propertyType == QQmlVMEMetaData::VarPropertyType);

    QV4::MemberData *md = propertiesAsMemberData();
    if (md)
        return (md->data() + id)->asReturnedValue();
    return QV4::Primitive::undefinedValue().asReturnedValue();
}

QVariant QQmlVMEMetaObject::readPropertyAsVariant(int id)
{
    QV4::MemberData *md = propertiesAsMemberData();
    if (md) {
        const QV4::QObjectWrapper *wrapper = (md->data() + id)->as<QV4::QObjectWrapper>();
        if (wrapper)
            return QVariant::fromValue(wrapper->object());
        const QV4::VariantObject *v = (md->data() + id)->as<QV4::VariantObject>();
        if (v)
            return v->d()->data;
        return cache->engine->toVariant(*(md->data() + id), -1);
    }
    return QVariant();
}

void QQmlVMEMetaObject::writeVarProperty(int id, const QV4::Value &value)
{
    Q_ASSERT((metaData->propertyData() + id)->propertyType == QQmlVMEMetaData::VarPropertyType);

    QV4::MemberData *md = propertiesAsMemberData();
    if (!md)
        return;

    // Importantly, if the current value is a scarce resource, we need to ensure that it
    // gets automatically released by the engine if no other references to it exist.
    QV4::VariantObject *oldVariant = (md->data() + id)->as<QV4::VariantObject>();
    if (oldVariant)
        oldVariant->removeVmePropertyReference();

    QObject *valueObject = 0;
    QQmlVMEVariantQObjectPtr *guard = getQObjectGuardForProperty(id);

    // And, if the new value is a scarce resource, we need to ensure that it does not get
    // automatically released by the engine until no other references to it exist.
    if (QV4::VariantObject *v = const_cast<QV4::VariantObject*>(value.as<QV4::VariantObject>())) {
        v->addVmePropertyReference();
    } else if (QV4::QObjectWrapper *wrapper = const_cast<QV4::QObjectWrapper*>(value.as<QV4::QObjectWrapper>())) {
        // We need to track this QObject to signal its deletion
        valueObject = wrapper->object();

        // Do we already have a QObject guard for this property?
        if (valueObject && !guard) {
            guard = new QQmlVMEVariantQObjectPtr();
            varObjectGuards.append(guard);
        }
    }

    if (guard)
        guard->setGuardedValue(valueObject, this, id);

    // Write the value and emit change signal as appropriate.
    *(md->data() + id) = value;
    activate(object, methodOffset() + id, 0);
}

void QQmlVMEMetaObject::writeProperty(int id, const QVariant &value)
{
    if ((metaData->propertyData() + id)->propertyType == QQmlVMEMetaData::VarPropertyType) {
        QV4::MemberData *md = propertiesAsMemberData();
        if (!md)
            return;

        // Importantly, if the current value is a scarce resource, we need to ensure that it
        // gets automatically released by the engine if no other references to it exist.
        QV4::VariantObject *oldv = (md->data() + id)->as<QV4::VariantObject>();
        if (oldv)
            oldv->removeVmePropertyReference();

        // And, if the new value is a scarce resource, we need to ensure that it does not get
        // automatically released by the engine until no other references to it exist.
        QV4::Scope scope(cache->engine);
        QV4::ScopedValue newv(scope, cache->engine->fromVariant(value));
        QV4::Scoped<QV4::VariantObject> v(scope, newv);
        if (!!v)
            v->addVmePropertyReference();

        // Write the value and emit change signal as appropriate.
        QVariant currentValue = readPropertyAsVariant(id);
        *(md->data() + id) = newv;
        if ((currentValue.userType() != value.userType() || currentValue != value))
            activate(object, methodOffset() + id, 0);
    } else {
        bool needActivate = false;
        if (value.userType() == QMetaType::QObjectStar) {
            QObject *o = *(QObject *const *)value.data();
            needActivate = readPropertyAsQObject(id) != o;  // TODO: still correct?
            writeProperty(id, o);
        } else {
            QV4::MemberData *md = propertiesAsMemberData();
            if (md) {
                QV4::VariantObject *v = (md->data() + id)->as<QV4::VariantObject>();
                needActivate = (!v ||
                                 v->d()->data.userType() != value.userType() ||
                                 v->d()->data != value);
                if (v)
                    v->removeVmePropertyReference();
                *(md->data() + id) = cache->engine->newVariantObject(value);
                v = static_cast<QV4::VariantObject *>(md->data() + id);
                v->addVmePropertyReference();
            }
        }

        if (needActivate)
            activate(object, methodOffset() + id, 0);
    }
}

void QQmlVMEMetaObject::listChanged(int id)
{
    activate(object, methodOffset() + id, 0);
}

void QQmlVMEMetaObject::list_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    QList<QObject *> *list = static_cast<QList<QObject *> *>(prop->data);
    list->append(o);
    static_cast<QQmlVMEMetaObject *>(prop->dummy1)->activate(prop->object, reinterpret_cast<quintptr>(prop->dummy2), 0);
}

int QQmlVMEMetaObject::list_count(QQmlListProperty<QObject> *prop)
{
    QList<QObject *> *list = static_cast<QList<QObject *> *>(prop->data);
    return list->count();
}

QObject *QQmlVMEMetaObject::list_at(QQmlListProperty<QObject> *prop, int index)
{
    QList<QObject *> *list = static_cast<QList<QObject *> *>(prop->data);
    return list->at(index);
}

void QQmlVMEMetaObject::list_clear(QQmlListProperty<QObject> *prop)
{
    QList<QObject *> *list = static_cast<QList<QObject *> *>(prop->data);
    list->clear();
    static_cast<QQmlVMEMetaObject *>(prop->dummy1)->activate(prop->object, reinterpret_cast<quintptr>(prop->dummy2), 0);
}

quint16 QQmlVMEMetaObject::vmeMethodLineNumber(int index)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeMethodLineNumber(index);
    }

    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));

    int rawIndex = index - methodOffset() - plainSignals;

    QQmlVMEMetaData::MethodData *data = metaData->methodData() + rawIndex;
    return data->lineNumber;
}

QV4::ReturnedValue QQmlVMEMetaObject::vmeMethod(int index)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeMethod(index);
    }
    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));
    return method(index - methodOffset() - plainSignals);
}

// Used by debugger
void QQmlVMEMetaObject::setVmeMethod(int index, const QV4::Value &function)
{
    if (index < methodOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->setVmeMethod(index, function);
    }
    int plainSignals = metaData->signalCount + metaData->propertyCount + metaData->aliasCount;
    Q_ASSERT(index >= (methodOffset() + plainSignals) && index < (methodOffset() + plainSignals + metaData->methodCount));

    if (!methods)
        methods = new QV4::PersistentValue[metaData->methodCount];

    int methodIndex = index - methodOffset() - plainSignals;
    methods[methodIndex].set(function.as<QV4::Object>()->engine(), function);
}

QV4::ReturnedValue QQmlVMEMetaObject::vmeProperty(int index)
{
    if (index < propOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        return parentVMEMetaObject()->vmeProperty(index);
    }
    return readVarProperty(index - propOffset());
}

void QQmlVMEMetaObject::setVMEProperty(int index, const QV4::Value &v)
{
    if (index < propOffset()) {
        Q_ASSERT(parentVMEMetaObject());
        parentVMEMetaObject()->setVMEProperty(index, v);
        return;
    }
    return writeVarProperty(index - propOffset(), v);
}

void QQmlVMEMetaObject::ensureQObjectWrapper()
{
    Q_ASSERT(cache && cache->engine);
    QV4::ExecutionEngine *v4 = cache->engine;
    QV4::QObjectWrapper::wrap(v4, object);
}

void QQmlVMEMetaObject::mark(QV4::ExecutionEngine *e)
{
    QV4::ExecutionEngine *v4 = cache ? cache->engine : 0;
    if (v4 != e)
        return;

    properties.markOnce(e);

    if (QQmlVMEMetaObject *parent = parentVMEMetaObject())
        parent->mark(e);
}

void QQmlVMEMetaObject::allocateProperties()
{
    Q_ASSERT(cache && cache->engine);
    Q_ASSERT(!properties.valueRef());
    QV4::ExecutionEngine *v4 = cache->engine;
    QV4::Heap::MemberData *data = QV4::MemberData::allocate(v4, metaData->propertyCount);
    properties.set(v4, data);
    for (uint i = 0; i < data->size; ++i)
        data->data[i] = QV4::Encode::undefined();
}

bool QQmlVMEMetaObject::aliasTarget(int index, QObject **target, int *coreIndex, int *valueTypeIndex) const
{
    Q_ASSERT(index >= propOffset() + metaData->propertyCount);

    *target = 0;
    *coreIndex = -1;
    *valueTypeIndex = -1;

    if (!ctxt)
        return false;

    QQmlVMEMetaData::AliasData *d = metaData->aliasData() + (index - propOffset() - metaData->propertyCount);
    QQmlContext *context = ctxt->asQQmlContext();
    QQmlContextPrivate *ctxtPriv = QQmlContextPrivate::get(context);

    *target = ctxtPriv->data->idValues[d->contextIdx].data();
    if (!*target)
        return false;

    if (d->isObjectAlias()) {
    } else if (d->isValueTypeAlias()) {
        *coreIndex = d->propertyIndex();
        *valueTypeIndex = d->valueTypeIndex();
    } else {
        *coreIndex = d->propertyIndex();
    }

    return true;
}

void QQmlVMEMetaObject::connectAlias(int aliasId)
{
    if (!aliasEndpoints)
        aliasEndpoints = new QQmlVMEMetaObjectEndpoint[metaData->aliasCount];

    QQmlVMEMetaData::AliasData *d = metaData->aliasData() + aliasId;

    QQmlVMEMetaObjectEndpoint *endpoint = aliasEndpoints + aliasId;
    if (endpoint->metaObject.data()) {
        // already connected
        Q_ASSERT(endpoint->metaObject.data() == this);
        return;
    }

    endpoint->metaObject = this;
    endpoint->connect(&ctxt->idValues[d->contextIdx].bindings);
    endpoint->tryConnect();
}

void QQmlVMEMetaObject::connectAliasSignal(int index, bool indexInSignalRange)
{
    int aliasId = (index - (indexInSignalRange ? signalOffset() : methodOffset())) - metaData->propertyCount;
    if (aliasId < 0 || aliasId >= metaData->aliasCount)
        return;

    connectAlias(aliasId);
}

/*! \internal
    \a index is in the method index range (QMetaMethod::methodIndex()).
*/
void QQmlVMEMetaObject::activate(QObject *object, int index, void **args)
{
    QMetaObject::activate(object, signalOffset(), index - methodOffset(), args);
}

QQmlVMEMetaObject *QQmlVMEMetaObject::getForProperty(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->propOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

QQmlVMEMetaObject *QQmlVMEMetaObject::getForMethod(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->methodOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

/*! \internal
    \a coreIndex is in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
QQmlVMEMetaObject *QQmlVMEMetaObject::getForSignal(QObject *o, int coreIndex)
{
    QQmlVMEMetaObject *vme = QQmlVMEMetaObject::get(o);
    while (vme && vme->signalOffset() > coreIndex)
        vme = vme->parentVMEMetaObject();

    Q_ASSERT(vme);
    return vme;
}

QQmlVMEVariantQObjectPtr *QQmlVMEMetaObject::getQObjectGuardForProperty(int index) const
{
    QList<QQmlVMEVariantQObjectPtr *>::ConstIterator it = varObjectGuards.constBegin(), end = varObjectGuards.constEnd();
    for ( ; it != end; ++it) {
        if ((*it)->m_index == index) {
            return *it;
        }
    }

    return 0;
}

QT_END_NAMESPACE
