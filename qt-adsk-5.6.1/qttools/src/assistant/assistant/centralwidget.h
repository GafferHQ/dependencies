/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef CENTRALWIDGET_H
#define CENTRALWIDGET_H

#include <QtCore/QUrl>

#include <QtWidgets/QTabBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class FindWidget;
class HelpViewer;
class QStackedWidget;
class QPrinter;

class TabBar : public QTabBar
{
    Q_OBJECT
    friend class CentralWidget;

public:
    ~TabBar();

    int addNewTab(const QString &title);
    void setCurrent(HelpViewer *viewer);
    void removeTabAt(HelpViewer *viewer);

public slots:
    void titleChanged();

signals:
    void currentTabChanged(HelpViewer *viewer);
    void addBookmark(const QString &title, const QString &url);

private:
    TabBar(QWidget *parent = 0);

private slots:
    void slotCurrentChanged(int index);
    void slotTabCloseRequested(int index);
    void slotCustomContextMenuRequested(const QPoint &pos);
};

class CentralWidget : public QWidget
{
    Q_OBJECT
    friend class OpenPagesManager;

public:
    CentralWidget(QWidget *parent = 0);
    ~CentralWidget();

    static CentralWidget *instance();

    QUrl currentSource() const;
    QString currentTitle() const;

    bool hasSelection() const;
    bool isForwardAvailable() const;
    bool isBackwardAvailable() const;

    HelpViewer *viewerAt(int index) const;
    HelpViewer *currentHelpViewer() const;
    int currentIndex() const;

    void connectTabBar();

public slots:
#ifndef QT_NO_CLIPBOARD
    void copy();
#endif
    void home();

    void zoomIn();
    void zoomOut();
    void resetZoom();

    void forward();
    void nextPage();

    void backward();
    void previousPage();

    void print();
    void pageSetup();
    void printPreview();

    void setSource(const QUrl &url);
    void setSourceFromSearch(const QUrl &url);

    void findNext();
    void findPrevious();
    void find(const QString &text, bool forward, bool incremental);

    void activateTab();
    void showTextSearch();
    void updateBrowserFont();
    void updateUserInterface();

signals:
    void currentViewerChanged();
    void copyAvailable(bool yes);
    void sourceChanged(const QUrl &url);
    void highlighted(const QString &link);
    void forwardAvailable(bool available);
    void backwardAvailable(bool available);
    void addBookmark(const QString &title, const QString &url);

protected:
    void keyPressEvent(QKeyEvent *);
    void focusInEvent(QFocusEvent *event);

private slots:
    void highlightSearchTerms();
    void printPreview(QPrinter *printer);
    void handleSourceChanged(const QUrl &url);
    void slotHighlighted(const QString &link);

private:
    void initPrinter();
    void connectSignals(HelpViewer *page);
    bool eventFilter(QObject *object, QEvent *e);

    void removePage(int index);
    void setCurrentPage(HelpViewer *page);
    void addPage(HelpViewer *page, bool fromSearch = false);

private:
#ifndef QT_NO_PRINTER
    QPrinter *m_printer;
#endif
    FindWidget *m_findWidget;
    QStackedWidget *m_stackedWidget;
    TabBar *m_tabBar;
    QHash<QString, QString> m_resolvedLinks;
};

QT_END_NAMESPACE

#endif  // CENTRALWIDGET_H
