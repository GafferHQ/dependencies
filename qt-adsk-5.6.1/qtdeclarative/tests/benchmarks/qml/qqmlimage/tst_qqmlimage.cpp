/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QQmlEngine>
#include <QQmlComponent>
#include <private/qquickimage_p.h>

class tst_qmlgraphicsimage : public QObject
{
    Q_OBJECT
public:
    tst_qmlgraphicsimage() {}

private slots:
    void qmlgraphicsimage();
    void qmlgraphicsimage_file();
    void qmlgraphicsimage_url();

private:
    QQmlEngine engine;
};

void tst_qmlgraphicsimage::qmlgraphicsimage()
{
    int x = 0;
    QUrl url(SRCDIR "/image.png");
    QBENCHMARK {
        QUrl url2("http://localhost/image" + QString::number(x++) + ".png");
        QQuickImage *image = new QQuickImage;
        QQmlEngine::setContextForObject(image, engine.rootContext());
        delete image;
    }
}

void tst_qmlgraphicsimage::qmlgraphicsimage_file()
{
    int x = 0;
    QUrl url(SRCDIR "/image.png");
    //get rid of initialization effects
    {
        QQuickImage *image = new QQuickImage;
        QQmlEngine::setContextForObject(image, engine.rootContext());
        image->setSource(url);
    }
    QBENCHMARK {
        QUrl url2("http://localhost/image" + QString::number(x++) + ".png");
        QQuickImage *image = new QQuickImage;
        QQmlEngine::setContextForObject(image, engine.rootContext());
        image->setSource(url);
        delete image;
    }
}

void tst_qmlgraphicsimage::qmlgraphicsimage_url()
{
    int x = 0;
    QUrl url(SRCDIR "/image.png");
    QBENCHMARK {
        QUrl url2("http://localhost/image" + QString::number(x++) + ".png");
        QQuickImage *image = new QQuickImage;
        QQmlEngine::setContextForObject(image, engine.rootContext());
        image->setSource(url2);
        delete image;
    }
}

QTEST_MAIN(tst_qmlgraphicsimage)

#include "tst_qqmlimage.moc"
