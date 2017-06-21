/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "propertyfield.h"
#include <QDebug>

PropertyField::PropertyField(QObject* subject, const QMetaProperty& prop, QWidget *parent)
  : QLineEdit(parent), m_subject(subject), m_lastChangeTime(QTime::currentTime()), m_prop(prop)
  , m_defaultBrush(palette().brush(QPalette::Active, QPalette::Text))
{
    setReadOnly(true);
    if (prop.hasNotifySignal()) {
        QMetaMethod signal = prop.notifySignal();
        QMetaMethod updateSlot = metaObject()->method(metaObject()->indexOfSlot("propertyChanged()"));
        connect(m_subject, signal, this, updateSlot);
    }
    propertyChanged();
}

QString PropertyField::valueToString(QVariant val)
{
    QString text;
    switch (val.type()) {
    case QVariant::Double:
        text = QString("%1").arg(val.toReal(), 0, 'f', 4);
        break;
    case QVariant::Size:
        text = QString("%1 x %2").arg(val.toSize().width()).arg(val.toSize().height());
        break;
    case QVariant::SizeF:
        text = QString("%1 x %2").arg(val.toSizeF().width()).arg(val.toSizeF().height());
        break;
    case QVariant::Rect: {
        QRect rect = val.toRect();
        text = QString("%1 x %2 %3%4 %5%6").arg(rect.width())
                .arg(rect.height()).arg(rect.x() < 0 ? "" : "+").arg(rect.x())
                .arg(rect.y() < 0 ? "" : "+").arg(rect.y());
        } break;
    default:
        text = val.toString();
    }
    return text;
}

void PropertyField::propertyChanged()
{
    if (m_prop.isReadable()) {
        QVariant val = m_prop.read(m_subject);
        QString text = valueToString(val);
        QPalette modPalette = palette();

        // If we are seeing a value for the first time,
        // pretend it was that way for a while already.
        if (m_lastText.isEmpty()) {
            m_lastText = text;
            m_lastTextShowing = text;
            m_lastChangeTime = QTime::currentTime().addSecs(-5);
        }

        qDebug() << "  " << QString::fromUtf8(m_prop.name()) << ":" << val;
        // If the value has recently changed, show the change
        if (text != m_lastText || m_lastChangeTime.elapsed() < 1000) {
            setText(m_lastTextShowing + " -> " + text);
            modPalette.setBrush(QPalette::Text, Qt::red);
            m_lastChangeTime.start();
            m_lastText = text;
        }
        // If the value hasn't changed recently, just show the current value
        else {
            setText(text);
            m_lastText = text;
            m_lastTextShowing = text;
            modPalette.setBrush(QPalette::Text, m_defaultBrush);
        }
        setPalette(modPalette);
    }
}
