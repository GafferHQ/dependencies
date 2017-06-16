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

#ifndef MOCKMETADATAREADERCONTROL_H
#define MOCKMETADATAREADERCONTROL_H

#include "qmetadatareadercontrol.h"

class MockMetaDataReaderControl : public QMetaDataReaderControl
{
    Q_OBJECT
public:
    MockMetaDataReaderControl(QObject *parent = 0)
        : QMetaDataReaderControl(parent)
        , m_available(false)
    {
    }

    bool isMetaDataAvailable() const
    {
        return m_available;
    }
    void setMetaDataAvailable(bool available)
    {
        if (m_available != available)
            emit metaDataAvailableChanged(m_available = available);
    }
    QStringList availableMetaData() const
    {
        return m_data.keys();
    }

    QVariant metaData(const QString &key) const
    {
        return m_data.value(key);
    }

    void setMetaData(const QString &key, const QVariant &value)
    {
        if (m_data[key] != value) {
            if (value.isNull())
                m_data.remove(key);
            else
                m_data[key] = value;

            emit metaDataChanged(key, value);
            emit metaDataChanged();
        }
    }

    using QMetaDataReaderControl::metaDataChanged;

    void populateMetaData()
    {
        m_available = true;
    }

    bool m_available;
    QMap<QString, QVariant> m_data;
};

#endif // MOCKMETADATAREADERCONTROL_H
