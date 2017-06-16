/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of PySide2.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "testmodifyfunction.h"
#include <QtTest/QTest>
#include "testutil.h"
#include <abstractmetalang.h>
#include <typesystem.h>

void TestModifyFunction::testRenameArgument()
{
    const char* cppCode ="\
    struct A {\n\
        void method(int=0);\n\
    };\n";
    const char* xmlCode = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <object-type name='A'>\n\
        <modify-function signature='method(int)'>\n\
            <modify-argument index='1'>\n\
                <rename to='otherArg'/>\n\
            </modify-argument>\n\
        </modify-function>\n\
        </object-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classA = AbstractMetaClass::findClass(classes, QLatin1String("A"));
    const AbstractMetaFunction* func = classA->findFunction(QLatin1String("method"));
    Q_ASSERT(func);

    QCOMPARE(func->argumentName(1), QLatin1String("otherArg"));
}

void TestModifyFunction::testOwnershipTransfer()
{
    const char* cppCode ="\
    struct A {};\n\
    struct B {\n\
        virtual A* method();\n\
    };\n";
    const char* xmlCode = "\
    <typesystem package=\"Foo\">\n\
        <object-type name='A' />\n\
        <object-type name='B'>\n\
        <modify-function signature='method()'>\n\
            <modify-argument index='return'>\n\
                <define-ownership owner='c++'/>\n\
            </modify-argument>\n\
        </modify-function>\n\
        </object-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classB = AbstractMetaClass::findClass(classes, QLatin1String("B"));
    const AbstractMetaFunction* func = classB->findFunction(QLatin1String("method"));

    QCOMPARE(func->ownership(func->ownerClass(), TypeSystem::TargetLangCode, 0), TypeSystem::CppOwnership);
}


void TestModifyFunction::invalidateAfterUse()
{
    const char* cppCode ="\
    struct A {\n\
        virtual void call(int *a);\n\
    };\n\
    struct B : A {\n\
    };\n\
    struct C : B {\n\
        virtual void call2(int *a);\n\
    };\n\
    struct D : C {\n\
        virtual void call2(int *a);\n\
    };\n\
    struct E : D {\n\
    };\n";
    const char* xmlCode = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='int'/>\n\
        <object-type name='A'>\n\
        <modify-function signature='call(int*)'>\n\
          <modify-argument index='1' invalidate-after-use='true'/>\n\
        </modify-function>\n\
        </object-type>\n\
        <object-type name='B' />\n\
        <object-type name='C'>\n\
        <modify-function signature='call2(int*)'>\n\
          <modify-argument index='1' invalidate-after-use='true'/>\n\
        </modify-function>\n\
        </object-type>\n\
        <object-type name='D'>\n\
        <modify-function signature='call2(int*)'>\n\
          <modify-argument index='1' invalidate-after-use='true'/>\n\
        </modify-function>\n\
        </object-type>\n\
        <object-type name='E' />\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false, "0.1"));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    const AbstractMetaClass *classB = AbstractMetaClass::findClass(classes, QLatin1String("B"));
    const AbstractMetaFunction* func = classB->findFunction(QLatin1String("call"));
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    const AbstractMetaClass *classC = AbstractMetaClass::findClass(classes, QLatin1String("C"));
    QVERIFY(classC);
    func = classC->findFunction(QLatin1String("call"));
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    func = classC->findFunction(QLatin1String("call2"));
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    const AbstractMetaClass *classD =  AbstractMetaClass::findClass(classes, QLatin1String("D"));
    QVERIFY(classD);
    func = classD->findFunction(QLatin1String("call"));
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    func = classD->findFunction(QLatin1String("call2"));
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    const AbstractMetaClass *classE = AbstractMetaClass::findClass(classes, QLatin1String("E"));
    QVERIFY(classE);
    func = classE->findFunction(QLatin1String("call"));
    QVERIFY(func);
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);

    func = classE->findFunction(QLatin1String("call2"));
    QVERIFY(func);
    QCOMPARE(func->modifications().size(), 1);
    QCOMPARE(func->modifications().at(0).argument_mods.size(), 1);
    QVERIFY(func->modifications().at(0).argument_mods.at(0).resetAfterUse);
}

void TestModifyFunction::testWithApiVersion()
{
    const char* cppCode ="\
    struct A {};\n\
    struct B {\n\
        virtual A* method();\n\
        virtual B* methodB();\n\
    };\n";
    const char* xmlCode = "\
    <typesystem package='Foo'>\n\
        <object-type name='A' />\n\
        <object-type name='B'>\n\
        <modify-function signature='method()' since='0.1'>\n\
            <modify-argument index='return'>\n\
                <define-ownership owner='c++'/>\n\
            </modify-argument>\n\
        </modify-function>\n\
        <modify-function signature='methodB()' since='0.2'>\n\
            <modify-argument index='return'>\n\
                <define-ownership owner='c++'/>\n\
            </modify-argument>\n\
        </modify-function>\n\
        </object-type>\n\
    </typesystem>\n";
    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false, "0.1"));
    QVERIFY(!builder.isNull());
    AbstractMetaClassList classes = builder->classes();
    AbstractMetaClass* classB = AbstractMetaClass::findClass(classes, QLatin1String("B"));
    const AbstractMetaFunction* func = classB->findFunction(QLatin1String("method"));

    QCOMPARE(func->ownership(func->ownerClass(), TypeSystem::TargetLangCode, 0), TypeSystem::CppOwnership);

    func = classB->findFunction(QLatin1String("methodB"));
    QVERIFY(func->ownership(func->ownerClass(), TypeSystem::TargetLangCode, 0) != TypeSystem::CppOwnership);
}

void TestModifyFunction::testGlobalFunctionModification()
{
    const char* cppCode ="\
    struct A {};\n\
    void function(A* a = 0);\n";
    const char* xmlCode = "\
    <typesystem package='Foo'>\n\
        <primitive-type name='A'/>\n\
        <function signature='function(A*)'>\n\
            <modify-function signature='function(A*)'>\n\
                <modify-argument index='1'>\n\
                    <replace-type modified-type='A'/>\n\
                    <replace-default-expression with='A()'/>\n\
                </modify-argument>\n\
            </modify-function>\n\
        </function>\n\
    </typesystem>\n";

    QScopedPointer<AbstractMetaBuilder> builder(TestUtil::parse(cppCode, xmlCode, false));
    QVERIFY(!builder.isNull());
    QCOMPARE(builder->globalFunctions().size(), 1);

    FunctionModificationList mods = TypeDatabase::instance()->functionModifications(QLatin1String("function(A*)"));
    QCOMPARE(mods.count(), 1);
    QList<ArgumentModification> argMods = mods.first().argument_mods;
    QCOMPARE(argMods.count(), 1);
    ArgumentModification argMod = argMods.first();
    QCOMPARE(argMod.replacedDefaultExpression, QLatin1String("A()"));

    const AbstractMetaFunction* func = builder->globalFunctions().first();
    QVERIFY(func);
    QCOMPARE(func->arguments().count(), 1);
    const AbstractMetaArgument* arg = func->arguments().first();
    QCOMPARE(arg->type()->cppSignature(), QLatin1String("A *"));
    QCOMPARE(arg->originalDefaultValueExpression(), QLatin1String("0"));
    QCOMPARE(arg->defaultValueExpression(), QLatin1String("A()"));
}

QTEST_APPLESS_MAIN(TestModifyFunction)
