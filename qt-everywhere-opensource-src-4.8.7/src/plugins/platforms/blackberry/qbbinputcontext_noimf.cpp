/****************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion <blackberry-qt@qnx.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

// #define QBBINPUTCONTEXT_DEBUG

#include <qbbinputcontext.h>
#include <qbbabstractvirtualkeyboard.h>

#include <QDebug>
#include <QAbstractSpinBox>
#include <QAbstractItemView>

QT_BEGIN_NAMESPACE

QBBInputContext::QBBInputContext(QBBAbstractVirtualKeyboard &keyboard, QObject* parent)
    : QInputContext(parent),
      mVirtualKeyboard(keyboard)
{
}

QBBInputContext::~QBBInputContext()
{
}

QString QBBInputContext::language()
{
    // Once we enable full IMF support, we need to hook that up here.
    return mVirtualKeyboard.languageId();
}

bool QBBInputContext::hasPhysicalKeyboard()
{
    // TODO: This should query the system to check if a USB keyboard is connected.
    return false;
}

void QBBInputContext::reset()
{
}

bool QBBInputContext::filterEvent( const QEvent *event )
{
    if (hasPhysicalKeyboard())
        return false;

    if (event->type() == QEvent::CloseSoftwareInputPanel) {
        mVirtualKeyboard.hideKeyboard();
#if defined(QBBINPUTCONTEXT_DEBUG)
        qDebug() << "QBB: hiding virtual keyboard";
#endif
        return false;
    }

    if (event->type() == QEvent::RequestSoftwareInputPanel) {
        mVirtualKeyboard.showKeyboard();
#if defined(QBBINPUTCONTEXT_DEBUG)
        qDebug() << "QBB: requesting virtual keyboard";
#endif
        return false;
    }

    return false;

}

bool QBBInputContext::handleKeyboardEvent(int flags, int sym, int mod, int scan, int cap)
{
    return false;
}

void QBBInputContext::setFocusWidget(QWidget *w)
{
#if defined(QBBINPUTCONTEXT_DEBUG)
    qDebug() << Q_FUNC_INFO << (w ? "requesting" : "hiding") << "virtual keyboard";
#endif
    QInputContext::setFocusWidget(w);

    if (w) {
        // Special case for item view which should not show the keyboard when focused
        if (qobject_cast<QAbstractItemView*>(w))
            return;

        mVirtualKeyboard.setInputHintsFromWidget(w);
        mVirtualKeyboard.showKeyboard();
    } else {
        mVirtualKeyboard.hideKeyboard();
    }
}

QT_END_NAMESPACE
