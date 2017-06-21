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

// IMPORTANT!!!! If you want to add testdata to this file,
// always add it to the end in order to not change the linenumbers of translations!!!
#include <QtCore>
#include <QtGui>

//
// Test namespace scoping
//

class D : public QObject {
    Q_OBJECT
    public:
    QString foo() {
        return tr("test", "D");
    }

};

namespace A {

    class C : public QObject {
        Q_OBJECT
        public:
        void foo();
    };

    void C::foo() {
        tr("Bla", "A::C");
    }

    void goo() {
        C::tr("Bla", "A::C");       // Is identical to the previous tr(), (same context, sourcetext and comment,
                                    // so it should not add another entry to the list of messages)
    }

    void goo2() {
        C::tr("Bla 2", "A::C");     //Should be in the same namespace as the previous tr()
    }

}


namespace X {

    class D : public QObject {
        Q_OBJECT
        public:

    };

    class E : public QObject {
        Q_OBJECT
        public:
        void foo() { D::tr("foo", "D"); }  // Note that this is X::D from 440 on
    };


    namespace Y {
        class E : public QObject {
            Q_OBJECT

        };

        class C : public QObject {
            Q_OBJECT
            void foo();
        };

        void C::foo() {
            tr("Bla", "X::Y::C");
        }

        void goo() {
            D::tr("Bla", "X::D");   //This should be assigned to the X::D context
        }

        void goo2() {
            E::tr("Bla", "X::Y::E");   //This should be assigned to the X::Y::E context
            Y::E::tr("Bla", "X::Y::E");   //This should be assigned to the X::Y::E context
        }

    }; // namespace Y

    class F : public QObject {
        Q_OBJECT
        inline void inlinefunc() {
            tr("inline function", "X::F");
        }
    };
} // namespace X

namespace ico {
    namespace foo {
        class A : public QObject {
            A();
        };

        A::A() {
            tr("myfoo", "ico::foo::A");
            QObject::tr("task 161186", "QObject");
        }
    }
}

namespace AA {
class C {};
}

/**
 * the context of a message should not be affected by any inherited classes
 *
 * Keep this disabled for now, but at a long-term range it should work.
 */
namespace Gui {
    class MainWindow : public QMainWindow,
                    public AA::C
    {
        Q_OBJECT
public:
        MainWindow()
        {
            tr("More bla", "Gui::MainWindow");
        }

    };
} //namespace Gui


namespace A1 {
    class AB : public QObject {
        Q_OBJECT
        public:

        friend class OtherClass;

        QString inlineFuncAfterFriendDeclaration() const {
            return tr("inlineFuncAfterFriendDeclaration", "A1::AB");
        }
    };
    class B : AB {
        Q_OBJECT
        public:
        QString foo() const { return tr("foo", "A1::B"); }
    };

    // This is valid C++ too....
    class V : virtual AB {
        Q_OBJECT
        public:
        QString bar() const { return tr("bar", "A1::V"); }
    };

    class W : virtual public AB {
        Q_OBJECT
        public:
        QString baz() const { return tr("baz", "A1::W"); }
    };
}

class ForwardDecl;


class B1 : public QObject {
};

class C1 : public QObject {
};

namespace A1 {

class B2 : public QObject {
};

}

void func1()
{
    B1::tr("test TRANSLATOR comment (1)", "B1");

}

using namespace A1;
/*
    TRANSLATOR A1::B2
*/
void func2()
{
    B2::tr("test TRANSLATOR comment (2)", "A1::B2");
    C1::tr("test TRANSLATOR comment (3)", "C1");
}

void func3()
{
    B2::tr("test TRANSLATOR comment (4)", "A1::B2");
}

/*
    TRANSLATOR B2
    This is a comment to the translator.
*/
void func4()
{
    B2::tr("test TRANSLATOR comment (5)", "A1::B2");
}

namespace A1 {
namespace B3 {
class C2 : public QObject {
QString foo();
};
}
}

namespace D1 = A1::B3;
using namespace D1;

// TRANSLATOR A1::B3::C2
QString C2::foo()
{
    return tr("test TRANSLATOR comment (6)", "A1::B3::C2"); // 4.4 screws up
}


namespace Fooish {
    Q_DECLARE_TR_FUNCTIONS(Bears::And::Spiders)
}

void Fooish::bar()
{
    return tr("whatever the context", "Bears::And::Spiders");
}


int main(int /*argc*/, char ** /*argv*/) {
    return 0;
}

#include "main.moc"
