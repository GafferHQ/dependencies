/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qfont.h"
#include "qfont_p.h"
#include <private/qt_s60_p.h>
#include <private/qpixmap_raster_symbian_p.h>
#include "qmutex.h"

QT_BEGIN_NAMESPACE

#ifdef QT_NO_FREETYPE
Q_GLOBAL_STATIC(QMutex, lastResortFamilyMutex);
extern QStringList qt_symbian_fontFamiliesOnFontServer(); // qfontdatabase_s60.cpp
Q_GLOBAL_STATIC_WITH_INITIALIZER(QStringList, fontFamiliesOnFontServer, {
    // We are only interested in the initial font families. No Application fonts.
    // Therefore, we are allowed to cache the list.
    x->append(qt_symbian_fontFamiliesOnFontServer());
});

extern bool qt_symbian_isLinkedFont(const TDesC &typefaceName); // qfontdatabase_s60.cpp

static QString classicalSymbianSystemFont()
{
    static QString font;
    if (font.isEmpty()) {
        if (User::Language() == ELangPrcChinese) {
            // Use font with simplified Chinese characters as default
            QString scfont("MHeiM-C-GB18030-S60");
            if (fontFamiliesOnFontServer()->contains(scfont)) {
                font = scfont;
                return font;
            }
        }
        static const char* const classicSymbianSystemFonts[] = { "Nokia Sans S60", "Series 60 Sans" };
        for (int i = 0; i < sizeof classicSymbianSystemFonts / sizeof classicSymbianSystemFonts[0]; ++i) {
            const QString classicFont = QLatin1String(classicSymbianSystemFonts[i]);
            if (fontFamiliesOnFontServer()->contains(classicFont)) {
                font = classicFont;
                break;
            }
        }
    }
    return font;
}
#endif // QT_NO_FREETYPE

QString QFont::lastResortFont() const
{
    // Symbian's font Api does not distinguish between font and family.
    // Therefore we try to get a "Family" first, then fall back to "Sans".
    static QString font = lastResortFamily();
    if (font.isEmpty())
        font = QLatin1String("Sans");
    return font;
}

QString QFont::lastResortFamily() const
{
#ifdef QT_NO_FREETYPE
    QMutexLocker locker(lastResortFamilyMutex());
    static QString family;
    if (family.isEmpty()) {
        
        QSymbianFbsHeapLock lock(QSymbianFbsHeapLock::Unlock);
        
        CFont *font;
        const TInt err = S60->screenDevice()->GetNearestFontInTwips(font, TFontSpec());
        Q_ASSERT(err == KErrNone);
        const TFontSpec spec = font->FontSpecInTwips();
        family = QString((const QChar *)spec.iTypeface.iName.Ptr(), spec.iTypeface.iName.Length());
        S60->screenDevice()->ReleaseFont(font);
        
        lock.relock();

        // We must not return a Symbian Linked Font. See QTBUG-20007
        if (qt_symbian_isLinkedFont(spec.iTypeface.iName) && !classicalSymbianSystemFont().isEmpty())
            family = classicalSymbianSystemFont();
    }
    return family;
#else // QT_NO_FREETYPE
    // For the FreeType case we just hard code the face name, since otherwise on
    // East Asian systems we may get a name for a stroke based (non-ttf) font.

    // TODO: Get the type face name in a proper way

    const bool isJapaneseOrChineseSystem =
        User::Language() == ELangJapanese || User::Language() == ELangPrcChinese;

    static QString family;
    if (family.isEmpty()) {
        QStringList families = qt_symbian_fontFamiliesOnFontServer();
        const char* const preferredFamilies[] = {"Nokia Sans S60", "Series 60 Sans"};
        for (int i = 0; i < sizeof preferredFamilies / sizeof preferredFamilies[0]; ++i) {
            const QString preferredFamily = QLatin1String(preferredFamilies[i]);
            if (families.contains(preferredFamily)) {
                family = preferredFamily;
                break;
            }
        }
    }

    return QLatin1String(isJapaneseOrChineseSystem?"Heisei Kaku Gothic S60":family.toLatin1());
#endif // QT_NO_FREETYPE
}

QString QFont::defaultFamily() const
{
#ifdef QT_NO_FREETYPE
    switch(d->request.styleHint) {
        case QFont::SansSerif:
        if (!classicalSymbianSystemFont().isEmpty())
            return classicalSymbianSystemFont();
        // No break. Intentional fall through.
        default:
            return lastResortFamily();
    }
#endif // QT_NO_FREETYPE
    return lastResortFamily();
}

QT_END_NAMESPACE
