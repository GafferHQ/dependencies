/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef ABSTRACTFINDWIDGET_H
#define ABSTRACTFINDWIDGET_H

#include <QtGui/QIcon>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class QCheckBox;
class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QObject;
class QToolButton;

class AbstractFindWidget : public QWidget
{
    Q_OBJECT

public:
    enum FindFlag {
        /// Use a layout that is roughly half as wide and twice as high as the regular one.
        NarrowLayout = 1,
        /// Do not show the "Whole words" checkbox.
        NoWholeWords = 2,
        /// Do not show the "Case sensitive" checkbox.
        NoCaseSensitive = 4
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    explicit AbstractFindWidget(FindFlags flags = FindFlags(), QWidget *parent = 0);
    virtual ~AbstractFindWidget();

    bool eventFilter(QObject *object, QEvent *e);

    static QIcon findIconSet();

public slots:
    void activate();
    virtual void deactivate();
    void findNext();
    void findPrevious();
    void findCurrentText();

protected:
    void keyPressEvent(QKeyEvent *event);

private slots:
    void updateButtons();

protected:
    virtual void find(const QString &textToFind, bool skipCurrent, bool backward, bool *found, bool *wrapped) = 0;

    bool caseSensitive() const;
    bool wholeWords() const;

private:
    void findInternal(const QString &textToFind, bool skipCurrent, bool backward);

    QLineEdit *m_editFind;
    QLabel *m_labelWrapped;
    QToolButton *m_toolNext;
    QToolButton *m_toolClose;
    QToolButton *m_toolPrevious;
    QCheckBox *m_checkCase;
    QCheckBox *m_checkWholeWords;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractFindWidget::FindFlags)

QT_END_NAMESPACE

#endif  // ABSTRACTFINDWIDGET_H
