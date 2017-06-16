/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
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

#include "qblackberrytheme.h"

#include "qqnxfiledialoghelper.h"
#include "qqnxsystemsettings.h"
#include "qqnxintegration.h"

QT_BEGIN_NAMESPACE

QBlackberryTheme::QBlackberryTheme(const QQnxIntegration *integration) : m_integration(integration)
{
    // Set the dark theme as default palette
    QColor color = QColor(211, 211, 211);
    m_defaultPalette.setBrush(QPalette::ButtonText, color);
    m_defaultPalette.setBrush(QPalette::WindowText, color);
    m_defaultPalette.setBrush(QPalette::Text, color);

    color.setAlpha(179);
    m_defaultPalette.setBrush(QPalette::Disabled, QPalette::ButtonText, color);
    m_defaultPalette.setBrush(QPalette::Disabled, QPalette::WindowText, color);
    m_defaultPalette.setBrush(QPalette::Disabled, QPalette::Text, color);

    color.setRgb(18, 18, 18);
    m_defaultPalette.setColor(QPalette::Window, color);
    m_defaultPalette.setColor(QPalette::Base, color);
    m_defaultPalette.setColor(QPalette::AlternateBase, QColor(50, 50, 50));
    m_defaultPalette.setColor(QPalette::Button, color);

    m_defaultPalette.setBrush(QPalette::Highlight, QColor(0, 168, 223));
    m_defaultPalette.setBrush(QPalette::HighlightedText, QColor(250, 250,250));
}

QBlackberryTheme::~QBlackberryTheme()
{
    qDeleteAll(m_fonts);
}

bool QBlackberryTheme::usePlatformNativeDialog(DialogType type) const
{
    if (type == QPlatformTheme::FileDialog)
        return true;
#if !defined(QT_NO_COLORDIALOG)
    if (type == QPlatformTheme::ColorDialog)
        return false;
#endif
#if !defined(QT_NO_FONTDIALOG)
    if (type == QPlatformTheme::FontDialog)
        return false;
#endif
    return false;
}

QPlatformDialogHelper *QBlackberryTheme::createPlatformDialogHelper(DialogType type) const
{
    switch (type) {
    case QPlatformTheme::FileDialog:
        return new QQnxFileDialogHelper(m_integration);
#if !defined(QT_NO_COLORDIALOG)
    case QPlatformTheme::ColorDialog:
#endif
#if !defined(QT_NO_FONTDIALOG)
    case QPlatformTheme::FontDialog:
#endif
    default:
        return 0;
    }
}

const QFont *QBlackberryTheme::font(Font type) const
{
    QPlatformFontDatabase *fontDatabase = m_integration->fontDatabase();

    if (fontDatabase && m_fonts.isEmpty())
        m_fonts = qt_qnx_createRoleFonts(fontDatabase);
    return m_fonts.value(type, 0);
}

const QPalette *QBlackberryTheme::palette(Palette type) const
{
    // Return the default palette
    if (type == SystemPalette)
        return &m_defaultPalette;

    return QPlatformTheme::palette(type);
}

QT_END_NAMESPACE
