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

#ifndef QLABEL_H
#define QLABEL_H

#include <QtGui/qframe.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QLabelPrivate;

class Q_GUI_EXPORT QLabel : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
    Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
    Q_PROPERTY(bool scaledContents READ hasScaledContents WRITE setScaledContents)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
    Q_PROPERTY(int indent READ indent WRITE setIndent)
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)
    Q_PROPERTY(Qt::TextInteractionFlags textInteractionFlags READ textInteractionFlags WRITE setTextInteractionFlags)
    Q_PROPERTY(bool hasSelectedText READ hasSelectedText)
    Q_PROPERTY(QString selectedText READ selectedText)

public:
    explicit QLabel(QWidget *parent=0, Qt::WindowFlags f=0);
    explicit QLabel(const QString &text, QWidget *parent=0, Qt::WindowFlags f=0);
    ~QLabel();

    QString text() const;
    const QPixmap *pixmap() const;
#ifndef QT_NO_PICTURE
    const QPicture *picture() const;
#endif
#ifndef QT_NO_MOVIE
    QMovie *movie() const;
#endif

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment);

    void setWordWrap(bool on);
    bool wordWrap() const;

    int indent() const;
    void setIndent(int);

    int margin() const;
    void setMargin(int);

    bool hasScaledContents() const;
    void setScaledContents(bool);
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
#ifndef QT_NO_SHORTCUT
    void setBuddy(QWidget *);
    QWidget *buddy() const;
#endif
    int heightForWidth(int) const;

    bool openExternalLinks() const;
    void setOpenExternalLinks(bool open);

    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    Qt::TextInteractionFlags textInteractionFlags() const;

    void setSelection(int, int);
    bool hasSelectedText() const;
    QString selectedText() const;
    int selectionStart() const;

public Q_SLOTS:
    void setText(const QString &);
    void setPixmap(const QPixmap &);
#ifndef QT_NO_PICTURE
    void setPicture(const QPicture &);
#endif
#ifndef QT_NO_MOVIE
    void setMovie(QMovie *movie);
#endif
    void setNum(int);
    void setNum(double);
    void clear();

Q_SIGNALS:
    void linkActivated(const QString& link);
    void linkHovered(const QString& link);

protected:
    bool event(QEvent *e);
    void keyPressEvent(QKeyEvent *ev);
    void paintEvent(QPaintEvent *);
    void changeEvent(QEvent *);
    void mousePressEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);
    void contextMenuEvent(QContextMenuEvent *ev);
    void focusInEvent(QFocusEvent *ev);
    void focusOutEvent(QFocusEvent *ev);
    bool focusNextPrevChild(bool next);

#ifdef QT3_SUPPORT
public:
    QT3_SUPPORT_CONSTRUCTOR QLabel(QWidget *parent, const char* name, Qt::WindowFlags f=0);
    QT3_SUPPORT_CONSTRUCTOR QLabel(const QString &text, QWidget *parent, const char* name,
           Qt::WindowFlags f=0);
    QT3_SUPPORT_CONSTRUCTOR QLabel(QWidget *buddy, const QString &,
           QWidget *parent=0, const char* name=0, Qt::WindowFlags f=0);
    QT3_SUPPORT void setAlignment(int alignment);

    // don't mark the next function with QT3_SUPPORT
    inline void setAlignment(Qt::AlignmentFlag flag) { setAlignment((Qt::Alignment)flag); }
#endif

private:
    Q_DISABLE_COPY(QLabel)
    Q_DECLARE_PRIVATE(QLabel)
#ifndef QT_NO_MOVIE
    Q_PRIVATE_SLOT(d_func(), void _q_movieUpdated(const QRect&))
    Q_PRIVATE_SLOT(d_func(), void _q_movieResized(const QSize&))
#endif
    Q_PRIVATE_SLOT(d_func(), void _q_linkHovered(const QString &))

    friend class QTipLabel;
    friend class QMessageBoxPrivate;
    friend class QBalloonTip;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QLABEL_H
