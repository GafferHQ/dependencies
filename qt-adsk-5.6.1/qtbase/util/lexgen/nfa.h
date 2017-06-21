/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
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

#ifndef NFA_H
#define NFA_H

#include <QMap>
#include <QHash>
#include <QString>
#include <QVector>
#include <QDebug>
#include <QStack>
#include <QByteArray>

#include "global.h"

typedef QHash<InputType, int> TransitionMap;

struct State
{
    QString symbol;
    TransitionMap transitions;
};

inline QDataStream &operator<<(QDataStream &stream, const State &state)
{
    return stream << state.symbol << state.transitions;
}

inline QDataStream &operator>>(QDataStream &stream, State &state)
{
    return stream >> state.symbol >> state.transitions;
}

struct DFA : public QVector<State>
{
    void debug() const;
    DFA minimize() const;
};

class NFA
{
public:
    static NFA createSingleInputNFA(InputType input);
    static NFA createSymbolNFA(const QString &symbol);
    static NFA createAlternatingNFA(const NFA &a, const NFA &b);
    static NFA createConcatenatingNFA(const NFA &a, const NFA &b);
    static NFA createOptionalNFA(const NFA &a);

    // convenience
    static NFA createStringNFA(const QByteArray &str);
    static NFA createSetNFA(const QSet<InputType> &set);
    static NFA createZeroOrOneNFA(const NFA &a);
    static NFA applyQuantity(const NFA &a, int minOccurrences, int maxOccurrences);

    void setTerminationSymbol(const QString &symbol);

    DFA toDFA() const;

    inline bool isEmpty() const { return states.isEmpty(); }
    inline int stateCount() const { return states.count(); }

    void debug();

private:
    void initialize(int size);
    void addTransition(int from, InputType input, int to);
    void copyFrom(const NFA &other, int baseState);

    void initializeFromPair(const NFA &a, const NFA &b,
                            int *initialA, int *finalA,
                            int *initialB, int *finalB);

    QSet<int> epsilonClosure(const QSet<int> &initialClosure) const;

    inline void assertValidState(int state)
    { Q_UNUSED(state); Q_ASSERT(state >= 0); Q_ASSERT(state < states.count()); }

#if defined(AUTOTEST)
public:
#endif
    int initialState;
    int finalState;

    QVector<State> states;
};

#endif // NFA_H

