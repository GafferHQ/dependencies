/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "imagemodel.h"
#include "qabstractitemmodel.h"
#include <qjsonvalue.h>
#include <qjsonobject.h>
#include <qvariant.h>
#include <qicon.h>
#include <QtCore/qdatetime.h>

ImageModel::ImageModel(QObject *parent)
    : EnginioModel(parent)
{
    connect(this, SIGNAL(modelReset()), this, SLOT(reset()));
    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(updateRows(QModelIndex,int,int)));

    connect(this, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            this, SLOT(onDataChanged(QModelIndex,QModelIndex)));
}

void ImageModel::reset()
{
    updateRows(QModelIndex(), 0, rowCount() - 1);
}

void ImageModel::updateRows(const QModelIndex &, int start, int end)
{
    for (int row = start; row <= end; ++row) {
        QJsonValue rowData = EnginioModel::data(index(row)).value<QJsonValue>();
        QString id = rowData.toObject().value("id").toString();
        if (id.isEmpty() || m_images.contains(id))
            continue;
        ImageObject *image = new ImageObject(client());
        connect(image, SIGNAL(imageChanged(QString)), this, SLOT(imageChanged(QString)));
        m_images.insert(id, image);
        image->setObject(rowData.toObject());
        QModelIndex changedIndex = index(row);
        emit dataChanged(changedIndex, changedIndex);
    }
}

void ImageModel::imageChanged(const QString &id)
{
    for (int row = 0; row < rowCount(); ++row) {
        if (data(index(row), Enginio::IdRole).toString() == id) {
            QModelIndex changedIndex = index(row);
            emit dataChanged(changedIndex, changedIndex);
        }
    }
}

void ImageModel::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    int start = topLeft.row();
    int end = bottomRight.row();

    updateRows(QModelIndex(), start, end);
}

QVariant ImageModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Qt::DecorationRole: {
        QJsonObject rowData = EnginioModel::data(index).value<QJsonValue>().toObject();
        QString id = rowData.value("id").toString();
        if (m_images.contains(id))
            return m_images.value(id)->thumbnail();
        return QVariant();
    }
    case Qt::SizeHintRole: {
        return QVariant(QSize(100, 100));
    }
    }

    return EnginioModel::data(index, role);
}

Qt::ItemFlags ImageModel::flags(const QModelIndex &index) const
{
    return QAbstractListModel::flags(index) & ~Qt::ItemIsSelectable;
}
