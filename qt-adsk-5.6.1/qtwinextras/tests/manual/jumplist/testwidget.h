/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
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

#ifndef TESTWIDGET_H
#define TESTWIDGET_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class TestWidget;
}
QT_END_NAMESPACE

#if defined(QT_NAMESPACE)
namespace Ui = QT_NAMESPACE::Ui;
#endif

class TestWidget : public QMainWindow
{
    Q_OBJECT

public:
    explicit TestWidget(QWidget *parent = Q_NULLPTR);
    ~TestWidget();

    void showFile(const QString &path);
    void setText(const QString &text);

    static QStringList supportedMimeTypes();

    QString id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }

protected:
    void changeEvent(QEvent *e) Q_DECL_OVERRIDE;

private slots:
    void updateJumpList();
    void showInExplorer();
    void runJumpListView();
    void openFile();

private:
    Ui::TestWidget *ui;
    QString m_id;
};

#endif // TESTWIDGET_H
