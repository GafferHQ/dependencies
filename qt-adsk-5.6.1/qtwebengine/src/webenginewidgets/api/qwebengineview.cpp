/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "qwebengineview.h"
#include "qwebengineview_p.h"

#include "qwebenginepage_p.h"
#include "web_contents_adapter.h"

#ifdef QT_UI_DELEGATES
#include "ui/messagebubblewidget_p.h"
#endif

#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QStackedLayout>

QT_BEGIN_NAMESPACE

void QWebEngineViewPrivate::bind(QWebEngineView *view, QWebEnginePage *page)
{
    if (view && page == view->d_func()->page)
        return;

    if (page) {
        // Un-bind page from its current view.
        if (QWebEngineView *oldView = page->d_func()->view) {
            page->disconnect(oldView);
            oldView->d_func()->page = 0;
        }
        page->d_func()->view = view;
        page->d_func()->adapter->reattachRWHV();
    }

    if (view) {
        // Un-bind view from its current page.
        if (QWebEnginePage *oldPage = view->d_func()->page) {
            oldPage->disconnect(view);
            oldPage->d_func()->view = 0;
            oldPage->d_func()->adapter->reattachRWHV();
            if (oldPage->parent() == view)
                delete oldPage;
        }
        view->d_func()->page = page;
    }

    if (view && page) {
        QObject::connect(page, &QWebEnginePage::titleChanged, view, &QWebEngineView::titleChanged);
        QObject::connect(page, &QWebEnginePage::urlChanged, view, &QWebEngineView::urlChanged);
        QObject::connect(page, &QWebEnginePage::iconUrlChanged, view, &QWebEngineView::iconUrlChanged);
        QObject::connect(page, &QWebEnginePage::loadStarted, view, &QWebEngineView::loadStarted);
        QObject::connect(page, &QWebEnginePage::loadProgress, view, &QWebEngineView::loadProgress);
        QObject::connect(page, &QWebEnginePage::loadFinished, view, &QWebEngineView::loadFinished);
        QObject::connect(page, &QWebEnginePage::selectionChanged, view, &QWebEngineView::selectionChanged);
        QObject::connect(page, &QWebEnginePage::renderProcessTerminated, view, &QWebEngineView::renderProcessTerminated);
    }
}

#ifndef QT_NO_ACCESSIBILITY
static QAccessibleInterface *webAccessibleFactory(const QString &, QObject *object)
{
    if (QWebEngineView *v = qobject_cast<QWebEngineView*>(object))
        return new QWebEngineViewAccessible(v);
    return Q_NULLPTR;
}
#endif // QT_NO_ACCESSIBILITY

QWebEngineViewPrivate::QWebEngineViewPrivate()
    : page(0)
    , m_pendingContextMenuEvent(false)
{
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(&webAccessibleFactory);
#endif // QT_NO_ACCESSIBILITY
}

/*!
    \fn QWebEngineView::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
    \since 5.6

    This signal is emitted when the render process is terminated with a non-zero exit status.
    \a terminationStatus is the termination status of the process and \a exitCode is the status code
    with which the process terminated.
*/

QWebEngineView::QWebEngineView(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new QWebEngineViewPrivate)
{
    Q_D(QWebEngineView);
    d->q_ptr = this;

    // This causes the child RenderWidgetHostViewQtDelegateWidgets to fill this widget.
    setLayout(new QStackedLayout);
}

QWebEngineView::~QWebEngineView()
{
    Q_D(QWebEngineView);
    QWebEngineViewPrivate::bind(0, d->page);

#ifdef QT_UI_DELEGATES
    QtWebEngineWidgetUI::MessageBubbleWidget::hideBubble();
#endif
}

QWebEnginePage* QWebEngineView::page() const
{
    Q_D(const QWebEngineView);
    if (!d->page) {
        QWebEngineView *that = const_cast<QWebEngineView*>(this);
        that->setPage(new QWebEnginePage(that));
    }
    return d->page;
}

void QWebEngineView::setPage(QWebEnginePage* page)
{
    QWebEngineViewPrivate::bind(this, page);
}

void QWebEngineView::load(const QUrl& url)
{
    page()->load(url);
}

void QWebEngineView::setHtml(const QString& html, const QUrl& baseUrl)
{
    page()->setHtml(html, baseUrl);
}

void QWebEngineView::setContent(const QByteArray& data, const QString& mimeType, const QUrl& baseUrl)
{
    page()->setContent(data, mimeType, baseUrl);
}

QWebEngineHistory* QWebEngineView::history() const
{
    return page()->history();
}

QString QWebEngineView::title() const
{
    return page()->title();
}

void QWebEngineView::setUrl(const QUrl &url)
{
    page()->setUrl(url);
}

QUrl QWebEngineView::url() const
{
    return page()->url();
}

QUrl QWebEngineView::iconUrl() const
{
    return page()->iconUrl();
}

bool QWebEngineView::hasSelection() const
{
    return page()->hasSelection();
}

QString QWebEngineView::selectedText() const
{
    return page()->selectedText();
}

#ifndef QT_NO_ACTION
QAction* QWebEngineView::pageAction(QWebEnginePage::WebAction action) const
{
    return page()->action(action);
}
#endif

void QWebEngineView::triggerPageAction(QWebEnginePage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

void QWebEngineView::findText(const QString &subString, QWebEnginePage::FindFlags options, const QWebEngineCallback<bool> &resultCallback)
{
    page()->findText(subString, options, resultCallback);
}

/*!
 * \reimp
 */
QSize QWebEngineView::sizeHint() const
{
    return QSize(800, 600);
}

QWebEngineSettings *QWebEngineView::settings() const
{
    return page()->settings();
}

void QWebEngineView::stop()
{
    page()->triggerAction(QWebEnginePage::Stop);
}

void QWebEngineView::back()
{
    page()->triggerAction(QWebEnginePage::Back);
}

void QWebEngineView::forward()
{
    page()->triggerAction(QWebEnginePage::Forward);
}

void QWebEngineView::reload()
{
    page()->triggerAction(QWebEnginePage::Reload);
}

QWebEngineView *QWebEngineView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)
    return 0;
}


qreal QWebEngineView::zoomFactor() const
{
    return page()->zoomFactor();
}

void QWebEngineView::setZoomFactor(qreal factor)
{
    page()->setZoomFactor(factor);
}

/*!
 * \reimp
 */
bool QWebEngineView::event(QEvent *ev)
{
    Q_D(QWebEngineView);
    // We swallow spontaneous contextMenu events and synthethize those back later on when we get the
    // HandleContextMenu callback from chromium
    if (ev->type() == QEvent::ContextMenu) {
        ev->accept();
        d->m_pendingContextMenuEvent = true;
        return true;
    }
    return QWidget::event(ev);
}

/*!
 * \reimp
 */
void QWebEngineView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = page()->createStandardContextMenu();
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(event->globalPos());
}

/*!
 * \reimp
 */
void QWebEngineView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    page()->d_ptr->wasShown();
}

/*!
 * \reimp
 */
void QWebEngineView::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    page()->d_ptr->wasHidden();
}

#ifndef QT_NO_ACCESSIBILITY
int QWebEngineViewAccessible::childCount() const
{
    if (view() && child(0))
        return 1;
    return 0;
}

QAccessibleInterface *QWebEngineViewAccessible::child(int index) const
{
    if (index == 0 && view() && view()->page())
        return view()->page()->d_func()->adapter->browserAccessible();
    return Q_NULLPTR;
}

int QWebEngineViewAccessible::indexOfChild(const QAccessibleInterface *c) const
{
    if (c == child(0))
        return 0;
    return -1;
}
#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#include "moc_qwebengineview.cpp"
