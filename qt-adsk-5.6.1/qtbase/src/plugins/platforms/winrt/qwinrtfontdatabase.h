/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QWINRTFONTDATABASE_H
#define QWINRTFONTDATABASE_H

#include <QtPlatformSupport/private/qbasicfontdatabase_p.h>
#include <QtCore/QLoggingCategory>

struct IDWriteFontFile;
struct IDWriteFontFamily;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQpaFonts)

struct FontDescription
{
    quint32 index;
    QByteArray uuid;
};

class QWinRTFontDatabase : public QBasicFontDatabase
{
public:
    QString fontDir() const;
    ~QWinRTFontDatabase();
    QFont defaultFont() const Q_DECL_OVERRIDE;
    bool fontsAlwaysScalable() const Q_DECL_OVERRIDE;
    void populateFontDatabase() Q_DECL_OVERRIDE;
    void populateFamily(const QString &familyName) Q_DECL_OVERRIDE;
    QFontEngine *fontEngine(const QFontDef &fontDef, void *handle) Q_DECL_OVERRIDE;
    QStringList fallbacksForFamily(const QString &family, QFont::Style style,
                                   QFont::StyleHint styleHint, QChar::Script script) const Q_DECL_OVERRIDE;
    void releaseHandle(void *handle) Q_DECL_OVERRIDE;
private:
    QHash<IDWriteFontFile *, FontDescription> m_fonts;
    QHash<QString, IDWriteFontFamily *> m_fontFamilies;
};

QT_END_NAMESPACE

#endif // QWINRTFONTDATABASE_H
