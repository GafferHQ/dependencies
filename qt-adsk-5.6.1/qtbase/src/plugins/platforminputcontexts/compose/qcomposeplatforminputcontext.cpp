/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qcomposeplatforminputcontext.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QKeyEvent>
#include <QtCore/QDebug>

#include <algorithm>

QT_BEGIN_NAMESPACE

//#define DEBUG_COMPOSING

static const int ignoreKeys[] = {
    Qt::Key_Shift,
    Qt::Key_Control,
    Qt::Key_Meta,
    Qt::Key_Alt,
    Qt::Key_CapsLock,
    Qt::Key_Super_L,
    Qt::Key_Super_R,
    Qt::Key_Hyper_L,
    Qt::Key_Hyper_R,
    Qt::Key_Mode_switch
};

static const int composingKeys[] = {
    Qt::Key_Multi_key,
    Qt::Key_Dead_Grave,
    Qt::Key_Dead_Acute,
    Qt::Key_Dead_Circumflex,
    Qt::Key_Dead_Tilde,
    Qt::Key_Dead_Macron,
    Qt::Key_Dead_Breve,
    Qt::Key_Dead_Abovedot,
    Qt::Key_Dead_Diaeresis,
    Qt::Key_Dead_Abovering,
    Qt::Key_Dead_Doubleacute,
    Qt::Key_Dead_Caron,
    Qt::Key_Dead_Cedilla,
    Qt::Key_Dead_Ogonek,
    Qt::Key_Dead_Iota,
    Qt::Key_Dead_Voiced_Sound,
    Qt::Key_Dead_Semivoiced_Sound,
    Qt::Key_Dead_Belowdot,
    Qt::Key_Dead_Hook,
    Qt::Key_Dead_Horn
};

QComposeInputContext::QComposeInputContext()
    : m_tableState(TableGenerator::EmptyTable)
    , m_compositionTableInitialized(false)
{
    clearComposeBuffer();
}

bool QComposeInputContext::filterEvent(const QEvent *event)
{
    const QKeyEvent *keyEvent = (const QKeyEvent *)event;
    // should pass only the key presses
    if (keyEvent->type() != QEvent::KeyPress) {
        return false;
    }

    // if there were errors when generating the compose table input
    // context should not try to filter anything, simply return false
    if (m_compositionTableInitialized && (m_tableState & TableGenerator::NoErrors) != TableGenerator::NoErrors)
        return false;

    int keyval = keyEvent->key();
    int keysym = 0;

    if (ignoreKey(keyval))
        return false;

    if (!composeKey(keyval) && keyEvent->text().isEmpty())
        return false;

    keysym = keyEvent->nativeVirtualKey();

    int nCompose = 0;
    while (m_composeBuffer[nCompose] != 0 && nCompose < QT_KEYSEQUENCE_MAX_LEN)
        nCompose++;

    if (nCompose == QT_KEYSEQUENCE_MAX_LEN) {
        reset();
        nCompose = 0;
    }

    m_composeBuffer[nCompose] = keysym;
    // check sequence
    if (checkComposeTable())
        return true;

    return false;
}

bool QComposeInputContext::isValid() const
{
    return true;
}

void QComposeInputContext::setFocusObject(QObject *object)
{
    m_focusObject = object;
}

void QComposeInputContext::reset()
{
    clearComposeBuffer();
}

void QComposeInputContext::update(Qt::InputMethodQueries q)
{
    QPlatformInputContext::update(q);
}

static bool isDuplicate(const QComposeTableElement &lhs, const QComposeTableElement &rhs)
{
    for (size_t i = 0; i < QT_KEYSEQUENCE_MAX_LEN; i++) {
        if (lhs.keys[i] != rhs.keys[i])
            return false;
    }
    return true;
}

bool QComposeInputContext::checkComposeTable()
{
    if (!m_compositionTableInitialized) {
        TableGenerator reader;
        m_tableState = reader.tableState();

        if ((m_tableState & TableGenerator::NoErrors) == TableGenerator::NoErrors)
            m_composeTable = reader.composeTable();

        m_compositionTableInitialized = true;
    }
    QVector<QComposeTableElement>::const_iterator it =
            std::lower_bound(m_composeTable.constBegin(), m_composeTable.constEnd(), m_composeBuffer, Compare());

    // prevent dereferencing an 'end' iterator, which would result in a crash
    if (it == m_composeTable.constEnd())
        it -= 1;

    QComposeTableElement elem = *it;
    // would be nicer if qLowerBound had API that tells if the item was actually found
    if (m_composeBuffer[0] != elem.keys[0]) {
#ifdef DEBUG_COMPOSING
        qDebug( "### no match ###" );
#endif
        reset();
        return false;
    }
    // check if compose buffer is matched
    for (int i=0; i < QT_KEYSEQUENCE_MAX_LEN; i++) {

        // check if partial match
        if (m_composeBuffer[i] == 0 && elem.keys[i]) {
#ifdef DEBUG_COMPOSING
            qDebug("### partial match ###");
#endif
            return true;
        }

        if (m_composeBuffer[i] != elem.keys[i]) {
#ifdef DEBUG_COMPOSING
            qDebug("### different entry ###");
#endif
            reset();
            return i != 0;
        }
    }
#ifdef DEBUG_COMPOSING
    qDebug("### match exactly ###");
#endif

    // check if the key sequence is overwriten - see the comment in
    // TableGenerator::orderComposeTable()
    int next = 1;
    do {
        // if we are at the end of the table, then we have nothing to do here
        if (it + next != m_composeTable.end()) {
            QComposeTableElement nextElem = *(it + next);
            if (isDuplicate(elem, nextElem)) {
                elem = nextElem;
                next++;
                continue;
            } else {
                break;
            }
        }
        break;
    } while (true);

    commitText(elem.value);
    reset();

    return true;
}

void QComposeInputContext::commitText(uint character) const
{
    QInputMethodEvent event;
    event.setCommitString(QChar(character));
    QCoreApplication::sendEvent(m_focusObject, &event);
}

bool QComposeInputContext::ignoreKey(int keyval) const
{
    for (uint i = 0; i < (sizeof(ignoreKeys) / sizeof(ignoreKeys[0])); i++)
        if (keyval == ignoreKeys[i])
            return true;

    return false;
}

bool QComposeInputContext::composeKey(int keyval) const
{
    for (uint i = 0; i < (sizeof(composingKeys) / sizeof(composingKeys[0])); i++)
        if (keyval == composingKeys[i])
            return true;

    return false;
}

void QComposeInputContext::clearComposeBuffer()
{
    for (uint i=0; i < (sizeof(m_composeBuffer) / sizeof(int)); i++)
        m_composeBuffer[i] = 0;
}

QComposeInputContext::~QComposeInputContext() {}

QT_END_NAMESPACE
