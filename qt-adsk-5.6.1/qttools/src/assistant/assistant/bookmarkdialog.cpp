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
#include "bookmarkdialog.h"
#include "bookmarkfiltermodel.h"
#include "bookmarkitem.h"
#include "bookmarkmodel.h"
#include "helpenginewrapper.h"
#include "tracer.h"

#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>

QT_BEGIN_NAMESPACE

BookmarkDialog::BookmarkDialog(BookmarkModel *sourceModel, const QString &title,
        const QString &url, QWidget *parent)
    : QDialog(parent)
    , m_url(url)
    , m_title(title)
    , bookmarkModel(sourceModel)
{
    TRACE_OBJ
    ui.setupUi(this);

    ui.bookmarkEdit->setText(m_title);
    ui.newFolderButton->setVisible(false);
    ui.buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

    connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accepted()));
    connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(rejected()));
    connect(ui.newFolderButton, SIGNAL(clicked()), this, SLOT(addFolder()));
    connect(ui.toolButton, SIGNAL(clicked()), this, SLOT(toolButtonClicked()));
    connect(ui.bookmarkEdit, SIGNAL(textChanged(QString)), this,
        SLOT(textChanged(QString)));

    bookmarkProxyModel = new BookmarkFilterModel(this);
    bookmarkProxyModel->setSourceModel(bookmarkModel);
    ui.bookmarkFolders->setModel(bookmarkProxyModel);
    connect(ui.bookmarkFolders, SIGNAL(currentIndexChanged(int)), this,
        SLOT(currentIndexChanged(int)));

    bookmarkTreeModel = new BookmarkTreeModel(this);
    bookmarkTreeModel->setSourceModel(bookmarkModel);
    ui.treeView->setModel(bookmarkTreeModel);

    ui.treeView->expandAll();
    ui.treeView->setVisible(false);
    ui.treeView->installEventFilter(this);
    ui.treeView->viewport()->installEventFilter(this);
    ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.treeView, SIGNAL(customContextMenuRequested(QPoint)), this,
        SLOT(customContextMenuRequested(QPoint)));
    connect(ui.treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,
        QModelIndex)), this, SLOT(currentIndexChanged(QModelIndex)));

    ui.bookmarkFolders->setCurrentIndex(ui.bookmarkFolders->count() > 1 ? 1 : 0);

    const HelpEngineWrapper &helpEngine = HelpEngineWrapper::instance();
    if (helpEngine.usesAppFont())
        setFont(helpEngine.appFont());
}

BookmarkDialog::~BookmarkDialog()
{
    TRACE_OBJ
}

bool BookmarkDialog::isRootItem(const QModelIndex &index) const
{
    return !bookmarkTreeModel->parent(index).isValid();
}

bool BookmarkDialog::eventFilter(QObject *object, QEvent *event)
{
    TRACE_OBJ
    if (object != ui.treeView && object != ui.treeView->viewport())
        return QWidget::eventFilter(object, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
            case Qt::Key_F2: {
                const QModelIndex &index = ui.treeView->currentIndex();
                if (!isRootItem(index)) {
                    bookmarkModel->setItemsEditable(true);
                    ui.treeView->edit(index);
                    bookmarkModel->setItemsEditable(false);
                }
            }   break;
            default: break;
        }
    }

    return QObject::eventFilter(object, event);
}

void BookmarkDialog::currentIndexChanged(int row)
{
    TRACE_OBJ
    QModelIndex next = bookmarkProxyModel->index(row, 0, QModelIndex());
    if (next.isValid()) {
        next = bookmarkProxyModel->mapToSource(next);
        ui.treeView->setCurrentIndex(bookmarkTreeModel->mapFromSource(next));
    }
}

void BookmarkDialog::currentIndexChanged(const QModelIndex &index)
{
    TRACE_OBJ
    const QModelIndex current = bookmarkTreeModel->mapToSource(index);
    if (current.isValid()) {
        const int row = bookmarkProxyModel->mapFromSource(current).row();
        ui.bookmarkFolders->setCurrentIndex(row);
    }
}

void BookmarkDialog::accepted()
{
    TRACE_OBJ
    QModelIndex index = ui.treeView->currentIndex();
    if (index.isValid()) {
        index = bookmarkModel->addItem(bookmarkTreeModel->mapToSource(index));
        bookmarkModel->setData(index, DataVector() << m_title << m_url << false);
    } else
        rejected();

    accept();
}

void BookmarkDialog::rejected()
{
    TRACE_OBJ
    foreach (const QPersistentModelIndex &index, cache)
        bookmarkModel->removeItem(index);
    reject();
}

void BookmarkDialog::addFolder()
{
    TRACE_OBJ
    QModelIndex index = ui.treeView->currentIndex();
    if (index.isValid()) {
        index = bookmarkModel->addItem(bookmarkTreeModel->mapToSource(index),
            true);
        cache.append(index);

        index = bookmarkTreeModel->mapFromSource(index);
        if (index.isValid()) {
            bookmarkModel->setItemsEditable(true);
            ui.treeView->edit(index);
            ui.treeView->expand(index);
            ui.treeView->setCurrentIndex(index);
            bookmarkModel->setItemsEditable(false);
        }
    }
}

void BookmarkDialog::toolButtonClicked()
{
    TRACE_OBJ
    const bool visible = !ui.treeView->isVisible();
    ui.treeView->setVisible(visible);
    ui.newFolderButton->setVisible(visible);

    if (visible) {
        resize(QSize(width(), 400));
        ui.toolButton->setText(QLatin1String("-"));
    } else {
        resize(width(), minimumHeight());
        ui.toolButton->setText(QLatin1String("+"));
    }
}

void BookmarkDialog::textChanged(const QString& text)
{
    m_title = text;
}

void BookmarkDialog::customContextMenuRequested(const QPoint &point)
{
    TRACE_OBJ
    const QModelIndex &index = ui.treeView->currentIndex();
    if (isRootItem(index))
        return; // check if we go to rename the "Bookmarks Menu", bail

    QMenu menu(QLatin1String(""), this);
    QAction *renameItem = menu.addAction(tr("Rename Folder"));

    QAction *picked = menu.exec(ui.treeView->mapToGlobal(point));
    if (picked == renameItem) {
        bookmarkModel->setItemsEditable(true);
        ui.treeView->edit(index);
        bookmarkModel->setItemsEditable(false);
    }
}

QT_END_NAMESPACE
