/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include <qglobal.h>

// ### Qt 5: eliminate this file

/*
    This is evil. MSVC doesn't let us remove private symbols, nor change their
    visibility; yet there are some symbols we really needed to make public, e.g.,
    ~QColorDialog(), and then there were some totally needless symbols in our
    header files, e.g., setSelectedAlpha(). So we define a new version of
    QColorDialog & Co. with only the private symbols that we removed from the
    public header files. The friends are there only to prevent potential compiler
    warnings.

    It would have been nicer to export the missing symbols as mangled "C" symbols
    instead but unfortunately MSVC uses out-of-reach characters like @ and . in
    their mangled C++ symbols.
*/

#if QT_VERSION < 0x050000 && defined(Q_CC_MSVC)

QT_BEGIN_NAMESPACE

#include <QtGui/QColor>
#include <QtGui/QFont>

class QColorDialogPrivate;
class QFontDialogPrivate;
class QInputDialogPrivate;
class QWidget;

class Q_GUI_EXPORT QColorDialog
{
private:
    explicit QColorDialog(QWidget *, bool);
    ~QColorDialog();

    void setColor(const QColor &);
    QColor color() const;
    bool selectColor(const QColor &);
    void setSelectedAlpha(int);
    int selectedAlpha() const;

    friend class QColorDialogPrivate;
};

QColorDialog::QColorDialog(QWidget *, bool) {}
QColorDialog::~QColorDialog() {}
void QColorDialog::setColor(const QColor &) {}
QColor QColorDialog::color() const { return QColor(); }
bool QColorDialog::selectColor(const QColor &) { return false; }
void QColorDialog::setSelectedAlpha(int) {}
int QColorDialog::selectedAlpha() const { return 0; }

class Q_GUI_EXPORT QFontDialog
{
private:
    explicit QFontDialog(QWidget *, bool, Qt::WindowFlags);
    ~QFontDialog();

    QFont font() const;
    void setFont(const QFont &);
    void updateFamilies();
    void updateStyles();
    void updateSizes();

    static QFont getFont(bool *, const QFont *, QWidget *);

    friend class QFontDialogPrivate;
};

QFontDialog::QFontDialog(QWidget *, bool, Qt::WindowFlags) {}
QFontDialog::~QFontDialog() {}
QFont QFontDialog::font() const { return QFont(); }
void QFontDialog::setFont(const QFont &) { }
void QFontDialog::updateFamilies() {}
void QFontDialog::updateStyles() {}
void QFontDialog::updateSizes() {}
QFont QFontDialog::getFont(bool *, const QFont *, QWidget *) { return QFont(); }

class Q_GUI_EXPORT QInputDialog
{
private:
    enum Type { LineEdit, SpinBox, DoubleSpinBox, ComboBox, EditableComboBox };

    QInputDialog(const QString &, QWidget *, Type, Qt::WindowFlags);
    QInputDialog(const QString &, const QString &, QWidget *, QWidget *, Qt::WindowFlags);
    ~QInputDialog();
};

QInputDialog::QInputDialog(const QString &, QWidget *, Type, Qt::WindowFlags) {}
QInputDialog::QInputDialog(const QString &, const QString &, QWidget *, QWidget *, Qt::WindowFlags) {}
QInputDialog::~QInputDialog() {}

QT_END_NAMESPACE

#endif
