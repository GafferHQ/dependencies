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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtWidgets/QMainWindow>

QT_BEGIN_NAMESPACE

class QAction;
class QComboBox;
class QFileSystemWatcher;
class QLineEdit;
class QMenu;

class CentralWidget;
class CmdLineParser;
class ContentWindow;
class IndexWindow;
class OpenPagesWindow;
class QtDocInstaller;
class QHelpEngineCore;
class QHelpEngine;
class SearchWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(CmdLineParser *cmdLine, QWidget *parent = 0);
    ~MainWindow();

    static void activateCurrentBrowser();
    static QString collectionFileDirectory(bool createDir = false,
        const QString &cacheDir = QString());
    static QString defaultHelpCollectionFileName();

public:
    void setIndexString(const QString &str);
    void expandTOC(int depth);
    bool usesDefaultCollection() const;

signals:
    void initDone();

public slots:
    void setContentsVisible(bool visible);
    void setIndexVisible(bool visible);
    void setBookmarksVisible(bool visible);
    void setSearchVisible(bool visible);
    void syncContents();
    void activateCurrentCentralWidgetTab();
    void currentFilterChanged(const QString &filter);

private slots:
    void showContents();
    void showIndex();
    void showSearch();
    void showOpenPages();
    void insertLastPages();
    void gotoAddress();
    void showPreferences();
    void showNewAddress();
    void showAboutDialog();
    void showNewAddress(const QUrl &url);
    void showTopicChooser(const QMap<QString, QUrl> &links, const QString &keyword);
    void updateApplicationFont();
    void filterDocumentation(const QString &customFilter);
    void setupFilterCombo();
    void lookForNewQtDocumentation();
    void indexingStarted();
    void indexingFinished();
    void qtDocumentationInstalled();
    void registerDocumentation(const QString &component,
        const QString &absFileName);
    void resetQtDocInfo(const QString &component);
    void checkInitState();
    void documentationRemoved(const QString &namespaceName);
    void documentationUpdated(const QString &namespaceName);

private:
    bool initHelpDB(bool registerInternalDoc);
    void setupActions();
    void closeEvent(QCloseEvent *e);
    void activateDockWidget(QWidget *w);
    void updateAboutMenuText();
    void setupFilterToolbar();
    void setupAddressToolbar();
    QMenu *toolBarMenu();
    void hideContents();
    void hideIndex();
    void hideSearch();

private slots:
    void showBookmarksDockWidget();
    void hideBookmarksDockWidget();
    void handlePageCountChanged();

private:
    QWidget *m_bookmarkWidget;

private:
    CentralWidget *m_centralWidget;
    IndexWindow *m_indexWindow;
    ContentWindow *m_contentWindow;
    SearchWidget *m_searchWindow;
    QLineEdit *m_addressLineEdit;
    QComboBox *m_filterCombo;

    QAction *m_syncAction;
    QAction *m_printPreviewAction;
    QAction *m_pageSetupAction;
    QAction *m_resetZoomAction;
    QAction *m_aboutAction;
    QAction *m_closeTabAction;
    QAction *m_newTabAction;

    QMenu *m_viewMenu;
    QMenu *m_toolBarMenu;

    CmdLineParser *m_cmdLine;

    QWidget *m_progressWidget;
    QtDocInstaller *m_qtDocInstaller;

    bool m_connectedInitSignals;
};

QT_END_NAMESPACE

#endif // MAINWINDOW_H
