/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qqmljavascriptexpression_p.h"

#include <private/qqmlexpression_p.h>
#include <private/qqmlcontextwrapper_p.h>
#include <private/qv4value_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4script_p.h>
#include <private/qv4errorobject_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qqmlglobal_p.h>

QT_BEGIN_NAMESPACE

bool QQmlDelayedError::addError(QQmlEnginePrivate *e)
{
    if (!e) return false;

    if (e->inProgressCreations == 0) return false; // Not in construction

    if (prevError) return true; // Already in error chain

    prevError = &e->erroredBindings;
    nextError = e->erroredBindings;
    e->erroredBindings = this;
    if (nextError) nextError->prevError = &nextError;

    return true;
}

void QQmlDelayedError::setErrorLocation(const QQmlSourceLocation &sourceLocation)
{
    m_error.setUrl(QUrl(sourceLocation.sourceFile));
    m_error.setLine(sourceLocation.line);
    m_error.setColumn(sourceLocation.column);
}

void QQmlDelayedError::setErrorDescription(const QString &description)
{
    m_error.setDescription(description);
}

void QQmlDelayedError::setErrorObject(QObject *object)
{
    m_error.setObject(object);
}

void QQmlDelayedError::catchJavaScriptException(QV4::ExecutionEngine *engine)
{
    m_error = engine->catchExceptionAsQmlError();
}


QQmlJavaScriptExpression::QQmlJavaScriptExpression()
    : m_error(0),
      m_context(0),
      m_prevExpression(0),
      m_nextExpression(0)
{
}

QQmlJavaScriptExpression::~QQmlJavaScriptExpression()
{
    if (m_prevExpression) {
        *m_prevExpression = m_nextExpression;
        if (m_nextExpression)
            m_nextExpression->m_prevExpression = m_prevExpression;
    }

    clearGuards();
    if (m_scopeObject.isT2()) // notify DeleteWatcher of our deletion.
        m_scopeObject.asT2()->_s = 0;
}

void QQmlJavaScriptExpression::setNotifyOnValueChanged(bool v)
{
    activeGuards.setFlagValue(v);
    if (!v) clearGuards();
}

void QQmlJavaScriptExpression::resetNotifyOnValueChanged()
{
    clearGuards();
}

void QQmlJavaScriptExpression::setContext(QQmlContextData *context)
{
    if (m_prevExpression) {
        *m_prevExpression = m_nextExpression;
        if (m_nextExpression)
            m_nextExpression->m_prevExpression = m_prevExpression;
        m_prevExpression = 0;
        m_nextExpression = 0;
    }

    m_context = context;

    if (context) {
        m_nextExpression = context->expressions;
        if (m_nextExpression)
            m_nextExpression->m_prevExpression = &m_nextExpression;
        m_prevExpression = &context->expressions;
        context->expressions = this;
    }
}

void QQmlJavaScriptExpression::refresh()
{
}

QV4::ReturnedValue QQmlJavaScriptExpression::evaluate(bool *isUndefined)
{
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(m_context->engine);
    QV4::Scope scope(v4);
    QV4::ScopedCallData callData(scope);

    return evaluate(callData, isUndefined);
}

QV4::ReturnedValue QQmlJavaScriptExpression::evaluate(QV4::CallData *callData, bool *isUndefined)
{
    Q_ASSERT(m_context && m_context->engine);

    QV4::Value *f = m_function.valueRef();
    if (!f || f->isUndefined()) {
        if (isUndefined)
            *isUndefined = true;
        return QV4::Encode::undefined();
    }

    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(m_context->engine);

    // All code that follows must check with watcher before it accesses data members
    // incase we have been deleted.
    DeleteWatcher watcher(this);

    Q_ASSERT(notifyOnValueChanged() || activeGuards.isEmpty());
    QQmlPropertyCapture capture(m_context->engine, this, &watcher);

    QQmlPropertyCapture *lastPropertyCapture = ep->propertyCapture;
    ep->propertyCapture = notifyOnValueChanged() ? &capture : 0;


    if (notifyOnValueChanged())
        capture.guards.copyAndClearPrepend(activeGuards);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::Scope scope(v4);
    QV4::ScopedValue result(scope, QV4::Primitive::undefinedValue());
    callData->thisObject = v4->globalObject;
    if (scopeObject()) {
        QV4::ScopedValue value(scope, QV4::QObjectWrapper::wrap(v4, scopeObject()));
        if (value->isObject())
            callData->thisObject = value;
    }

    result = f->as<QV4::FunctionObject>()->call(callData);
    if (scope.hasException()) {
        if (watcher.wasDeleted())
            scope.engine->catchException(); // ignore exception
        else
            delayedError()->catchJavaScriptException(scope.engine);
        if (isUndefined)
            *isUndefined = true;
    } else {
        if (isUndefined)
            *isUndefined = result->isUndefined();

        if (!watcher.wasDeleted() && hasDelayedError())
            delayedError()->clearError();
    }

    if (capture.errorString) {
        for (int ii = 0; ii < capture.errorString->count(); ++ii)
            qWarning("%s", qPrintable(capture.errorString->at(ii)));
        delete capture.errorString;
        capture.errorString = 0;
    }

    while (QQmlJavaScriptExpressionGuard *g = capture.guards.takeFirst())
        g->Delete();

    ep->propertyCapture = lastPropertyCapture;

    return result->asReturnedValue();
}

void QQmlPropertyCapture::captureProperty(QQmlNotifier *n)
{
    if (watcher->wasDeleted())
        return;

    Q_ASSERT(expression);
    // Try and find a matching guard
    while (!guards.isEmpty() && !guards.first()->isConnected(n))
        guards.takeFirst()->Delete();

    QQmlJavaScriptExpressionGuard *g = 0;
    if (!guards.isEmpty()) {
        g = guards.takeFirst();
        g->cancelNotify();
        Q_ASSERT(g->isConnected(n));
    } else {
        g = QQmlJavaScriptExpressionGuard::New(expression, engine);
        g->connect(n);
    }

    expression->activeGuards.prepend(g);
}

/*! \internal

    \a n is in the signal index range (see QObjectPrivate::signalIndex()).
*/
void QQmlPropertyCapture::captureProperty(QObject *o, int c, int n)
{
    if (watcher->wasDeleted())
        return;

    Q_ASSERT(expression);
    if (n == -1) {
        if (!errorString) {
            errorString = new QStringList;
            QString preamble = QLatin1String("QQmlExpression: Expression ") +
                    expression->expressionIdentifier() +
                    QLatin1String(" depends on non-NOTIFYable properties:");
            errorString->append(preamble);
        }

        const QMetaObject *metaObj = o->metaObject();
        QMetaProperty metaProp = metaObj->property(c);

        QString error = QLatin1String("    ") +
                QString::fromUtf8(metaObj->className()) +
                QLatin1String("::") +
                QString::fromUtf8(metaProp.name());
        errorString->append(error);
    } else {

        // Try and find a matching guard
        while (!guards.isEmpty() && !guards.first()->isConnected(o, n))
            guards.takeFirst()->Delete();

        QQmlJavaScriptExpressionGuard *g = 0;
        if (!guards.isEmpty()) {
            g = guards.takeFirst();
            g->cancelNotify();
            Q_ASSERT(g->isConnected(o, n));
        } else {
            g = QQmlJavaScriptExpressionGuard::New(expression, engine);
            g->connect(o, n, engine);
        }

        expression->activeGuards.prepend(g);
    }
}

void QQmlPropertyCapture::registerQmlDependencies(QV4::ExecutionEngine *engine, const QV4::CompiledData::Function *compiledFunction)
{
    // Let the caller check and avoid the function call :)
    Q_ASSERT(compiledFunction->hasQmlDependencies());

    QQmlEnginePrivate *ep = engine->qmlEngine() ? QQmlEnginePrivate::get(engine->qmlEngine()) : 0;
    if (!ep)
        return;
    QQmlPropertyCapture *capture = ep->propertyCapture;
    if (!capture)
        return;

    QV4::Scope scope(engine);
    QV4::Scoped<QV4::QmlContext> context(scope, engine->qmlContext());
    QQmlContextData *qmlContext = context->qmlContext();

    const quint32 *idObjectDependency = compiledFunction->qmlIdObjectDependencyTable();
    const int idObjectDependencyCount = compiledFunction->nDependingIdObjects;
    for (int i = 0; i < idObjectDependencyCount; ++i, ++idObjectDependency) {
        Q_ASSERT(int(*idObjectDependency) < qmlContext->idValueCount);
        capture->captureProperty(&qmlContext->idValues[*idObjectDependency].bindings);
    }

    Q_ASSERT(qmlContext->contextObject);
    const quint32 *contextPropertyDependency = compiledFunction->qmlContextPropertiesDependencyTable();
    const int contextPropertyDependencyCount = compiledFunction->nDependingContextProperties;
    for (int i = 0; i < contextPropertyDependencyCount; ++i) {
        const int propertyIndex = *contextPropertyDependency++;
        const int notifyIndex = *contextPropertyDependency++;
        capture->captureProperty(qmlContext->contextObject, propertyIndex, notifyIndex);
    }

    QObject *scopeObject = context->qmlScope();
    const quint32 *scopePropertyDependency = compiledFunction->qmlScopePropertiesDependencyTable();
    const int scopePropertyDependencyCount = compiledFunction->nDependingScopeProperties;
    for (int i = 0; i < scopePropertyDependencyCount; ++i) {
        const int propertyIndex = *scopePropertyDependency++;
        const int notifyIndex = *scopePropertyDependency++;
        capture->captureProperty(scopeObject, propertyIndex, notifyIndex);
    }

}


void QQmlJavaScriptExpression::clearError()
{
    if (m_error)
        delete m_error;
    m_error = 0;
}

QQmlError QQmlJavaScriptExpression::error(QQmlEngine *engine) const
{
    Q_UNUSED(engine);

    if (m_error)
        return m_error->error();
    else
        return QQmlError();
}

QQmlDelayedError *QQmlJavaScriptExpression::delayedError()
{
    if (!m_error)
        m_error = new QQmlDelayedError;
    return m_error;
}

QV4::ReturnedValue
QQmlJavaScriptExpression::evalFunction(QQmlContextData *ctxt, QObject *scopeObject,
                                       const QString &code, const QString &filename, quint16 line)
{
    QQmlEngine *engine = ctxt->engine;
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::Scope scope(v4);

    QV4::Scoped<QV4::QmlContext> qmlContext(scope, v4->rootContext()->newQmlContext(ctxt, scopeObject));
    QV4::Script script(v4, qmlContext, code, filename, line);
    QV4::ScopedValue result(scope);
    script.parse();
    if (!v4->hasException)
        result = script.run();
    if (v4->hasException) {
        QQmlError error = v4->catchExceptionAsQmlError();
        if (error.description().isEmpty())
            error.setDescription(QLatin1String("Exception occurred during function evaluation"));
        if (error.line() == -1)
            error.setLine(line);
        if (error.url().isEmpty())
            error.setUrl(QUrl::fromLocalFile(filename));
        error.setObject(scopeObject);
        ep->warning(error);
        return QV4::Encode::undefined();
    }
    return result->asReturnedValue();
}

void QQmlJavaScriptExpression::createQmlBinding(QQmlContextData *ctxt, QObject *qmlScope,
                                          const QString &code, const QString &filename, quint16 line)
{
    QQmlEngine *engine = ctxt->engine;
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);

    QV4::ExecutionEngine *v4 = QV8Engine::getV4(ep->v8engine());
    QV4::Scope scope(v4);

    QV4::Scoped<QV4::QmlContext> qmlContext(scope, v4->rootContext()->newQmlContext(ctxt, qmlScope));
    QV4::Script script(v4, qmlContext, code, filename, line);
    QV4::ScopedValue result(scope);
    script.parse();
    if (!v4->hasException)
        result = script.qmlBinding();
    if (v4->hasException) {
        QQmlError error = v4->catchExceptionAsQmlError();
        if (error.description().isEmpty())
            error.setDescription(QLatin1String("Exception occurred during function evaluation"));
        if (error.line() == -1)
            error.setLine(line);
        if (error.url().isEmpty())
            error.setUrl(QUrl::fromLocalFile(filename));
        error.setObject(qmlScope);
        ep->warning(error);
        result = QV4::Encode::undefined();
    }
    m_function.set(v4, result);
}


void QQmlJavaScriptExpression::clearGuards()
{
    while (QQmlJavaScriptExpressionGuard *g = activeGuards.takeFirst())
        g->Delete();
}

void QQmlJavaScriptExpressionGuard_callback(QQmlNotifierEndpoint *e, void **)
{
    QQmlJavaScriptExpression *expression =
        static_cast<QQmlJavaScriptExpressionGuard *>(e)->expression;

    expression->expressionChanged();
}

QT_END_NAMESPACE
