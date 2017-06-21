/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickaction_p.h"
#include "qquickexclusivegroup_p.h"
#include "qquickmenuitem_p.h"

#include <QtGui/qguiapplication.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include <private/qguiapplication_p.h>
#include <qqmlfile.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Action
    \instantiates QQuickAction
    \ingroup applicationwindow
    \ingroup controls
    \inqmlmodule QtQuick.Controls
    \brief Action provides an abstract user interface action that can be bound to items

    \image menubar-action.png

    In applications many common commands can be invoked via menus, toolbar buttons, and keyboard
    shortcuts. Since the user expects each command to be performed in the same way, regardless of
    the user interface used, it is useful to represent each command as an \e action.

    An action can be bound to a menu item and a toolbar button, and it will automatically keep them in sync.
    For example, in a word processor, if the user presses a Bold toolbar button, the Bold menu item will
    automatically be checked.

    QtQuick Controls supports actions in \l {QtQuick.Controls::}{Button}, \l ToolButton, and \l MenuItem.

    \quotefromfile gallery/main.qml
    \dots
    \skipto Action
    \printto TabView
    \dots
*/

/*!
    \qmlproperty string Action::text

    Text for the action. This text will show as the button text, or
    as title in a menu item.

    Mnemonics are supported by prefixing the shortcut letter with \&.
    For instance, \c "\&Open" will bind the \c Alt-O shortcut to the
    \c "Open" menu item. Note that not all platforms support mnemonics.

    Defaults to the empty string.
*/

/*!
    \qmlproperty url Action::iconSource

    Sets the icon file or resource url for the action. Defaults to the empty URL.
*/

/*!
    \qmlproperty string Action::iconName

    Sets the icon name for the action. This will pick the icon
    with the given name from the current theme.

    Defaults to the empty string.
*/

/*!
    \qmlproperty string Action::tooltip

    Tooltip to be shown when hovering the control bound to this action.
    Not all controls support tooltips on all platforms, especially \l MenuItem.

    Defaults to the empty string.
*/

/*!
    \qmlproperty bool Action::enabled

    Whether the action is enabled, and can be triggered. Defaults to \c true.

    \sa trigger(), triggered
*/

/*!
    \qmlproperty bool Action::checkable

     Whether the menu item can be checked, or toggled. Defaults to \c false.

     \sa checked, exclusiveGroup
*/

/*!
    \qmlproperty bool Action::checked

    If the action is \l checkable, this property reflects its checked state. Defaults to \c false.
    Its value is also false while \l checkable is false.

    \sa toggled, exclusiveGroup
*/

/*!
    \qmlproperty ExclusiveGroup Action::exclusiveGroup

    If an action is checkable, an \l ExclusiveGroup can be attached to it.
    All the actions sharing the same exclusive group become mutually exclusive
    selectable, meaning that only the last checked action will actually be checked.

    Defaults to \c null, meaning no exclusive behavior is to be expected.

    \sa checkable, checked
*/

/*!
    \qmlproperty keysequence Action::shortcut

    Shortcut bound to the action. The keysequence can be a string
    or a \l {QKeySequence::StandardKey}{standard key}.

    Defaults to an empty string.

    \qml
    Action {
        id: copyAction
        text: qsTr("&Copy")
        shortcut: StandardKey.Copy
    }
    \endqml
*/

/*! \qmlsignal Action::triggered(QObject *source)

    Emitted when either the menu item or its bound action have been activated. Includes
    the object that triggered the event if relevant (e.g. a Button or MenuItem).
    You shouldn't need to emit this signal, use \l trigger() instead.

    The corresponding handler is \c onTriggered.
*/

/*! \qmlmethod void Action::trigger(QObject *source)

    Will emit the \l triggered signal if the action is enabled. You may provide a source
    object if the Action would benefit from knowing the origin of the triggering (e.g.
    for analytics). Will also emit the \l toggled signal if it is checkable.
*/

/*! \qmlsignal Action::toggled(checked)

    Emitted whenever a action's \l checked property changes.
    This usually happens at the same time as \l triggered.

    The corresponding handler is \c onToggled.
*/

QQuickAction::QQuickAction(QObject *parent)
    : QObject(parent)
    , m_enabled(true)
    , m_checkable(false)
    , m_checked(false)
{
}

QQuickAction::~QQuickAction()
{
    setShortcut(QString());
    setMnemonicFromText(QString());
    setExclusiveGroup(0);
}

void QQuickAction::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    setMnemonicFromText(m_text);
    emit textChanged();
}

namespace {

bool qShortcutContextMatcher(QObject *o, Qt::ShortcutContext context)
{
    if (!static_cast<QQuickAction*>(o)->isEnabled())
        return false;

    switch (context) {
    case Qt::ApplicationShortcut:
        return true;
    case Qt::WindowShortcut: {
        QObject *w = o;
        while (w && !w->isWindowType()) {
            w = w->parent();
            if (QQuickItem * item = qobject_cast<QQuickItem*>(w))
                w = item->window();
        }
        if (w && w == QGuiApplication::focusWindow())
            return true;
    }
    case Qt::WidgetShortcut:
    case Qt::WidgetWithChildrenShortcut:
        break;
    }

    return false;
}

bool qMnemonicContextMatcher(QObject *o, Qt::ShortcutContext context)
{
    if (!static_cast<QQuickAction*>(o)->isEnabled())
        return false;

    switch (context) {
    case Qt::ApplicationShortcut:
        return true;
    case Qt::WindowShortcut: {
        QObject *w = o;
        while (w && !w->isWindowType()) {
            w = w->parent();
            if (QQuickItem * item = qobject_cast<QQuickItem*>(w))
                w = item->window();
            else if (QQuickMenuBase *mb = qobject_cast<QQuickMenuBase *>(w)) {
                QQuickItem *vi = mb->visualItem();
                if (vi && vi->isVisible())
                    w = vi->window();
                else
                    break; // Non visible menu objects don't get mnemonic match
            }
        }
        if (w && w == QGuiApplication::focusWindow())
            return true;
    }
    case Qt::WidgetShortcut:
    case Qt::WidgetWithChildrenShortcut:
        break;
    }

    return false;
}

} // namespace

QVariant QQuickAction::shortcut() const
{
    return m_shortcut.toString(QKeySequence::NativeText);
}

void QQuickAction::setShortcut(const QVariant &arg)
{
    QKeySequence sequence;
    if (arg.type() == QVariant::Int)
        sequence = QKeySequence(static_cast<QKeySequence::StandardKey>(arg.toInt()));
    else
        sequence = QKeySequence::fromString(arg.toString());

    if (sequence == m_shortcut)
        return;

    if (!m_shortcut.isEmpty())
        QGuiApplicationPrivate::instance()->shortcutMap.removeShortcut(0, this, m_shortcut);

    m_shortcut = sequence;

    if (!m_shortcut.isEmpty()) {
        Qt::ShortcutContext context = Qt::WindowShortcut;
        QGuiApplicationPrivate::instance()->shortcutMap.addShortcut(this, m_shortcut, context, qShortcutContextMatcher);
    }
    emit shortcutChanged(shortcut());
}

void QQuickAction::setMnemonicFromText(const QString &text)
{
    QKeySequence sequence = QKeySequence::mnemonic(text);
    if (m_mnemonic == sequence)
        return;

    if (!m_mnemonic.isEmpty())
        QGuiApplicationPrivate::instance()->shortcutMap.removeShortcut(0, this, m_mnemonic);

    m_mnemonic = sequence;

    if (!m_mnemonic.isEmpty()) {
        Qt::ShortcutContext context = Qt::WindowShortcut;
        QGuiApplicationPrivate::instance()->shortcutMap.addShortcut(this, m_mnemonic, context, qMnemonicContextMatcher);
    }
}

void QQuickAction::setIconSource(const QUrl &iconSource)
{
    if (iconSource == m_iconSource)
        return;

    m_iconSource = iconSource;
    if (m_iconName.isEmpty() || m_icon.isNull()) {
        QString fileString = QQmlFile::urlToLocalFileOrQrc(iconSource);
        m_icon = QIcon(fileString);

        emit iconChanged();
    }
    emit iconSourceChanged();
}

QString QQuickAction::iconName() const
{
    return m_iconName;
}

void QQuickAction::setIconName(const QString &iconName)
{
    if (iconName == m_iconName)
        return;
    m_iconName = iconName;
    m_icon = QIcon::fromTheme(m_iconName, QIcon(QQmlFile::urlToLocalFileOrQrc(m_iconSource)));
    emit iconNameChanged();
    emit iconChanged();
}

void QQuickAction::setTooltip(const QString &arg)
{
    if (m_tooltip != arg) {
        m_tooltip = arg;
        emit tooltipChanged(arg);
    }
}

void QQuickAction::setEnabled(bool e)
{
    if (e == m_enabled)
        return;
    m_enabled = e;

    emit enabledChanged();
}

void QQuickAction::setCheckable(bool c)
{
    if (c == m_checkable)
        return;
    m_checkable = c;
    emit checkableChanged();

    // Setting checkable to false will show checked as false, while setting checkable to
    // true will reveal checked's value again. If checked's internal value is true, we send
    // the signal to notify its visible value.
    if (m_checked)
        emit toggled(m_checkable);
}

void QQuickAction::setChecked(bool c)
{
    if (c == m_checked)
        return;
    m_checked = c;

    // Its value shows as false while checkable is false. See also comment in setCheckable().
    if (m_checkable)
        emit toggled(m_checked);
}

QQuickExclusiveGroup1 *QQuickAction::exclusiveGroup() const
{
    return m_exclusiveGroup.data();
}

void QQuickAction::setExclusiveGroup(QQuickExclusiveGroup1 *eg)
{
    if (m_exclusiveGroup == eg)
        return;

    if (m_exclusiveGroup)
        m_exclusiveGroup->unbindCheckable(this);
    m_exclusiveGroup = eg;
    if (m_exclusiveGroup)
        m_exclusiveGroup->bindCheckable(this);

    emit exclusiveGroupChanged();
}

bool QQuickAction::event(QEvent *e)
{
    if (!m_enabled)
        return false;

    if (e->type() != QEvent::Shortcut)
        return false;

    QShortcutEvent *se = static_cast<QShortcutEvent *>(e);

    Q_ASSERT_X(se->key() == m_shortcut || se->key() == m_mnemonic,
               "QQuickAction::event",
               "Received shortcut event from incorrect shortcut");
    if (se->isAmbiguous()) {
        qWarning("QQuickAction::event: Ambiguous shortcut overload: %s", se->key().toString(QKeySequence::NativeText).toLatin1().constData());
        return false;
    }

    trigger();

    return true;
}

void QQuickAction::trigger(QObject *source)
{
    if (!m_enabled)
        return;

    if (m_checkable && !(m_checked && m_exclusiveGroup))
        setChecked(!m_checked);

    emit triggered(source);
}

QT_END_NAMESPACE
