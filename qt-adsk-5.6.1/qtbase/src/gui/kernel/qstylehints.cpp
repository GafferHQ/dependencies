/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include <qstylehints.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformtheme.h>
#include <private/qguiapplication_p.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

static inline QVariant hint(QPlatformIntegration::StyleHint h)
{
    return QGuiApplicationPrivate::platformIntegration()->styleHint(h);
}

static inline QVariant themeableHint(QPlatformTheme::ThemeHint th,
                                     QPlatformIntegration::StyleHint ih)
{
    if (!QCoreApplication::instance()) {
        qWarning() << "Must construct a QGuiApplication before accessing a platform theme hint.";
        return QVariant();
    }
    if (const QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme()) {
        const QVariant themeHint = theme->themeHint(th);
        if (themeHint.isValid())
            return themeHint;
    }
    return QGuiApplicationPrivate::platformIntegration()->styleHint(ih);
}

class QStyleHintsPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QStyleHints)
public:
    inline QStyleHintsPrivate()
        : m_mouseDoubleClickInterval(-1)
        , m_startDragDistance(-1)
        , m_startDragTime(-1)
        , m_keyboardInputInterval(-1)
        , m_cursorFlashTime(-1)
        {}

    int m_mouseDoubleClickInterval;
    int m_startDragDistance;
    int m_startDragTime;
    int m_keyboardInputInterval;
    int m_cursorFlashTime;
};

/*!
    \class QStyleHints
    \since 5.0
    \brief The QStyleHints class contains platform specific hints and settings.
    \inmodule QtGui

    An object of this class, obtained from QGuiApplication, provides access to certain global
    user interface parameters of the current platform.

    Access is read only; typically the platform itself provides the user a way to tune these
    parameters.

    Access to these parameters are useful when implementing custom user interface components, in that
    they allow the components to exhibit the same behaviour and feel as other components.

    \sa QGuiApplication::styleHints()
 */
QStyleHints::QStyleHints()
    : QObject(*new QStyleHintsPrivate(), 0)
{
}

/*!
    Sets the \a mouseDoubleClickInterval.
    \internal
    \sa mouseDoubleClickInterval()
    \since 5.3
*/
void  QStyleHints::setMouseDoubleClickInterval(int mouseDoubleClickInterval)
{
    Q_D(QStyleHints);
    if (d->m_mouseDoubleClickInterval == mouseDoubleClickInterval)
        return;
    d->m_mouseDoubleClickInterval = mouseDoubleClickInterval;
    emit mouseDoubleClickIntervalChanged(mouseDoubleClickInterval);
}

/*!
    \property QStyleHints::mouseDoubleClickInterval
    \brief the time limit in milliseconds that distinguishes a double click
    from two consecutive mouse clicks.
*/
int QStyleHints::mouseDoubleClickInterval() const
{
    Q_D(const QStyleHints);
    return d->m_mouseDoubleClickInterval >= 0 ?
           d->m_mouseDoubleClickInterval :
           themeableHint(QPlatformTheme::MouseDoubleClickInterval, QPlatformIntegration::MouseDoubleClickInterval).toInt();
}

/*!
    \property QStyleHints::mousePressAndHoldInterval
    \brief the time limit in milliseconds that activates
    a press and hold.

    \since 5.3
*/
int QStyleHints::mousePressAndHoldInterval() const
{
    return themeableHint(QPlatformTheme::MousePressAndHoldInterval, QPlatformIntegration::MousePressAndHoldInterval).toInt();
}

/*!
    Sets the \a startDragDistance.
    \internal
    \sa startDragDistance()
    \since 5.3
*/
void QStyleHints::setStartDragDistance(int startDragDistance)
{
    Q_D(QStyleHints);
    if (d->m_startDragDistance == startDragDistance)
        return;
    d->m_startDragDistance = startDragDistance;
    emit startDragDistanceChanged(startDragDistance);
}

/*!
    \property QStyleHints::startDragDistance
    \brief the distance, in pixels, that the mouse must be moved with a button
    held down before a drag and drop operation will begin.

    If you support drag and drop in your application, and want to start a drag
    and drop operation after the user has moved the cursor a certain distance
    with a button held down, you should use this property's value as the
    minimum distance required.

    For example, if the mouse position of the click is stored in \c startPos
    and the current position (e.g. in the mouse move event) is \c currentPos,
    you can find out if a drag should be started with code like this:

    \snippet code/src_gui_kernel_qapplication.cpp 6

    \sa startDragTime, QPoint::manhattanLength(), {Drag and Drop}
*/
int QStyleHints::startDragDistance() const
{
    Q_D(const QStyleHints);
    return d->m_startDragDistance >= 0 ?
           d->m_startDragDistance :
           themeableHint(QPlatformTheme::StartDragDistance, QPlatformIntegration::StartDragDistance).toInt();
}

/*!
    Sets the \a startDragDragTime.
    \internal
    \sa startDragTime()
    \since 5.3
*/
void QStyleHints::setStartDragTime(int startDragTime)
{
    Q_D(QStyleHints);
    if (d->m_startDragTime == startDragTime)
        return;
    d->m_startDragTime = startDragTime;
    emit startDragTimeChanged(startDragTime);
}

/*!
    \property QStyleHints::startDragTime
    \brief the time, in milliseconds, that a mouse button must be held down
    before a drag and drop operation will begin.

    If you support drag and drop in your application, and want to start a drag
    and drop operation after the user has held down a mouse button for a
    certain amount of time, you should use this property's value as the delay.

    \sa startDragDistance, {Drag and Drop}
*/
int QStyleHints::startDragTime() const
{
    Q_D(const QStyleHints);
    return d->m_startDragTime >= 0 ?
           d->m_startDragTime :
           themeableHint(QPlatformTheme::StartDragTime, QPlatformIntegration::StartDragTime).toInt();
}

/*!
    \property QStyleHints::startDragVelocity
    \brief the limit for the velocity, in pixels per second, that the mouse may
    be moved, with a button held down, for a drag and drop operation to begin.
    A value of 0 means there is no such limit.

    \sa startDragDistance, {Drag and Drop}
*/
int QStyleHints::startDragVelocity() const
{
    return themeableHint(QPlatformTheme::StartDragVelocity, QPlatformIntegration::StartDragVelocity).toInt();
}

/*!
    Sets the \a keyboardInputInterval.
    \internal
    \sa keyboardInputInterval()
    \since 5.3
*/
void QStyleHints::setKeyboardInputInterval(int keyboardInputInterval)
{
    Q_D(QStyleHints);
    if (d->m_keyboardInputInterval == keyboardInputInterval)
        return;
    d->m_keyboardInputInterval = keyboardInputInterval;
    emit keyboardInputIntervalChanged(keyboardInputInterval);
}

/*!
    \property QStyleHints::keyboardInputInterval
    \brief the time limit, in milliseconds, that distinguishes a key press
    from two consecutive key presses.
*/
int QStyleHints::keyboardInputInterval() const
{
    Q_D(const QStyleHints);
    return d->m_keyboardInputInterval >= 0 ?
           d->m_keyboardInputInterval :
           themeableHint(QPlatformTheme::KeyboardInputInterval, QPlatformIntegration::KeyboardInputInterval).toInt();
}

/*!
    \property QStyleHints::keyboardAutoRepeatRate
    \brief the rate, in events per second,  in which additional repeated key
    presses will automatically be generated if a key is being held down.
*/
int QStyleHints::keyboardAutoRepeatRate() const
{
    return themeableHint(QPlatformTheme::KeyboardAutoRepeatRate, QPlatformIntegration::KeyboardAutoRepeatRate).toInt();
}

/*!
    Sets the \a cursorFlashTime.
    \internal
    \sa cursorFlashTime()
    \since 5.3
*/
void QStyleHints::setCursorFlashTime(int cursorFlashTime)
{
    Q_D(QStyleHints);
    if (d->m_cursorFlashTime == cursorFlashTime)
        return;
    d->m_cursorFlashTime = cursorFlashTime;
    emit cursorFlashTimeChanged(cursorFlashTime);
}

/*!
    \property QStyleHints::cursorFlashTime
    \brief the text cursor's flash (blink) time in milliseconds.

    The flash time is the time used to display, invert and restore the
    caret display. Usually the text cursor is displayed for half the cursor
    flash time, then hidden for the same amount of time.
*/
int QStyleHints::cursorFlashTime() const
{
    Q_D(const QStyleHints);
    return d->m_cursorFlashTime >= 0 ?
           d->m_cursorFlashTime :
           themeableHint(QPlatformTheme::CursorFlashTime, QPlatformIntegration::CursorFlashTime).toInt();
}

/*!
    \property QStyleHints::showIsFullScreen
    \brief \c true if the platform defaults to windows being fullscreen,
    otherwise \c false.

    \note The platform may still choose to show certain windows non-fullscreen,
    such as popups or dialogs. This property only reports the default behavior.

    \sa QWindow::show(), showIsMaximized()
*/
bool QStyleHints::showIsFullScreen() const
{
    return hint(QPlatformIntegration::ShowIsFullScreen).toBool();
}

/*!
    \property QStyleHints::showIsMaximized
    \brief \c true if the platform defaults to windows being maximized,
    otherwise \c false.

    \note The platform may still choose to show certain windows non-maximized,
    such as popups or dialogs. This property only reports the default behavior.

    \sa QWindow::show(), showIsFullScreen()
    \since 5.6
*/
bool QStyleHints::showIsMaximized() const
{
    return hint(QPlatformIntegration::ShowIsMaximized).toBool();
}

/*!
    \property QStyleHints::passwordMaskDelay
    \brief the time, in milliseconds, a typed letter is displayed unshrouded
    in a text input field in password mode.
*/
int QStyleHints::passwordMaskDelay() const
{
    return themeableHint(QPlatformTheme::PasswordMaskDelay, QPlatformIntegration::PasswordMaskDelay).toInt();
}

/*!
    \property QStyleHints::passwordMaskCharacter
    \brief the character used to mask the characters typed into text input
    fields in password mode.
*/
QChar QStyleHints::passwordMaskCharacter() const
{
    return themeableHint(QPlatformTheme::PasswordMaskCharacter, QPlatformIntegration::PasswordMaskCharacter).toChar();
}

/*!
    \property QStyleHints::fontSmoothingGamma
    \brief the gamma value used in font smoothing.
*/
qreal QStyleHints::fontSmoothingGamma() const
{
    return hint(QPlatformIntegration::FontSmoothingGamma).toReal();
}

/*!
    \property QStyleHints::useRtlExtensions
    \brief \c true if right-to-left writing direction is enabled,
    otherwise \c false.
*/
bool QStyleHints::useRtlExtensions() const
{
    return hint(QPlatformIntegration::UseRtlExtensions).toBool();
}

/*!
    \property QStyleHints::setFocusOnTouchRelease
    \brief \c true if focus objects (line edits etc) should receive
    input focus after a touch/mouse release. This is normal behavior on
    touch platforms. On desktop platforms, the standard is to set
    focus already on touch/mouse press.
*/
bool QStyleHints::setFocusOnTouchRelease() const
{
    return hint(QPlatformIntegration::SetFocusOnTouchRelease).toBool();
}

/*!
    \property QStyleHints::tabFocusBehavior
    \since 5.5
    \brief The focus behavior on press of the tab key.

    \note Do not bind this value in QML because the change notifier
    signal is not implemented yet.
*/

Qt::TabFocusBehavior QStyleHints::tabFocusBehavior() const
{
    return Qt::TabFocusBehavior(themeableHint(QPlatformTheme::TabFocusBehavior, QPlatformIntegration::TabFocusBehavior).toInt());
}

/*!
    \property QStyleHints::singleClickActivation
    \brief \c true if items should be activated by single click, \b false
    if they should be activated by double click instead.

    \since 5.5
*/
bool QStyleHints::singleClickActivation() const
{
    return themeableHint(QPlatformTheme::ItemViewActivateItemOnSingleClick, QPlatformIntegration::ItemViewActivateItemOnSingleClick).toBool();
}

QT_END_NAMESPACE
