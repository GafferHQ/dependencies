/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKANDROID9PATCH_P_H
#define QQUICKANDROID9PATCH_P_H

#include <QtQuick/qquickitem.h>
#include <QtQuick/qsggeometry.h>
#include <QtQuick/qsgtexturematerial.h>

QT_BEGIN_NAMESPACE

struct QQuickAndroid9PatchDivs
{
    QVector<qreal> coordsForSize(qreal size) const;

    void fill(const QVariantList &divs, qreal size);
    void clear();

    bool inverted;
    QVector<qreal> data;
};

class QQuickAndroid9Patch : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(QVariantList xDivs READ xDivs WRITE setXDivs NOTIFY xDivsChanged FINAL)
    Q_PROPERTY(QVariantList yDivs READ yDivs WRITE setYDivs NOTIFY yDivsChanged FINAL)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged FINAL)

public:
    explicit QQuickAndroid9Patch(QQuickItem *parent = 0);
    ~QQuickAndroid9Patch();

    QUrl source() const;
    QVariantList xDivs() const;
    QVariantList yDivs() const;
    QSize sourceSize() const;

Q_SIGNALS:
    void sourceChanged(const QUrl &source);
    void xDivsChanged(const QVariantList &divs);
    void yDivsChanged(const QVariantList &divs);
    void sourceSizeChanged(const QSize &size);

public Q_SLOTS:
    void setSource(const QUrl &source);
    void setXDivs(const QVariantList &divs);
    void setYDivs(const QVariantList &divs);

protected:
    void classBegin();
    void componentComplete();
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data);

private Q_SLOTS:
    void loadImage();
    void updateDivs();

private:
    QImage m_image;
    QUrl m_source;
    QSize m_sourceSize;
    QVariantList m_xVars;
    QVariantList m_yVars;
    QQuickAndroid9PatchDivs m_xDivs;
    QQuickAndroid9PatchDivs m_yDivs;
};

QT_END_NAMESPACE

#endif // QQUICKANDROID9PATCH_P_H
