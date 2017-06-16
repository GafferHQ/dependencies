/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2015 The Qt Company Ltd.
 ** Contact: http://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKTHUMBNAILTOOLBAR_P_H
#define QQUICKTHUMBNAILTOOLBAR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QQuickItem>
#include <QWinThumbnailToolBar>
#include <QUrl>

QT_BEGIN_NAMESPACE

class QVariant;

class QQuickThumbnailToolButton;

class QQuickThumbnailToolBar : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_PROPERTY(QQmlListProperty<QQuickThumbnailToolButton> buttons READ buttons NOTIFY buttonsChanged)
    Q_PROPERTY(bool iconicNotificationsEnabled READ iconicNotificationsEnabled WRITE setIconicNotificationsEnabled NOTIFY iconicNotificationsEnabledChanged)
    Q_PROPERTY(QUrl iconicThumbnailSource READ iconicThumbnailSource WRITE setIconicThumbnailSource NOTIFY iconicThumbnailSourceChanged)
    Q_PROPERTY(QUrl iconicLivePreviewSource READ iconicLivePreviewSource WRITE setIconicLivePreviewSource NOTIFY iconicLivePreviewSourceChanged)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit QQuickThumbnailToolBar(QQuickItem *parent = 0);
    ~QQuickThumbnailToolBar();

    int count() const;

    QQmlListProperty<QObject> data();
    QQmlListProperty<QQuickThumbnailToolButton> buttons();

    Q_INVOKABLE void addButton(QQuickThumbnailToolButton *button);
    Q_INVOKABLE void removeButton(QQuickThumbnailToolButton *button);

    bool iconicNotificationsEnabled() const;
    void setIconicNotificationsEnabled(bool);
    QUrl iconicThumbnailSource() const { return m_iconicThumbnailSource; }
    void setIconicThumbnailSource(const QUrl &);
    QUrl iconicLivePreviewSource() const { return m_iconicLivePreviewSource; }
    void setIconicLivePreviewSource(const QUrl &);

public Q_SLOTS:
    void clear();

Q_SIGNALS:
    void countChanged();
    void buttonsChanged();
    void iconicNotificationsEnabledChanged();
    void iconicThumbnailSourceChanged();
    void iconicThumbnailRequested();
    void iconicLivePreviewSourceChanged();
    void iconicLivePreviewRequested();

private Q_SLOTS:
    void iconicThumbnailLoaded(const QVariant &);
    void iconicLivePreviewLoaded(const QVariant &);

protected:
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data);

private:
    static void addData(QQmlListProperty<QObject> *property, QObject *button);
    static int buttonCount(QQmlListProperty<QQuickThumbnailToolButton> *property);
    static QQuickThumbnailToolButton *buttonAt(QQmlListProperty<QQuickThumbnailToolButton> *property, int index);

    QWinThumbnailToolBar m_toolbar;
    QList<QQuickThumbnailToolButton *> m_buttons;
    QUrl m_iconicThumbnailSource;
    QUrl m_iconicLivePreviewSource;
};

QT_END_NAMESPACE

#endif // QQUICKTHUMBNAILTOOLBAR_P_H
