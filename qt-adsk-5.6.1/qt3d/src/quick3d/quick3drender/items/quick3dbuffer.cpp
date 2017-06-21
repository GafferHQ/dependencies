/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "quick3dbuffer_p.h"
#include <QQmlEngine>
#include <QJSValue>
#include <QtQml/private/qqmlengine_p.h>
#include <QtQml/private/qjsvalue_p.h>
#include <QtQml/private/qv4typedarray_p.h>
#include <QtQml/private/qv4arraybuffer_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace Quick {

Quick3DBuffer::Quick3DBuffer(QObject *parent)
    : QObject(parent)
    , m_engine(Q_NULLPTR)
    , m_v4engine(Q_NULLPTR)
{
    QObject::connect(parentBuffer(), &Qt3DRender::QAbstractBuffer::dataChanged, this, &Quick3DBuffer::bufferDataChanged);
}

QByteArray Quick3DBuffer::convertToRawData(const QJSValue &jsValue)
{
    initEngines();
    Q_ASSERT(m_v4engine);
    QV4::Scope scope(m_v4engine);
    QV4::Scoped<QV4::TypedArray> typedArray(scope,
                                            QJSValuePrivate::convertedToValue(m_v4engine, jsValue));
    if (!typedArray)
        return QByteArray();

    char *dataPtr = reinterpret_cast<char *>(typedArray->arrayData()->data());
    dataPtr += typedArray->d()->byteOffset;
    uint byteLength = typedArray->byteLength();
    return QByteArray(dataPtr, byteLength);
}

QVariant Quick3DBuffer::bufferData() const
{
    return QVariant::fromValue(parentBuffer()->data());
}

void Quick3DBuffer::setBufferData(const QVariant &bufferData)
{
    QJSValue jsValue = bufferData.value<QJSValue>();
    parentBuffer()->setData(convertToRawData(jsValue));
}

void Quick3DBuffer::initEngines()
{
    if (m_engine == Q_NULLPTR) {
        m_engine = qmlEngine(parent());
        m_v4engine = QQmlEnginePrivate::getV4Engine(m_engine);
    }
}

} // Quick

} // Render

} // Qt3DRender

QT_END_NAMESPACE
