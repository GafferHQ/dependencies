/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
***************************************************************************/

#include <QtGui>
#include "mediaplayer.h"

const qreal DefaultVolume = -1.0;

int main (int argc, char *argv[])
{
    Q_INIT_RESOURCE(mediaplayer);
    QApplication app(argc, argv);
    app.setApplicationName("Media Player");
    app.setOrganizationName("Qt");
    app.setQuitOnLastWindowClosed(true);

    QString fileName;
    qreal volume = DefaultVolume;
    bool smallScreen = false;
#ifdef Q_OS_SYMBIAN
    smallScreen = true;
#endif

    QStringList args(app.arguments());
    args.removeFirst(); // remove name of executable
    while (!args.empty()) {
        const QString &arg = args.first();
        if (QLatin1String("-small-screen") == arg || QLatin1String("--small-screen") == arg) {
            smallScreen = true;
        } else if (QLatin1String("-volume") == arg || QLatin1String("--volume") == arg) {
            if (!args.empty()) {
                args.removeFirst();
                volume = qMax(qMin(args.first().toFloat(), float(1.0)), float(0.0));
            }
        } else if (fileName.isNull()) {
            fileName = arg;
        }
        args.removeFirst();
    }

    MediaPlayer player;
    player.setSmallScreen(smallScreen);
    if (DefaultVolume != volume)
        player.setVolume(volume);
    if (!fileName.isNull())
        player.setFile(fileName);

    if (smallScreen)
        player.showMaximized();
    else
        player.show();

    return app.exec();
}

