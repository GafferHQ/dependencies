/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef QM3UHANDLER_H
#define QM3UHANDLER_H

#include <private/qmediaplaylistioplugin_p.h>
#include <QtCore/QObject>

QT_USE_NAMESPACE

class QM3uPlaylistPlugin : public QMediaPlaylistIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaplaylistio/5.0" FILE "m3u.json")

public:
    explicit QM3uPlaylistPlugin(QObject *parent = 0);
    virtual ~QM3uPlaylistPlugin();

    virtual bool canRead(QIODevice *device, const QByteArray &format = QByteArray() ) const;
    virtual bool canRead(const QUrl& location, const QByteArray &format = QByteArray()) const;

    virtual bool canWrite(QIODevice *device, const QByteArray &format) const;

    virtual QMediaPlaylistReader *createReader(QIODevice *device, const QByteArray &format = QByteArray());
    virtual QMediaPlaylistReader *createReader(const QUrl& location, const QByteArray &format = QByteArray());

    virtual QMediaPlaylistWriter *createWriter(QIODevice *device, const QByteArray &format);
};

#endif // QM3UHANDLER_H
