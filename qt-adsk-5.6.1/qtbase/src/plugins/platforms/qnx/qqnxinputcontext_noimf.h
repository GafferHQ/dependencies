/***************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion
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

#ifndef QQNXINPUTCONTEXT_H
#define QQNXINPUTCONTEXT_H

#include <QtCore/QLocale>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatformintegration.h>

QT_BEGIN_NAMESPACE

class QQnxAbstractVirtualKeyboard;
class QQnxIntegration;

class QQnxInputContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    explicit QQnxInputContext(QQnxIntegration *integration, QQnxAbstractVirtualKeyboard &keyboard);
    ~QQnxInputContext();

    bool isValid() const;

    void reset();
    bool filterEvent( const QEvent *event );
    QRectF keyboardRect() const;
    bool handleKeyboardEvent(int flags, int sym, int mod, int scan, int cap);

    void showInputPanel();
    void hideInputPanel();
    bool isInputPanelVisible() const;

    QLocale locale() const;
    void setFocusObject(QObject *object);

private Q_SLOTS:
    void keyboardHeightChanged();
    void keyboardVisibilityChanged(bool visible);
    void keyboardLocaleChanged(const QLocale &locale);

private:
    bool hasPhysicalKeyboard();

    bool m_inputPanelVisible;
    QLocale m_inputPanelLocale;
    QQnxIntegration *m_integration;
    QQnxAbstractVirtualKeyboard &m_virtualKeyboard;
};

QT_END_NAMESPACE

#endif // QQNXINPUTCONTEXT_H
