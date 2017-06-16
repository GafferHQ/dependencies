/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include <QFile>
#include "theme.h"

#include "dummydatagen.h"

DummyDataGenerator::DummyDataGenerator() : m_isMale(false)
{
    QFile countryCodeFile(":/contact/areacodes.txt");
    countryCodeFile.open(QIODevice::ReadOnly);
    while (!countryCodeFile.atEnd()) {
        m_countryCodes << QString(countryCodeFile.readLine()).remove("\n");
    }

    QFile firstNameFFile(":/contact/firstnamesF.txt");
    firstNameFFile.open(QIODevice::ReadOnly);
    while (!firstNameFFile.atEnd()) {
        m_firstNamesF << QString(firstNameFFile.readLine()).remove("\n");
    }

    QFile firstNameMFile(":/contact/firstnamesM.txt");
    firstNameMFile.open(QIODevice::ReadOnly);
    while (!firstNameMFile.atEnd()) {
        m_firstNamesM << QString(firstNameMFile.readLine()).remove("\n");
    }

    QFile lastNameFile(":/contact/lastnames.txt");
    lastNameFile.open(QIODevice::ReadOnly);
    while (!lastNameFile.atEnd()) {
        m_lastNames << QString(lastNameFile.readLine()).remove("\n");
    }
    Reset();
}

DummyDataGenerator::~DummyDataGenerator()
{
}

void DummyDataGenerator::Reset()
{
    qsrand(100);
}

QString DummyDataGenerator::randomPhoneNumber(QString indexNumber)
{
    int index = qrand()%m_countryCodes.count();
    QString countryCode = m_countryCodes.at(index);
    QString areaCode = QString::number(index) + QString("0").repeated(2-QString::number(index).length());
    QString beginNumber = QString::number(555-index*2);
    QString endNumber = QString("0").repeated(4-indexNumber.length()) + indexNumber;

    return countryCode +" " + areaCode +" " + beginNumber +" " + endNumber;
}

QString DummyDataGenerator::randomFirstName()
{
    m_isMale = !m_isMale;
    if (m_isMale)
        return m_firstNamesM.at(qrand()%m_firstNamesM.count());
    return m_firstNamesF.at(qrand()%m_firstNamesF.count());
}

QString DummyDataGenerator::randomLastName()
{
    return m_lastNames.at(qrand()%m_lastNames.count());
}

QString DummyDataGenerator::randomName()
{
    return QString(randomFirstName()+QString(", ")+randomLastName());
}

QString DummyDataGenerator::randomIconItem()
{
    QString avatar = Theme::p()->pixmapPath() + "contact_default_icon.svg";
    if (qrand()%4) {
      int randVal = 1+qrand()%25;

      if (m_isMale && randVal > 15) {
          randVal -= 15;
      }
      if (!m_isMale && randVal <= 10) {
          randVal += 10;
      }

      avatar = QString(":/avatars/avatar_%1.png").arg(randVal, 3, 10, QChar('0'));
    }
    return avatar;
}

QString DummyDataGenerator::randomStatusItem()
{
    switch (qrand()%3) {
        case 0: return Theme::p()->pixmapPath() + "contact_status_online.svg";
        case 1: return Theme::p()->pixmapPath() + "contact_status_offline.svg";
        case 2: return Theme::p()->pixmapPath() + "contact_status_idle.svg";
    }
    return 0;
}
