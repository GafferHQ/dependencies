/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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
****************************************************************************/

#ifndef QDECLARATIVEFOLDERLISTMODEL_H
#define QDECLARATIVEFOLDERLISTMODEL_H

#include <qdeclarative.h>
#include <QStringList>
#include <QUrl>
#include <QAbstractListModel>

#ifndef QT_NO_DIRMODEL

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Declarative)

class QDeclarativeContext;
class QModelIndex;

class QDeclarativeFolderListModelPrivate;

//![class begin]
class QDeclarativeFolderListModel : public QAbstractListModel, public QDeclarativeParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QDeclarativeParserStatus)
//![class begin]

//![class props]
    Q_PROPERTY(QUrl folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(QUrl parentFolder READ parentFolder NOTIFY folderChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters)
    Q_PROPERTY(SortField sortField READ sortField WRITE setSortField)
    Q_PROPERTY(bool sortReversed READ sortReversed WRITE setSortReversed)
    Q_PROPERTY(bool showDirs READ showDirs WRITE setShowDirs)
    Q_PROPERTY(bool showDotAndDotDot READ showDotAndDotDot WRITE setShowDotAndDotDot)
    Q_PROPERTY(bool showOnlyReadable READ showOnlyReadable WRITE setShowOnlyReadable)
    Q_PROPERTY(int count READ count)
//![class props]

//![abslistmodel]
public:
    QDeclarativeFolderListModel(QObject *parent = 0);
    ~QDeclarativeFolderListModel();

    enum Roles { FileNameRole = Qt::UserRole+1, FilePathRole = Qt::UserRole+2 };

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
//![abslistmodel]

//![count]
    int count() const { return rowCount(QModelIndex()); }
//![count]

//![prop funcs]
    QUrl folder() const;
    void setFolder(const QUrl &folder);

    QUrl parentFolder() const;

    QStringList nameFilters() const;
    void setNameFilters(const QStringList &filters);

    enum SortField { Unsorted, Name, Time, Size, Type };
    SortField sortField() const;
    void setSortField(SortField field);
    Q_ENUMS(SortField)

    bool sortReversed() const;
    void setSortReversed(bool rev);

    bool showDirs() const;
    void  setShowDirs(bool);
    bool showDotAndDotDot() const;
    void  setShowDotAndDotDot(bool);
    bool showOnlyReadable() const;
    void  setShowOnlyReadable(bool);
//![prop funcs]

//![isfolder]
    Q_INVOKABLE bool isFolder(int index) const;
//![isfolder]

//![parserstatus]
    virtual void classBegin();
    virtual void componentComplete();
//![parserstatus]

//![notifier]
Q_SIGNALS:
    void folderChanged();
//![notifier]

//![class end]
private Q_SLOTS:
    void refresh();
    void inserted(const QModelIndex &index, int start, int end);
    void removed(const QModelIndex &index, int start, int end);
    void handleDataChanged(const QModelIndex &start, const QModelIndex &end);

private:
    Q_DISABLE_COPY(QDeclarativeFolderListModel)
    QDeclarativeFolderListModelPrivate *d;
};
//![class end]

QT_END_NAMESPACE

//![qml decl]
QML_DECLARE_TYPE(QDeclarativeFolderListModel)
//![qml decl]

QT_END_HEADER

#endif // QT_NO_DIRMODEL

#endif // QDECLARATIVEFOLDERLISTMODEL_H
