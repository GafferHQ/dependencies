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

#ifndef HELPVIEWERPRIVATE_H
#define HELPVIEWERPRIVATE_H

#include "centralwidget.h"
#include "helpviewer.h"
#include "openpagesmanager.h"

#include <QtCore/QObject>
#if defined(BROWSER_QTEXTBROWSER)
#  include <QtWidgets/QTextBrowser>
#elif defined(BROWSER_QTWEBKIT)
#  include <QtGui/QGuiApplication>
#  include <QtGui/QScreen>
#endif // BROWSER_QTWEBKIT

QT_BEGIN_NAMESPACE

class HelpViewer::HelpViewerPrivate : public QObject
{
    Q_OBJECT

public:
#if defined(BROWSER_QTEXTBROWSER)
    HelpViewerPrivate(int zoom)
        : zoomCount(zoom)
        , forceFont(false)
        , lastAnchor(QString())
        , m_loadFinished(false)
    { }
#elif defined(BROWSER_QTWEBKIT)
    HelpViewerPrivate()
        : m_loadFinished(false)
    {
        // The web uses 96dpi by default on the web to preserve the font size across platforms, but
        // since we control the content for the documentation, we want the system DPI to be used.
        // - OS X reports 72dpi but doesn't allow changing the DPI, ignore anything below a 1.0 ratio to handle this.
        // - On Windows and Linux don't zoom the default web 96dpi below a 1.25 ratio to avoid
        //   filtered images in the doc unless the font readability difference is considerable.
        webDpiRatio = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.;
        if (webDpiRatio < 1.25)
            webDpiRatio = 1.0;
    }
#endif // BROWSER_QTWEBKIT

#if defined(BROWSER_QTEXTBROWSER)
    bool hasAnchorAt(QTextBrowser *browser, const QPoint& pos)
    {
        lastAnchor = browser->anchorAt(pos);
        if (lastAnchor.isEmpty())
            return false;

        lastAnchor = browser->source().resolved(lastAnchor).toString();
        if (lastAnchor.at(0) == QLatin1Char('#')) {
            QString src = browser->source().toString();
            int hsh = src.indexOf(QLatin1Char('#'));
            lastAnchor = (hsh >= 0 ? src.left(hsh) : src) + lastAnchor;
        }
        return true;
    }

    void openLink(bool newPage)
    {
        if(lastAnchor.isEmpty())
            return;
        if (newPage)
            OpenPagesManager::instance()->createPage(lastAnchor);
        else
            CentralWidget::instance()->setSource(lastAnchor);
        lastAnchor.clear();
    }

public slots:
    void openLink()
    {
        openLink(false);
    }

    void openLinkInNewPage()
    {
        openLink(true);
    }

public:
    int zoomCount;
    bool forceFont;
    QString lastAnchor;
#elif defined(BROWSER_QTWEBKIT)
    qreal webDpiRatio;
#endif // BROWSER_QTWEBKIT

public:
    bool m_loadFinished;
};

QT_END_NAMESPACE

#endif  // HELPVIEWERPRIVATE_H
