/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include <QtGui>
#include <QtDeclarative/QtDeclarative>
#include <QtUiTools/QtUiTools>

#include "accessibilityinspector.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (app.arguments().count() < 2) {
        qDebug() << "Usage: accessebilityInspector [ ui-file | qml-file ] [Option]";
        qDebug() << "Option:";
#ifdef QT_ACCESSIBILITY_INSPECTOR_SCENE_GRAPH
        qDebug() << "-qtquick1: Use QDeclarativeView instead of QSGView for rendering QML files";
#endif
        return 0;
    }

    QString fileName = app.arguments().at(1);
    QString mode;
    if (app.arguments().count() > 2) {
        mode = app.arguments().at(2);
    }

    QWidget *window;

    if (fileName.endsWith(".ui")) {
        QUiLoader loader;
        QFile file(fileName);
        file.open(QFile::ReadOnly);
        window = loader.load(&file, 0);
    } else if (fileName.endsWith(".qml")){
        QUrl fileUrl;
        if (fileName.startsWith(":")) { // detect resources.
            QString name = fileName;
            name.remove(0, 2); // reomve ":/"
            fileUrl.setUrl(QLatin1String("qrc:/") + name);
        } else {
            fileUrl = QUrl::fromLocalFile(fileName);
        }

#ifdef QT_ACCESSIBILITY_INSPECTOR_SCENE_GRAPH
        if (mode == QLatin1String("-qtquick1"))
#endif
        {
            QDeclarativeView * declarativeView = new QDeclarativeView();
            declarativeView->setSource(fileUrl);
            window = declarativeView;
        }
#ifdef QT_ACCESSIBILITY_INSPECTOR_SCENE_GRAPH
        else {
            QSGView * sceneGraphView = new QSGView();
            sceneGraphView->setSource(fileUrl);
            window = sceneGraphView;
        }
#endif
    } else {
        qDebug() << "Error: don't know what to do with" << fileName;
    }

    AccessibilityInspector *accessibilityInspector = new AccessibilityInspector();

    accessibilityInspector->inspectWindow(window);

    window->move(50, 50);
    window->show();

    int ret = app.exec();

    accessibilityInspector->saveWindowGeometry();
    delete accessibilityInspector;

    return ret;


}

