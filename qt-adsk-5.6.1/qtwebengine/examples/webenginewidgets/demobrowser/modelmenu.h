/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef MODELMENU_H
#define MODELMENU_H

#include <QtWidgets/QMenu>
#include <QtCore/QAbstractItemModel>

// A QMenu that is dynamically populated from a QAbstractItemModel
class ModelMenu : public QMenu
{
    Q_OBJECT

signals:
    void activated(const QModelIndex &index);
    void hovered(const QString &text);

public:
    ModelMenu(QWidget *parent = 0);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void setMaxRows(int max);
    int maxRows() const;

    void setFirstSeparator(int offset);
    int firstSeparator() const;

    void setRootIndex(const QModelIndex &index);
    QModelIndex rootIndex() const;

    void setHoverRole(int role);
    int hoverRole() const;

    void setSeparatorRole(int role);
    int separatorRole() const;

    QAction *makeAction(const QIcon &icon, const QString &text, QObject *parent);

protected:
    // add any actions before the tree, return true if any actions are added.
    virtual bool prePopulated();
    // add any actions after the tree
    virtual void postPopulated();
    // put all of the children of parent into menu up to max
    void createMenu(const QModelIndex &parent, int max, QMenu *parentMenu = 0, QMenu *menu = 0);

private slots:
    void aboutToShow();
    void triggered(QAction *action);
    void hovered(QAction *action);

private:
    QAction *makeAction(const QModelIndex &index);
    int m_maxRows;
    int m_firstSeparator;
    int m_maxWidth;
    int m_hoverRole;
    int m_separatorRole;
    QAbstractItemModel *m_model;
    QPersistentModelIndex m_root;
};

#endif // MODELMENU_H
