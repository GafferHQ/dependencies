/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef QVFBPROTOCOL_H
#define QVFBPROTOCOL_H

#include <QImage>
#include <QVector>
#include <QColor>

QT_BEGIN_NAMESPACE

class QVFbKeyProtocol;
class QVFbMouseProtocol;
class QVFbViewProtocol : public QObject
{
    Q_OBJECT
public:
    QVFbViewProtocol(int display_id, QObject *parent = 0);

    virtual ~QVFbViewProtocol();

    int id() const { return mDisplayId; }

    void sendKeyboardData(QString unicode, int keycode,
            int modifiers, bool press, bool repeat);
    void sendMouseData(const QPoint &pos, int buttons, int wheel);

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual int depth() const = 0;
    virtual int linestep() const = 0;
    virtual int  numcols() const = 0;
    virtual QVector<QRgb> clut() const = 0;
    virtual unsigned char *data() const = 0;
    virtual int brightness() const = 0;

    virtual void setRate(int) {}
public slots:
    virtual void flushChanges();

signals:
    void displayDataChanged(const QRect &);
    void displayEmbedRequested(WId windowId);

protected:
    virtual QVFbKeyProtocol *keyHandler() const = 0;
    virtual QVFbMouseProtocol *mouseHandler() const = 0;

private:
    int mDisplayId;
};

class QVFbKeyProtocol
{
public:
    QVFbKeyProtocol(int display_id) : mDisplayId(display_id) {}
    virtual ~QVFbKeyProtocol() {}

    int id() const { return mDisplayId; }

    virtual void sendKeyboardData(QString unicode, int keycode,
            int modifiers, bool press, bool repeat) = 0;

private:
    int mDisplayId;
};

class QVFbMouseProtocol
{
public:
    QVFbMouseProtocol(int display_id) : mDisplayId(display_id) {}
    virtual ~QVFbMouseProtocol() {}

    int id() const { return mDisplayId; }

    virtual void sendMouseData(const QPoint &pos, int buttons, int wheel) = 0;

private:
    int mDisplayId;
};

/* since there is very little variation in input protocols defaults are
   provided */

class QVFbKeyPipeProtocol : public QVFbKeyProtocol
{
public:
    QVFbKeyPipeProtocol(int display_id);
    ~QVFbKeyPipeProtocol();

    void sendKeyboardData(QString unicode, int keycode,
            int modifiers, bool press, bool repeat);

    QString pipeName() const { return fileName; }
private:
    int fd;
    QString fileName;
};

class QVFbMousePipe: public QVFbMouseProtocol
{
public:
    QVFbMousePipe(int display_id);
    ~QVFbMousePipe();

    void sendMouseData(const QPoint &pos, int buttons, int wheel);

    QString pipeName() const { return fileName; }
protected:
    int fd;
    QString fileName;
};

class QTimer;
class QVFbMouseLinuxTP : public QObject, public QVFbMousePipe
{
    Q_OBJECT
public:
    QVFbMouseLinuxTP(int display_id);
    ~QVFbMouseLinuxTP();

    void sendMouseData(const QPoint &pos, int buttons, int wheel);

protected slots:
    void repeatLastPress();

protected:
    void writeToPipe(const QPoint &pos, ushort pressure);
    QPoint lastPos;
    QTimer *repeater;
};

QT_END_NAMESPACE

#endif
