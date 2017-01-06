/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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


#include "translationsettingsdialog.h"
#include "messagemodel.h"
#include "phrase.h"

#include <QtCore/QLocale>

QT_BEGIN_NAMESPACE

TranslationSettingsDialog::TranslationSettingsDialog(QWidget *parent)
  : QDialog(parent)
{
    m_ui.setupUi(this);

    for (int i = QLocale::C + 1; i < QLocale::LastLanguage; ++i) {
        QString lang = QLocale::languageToString(QLocale::Language(i));
        m_ui.srcCbLanguageList->addItem(lang, QVariant(i));
    }
    m_ui.srcCbLanguageList->model()->sort(0, Qt::AscendingOrder);
    m_ui.srcCbLanguageList->insertItem(0, QLatin1String("POSIX"), QVariant(QLocale::C));

    m_ui.tgtCbLanguageList->setModel(m_ui.srcCbLanguageList->model());
}

void TranslationSettingsDialog::setDataModel(DataModel *dataModel)
{
    m_dataModel = dataModel;
    m_phraseBook = 0;
    QString fn = QFileInfo(dataModel->srcFileName()).baseName();
    setWindowTitle(tr("Settings for '%1' - Qt Linguist").arg(fn));
}

void TranslationSettingsDialog::setPhraseBook(PhraseBook *phraseBook)
{
    m_phraseBook = phraseBook;
    m_dataModel = 0;
    QString fn = QFileInfo(phraseBook->fileName()).baseName();
    setWindowTitle(tr("Settings for '%1' - Qt Linguist").arg(fn));
}

static void fillCountryCombo(const QVariant &lng, QComboBox *combo)
{
    combo->clear();
    QLocale::Language lang = QLocale::Language(lng.toInt());
    if (lang != QLocale::C) {
        foreach (QLocale::Country cntr, QLocale::countriesForLanguage(lang)) {
            QString country = QLocale::countryToString(cntr);
            combo->addItem(country, QVariant(cntr));
        }
        combo->model()->sort(0, Qt::AscendingOrder);
    }
    combo->insertItem(0, TranslationSettingsDialog::tr("Any Country"), QVariant(QLocale::AnyCountry));
    combo->setCurrentIndex(0);
}

void TranslationSettingsDialog::on_srcCbLanguageList_currentIndexChanged(int idx)
{
    fillCountryCombo(m_ui.srcCbLanguageList->itemData(idx), m_ui.srcCbCountryList);
}

void TranslationSettingsDialog::on_tgtCbLanguageList_currentIndexChanged(int idx)
{
    fillCountryCombo(m_ui.tgtCbLanguageList->itemData(idx), m_ui.tgtCbCountryList);
}

void TranslationSettingsDialog::on_buttonBox_accepted()
{
    int itemindex = m_ui.tgtCbLanguageList->currentIndex();
    QVariant var = m_ui.tgtCbLanguageList->itemData(itemindex);
    QLocale::Language lang = QLocale::Language(var.toInt());

    itemindex = m_ui.tgtCbCountryList->currentIndex();
    var = m_ui.tgtCbCountryList->itemData(itemindex);
    QLocale::Country country = QLocale::Country(var.toInt());

    itemindex = m_ui.srcCbLanguageList->currentIndex();
    var = m_ui.srcCbLanguageList->itemData(itemindex);
    QLocale::Language lang2 = QLocale::Language(var.toInt());

    itemindex = m_ui.srcCbCountryList->currentIndex();
    var = m_ui.srcCbCountryList->itemData(itemindex);
    QLocale::Country country2 = QLocale::Country(var.toInt());

    if (m_phraseBook) {
        m_phraseBook->setLanguageAndCountry(lang, country);
        m_phraseBook->setSourceLanguageAndCountry(lang2, country2);
    } else {
        m_dataModel->setLanguageAndCountry(lang, country);
        m_dataModel->setSourceLanguageAndCountry(lang2, country2);
    }

    accept();
}

void TranslationSettingsDialog::showEvent(QShowEvent *)
{
    QLocale::Language lang, lang2;
    QLocale::Country country, country2;

    if (m_phraseBook) {
        lang = m_phraseBook->language();
        country = m_phraseBook->country();
        lang2 = m_phraseBook->sourceLanguage();
        country2 = m_phraseBook->sourceCountry();
    } else {
        lang = m_dataModel->language();
        country = m_dataModel->country();
        lang2 = m_dataModel->sourceLanguage();
        country2 = m_dataModel->sourceCountry();
    }

    int itemindex = m_ui.tgtCbLanguageList->findData(QVariant(int(lang)));
    m_ui.tgtCbLanguageList->setCurrentIndex(itemindex == -1 ? 0 : itemindex);

    itemindex = m_ui.tgtCbCountryList->findData(QVariant(int(country)));
    m_ui.tgtCbCountryList->setCurrentIndex(itemindex == -1 ? 0 : itemindex);

    itemindex = m_ui.srcCbLanguageList->findData(QVariant(int(lang2)));
    m_ui.srcCbLanguageList->setCurrentIndex(itemindex == -1 ? 0 : itemindex);

    itemindex = m_ui.srcCbCountryList->findData(QVariant(int(country2)));
    m_ui.srcCbCountryList->setCurrentIndex(itemindex == -1 ? 0 : itemindex);
}

QT_END_NAMESPACE
