/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#ifndef MESSAGEEDITORWIDGETS_H
#define MESSAGEEDITORWIDGETS_H

#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMap>
#include <QTextEdit>
#include <QUrl>
#include <QWidget>

QT_BEGIN_NAMESPACE

class QAbstractButton;
class QAction;
class QContextMenuEvent;
class QKeyEvent;
class QMenu;
class QSizeF;
class QString;
class QVariant;

class MessageHighlighter;

/*
  Automatically adapt height to document contents
 */
class ExpandingTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    ExpandingTextEdit(QWidget *parent = 0);
    QSize sizeHint() const;
    QSize minimumSizeHint() const;

private slots:
    void updateHeight(const QSizeF &documentSize);
    void reallyEnsureCursorVisible();

private:
    int m_minimumHeight;
};

/*
  Format markup & control characters
*/
class FormatTextEdit : public ExpandingTextEdit
{
    Q_OBJECT
public:
    FormatTextEdit(QWidget *parent = 0);
    ~FormatTextEdit();
    void setEditable(bool editable);

signals:
    void editorDestroyed();

public slots:
    void setPlainText(const QString & text, bool userAction);
    void setVisualizeWhitespace(bool value);

private:
    MessageHighlighter *m_highlighter;
};

/*
  Displays text field & associated label
*/
class FormWidget : public QWidget
{
    Q_OBJECT
public:
    FormWidget(const QString &label, bool isEditable, QWidget *parent = 0);
    void setLabel(const QString &label) { m_label->setText(label); }
    void setTranslation(const QString &text, bool userAction = false);
    void clearTranslation() { setTranslation(QString(), false); }
    QString getTranslation() { return m_editor->toPlainText(); }
    void setEditingEnabled(bool enable);
    void setHideWhenEmpty(bool optional) { m_hideWhenEmpty = optional; }
    FormatTextEdit *getEditor() { return m_editor; }

signals:
    void textChanged(QTextEdit *);
    void selectionChanged(QTextEdit *);
    void cursorPositionChanged();

private slots:
    void slotSelectionChanged();
    void slotTextChanged();

private:
    QLabel *m_label;
    FormatTextEdit *m_editor;
    bool m_hideWhenEmpty;
};

/*
  Displays text fields & associated label
*/
class FormMultiWidget : public QWidget
{
    Q_OBJECT
public:
    FormMultiWidget(const QString &label, QWidget *parent = 0);
    void setLabel(const QString &label) { m_label->setText(label); }
    void setTranslation(const QString &text, bool userAction = false);
    void clearTranslation() { setTranslation(QString(), false); }
    QString getTranslation() const;
    void setEditingEnabled(bool enable);
    void setMultiEnabled(bool enable);
    void setHideWhenEmpty(bool optional) { m_hideWhenEmpty = optional; }
    const QList<FormatTextEdit *> &getEditors() const { return m_editors; }

signals:
    void editorCreated(QTextEdit *);
    void textChanged(QTextEdit *);
    void selectionChanged(QTextEdit *);
    void cursorPositionChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void slotTextChanged();
    void slotSelectionChanged();
    void minusButtonClicked();
    void plusButtonClicked();

private:
    void addEditor(int idx);
    void updateLayout();
    QAbstractButton *makeButton(const QIcon &icon, const char *slot);
    void insertEditor(int idx);
    void deleteEditor(int idx);

    QLabel *m_label;
    QList<FormatTextEdit *> m_editors;
    QList<QWidget *> m_plusButtons;
    QList<QAbstractButton *> m_minusButtons;
    bool m_hideWhenEmpty;
    bool m_multiEnabled;
    QIcon m_plusIcon, m_minusIcon;
};

QT_END_NAMESPACE

#endif // MESSAGEEDITORWIDGETS_H
