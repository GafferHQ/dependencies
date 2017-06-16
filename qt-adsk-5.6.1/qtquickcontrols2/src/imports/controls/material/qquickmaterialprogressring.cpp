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

#include "qquickmaterialprogressring_p.h"

#include <cmath>

#include <QtCore/qset.h>
#include <QtGui/qpainter.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/qsgsimplerectnode.h>
#include <QtQuick/qsgsimpletexturenode.h>
#include <QtQuick/qquickwindow.h>

QT_BEGIN_NAMESPACE

/*
    Relevant Android code:

    - core/res/res/anim/progress_indeterminate_rotation_material.xml contains
      the rotation animation data.
    - core/res/res/anim/progress_indeterminate_material.xml contains the trim
      animation data.
    - core/res/res/interpolator/trim_start_interpolator.xml and
      core/res/res/interpolator/trim_end_interpolator.xml contain the start
      and end trim path interpolators.
    - addCommand() in core/java/android/util/PathParser.java has a list of the
      different path commands available.
*/

class QQuickMaterialRingAnimatorJob : public QQuickAnimatorJob
{
public:
    QQuickMaterialRingAnimatorJob();
    ~QQuickMaterialRingAnimatorJob();

    void initialize(QQuickAnimatorController *controller) Q_DECL_OVERRIDE;
    void updateCurrentTime(int time) Q_DECL_OVERRIDE;
    void writeBack() Q_DECL_OVERRIDE;
    void nodeWasDestroyed() Q_DECL_OVERRIDE;
    void afterNodeSync() Q_DECL_OVERRIDE;

private:
    qreal m_devicePixelRatio;
    QSGNode *m_containerNode;
    QQuickWindow *m_window;
};

class QQuickMaterialRingTexture : public QSGSimpleTextureNode
{
public:
    QQuickMaterialRingTexture();
    ~QQuickMaterialRingTexture();

    QColor color() const;
    void setColor(QColor color);

private:
    QColor m_color;
};

QQuickMaterialProgressRing::QQuickMaterialProgressRing(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents);
}

QQuickMaterialProgressRing::~QQuickMaterialProgressRing()
{
}

QSGNode *QQuickMaterialProgressRing::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if (!oldNode)
        oldNode = new QSGSimpleRectNode(boundingRect(), Qt::transparent);

    static_cast<QSGSimpleRectNode *>(oldNode)->setRect(boundingRect());

    QQuickMaterialRingTexture *textureNode = static_cast<QQuickMaterialRingTexture*>(oldNode->firstChild());
    if (!textureNode) {
        textureNode = new QQuickMaterialRingTexture;
        textureNode->setOwnsTexture(true);
        oldNode->appendChildNode(textureNode);
    }

    // A texture seems to be required here, but we don't have one yet, as we haven't drawn anything,
    // so just use a blank image.
    QImage blankImage(width(), height(), QImage::Format_ARGB32_Premultiplied);
    blankImage.fill(Qt::transparent);
    textureNode->setColor(m_color);
    textureNode->setRect(boundingRect());
    textureNode->setTexture(window()->createTextureFromImage(blankImage));

    return oldNode;
}

QColor QQuickMaterialProgressRing::color() const
{
    return m_color;
}

void QQuickMaterialProgressRing::setColor(QColor color)
{
    if (m_color != color) {
        m_color = color;
        update();
        emit colorChanged();
    }
}

static const int spanAnimationDuration = 700;
static const int rotationAnimationDuration = spanAnimationDuration * 6;
static const int targetRotation = 720;
static const int oneDegree = 16;
static const qreal minSweepSpan = 10 * oneDegree;
static const qreal maxSweepSpan = 300 * oneDegree;

QQuickMaterialRingAnimator::QQuickMaterialRingAnimator(QObject *parent) :
    QQuickAnimator(parent)
{
    setDuration(rotationAnimationDuration);
    setLoops(QQuickAnimator::Infinite);
}

QString QQuickMaterialRingAnimator::propertyName() const
{
    return QString();
}

QQuickAnimatorJob *QQuickMaterialRingAnimator::createJob() const
{
    return new QQuickMaterialRingAnimatorJob;
}

QQuickMaterialRingAnimatorJob::QQuickMaterialRingAnimatorJob() :
    m_devicePixelRatio(1.0),
    m_containerNode(Q_NULLPTR),
    m_window(Q_NULLPTR)
{
}

QQuickMaterialRingAnimatorJob::~QQuickMaterialRingAnimatorJob()
{
}

void QQuickMaterialRingAnimatorJob::initialize(QQuickAnimatorController *controller)
{
    QQuickAnimatorJob::initialize(controller);
    m_containerNode = QQuickItemPrivate::get(m_target)->childContainerNode();
    m_window = m_target->window();
    m_devicePixelRatio = m_window->effectiveDevicePixelRatio();
}

void QQuickMaterialRingAnimatorJob::updateCurrentTime(int time)
{
    if (!m_containerNode)
        return;

    QSGSimpleRectNode *rectNode = static_cast<QSGSimpleRectNode*>(m_containerNode->firstChild());
    if (!rectNode)
        return;

    Q_ASSERT(rectNode->type() == QSGNode::GeometryNodeType);

    const qreal width = rectNode->rect().width() * m_devicePixelRatio;
    const qreal height = rectNode->rect().height() * m_devicePixelRatio;
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen;
    QQuickMaterialRingTexture *textureNode = static_cast<QQuickMaterialRingTexture*>(rectNode->firstChild());
    pen.setColor(textureNode->color());
    pen.setWidth(4 * m_devicePixelRatio);
    painter.setPen(pen);

    const qreal percentageComplete = time / qreal(rotationAnimationDuration);
    const qreal spanPercentageComplete = (time % spanAnimationDuration) / qreal(spanAnimationDuration);
    const int iteration = time / spanAnimationDuration;
    int startAngle = 0;
    int endAngle = 0;
    static int lastStartAngle = 0;
    static int lastEndAngle = 0;

    if (iteration % 2 == 0) {
        if (lastStartAngle > 360 * oneDegree) {
            lastStartAngle -= 360 * oneDegree;
        }

        // The start angle is only affected by the rotation animation for the "grow" phase.
        startAngle = lastStartAngle;
        // TODO: use the correct curve here. QEasingCurve's bezier API doesn't support SVG path data.
        QEasingCurve angleCurve(QEasingCurve::OutQuad);
        const qreal percentage = angleCurve.valueForProgress(spanPercentageComplete);
        endAngle = lastStartAngle + minSweepSpan + percentage * (maxSweepSpan - minSweepSpan);
        lastEndAngle = endAngle;
    } else {
        // Both the start angle *and* the span are affected by the "shrink" phase.
        // TODO: use the correct curve here. QEasingCurve's bezier API doesn't support SVG path data.
        QEasingCurve angleCurve(QEasingCurve::InQuad);
        const qreal percentage = angleCurve.valueForProgress(spanPercentageComplete);
        startAngle = lastEndAngle - maxSweepSpan + percentage * (maxSweepSpan - minSweepSpan);
        endAngle = lastEndAngle;
        lastStartAngle = startAngle;
    }

    const int halfPen = pen.width() / 2;
    const QRectF arcBounds = QRectF(halfPen, halfPen, width - pen.width(), height - pen.width());
    // The current angle of the rotation animation.
    const qreal rotation = oneDegree * percentageComplete * -targetRotation;
    startAngle -= rotation;
    endAngle -= rotation;
    const int angleSpan = endAngle - startAngle;
    painter.drawArc(arcBounds, -startAngle, -angleSpan);
    painter.end();

    textureNode->setTexture(m_window->createTextureFromImage(image));
}

void QQuickMaterialRingAnimatorJob::writeBack()
{
}

void QQuickMaterialRingAnimatorJob::nodeWasDestroyed()
{
    m_containerNode = Q_NULLPTR;
    m_window = Q_NULLPTR;
}

void QQuickMaterialRingAnimatorJob::afterNodeSync()
{
    m_containerNode = QQuickItemPrivate::get(m_target)->childContainerNode();
}

QQuickMaterialRingTexture::QQuickMaterialRingTexture()
{
}

QQuickMaterialRingTexture::~QQuickMaterialRingTexture()
{
}

QColor QQuickMaterialRingTexture::color() const
{
    return m_color;
}

void QQuickMaterialRingTexture::setColor(QColor color)
{
    m_color = color;
}

QT_END_NAMESPACE
