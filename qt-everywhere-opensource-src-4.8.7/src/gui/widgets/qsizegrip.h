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

#ifndef QSIZEGRIP_H
#define QSIZEGRIP_H

#include <QtGui/qwidget.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

#ifndef QT_NO_SIZEGRIP
class QSizeGripPrivate;
class Q_GUI_EXPORT QSizeGrip : public QWidget
{
    Q_OBJECT
public:
    explicit QSizeGrip(QWidget *parent);
    ~QSizeGrip();

    QSize sizeHint() const;
    void setVisible(bool);

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *mouseEvent);
    void moveEvent(QMoveEvent *moveEvent);
    void showEvent(QShowEvent *showEvent);
    void hideEvent(QHideEvent *hideEvent);
    bool eventFilter(QObject *, QEvent *);
    bool event(QEvent *);
#ifdef Q_WS_WIN
    bool winEvent(MSG *m, long *result);
#endif

public:
#ifdef QT3_SUPPORT
    QT3_SUPPORT_CONSTRUCTOR QSizeGrip(QWidget *parent, const char *name);
#endif

private:
    Q_DECLARE_PRIVATE(QSizeGrip)
    Q_DISABLE_COPY(QSizeGrip)
    Q_PRIVATE_SLOT(d_func(), void _q_showIfNotHidden())
};
#endif // QT_NO_SIZEGRIP

QT_END_NAMESPACE

QT_END_HEADER

#endif // QSIZEGRIP_H
