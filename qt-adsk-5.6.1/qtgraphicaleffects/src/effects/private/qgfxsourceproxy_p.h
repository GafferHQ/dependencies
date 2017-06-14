/****************************************************************************
**
** Copyright (C) 2015 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module of the Qt Toolkit.
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

#ifndef QGFXSOURCEPROXY_P_H
#define QGFXSOURCEPROXY_P_H

#include <QtQuick/QQuickItem>

QT_BEGIN_NAMESPACE

class QQuickShaderEffectSource;

class QGfxSourceProxy : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QQuickItem *input READ input WRITE setInput NOTIFY inputChanged RESET resetInput)
    Q_PROPERTY(QQuickItem *output READ output NOTIFY outputChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect NOTIFY sourceRectChanged)

    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(Interpolation interpolation READ interpolation WRITE setInterpolation NOTIFY interpolationChanged)

    Q_ENUMS(Interpolation)

public:
    enum Interpolation {
        AnyInterpolation,
        NearestInterpolation,
        LinearInterpolation
    };

    QGfxSourceProxy(QQuickItem *item = 0);
    ~QGfxSourceProxy();

    QQuickItem *input() const { return m_input; }
    void setInput(QQuickItem *input);
    void resetInput() { setInput(0); }

    QQuickItem *output() const { return m_output; }

    QRectF sourceRect() const { return m_sourceRect; }
    void setSourceRect(const QRectF &sourceRect);

    bool isActive() const { return m_output && m_output != m_input; }

    void setInterpolation(Interpolation i);
    Interpolation interpolation() const { return m_interpolation; }

protected:
    void updatePolish() Q_DECL_OVERRIDE;

signals:
    void inputChanged();
    void outputChanged();
    void sourceRectChanged();
    void activeChanged();
    void interpolationChanged();

private:
    void setOutput(QQuickItem *output);
    void useProxy();
    static QObject *findLayer(QQuickItem *);

    QRectF m_sourceRect;
    QQuickItem *m_input;
    QQuickItem *m_output;
    QQuickShaderEffectSource *m_proxy;

    Interpolation m_interpolation;
};

QT_END_NAMESPACE

#endif // QGFXSOURCEPROXY_P_H
