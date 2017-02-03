/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qdeclarativeinclude_p.h"

#include <QtScript/qscriptengine.h>
#include <QtNetwork/qnetworkrequest.h>
#include <QtNetwork/qnetworkreply.h>
#include <QtCore/qfile.h>

#include <private/qdeclarativeengine_p.h>
#include <private/qdeclarativeglobalscriptclass_p.h>

QT_BEGIN_NAMESPACE

QDeclarativeInclude::QDeclarativeInclude(const QUrl &url, 
                                                       QDeclarativeEngine *engine, 
                                                       QScriptContext *ctxt)
: QObject(engine), m_engine(engine), m_network(0), m_reply(0), m_url(url), m_redirectCount(0)
{
    QDeclarativeEnginePrivate *ep = QDeclarativeEnginePrivate::get(engine);
    m_context = ep->contextClass->contextFromValue(QScriptDeclarativeClass::scopeChainValue(ctxt, -3));

    m_scope[0] = QScriptDeclarativeClass::scopeChainValue(ctxt, -4);
    m_scope[1] = QScriptDeclarativeClass::scopeChainValue(ctxt, -5);

    m_scriptEngine = QDeclarativeEnginePrivate::getScriptEngine(engine);
    m_network = QDeclarativeScriptEngine::get(m_scriptEngine)->networkAccessManager();

    m_result = resultValue(m_scriptEngine);

    QNetworkRequest request;
    request.setUrl(url);

    m_reply = m_network->get(request);
    QObject::connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
}

QDeclarativeInclude::~QDeclarativeInclude()
{
    delete m_reply;
}

QScriptValue QDeclarativeInclude::resultValue(QScriptEngine *engine, Status status)
{
    QScriptValue result = engine->newObject();
    result.setProperty(QLatin1String("OK"), QScriptValue(engine, Ok));
    result.setProperty(QLatin1String("LOADING"), QScriptValue(engine, Loading));
    result.setProperty(QLatin1String("NETWORK_ERROR"), QScriptValue(engine, NetworkError));
    result.setProperty(QLatin1String("EXCEPTION"), QScriptValue(engine, Exception));

    result.setProperty(QLatin1String("status"), QScriptValue(engine, status));
    return result;
}

QScriptValue QDeclarativeInclude::result() const
{
    return m_result;
}

void QDeclarativeInclude::setCallback(const QScriptValue &c)
{
    m_callback = c;
}

QScriptValue QDeclarativeInclude::callback() const
{
    return m_callback;
}

#define INCLUDE_MAXIMUM_REDIRECT_RECURSION 15
void QDeclarativeInclude::finished()
{
    m_redirectCount++;

    if (m_redirectCount < INCLUDE_MAXIMUM_REDIRECT_RECURSION) {
        QVariant redirect = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid()) {
            m_url = m_url.resolved(redirect.toUrl());
            delete m_reply; 
            
            QNetworkRequest request;
            request.setUrl(m_url);

            m_reply = m_network->get(request);
            QObject::connect(m_reply, SIGNAL(finished()), this, SLOT(finished()));
            return;
        }
    }

    if (m_reply->error() == QNetworkReply::NoError) {
        QDeclarativeEnginePrivate *ep = QDeclarativeEnginePrivate::get(m_engine);

        QByteArray data = m_reply->readAll();

        QString code = QString::fromUtf8(data);

        QString urlString = m_url.toString();
        QScriptContext *scriptContext = QScriptDeclarativeClass::pushCleanContext(m_scriptEngine);
        scriptContext->pushScope(ep->contextClass->newUrlContext(m_context, 0, urlString));
        scriptContext->pushScope(m_scope[0]);

        scriptContext->pushScope(m_scope[1]);
        scriptContext->setActivationObject(m_scope[1]);
        QDeclarativeScriptParser::extractPragmas(code);

        m_scriptEngine->evaluate(code, urlString, 1);

        m_scriptEngine->popContext();

        if (m_scriptEngine->hasUncaughtException()) {
            m_result.setProperty(QLatin1String("status"), QScriptValue(m_scriptEngine, Exception));
            m_result.setProperty(QLatin1String("exception"), m_scriptEngine->uncaughtException());
            m_scriptEngine->clearExceptions();
        } else {
            m_result.setProperty(QLatin1String("status"), QScriptValue(m_scriptEngine, Ok));
        }
    } else {
        m_result.setProperty(QLatin1String("status"), QScriptValue(m_scriptEngine, NetworkError));
    }

    callback(m_scriptEngine, m_callback, m_result);

    disconnect();
    deleteLater();
}

void QDeclarativeInclude::callback(QScriptEngine *engine, QScriptValue &callback, QScriptValue &status)
{
    if (callback.isValid()) {
        QScriptValue args = engine->newArray(1);
        args.setProperty(0, status);
        callback.call(QScriptValue(), args);
    }
}

/*
    Documented in qdeclarativeengine.cpp
*/
QScriptValue QDeclarativeInclude::include(QScriptContext *ctxt, QScriptEngine *engine)
{
    if (ctxt->argumentCount() == 0)
        return engine->undefinedValue();

    QDeclarativeEnginePrivate *ep = QDeclarativeEnginePrivate::get(engine);

    QUrl contextUrl = ep->contextClass->urlFromValue(QScriptDeclarativeClass::scopeChainValue(ctxt, -3));
    if (contextUrl.isEmpty()) 
        return ctxt->throwError(QLatin1String("Qt.include(): Can only be called from JavaScript files"));

    QString urlString = ctxt->argument(0).toString();
    QUrl url(urlString);
    if (url.isRelative()) {
        url = QUrl(contextUrl).resolved(url);
        urlString = url.toString();
    }

    QString localFile = QDeclarativeEnginePrivate::urlToLocalFileOrQrc(url);

    QScriptValue func = ctxt->argument(1);
    if (!func.isFunction())
        func = QScriptValue();

    QScriptValue result;
    if (localFile.isEmpty()) {
        QDeclarativeInclude *i = 
            new QDeclarativeInclude(url, QDeclarativeEnginePrivate::getEngine(engine), ctxt);

        if (func.isValid())
            i->setCallback(func);

        result = i->result();
    } else {

        QFile f(localFile);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            QString code = QString::fromUtf8(data);

            QDeclarativeContextData *context = 
                ep->contextClass->contextFromValue(QScriptDeclarativeClass::scopeChainValue(ctxt, -3));

            QScriptContext *scriptContext = QScriptDeclarativeClass::pushCleanContext(engine);
            scriptContext->pushScope(ep->contextClass->newUrlContext(context, 0, urlString));
            scriptContext->pushScope(ep->globalClass->staticGlobalObject());
            QScriptValue scope = QScriptDeclarativeClass::scopeChainValue(ctxt, -5);
            scriptContext->pushScope(scope);
            scriptContext->setActivationObject(scope);
            QDeclarativeScriptParser::extractPragmas(code);

            engine->evaluate(code, urlString, 1);

            engine->popContext();

            if (engine->hasUncaughtException()) {
                result = resultValue(engine, Exception);
                result.setProperty(QLatin1String("exception"), engine->uncaughtException());
                engine->clearExceptions();
            } else {
                result = resultValue(engine, Ok);
            }
            callback(engine, func, result);
        } else {
            result = resultValue(engine, NetworkError);
            callback(engine, func, result);
        }
    }

    return result;
}

QScriptValue QDeclarativeInclude::worker_include(QScriptContext *ctxt, QScriptEngine *engine)
{
    if (ctxt->argumentCount() == 0)
        return engine->undefinedValue();

    QString urlString = ctxt->argument(0).toString();
    QUrl url(ctxt->argument(0).toString());
    if (url.isRelative()) {
        QString contextUrl = QScriptDeclarativeClass::scopeChainValue(ctxt, -3).data().toString();
        Q_ASSERT(!contextUrl.isEmpty());

        url = QUrl(contextUrl).resolved(url);
        urlString = url.toString();
    }

    QString localFile = QDeclarativeEnginePrivate::urlToLocalFileOrQrc(url);

    QScriptValue func = ctxt->argument(1);
    if (!func.isFunction())
        func = QScriptValue();

    QScriptValue result;
    if (!localFile.isEmpty()) {

        QFile f(localFile);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray data = f.readAll();
            QString code = QString::fromUtf8(data);

            QScriptContext *scriptContext = QScriptDeclarativeClass::pushCleanContext(engine);
            QScriptValue urlContext = engine->newObject();
            urlContext.setData(QScriptValue(engine, urlString));
            scriptContext->pushScope(urlContext);

            QScriptValue scope = QScriptDeclarativeClass::scopeChainValue(ctxt, -4);
            scriptContext->pushScope(scope);
            scriptContext->setActivationObject(scope);
            QDeclarativeScriptParser::extractPragmas(code);

            engine->evaluate(code, urlString, 1);

            engine->popContext();

            if (engine->hasUncaughtException()) {
                result = resultValue(engine, Exception);
                result.setProperty(QLatin1String("exception"), engine->uncaughtException());
                engine->clearExceptions();
            } else {
                result = resultValue(engine, Ok);
            }
            callback(engine, func, result);
        } else {
            result = resultValue(engine, NetworkError);
            callback(engine, func, result);
        }
    }

    return result;
}

QT_END_NAMESPACE
