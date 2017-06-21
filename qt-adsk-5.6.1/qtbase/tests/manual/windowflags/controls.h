/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef CONTROLS_H
#define CONTROLS_H

#include <QGroupBox>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QRadioButton;
class QButtonGroup;
QT_END_NAMESPACE

enum { ControlLayoutMargin = 4 };

// Control for the hint part of Qt::WindowFlags
class HintControl : public QGroupBox
{
    Q_OBJECT
public:
    explicit HintControl(QWidget *parent= 0);

    Qt::WindowFlags hints() const;
    void setHints(Qt::WindowFlags hints);

signals:
    void changed(Qt::WindowFlags);

private slots:
    void slotCheckBoxChanged();

private:
    QCheckBox *msWindowsFixedSizeDialogCheckBox;
    QCheckBox *x11BypassWindowManagerCheckBox;
    QCheckBox *framelessWindowCheckBox;
    QCheckBox *windowTitleCheckBox;
    QCheckBox *windowSystemMenuCheckBox;
    QCheckBox *windowMinimizeButtonCheckBox;
    QCheckBox *windowMaximizeButtonCheckBox;
    QCheckBox *windowFullscreenButtonCheckBox;
    QCheckBox *windowCloseButtonCheckBox;
    QCheckBox *windowContextHelpButtonCheckBox;
    QCheckBox *windowShadeButtonCheckBox;
    QCheckBox *windowStaysOnTopCheckBox;
    QCheckBox *windowStaysOnBottomCheckBox;
    QCheckBox *customizeWindowHintCheckBox;
    QCheckBox *transparentForInputCheckBox;
};

// Control for the Qt::WindowState enum, optional with a "visible" QCheckbox
class WindowStateControl : public QWidget {
    Q_OBJECT
public:
    enum Flags {
        WantVisibleCheckBox = 0x1,
        WantMinimizeRadioButton = 0x2
    };

    explicit WindowStateControl(unsigned flags, QWidget *parent= 0);

    Qt::WindowState state() const;
    void setState(Qt::WindowState s);

    bool visibleValue() const;
    void setVisibleValue(bool);

signals:
    void changed();

private:
    QButtonGroup *group;
    QCheckBox    *visibleCheckBox;
    QRadioButton *restoreButton;
    QRadioButton *minimizeButton;
    QRadioButton *maximizeButton;
    QRadioButton *fullscreenButton;
};

// Control for the Qt::WindowStates flags (normal, maximized, fullscreen exclusively
// combined with minimized and optionally, with a "visible" QCheckbox)
class WindowStatesControl : public QGroupBox
{
    Q_OBJECT
public:
    enum Flags {
        WantVisibleCheckBox = 0x1,
        WantActiveCheckBox = 0x2
    };

    explicit WindowStatesControl(unsigned flags, QWidget *parent= 0);

    Qt::WindowStates states() const;
    void setStates(Qt::WindowStates s);

    bool visibleValue() const;
    void setVisibleValue(bool);
    bool activeValue() const;
    void setActiveValue(bool v);

signals:
    void changed();

private:
    QCheckBox *visibleCheckBox;
    QCheckBox *activeCheckBox;
    QCheckBox *minimizeCheckBox;
    WindowStateControl *stateControl;
};

// Control for the type part of Qt::WindowFlags
class TypeControl : public QGroupBox
{
    Q_OBJECT
public:
    explicit TypeControl(QWidget *parent= 0);

    Qt::WindowFlags type() const;
    void setType(Qt::WindowFlags);

signals:
    void changed(Qt::WindowFlags);

private slots:
    void slotChanged();

private:
    QButtonGroup *group;
    QRadioButton *windowRadioButton;
    QRadioButton *dialogRadioButton;
    QRadioButton *sheetRadioButton;
    QRadioButton *drawerRadioButton;
    QRadioButton *popupRadioButton;
    QRadioButton *toolRadioButton;
    QRadioButton *toolTipRadioButton;
    QRadioButton *splashScreenRadioButton;
};

#endif // CONTROLS_H
