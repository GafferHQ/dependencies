/****************************************************************************
**
** Copyright (C) 2012 BogDan Vatra <bogdan@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QANDROIDPLATFORMTHEME_H
#define QANDROIDPLATFORMTHEME_H

#include <qpa/qplatformtheme.h>
#include <QtGui/qfont.h>
#include <QtGui/qpalette.h>

#include <QJsonObject>

#include <memory>

QT_BEGIN_NAMESPACE

struct AndroidStyle
{
    static QJsonObject loadStyleData();
    QJsonObject m_styleData;
    QPalette m_standardPalette;
    QHash<int, QPalette> m_palettes;
    QHash<int, QFont> m_fonts;
    QHash<QByteArray, QFont> m_QWidgetsFonts;
};

class QAndroidPlatformNativeInterface;
class QAndroidPlatformTheme: public QPlatformTheme
{
public:
    QAndroidPlatformTheme(QAndroidPlatformNativeInterface * androidPlatformNativeInterface);
    virtual QPlatformMenuBar *createPlatformMenuBar() const;
    virtual QPlatformMenu *createPlatformMenu() const;
    virtual QPlatformMenuItem *createPlatformMenuItem() const;
    virtual void showPlatformMenuBar();
    virtual const QPalette *palette(Palette type = SystemPalette) const;
    virtual const QFont *font(Font type = SystemFont) const;
    virtual QVariant themeHint(ThemeHint hint) const;
    QString standardButtonText(int button) const Q_DECL_OVERRIDE;
    virtual bool usePlatformNativeDialog(DialogType type) const;
    virtual QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const;


private:
    std::shared_ptr<AndroidStyle> m_androidStyleData;
    QPalette m_defaultPalette;
    QFont m_systemFont;
};

QT_END_NAMESPACE

#endif // QANDROIDPLATFORMTHEME_H
