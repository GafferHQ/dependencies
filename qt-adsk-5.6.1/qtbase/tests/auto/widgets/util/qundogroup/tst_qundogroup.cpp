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
#include <QtTest/QtTest>
#include <QUndoGroup>
#include <QUndoStack>
#include <QAction>

// Temporarily disabling IRIX due to build issuues with GCC
#if defined(__sgi) && defined(__GNUC__)

class tst_QUndoGroup : public QObject
{
    Q_OBJECT
public:
    tst_QUndoGroup() {}

private slots:
    void setActive() { QSKIP( "Not tested on irix-g++"); }
    void addRemoveStack() { QSKIP( "Not tested on irix-g++"); }
    void deleteStack() { QSKIP( "Not tested on irix-g++"); }
    void checkSignals() { QSKIP( "Not tested on irix-g++"); }
    void addStackAndDie() { QSKIP( "Not tested on irix-g++"); }
};
#else

/******************************************************************************
** Commands
*/

class InsertCommand : public QUndoCommand
{
public:
    InsertCommand(QString *str, int idx, const QString &text,
                    QUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:
    QString *m_str;
    int m_idx;
    QString m_text;
};

class RemoveCommand : public QUndoCommand
{
public:
    RemoveCommand(QString *str, int idx, int len, QUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:
    QString *m_str;
    int m_idx;
    QString m_text;
};

class AppendCommand : public QUndoCommand
{
public:
    AppendCommand(QString *str, const QString &text, QUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();
    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand *other);

    bool merged;

private:
    QString *m_str;
    QString m_text;
};

InsertCommand::InsertCommand(QString *str, int idx, const QString &text,
                            QUndoCommand *parent)
    : QUndoCommand(parent)
{
    QVERIFY(str->length() >= idx);

    setText("insert");

    m_str = str;
    m_idx = idx;
    m_text = text;
}

void InsertCommand::redo()
{
    QVERIFY(m_str->length() >= m_idx);

    m_str->insert(m_idx, m_text);
}

void InsertCommand::undo()
{
    QCOMPARE(m_str->mid(m_idx, m_text.length()), m_text);

    m_str->remove(m_idx, m_text.length());
}

RemoveCommand::RemoveCommand(QString *str, int idx, int len, QUndoCommand *parent)
    : QUndoCommand(parent)
{
    QVERIFY(str->length() >= idx + len);

    setText("remove");

    m_str = str;
    m_idx = idx;
    m_text = m_str->mid(m_idx, len);
}

void RemoveCommand::redo()
{
    QCOMPARE(m_str->mid(m_idx, m_text.length()), m_text);

    m_str->remove(m_idx, m_text.length());
}

void RemoveCommand::undo()
{
    QVERIFY(m_str->length() >= m_idx);

    m_str->insert(m_idx, m_text);
}

AppendCommand::AppendCommand(QString *str, const QString &text, QUndoCommand *parent)
    : QUndoCommand(parent)
{
    setText("append");

    m_str = str;
    m_text = text;
    merged = false;
}

void AppendCommand::redo()
{
    m_str->append(m_text);
}

void AppendCommand::undo()
{
    QCOMPARE(m_str->mid(m_str->length() - m_text.length()), m_text);

    m_str->truncate(m_str->length() - m_text.length());
}

int AppendCommand::id() const
{
    return 1;
}

bool AppendCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
        return false;
    m_text += static_cast<const AppendCommand*>(other)->m_text;
    merged = true;
    return true;
}

/******************************************************************************
** tst_QUndoStack
*/

class tst_QUndoGroup : public QObject
{
    Q_OBJECT
public:
    tst_QUndoGroup();

private slots:
    void setActive();
    void addRemoveStack();
    void deleteStack();
    void checkSignals();
    void addStackAndDie();
    void commandTextFormat();
};

tst_QUndoGroup::tst_QUndoGroup()
{
}

void tst_QUndoGroup::setActive()
{
    QUndoGroup group;
    QUndoStack stack1(&group), stack2(&group);

    QCOMPARE(group.activeStack(), (QUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), false);

    QUndoStack stack3;
    QCOMPARE(stack3.isActive(), true);

    group.addStack(&stack3);
    QCOMPARE(stack3.isActive(), false);

    stack1.setActive();
    QCOMPARE(group.activeStack(), &stack1);
    QCOMPARE(stack1.isActive(), true);
    QCOMPARE(stack2.isActive(), false);
    QCOMPARE(stack3.isActive(), false);

    group.setActiveStack(&stack2);
    QCOMPARE(group.activeStack(), &stack2);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);

    group.removeStack(&stack2);
    QCOMPARE(group.activeStack(), (QUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);

    group.removeStack(&stack2);
    QCOMPARE(group.activeStack(), (QUndoStack*)0);
    QCOMPARE(stack1.isActive(), false);
    QCOMPARE(stack2.isActive(), true);
    QCOMPARE(stack3.isActive(), false);
}

void tst_QUndoGroup::addRemoveStack()
{
    QUndoGroup group;

    QUndoStack stack1(&group);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << &stack1);

    QUndoStack stack2;
    group.addStack(&stack2);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << &stack1 << &stack2);

    group.addStack(&stack1);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << &stack1 << &stack2);

    group.removeStack(&stack1);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << &stack2);

    group.removeStack(&stack1);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << &stack2);

    group.removeStack(&stack2);
    QCOMPARE(group.stacks(), QList<QUndoStack*>());
}

void tst_QUndoGroup::deleteStack()
{
    QUndoGroup group;

    QUndoStack *stack1 = new QUndoStack(&group);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << stack1);
    QCOMPARE(group.activeStack(), (QUndoStack*)0);

    stack1->setActive();
    QCOMPARE(group.activeStack(), stack1);

    QUndoStack *stack2 = new QUndoStack(&group);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << stack1 << stack2);
    QCOMPARE(group.activeStack(), stack1);

    QUndoStack *stack3 = new QUndoStack(&group);
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << stack1 << stack2 << stack3);
    QCOMPARE(group.activeStack(), stack1);

    delete stack2;
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << stack1 << stack3);
    QCOMPARE(group.activeStack(), stack1);

    delete stack1;
    QCOMPARE(group.stacks(), QList<QUndoStack*>() << stack3);
    QCOMPARE(group.activeStack(), (QUndoStack*)0);

    stack3->setActive(false);
    QCOMPARE(group.activeStack(), (QUndoStack*)0);

    stack3->setActive(true);
    QCOMPARE(group.activeStack(), stack3);

    group.removeStack(stack3);
    QCOMPARE(group.stacks(), QList<QUndoStack*>());
    QCOMPARE(group.activeStack(), (QUndoStack*)0);

    delete stack3;
}

static QString glue(const QString &s1, const QString &s2)
{
    QString result;

    result.append(s1);
    if (!s1.isEmpty() && !s2.isEmpty())
        result.append(' ');
    result.append(s2);

    return result;
}

#define CHECK_STATE(_activeStack, _clean, _canUndo, _undoText, _canRedo, _redoText, \
                    _cleanChanged, _indexChanged, _undoChanged, _redoChanged) \
    QCOMPARE(group.activeStack(), (QUndoStack*)_activeStack); \
    QCOMPARE(group.isClean(), _clean); \
    QCOMPARE(group.canUndo(), _canUndo); \
    QCOMPARE(group.undoText(), QString(_undoText)); \
    QCOMPARE(group.canRedo(), _canRedo); \
    QCOMPARE(group.redoText(), QString(_redoText)); \
    if (_indexChanged) { \
        QCOMPARE(indexChangedSpy.count(), 1); \
        indexChangedSpy.clear(); \
    } else { \
        QCOMPARE(indexChangedSpy.count(), 0); \
    } \
    if (_cleanChanged) { \
        QCOMPARE(cleanChangedSpy.count(), 1); \
        QCOMPARE(cleanChangedSpy.at(0).at(0).toBool(), _clean); \
        cleanChangedSpy.clear(); \
    } else { \
        QCOMPARE(cleanChangedSpy.count(), 0); \
    } \
    if (_undoChanged) { \
        QCOMPARE(canUndoChangedSpy.count(), 1); \
        QCOMPARE(canUndoChangedSpy.at(0).at(0).toBool(), _canUndo); \
        QCOMPARE(undo_action->isEnabled(), _canUndo); \
        QCOMPARE(undoTextChangedSpy.count(), 1); \
        QCOMPARE(undoTextChangedSpy.at(0).at(0).toString(), QString(_undoText)); \
        QCOMPARE(undo_action->text(), glue("foo", _undoText)); \
        canUndoChangedSpy.clear(); \
        undoTextChangedSpy.clear(); \
    } else { \
        QCOMPARE(canUndoChangedSpy.count(), 0); \
        QCOMPARE(undoTextChangedSpy.count(), 0); \
    } \
    if (_redoChanged) { \
        QCOMPARE(canRedoChangedSpy.count(), 1); \
        QCOMPARE(canRedoChangedSpy.at(0).at(0).toBool(), _canRedo); \
        QCOMPARE(redo_action->isEnabled(), _canRedo); \
        QCOMPARE(redoTextChangedSpy.count(), 1); \
        QCOMPARE(redoTextChangedSpy.at(0).at(0).toString(), QString(_redoText)); \
        QCOMPARE(redo_action->text(), glue("bar", _redoText)); \
        canRedoChangedSpy.clear(); \
        redoTextChangedSpy.clear(); \
    } else { \
        QCOMPARE(canRedoChangedSpy.count(), 0); \
        QCOMPARE(redoTextChangedSpy.count(), 0); \
    }

void tst_QUndoGroup::checkSignals()
{
    QUndoGroup group;
    QAction *undo_action = group.createUndoAction(0, QString("foo"));
    QAction *redo_action = group.createRedoAction(0, QString("bar"));
    QSignalSpy indexChangedSpy(&group, SIGNAL(indexChanged(int)));
    QSignalSpy cleanChangedSpy(&group, SIGNAL(cleanChanged(bool)));
    QSignalSpy canUndoChangedSpy(&group, SIGNAL(canUndoChanged(bool)));
    QSignalSpy undoTextChangedSpy(&group, SIGNAL(undoTextChanged(QString)));
    QSignalSpy canRedoChangedSpy(&group, SIGNAL(canRedoChanged(bool)));
    QSignalSpy redoTextChangedSpy(&group, SIGNAL(redoTextChanged(QString)));

    QString str;

    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.undo();
    CHECK_STATE(0,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.redo();
    CHECK_STATE(0,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    QUndoStack *stack1 = new QUndoStack(&group);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack1->push(new AppendCommand(&str, "foo"));
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack1->setActive();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->push(new InsertCommand(&str, 0, "bar"));
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "insert",   // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                false,      // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.undo();
    CHECK_STATE(stack1,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                true,       // canRedo
                "append",   // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    group.redo();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->setActive(false);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    QUndoStack *stack2 = new QUndoStack(&group);
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                false,      // cleanChanged
                false,      // indexChanged
                false,      // undoChanged
                false)      // redoChanged

    stack2->setActive();
    CHECK_STATE(stack2,     // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    stack1->setActive();
    CHECK_STATE(stack1,     // activeStack
                false,      // clean
                true,       // canUndo
                "append",   // undoText
                true,       // canRedo
                "insert",   // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    delete stack1;
    CHECK_STATE(0,          // activeStack
                true,       // clean
                false,      // canUndo
                "",         // undoText
                false,      // canRedo
                "",         // redoText
                true,       // cleanChanged
                true,       // indexChanged
                true,       // undoChanged
                true)       // redoChanged

    delete undo_action;
    delete redo_action;
}

void tst_QUndoGroup::addStackAndDie()
{
    // Test that QUndoStack doesn't keep a reference to QUndoGroup after the
    // group is deleted.
    QUndoStack *stack = new QUndoStack;
    QUndoGroup *group = new QUndoGroup;
    group->addStack(stack);
    delete group;
    stack->setActive(true);
    delete stack;
}

void tst_QUndoGroup::commandTextFormat()
{
#ifdef QT_NO_PROCESS
    QSKIP("No QProcess available");
#else
    QString binDir = QLibraryInfo::location(QLibraryInfo::BinariesPath);

    if (QProcess::execute(binDir + "/lrelease -version") != 0)
        QSKIP("lrelease is missing or broken");

    const QString tsFile = QFINDTESTDATA("testdata/qundogroup.ts");
    QVERIFY(!tsFile.isEmpty());
    QFile::remove("qundogroup.qm"); // Avoid confusion by strays.
    QVERIFY(!QProcess::execute(binDir + "/lrelease -silent " + tsFile + " -qm qundogroup.qm"));

    QTranslator translator;

    QVERIFY(translator.load("qundogroup.qm"));
    QFile::remove("qundogroup.qm");
    qApp->installTranslator(&translator);

    QUndoGroup group;
    QAction *undo_action = group.createUndoAction(0);
    QAction *redo_action = group.createRedoAction(0);

    QCOMPARE(undo_action->text(), QString("Undo-default-text"));
    QCOMPARE(redo_action->text(), QString("Redo-default-text"));

    QUndoStack stack(&group);
    stack.setActive();
    QString str;

    stack.push(new AppendCommand(&str, "foo"));
    QCOMPARE(undo_action->text(), QString("undo-prefix append undo-suffix"));
    QCOMPARE(redo_action->text(), QString("Redo-default-text"));

    stack.push(new InsertCommand(&str, 0, "bar"));
    stack.undo();
    QCOMPARE(undo_action->text(), QString("undo-prefix append undo-suffix"));
    QCOMPARE(redo_action->text(), QString("redo-prefix insert redo-suffix"));

    stack.undo();
    QCOMPARE(undo_action->text(), QString("Undo-default-text"));
    QCOMPARE(redo_action->text(), QString("redo-prefix append redo-suffix"));

    qApp->removeTranslator(&translator);
#endif
}
#endif // !(SGI && GCC)

QTEST_MAIN(tst_QUndoGroup)

#include "tst_qundogroup.moc"

