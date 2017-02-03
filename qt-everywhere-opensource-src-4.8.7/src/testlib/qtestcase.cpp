/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtTest module of the Qt Toolkit.
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

#include "QtTest/qtestcase.h"
#include "QtTest/qtestassert.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvector.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qdebug.h>

#include "QtTest/private/qtestlog_p.h"
#include "QtTest/private/qtesttable_p.h"
#include "QtTest/qtestdata.h"
#include "QtTest/private/qtestresult_p.h"
#include "QtTest/private/qsignaldumper_p.h"
#include "QtTest/private/qbenchmark_p.h"
#include "3rdparty/cycle_p.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep
#endif
#ifdef Q_OS_UNIX
#include <errno.h>
#include <signal.h>
#include <time.h>
#endif

#ifdef Q_WS_MAC
#include <Carbon/Carbon.h> // for SetFrontProcess
#ifdef QT_MAC_USE_COCOA
#include <IOKit/pwr_mgt/IOPMLib.h>
#else
#include <Security/AuthSession.h>
#endif
#undef verify
#endif

QT_BEGIN_NAMESPACE

/*!
   \namespace QTest
   \inmodule QtTest

   \brief The QTest namespace contains all the functions and
   declarations that are related to the QTestLib tool.

   Please refer to the \l{QTestLib Manual} documentation for information on
   how to write unit tests.
*/

/*! \macro QVERIFY(condition)

   \relates QTest

   The QVERIFY() macro checks whether the \a condition is true or not. If it is
   true, execution continues. If not, a failure is recorded in the test log
   and the test won't be executed further.

   \bold {Note:} This macro can only be used in a test function that is invoked
   by the test framework.

   Example:
   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 0

   \sa QCOMPARE()
*/

/*! \macro QVERIFY2(condition, message)

    \relates QTest

    The QVERIFY2() macro behaves exactly like QVERIFY(), except that it outputs
    a verbose \a message when \a condition is false. The \a message is a plain
    C string.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 1

    \sa QVERIFY(), QCOMPARE()
*/

/*! \macro QCOMPARE(actual, expected)

   \relates QTest

   The QCOMPARE macro compares an \a actual value to an \a expected value using
   the equals operator. If \a actual and \a expected are identical, execution
   continues. If not, a failure is recorded in the test log and the test
   won't be executed further.

   In the case of comparing floats and doubles, qFuzzyCompare() is used for
   comparing. This means that comparing to 0 will likely fail. One solution
   to this is to compare to 1, and add 1 to the produced output.

   QCOMPARE tries to output the contents of the values if the comparison fails,
   so it is visible from the test log why the comparison failed.

   QCOMPARE is very strict on the data types. Both \a actual and \a expected
   have to be of the same type, otherwise the test won't compile. This prohibits
   unspecified behavior from being introduced; that is behavior that usually
   occurs when the compiler implicitly casts the argument.

   If you use QCOMPARE() to compare two QStringList objects, it will start
   comparing the objects from the end of the lists.

   For your own classes, you can use \l QTest::toString() to format values for
   outputting into the test log.

   \note This macro can only be used in a test function that is invoked
   by the test framework.

   Example:
   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 2

   \sa QVERIFY(), QTest::toString()
*/

/*! \macro QFETCH(type, name)

   \relates QTest

   The fetch macro creates a local variable named \a name with the type \a type
   on the stack. \a name has to match the element name from the test's data.
   If no such element exists, the test will assert.

   Assuming a test has the following data:

   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 3

   The test data has two elements, a QString called \c aString and an integer
   called \c expected. To fetch these values in the actual test:

   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 4

   \c aString and \c expected are variables on the stack that are initialized with
   the current test data.

   \bold {Note:} This macro can only be used in a test function that is invoked
   by the test framework. The test function must have a _data function.
*/

/*! \macro QWARN(message)

   \relates QTest
   \threadsafe

   Appends \a message as a warning to the test log. This macro can be used anywhere
   in your tests.
*/

/*! \macro QFAIL(message)

   \relates QTest

   This macro can be used to force a test failure. The test stops
   executing and the failure \a message is appended to the test log.

   \bold {Note:} This macro can only be used in a test function that is invoked
   by the test framework.

   Example:

   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 5
*/

/*! \macro QTEST(actual, testElement)

   \relates QTest

   QTEST() is a convenience macro for \l QCOMPARE() that compares
   the value \a actual with the element \a testElement from the test's data.
   If there is no such element, the test asserts.

   Apart from that, QTEST() behaves exactly as \l QCOMPARE().

   Instead of writing:

   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 6

   you can write:

   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 7

   \sa QCOMPARE()
*/

/*! \macro QSKIP(description, mode)

   \relates QTest

   The QSKIP() macro stops execution of the test without adding a failure to the
   test log. You can use it to skip tests that wouldn't make sense in the current
   configuration. The text \a description is appended to the test log and should
   contain an explanation why the test couldn't be executed. \a mode is a QTest::SkipMode
   and describes whether to proceed with the rest of the test data or not.

   \bold {Note:} This macro can only be used in a test function that is invoked
   by the test framework.

   Example:
   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 8

   \sa QTest::SkipMode
*/

/*! \macro QEXPECT_FAIL(dataIndex, comment, mode)

   \relates QTest

   The QEXPECT_FAIL() macro marks the next \l QCOMPARE() or \l QVERIFY() as an
   expected failure. Instead of adding a failure to the test log, an expected
   failure will be reported.

   If a \l QVERIFY() or \l QCOMPARE() is marked as an expected failure,
   but passes instead, an unexpected pass (XPASS) is written to the test log.

   The parameter \a dataIndex describes for which entry in the test data the
   failure is expected. Pass an empty string (\c{""}) if the failure
   is expected for all entries or if no test data exists.

   \a comment will be appended to the test log for the expected failure.

   \a mode is a \l QTest::TestFailMode and sets whether the test should
   continue to execute or not.

   \bold {Note:} This macro can only be used in a test function that is invoked
   by the test framework.

   Example 1:
   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 9

   In the example above, an expected fail will be written into the test output
   if the variable \c i is not 42. If the variable \c i is 42, an unexpected pass
   is written instead. The QEXPECT_FAIL() has no influence on the second QCOMPARE()
   statement in the example.

   Example 2:
   \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 10

   The above testfunction will not continue executing for the test data
   entry \c{data27}.

   \sa QTest::TestFailMode, QVERIFY(), QCOMPARE()
*/

/*! \macro QTEST_MAIN(TestClass)

    \relates QTest

    Implements a main() function that instantiates an application object and
    the \a TestClass, and executes all tests in the order they were defined.
    Use this macro to build stand-alone executables.

    If \c QT_GUI_LIB is defined, the application object will be a QApplication,
    otherwise it will be a QCoreApplication.  If qmake is used and the configuration
    includes \c{QT += gui}, then \c QT_GUI_LIB will be defined automatically.

    \bold {Note:} On platforms that have keypad navigation enabled by default (eg: Symbian),
    this macro will forcfully disable it to simplify the usage of key events when writing
    autotests. If you wish to write a test case that uses keypad navigation, you should
    enable it either in the \c {initTestCase()} or \c {init()} functions of your test case.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 11

    \sa QTEST_APPLESS_MAIN(), QTest::qExec(), QApplication::setNavigationMode()
*/

/*! \macro QTEST_APPLESS_MAIN(TestClass)

    \relates QTest

    Implements a main() function that executes all tests in \a TestClass.

    Behaves like \l QTEST_MAIN(), but doesn't instantiate a QApplication
    object. Use this macro for really simple stand-alone non-GUI tests.

    \sa QTEST_MAIN()
*/

/*! \macro QTEST_NOOP_MAIN()

    \relates QTest

    Implements a main() function with a test class that does absolutely nothing.
    Use this macro to create a test that produces valid test output but just
    doesn't execute any test, for example in conditional compilations:

    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 12

    \sa QTEST_MAIN()
*/

/*!
    \macro QBENCHMARK

    \relates QTest

    This macro is used to measure the performance of code within a test.
    The code to be benchmarked is contained within a code block following
    this macro.

    For example:

    \snippet examples/qtestlib/tutorial5/benchmarking.cpp 0

    \sa {QTestLib Manual#Creating a Benchmark}{Creating a Benchmark},
        {Chapter 5: Writing a Benchmark}{Writing a Benchmark}
*/

/*!
    \macro QBENCHMARK_ONCE
    \since 4.6

    \relates QTest

    \brief The QBENCHMARK_ONCE macro is for measuring performance of a
    code block by running it once.

    This macro is used to measure the performance of code within a test.
    The code to be benchmarked is contained within a code block following
    this macro.

    Unlike QBENCHMARK, the contents of the contained code block is only run
    once. The elapsed time will be reported as "0" if it's to short to
    be measured by the selected backend. (Use)

    \sa {QTestLib Manual#Creating a Benchmark}{Creating a Benchmark},
    {Chapter 5: Writing a Benchmark}{Writing a Benchmark}
*/



/*! \enum QTest::SkipMode

    This enum describes the modes for skipping tests during execution
    of the test data.

    \value SkipSingle Skips the current entry in the test table; continues
           execution of all the other entries in the table.

    \value SkipAll Skips all the entries in the test table; the test won't
           be executed further.

    \sa QSKIP()
*/

/*! \enum QTest::TestFailMode

    This enum describes the modes for handling an expected failure of the
    \l QVERIFY() or \l QCOMPARE() macros.

    \value Abort Aborts the execution of the test. Use this mode when it
           doesn't make sense to execute the test any further after the
           expected failure.

    \value Continue Continues execution of the test after the expected failure.

    \sa QEXPECT_FAIL()
*/

/*! \enum QTest::KeyAction

    This enum describes possible actions for key handling.

    \value Press    The key is pressed.
    \value Release  The key is released.
    \value Click    The key is clicked (pressed and released).
*/

/*! \enum QTest::MouseAction

    This enum describes possible actions for mouse handling.

    \value MousePress    A mouse button is pressed.
    \value MouseRelease  A mouse button is released.
    \value MouseClick    A mouse button is clicked (pressed and released).
    \value MouseDClick   A mouse button is double clicked (pressed and released twice).
    \value MouseMove     The mouse pointer has moved.
*/

/*! \fn void QTest::keyClick(QWidget *widget, char key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    \overload

    Simulates clicking of \a key with an optional \a modifier on a \a widget.
    If \a delay is larger than 0, the test will wait for \a delay milliseconds.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 13

    The example above simulates clicking \c a on \c myWidget without
    any keyboard modifiers and without delay of the test.

    \sa QTest::keyClicks()
*/

/*! \fn void QTest::keyClick(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    Simulates clicking of \a key with an optional \a modifier on a \a widget.
    If \a delay is larger than 0, the test will wait for \a delay milliseconds.

    Examples:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 14

    The first example above simulates clicking the \c escape key on \c
    myWidget without any keyboard modifiers and without delay. The
    second example simulates clicking \c shift-escape on \c myWidget
    with a following 200 ms delay of the test.

    \sa QTest::keyClicks()
*/

/*! \fn void QTest::keyEvent(KeyAction action, QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    Sends a Qt key event to \a widget with the given \a key and an associated \a action.
    Optionally, a keyboard \a modifier can be specified, as well as a \a delay
    (in milliseconds) of the test before sending the event.
*/

/*! \fn void QTest::keyEvent(KeyAction action, QWidget *widget, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    \overload

    Sends a Qt key event to \a widget with the given key \a ascii and an associated \a action.
    Optionally, a keyboard \a modifier can be specified, as well as a \a delay
    (in milliseconds) of the test before sending the event.

*/

/*! \fn void QTest::keyPress(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    Simulates pressing a \a key with an optional \a modifier on a \a widget. If \a delay
    is larger than 0, the test will wait for \a delay milliseconds.

    \bold {Note:} At some point you should release the key using \l keyRelease().

    \sa QTest::keyRelease(), QTest::keyClick()
*/

/*! \fn void QTest::keyPress(QWidget *widget, char key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    \overload

    Simulates pressing a \a key with an optional \a modifier on a \a widget.
    If \a delay is larger than 0, the test will wait for \a delay milliseconds.

    \bold {Note:} At some point you should release the key using \l keyRelease().

    \sa QTest::keyRelease(), QTest::keyClick()
*/

/*! \fn void QTest::keyRelease(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    Simulates releasing a \a key with an optional \a modifier on a \a widget.
    If \a delay is larger than 0, the test will wait for \a delay milliseconds.

    \sa QTest::keyPress(), QTest::keyClick()
*/

/*! \fn void QTest::keyRelease(QWidget *widget, char key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    \overload

    Simulates releasing a \a key with an optional \a modifier on a \a widget.
    If \a delay is larger than 0, the test will wait for \a delay milliseconds.

    \sa QTest::keyClick()
*/


/*! \fn void QTest::keyClicks(QWidget *widget, const QString &sequence, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay=-1)

    Simulates clicking a \a sequence of keys on a \a
    widget. Optionally, a keyboard \a modifier can be specified as
    well as a \a delay (in milliseconds) of the test before each key
    click.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 15

    The example above simulates clicking the sequence of keys
    representing "hello world" on \c myWidget without any keyboard
    modifiers and without delay of the test.

    \sa QTest::keyClick()
*/

/*! \fn void QTest::mousePress(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers modifier = 0, QPoint pos = QPoint(), int delay=-1)

    Simulates pressing a mouse \a button with an optional \a modifier
    on a \a widget.  The position is defined by \a pos; the default
    position is the center of the widget. If \a delay is specified,
    the test will wait for the specified amount of milliseconds before
    the press.

    \sa QTest::mouseRelease(), QTest::mouseClick()
*/

/*! \fn void QTest::mouseRelease(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers modifier = 0, QPoint pos = QPoint(), int delay=-1)

    Simulates releasing a mouse \a button with an optional \a modifier
    on a \a widget.  The position of the release is defined by \a pos;
    the default position is the center of the widget. If \a delay is
    specified, the test will wait for the specified amount of
    milliseconds before releasing the button.

    \sa QTest::mousePress(), QTest::mouseClick()
*/

/*! \fn void QTest::mouseClick(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers modifier = 0, QPoint pos = QPoint(), int delay=-1)

    Simulates clicking a mouse \a button with an optional \a modifier
    on a \a widget.  The position of the click is defined by \a pos;
    the default position is the center of the widget. If \a delay is
    specified, the test will wait for the specified amount of
    milliseconds before pressing and before releasing the button.

    \sa QTest::mousePress(), QTest::mouseRelease()
*/

/*! \fn void QTest::mouseDClick(QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers modifier = 0, QPoint pos = QPoint(), int delay=-1)

    Simulates double clicking a mouse \a button with an optional \a
    modifier on a \a widget.  The position of the click is defined by
    \a pos; the default position is the center of the widget. If \a
    delay is specified, the test will wait for the specified amount of
    milliseconds before each press and release.

    \sa QTest::mouseClick()
*/

/*! \fn void QTest::mouseMove(QWidget *widget, QPoint pos = QPoint(), int delay=-1)

    Moves the mouse pointer to a \a widget. If \a pos is not
    specified, the mouse pointer moves to the center of the widget. If
    a \a delay (in milliseconds) is given, the test will wait before
    moving the mouse pointer.
*/

/*!
    \fn char *QTest::toString(const T &value)

    Returns a textual representation of \a value. This function is used by
    \l QCOMPARE() to output verbose information in case of a test failure.

    You can add specializations of this function to your test to enable
    verbose output.

    \bold {Note:} The caller of toString() must delete the returned data
    using \c{delete[]}.  Your implementation should return a string
    created with \c{new[]} or qstrdup().

    Example:

    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 16

    The example above defines a toString() specialization for a class
    called \c MyPoint. Whenever a comparison of two instances of \c
    MyPoint fails, \l QCOMPARE() will call this function to output the
    contents of \c MyPoint to the test log.

    \sa QCOMPARE()
*/

/*!
    \fn char *QTest::toString(const QLatin1String &string)
    \overload

    Returns a textual representation of the given \a string.
*/

/*!
    \fn char *QTest::toString(const QString &string)
    \overload

    Returns a textual representation of the given \a string.
*/

/*!
    \fn char *QTest::toString(const QByteArray &ba)
    \overload

    Returns a textual representation of the byte array \a ba.

    \sa QTest::toHexRepresentation()
*/

/*!
    \fn char *QTest::toString(const QTime &time)
    \overload

    Returns a textual representation of the given \a time.
*/

/*!
    \fn char *QTest::toString(const QDate &date)
    \overload

    Returns a textual representation of the given \a date.
*/

/*!
    \fn char *QTest::toString(const QDateTime &dateTime)
    \overload

    Returns a textual representation of the date and time specified by
    \a dateTime.
*/

/*!
    \fn char *QTest::toString(const QChar &character)
    \overload

    Returns a textual representation of the given \a character.
*/

/*!
    \fn char *QTest::toString(const QPoint &point)
    \overload

    Returns a textual representation of the given \a point.
*/

/*!
    \fn char *QTest::toString(const QSize &size)
    \overload

    Returns a textual representation of the given \a size.
*/

/*!
    \fn char *QTest::toString(const QRect &rectangle)
    \overload

    Returns a textual representation of the given \a rectangle.
*/

/*!
    \fn char *QTest::toString(const QUrl &url)
    \since 4.4
    \overload

    Returns a textual representation of the given \a url.
*/

/*!
    \fn char *QTest::toString(const QPointF &point)
    \overload

    Returns a textual representation of the given \a point.
*/

/*!
    \fn char *QTest::toString(const QSizeF &size)
    \overload

    Returns a textual representation of the given \a size.
*/

/*!
    \fn char *QTest::toString(const QRectF &rectangle)
    \overload

    Returns a textual representation of the given \a rectangle.
*/

/*!
    \fn char *QTest::toString(const QVariant &variant)
    \overload

    Returns a textual representation of the given \a variant.
*/

/*! \fn void QTest::qWait(int ms)

    Waits for \a ms milliseconds. While waiting, events will be processed and
    your test will stay responsive to user interface events or network communication.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 17

    The code above will wait until the network server is responding for a
    maximum of about 12.5 seconds.

    \sa QTest::qSleep()
*/

/*! \fn bool QTest::qWaitForWindowShown(QWidget *window)
    \since 4.6

    Waits until the \a window is shown in the screen. This is mainly useful for
    asynchronous systems like X11, where a window will be mapped to screen some
    time after being asked to show itself on the screen. Returns true.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 24
*/

/*!
    \class QTest::QTouchEventSequence
    \inmodule QtTest
    \since 4.6

    \brief The QTouchEventSequence class is used to simulate a sequence of touch events.

    To simulate a sequence of touch events on a specific device for a widget, call
    QTest::touchEvent to create a QTouchEventSequence instance. Add touch events to
    the sequence by calling press(), move(), release() and stationary(), and let the
    instance run out of scope to commit the sequence to the event system.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 25
*/

/*!
    \fn QTest::QTouchEventSequence::~QTouchEventSequence()

    Commits this sequence of touch events and frees allocated resources.
*/

/*!
    \fn QTouchEventSequence &QTest::QTouchEventSequence::press(int touchId, const QPoint &pt, QWidget *widget)

    Adds a press event for touchpoint \a touchId at position \a pt to this sequence and returns
    a reference to this QTouchEventSequence.

    The position \a pt is interpreted as relative to \a widget. If \a widget is the null pointer, then
    \a pt is interpreted as relative to the widget provided when instantiating this QTouchEventSequence.

    Simulates that the user pressed the touch screen or pad with the finger identified by \a touchId.
*/

/*!
    \fn QTouchEventSequence &QTest::QTouchEventSequence::move(int touchId, const QPoint &pt, QWidget *widget)

    Adds a move event for touchpoint \a touchId at position \a pt to this sequence and returns
    a reference to this QTouchEventSequence.

    The position \a pt is interpreted as relative to \a widget. If \a widget is the null pointer, then
    \a pt is interpreted as relative to the widget provided when instantiating this QTouchEventSequence.

    Simulates that the user moved the finger identified by \a touchId.
*/

/*!
    \fn QTouchEventSequence &QTest::QTouchEventSequence::release(int touchId, const QPoint &pt, QWidget *widget)

    Adds a release event for touchpoint \a touchId at position \a pt to this sequence and returns
    a reference to this QTouchEventSequence.

    The position \a pt is interpreted as relative to \a widget. If \a widget is the null pointer, then
    \a pt is interpreted as relative to the widget provided when instantiating this QTouchEventSequence.

    Simulates that the user lifted the finger identified by \a touchId.
*/

/*!
    \fn QTouchEventSequence &QTest::QTouchEventSequence::stationary(int touchId)

    Adds a stationary event for touchpoint \a touchId to this sequence and returns
    a reference to this QTouchEventSequence.

    Simulates that the user did not move the finger identified by \a touchId.
*/

/*!
    \fn QTouchEventSequence QTest::touchEvent(QWidget *widget, QTouchEvent::DeviceType deviceType)

    Creates and returns a QTouchEventSequence for the device \a deviceType to
    simulate events for \a widget.

    When adding touch events to the sequence, \a widget will also be used to translate
    the position provided to screen coordinates, unless another widget is provided in the
    respective calls to press(), move() etc.

    The touch events are committed to the event system when the destructor of the
    QTouchEventSequence is called (ie when the object returned runs out of scope).
*/

namespace QTest
{
    static QObject *currentTestObject = 0;

    class TestFunction {
    public:
        TestFunction() : function_(-1), data_(0) {}
        void set(int function, char *data) { function_ = function; data_ = data; }
        char *data() const { return data_; }
        int function() const { return function_; }
        ~TestFunction() { delete[] data_; }
    private:
        int function_;
        char *data_;
    };
    /**
     * Contains the list of test functions that was supplied
     * on the command line, if any. Hence, if not empty,
     * those functions should be run instead of
     * all appearing in the test case.
     */
    static TestFunction * testFuncs = 0;
    static int testFuncCount = 0;

    /** Don't leak testFuncs on exit even on error */
    static struct TestFuncCleanup
    {
        void cleanup()
        {
            delete[] testFuncs;
            testFuncCount = 0;
            testFuncs = 0;
        }

        ~TestFuncCleanup() { cleanup(); }
    } testFuncCleaner;

    static int keyDelay = -1;
    static int mouseDelay = -1;
    static int eventDelay = -1;
    static bool randomOrder = false;
    static int keyVerbose = -1;
    static unsigned int seed = 0;
    static bool seedSet = false;
#if defined(Q_OS_UNIX) && !defined(Q_OS_SYMBIAN)
    static bool noCrashHandler = false;
#endif

void filter_unprintable(char *str)
{
    char *idx = str;
    while (*idx) {
        if (((*idx < 0x20 && *idx != '\n' && *idx != '\t') || *idx > 0x7e))
            *idx = '?';
        ++idx;
    }
}

/*! \internal
 */
int qt_snprintf(char *str, int size, const char *format, ...)
{
    va_list ap;
    int res = 0;

    va_start(ap, format);
    qvsnprintf(str, size, format, ap);
    va_end(ap);
    str[size - 1] = '\0';

    filter_unprintable(str);

    return res;
}

/*! \internal
    Invoke a method of the object without generating warning if the method does not exist
 */
static void invokeMethod(QObject *obj, const char *methodName)
{
    const QMetaObject *metaObject = obj->metaObject();
    int funcIndex = metaObject->indexOfMethod(methodName);
    if (funcIndex >= 0) {
        QMetaMethod method = metaObject->method(funcIndex);
        method.invoke(obj, Qt::DirectConnection);
    }
}

bool Q_TESTLIB_EXPORT defaultKeyVerbose()
{
    if (keyVerbose == -1) {
        keyVerbose = qgetenv("QTEST_KEYEVENT_VERBOSE").constData() ? 1 : 0;
    }
    return keyVerbose == 1;
}

int defaultEventDelay()
{
    if (eventDelay == -1) {
        if (qgetenv("QTEST_EVENT_DELAY").constData())
            eventDelay = atoi(qgetenv("QTEST_EVENT_DELAY"));
        else
            eventDelay = 0;
    }
    return eventDelay;
}

int Q_TESTLIB_EXPORT defaultMouseDelay()
{
    if (mouseDelay == -1) {
        if (qgetenv("QTEST_MOUSEEVENT_DELAY").constData())
            mouseDelay = atoi((qgetenv("QTEST_MOUSEEVENT_DELAY")));
        else
            mouseDelay = defaultEventDelay();
    }
    return mouseDelay;
}

int Q_TESTLIB_EXPORT defaultKeyDelay()
{
    if (keyDelay == -1) {
        if (qgetenv("QTEST_KEYEVENT_DELAY").constData())
            keyDelay = atoi(qgetenv("QTEST_KEYEVENT_DELAY").constData());
        else
            keyDelay = defaultEventDelay();
    }
    return keyDelay;
}

void seedRandom()
{
    static bool randomSeeded = false;
    if (!randomSeeded) {
        if (!QTest::seedSet) {
            QElapsedTimer timer;
            timer.start();
            QTest::seed = timer.msecsSinceReference();
        }
        qsrand(QTest::seed);
        randomSeeded = true;
    }
}

int qTestRandomSeed()
{
    Q_ASSERT(QTest::seedSet);
    return QTest::seed;
}

template<typename T>
void swap(T * array, int pos, int otherPos)
{
    T tmp = array[pos];
    array[pos] = array[otherPos];
    array[otherPos] = tmp;
}

template<typename T>
static void randomizeList(T * array, int size)
{
    for (int i = 0; i != size; i++) {
        int pos = qrand() % size;
        swap(array, pos, i);
    }
}

static bool isValidSlot(const QMetaMethod &sl)
{
    if (sl.access() != QMetaMethod::Private || !sl.parameterTypes().isEmpty()
        || qstrlen(sl.typeName()) || sl.methodType() != QMetaMethod::Slot)
        return false;
    const char *sig = sl.signature();
    int len = qstrlen(sig);
    if (len < 2)
        return false;
    if (sig[len - 2] != '(' || sig[len - 1] != ')')
        return false;
    if (len > 7 && strcmp(sig + (len - 7), "_data()") == 0)
        return false;
    if (strcmp(sig, "initTestCase()") == 0 || strcmp(sig, "cleanupTestCase()") == 0
        || strcmp(sig, "cleanup()") == 0 || strcmp(sig, "init()") == 0)
        return false;
    return true;
}

Q_TESTLIB_EXPORT bool printAvailableFunctions = false;
Q_TESTLIB_EXPORT bool printAvailableTags = false;
Q_TESTLIB_EXPORT QStringList testFunctions;
Q_TESTLIB_EXPORT QStringList testTags;

static void qPrintTestSlots()
{
    for (int i = 0; i < QTest::currentTestObject->metaObject()->methodCount(); ++i) {
        QMetaMethod sl = QTest::currentTestObject->metaObject()->method(i);
        if (isValidSlot(sl))
            printf("%s\n", sl.signature());
    }
}

static void qPrintDataTags()
{
    // Get global data tags:
    QTestTable::globalTestTable();
    invokeMethod(QTest::currentTestObject, "initTestCase_data()");
    const QTestTable *gTable = QTestTable::globalTestTable();

    const QMetaObject *currTestMetaObj = QTest::currentTestObject->metaObject();

    // Process test functions:
    for (int i = 0; i < currTestMetaObj->methodCount(); ++i) {
        QMetaMethod tf = currTestMetaObj->method(i);
        if (isValidSlot(tf)) {
            // Retrieve local tags:
            QStringList localTags;
            QTestTable table;
            char member[512];
            char *slot = qstrdup(tf.signature());
            slot[strlen(slot) - 2] = '\0';
            QTest::qt_snprintf(member, 512, "%s_data()", slot);
            invokeMethod(QTest::currentTestObject, member);
            for (int j = 0; j < table.dataCount(); ++j)
                localTags << QLatin1String(table.testData(j)->dataTag());

            // Print all tag combinations:
            if (gTable->dataCount() == 0) {
                if (localTags.count() == 0) {
                    // No tags at all, so just print the test function:
                    printf("%s %s\n", currTestMetaObj->className(), slot);
                } else {
                    // Only local tags, so print each of them:
                    for (int k = 0; k < localTags.size(); ++k)
                        printf(
                            "%s %s %s\n",
                            currTestMetaObj->className(), slot, localTags.at(k).toLatin1().data());
                }
            } else {
                for (int j = 0; j < gTable->dataCount(); ++j) {
                    if (localTags.count() == 0) {
                        // Only global tags, so print the current one:
                        printf(
                            "%s %s __global__ %s\n",
                            currTestMetaObj->className(), slot, gTable->testData(j)->dataTag());
                    } else {
                        // Local and global tags, so print each of the local ones and
                        // the current global one:
                        for (int k = 0; k < localTags.size(); ++k)
                            printf(
                                "%s %s %s __global__ %s\n", currTestMetaObj->className(), slot,
                                localTags.at(k).toLatin1().data(), gTable->testData(j)->dataTag());
                    }
                }
            }

            delete[] slot;
        }
    }
}

static int qToInt(char *str)
{
    char *pEnd;
    int l = (int)strtol(str, &pEnd, 10);
    if (*pEnd != 0) {
        printf("Invalid numeric parameter: '%s'\n", str);
        exit(1);
    }
    return l;
}

Q_TESTLIB_EXPORT void qtest_qParseArgs(int argc, char *argv[], bool qml)
{
    const char *testOptions =
         " options:\n"
         " -functions : Returns a list of current testfunctions\n"
         " -datatags  : Returns a list of current data tags.\n"
         "              A global data tag is preceded by ' __global__ '.\n"
         " -xunitxml  : Outputs results as XML XUnit document\n"
         " -xml       : Outputs results as XML document\n"
         " -lightxml  : Outputs results as stream of XML tags\n"
         " -flush     : Flushes the results\n"
         " -o filename: Writes all output into a file\n"
         " -silent    : Only outputs warnings and failures\n"
         " -v1        : Print enter messages for each testfunction\n"
         " -v2        : Also print out each QVERIFY/QCOMPARE/QTEST\n"
         " -vs        : Print every signal emitted\n"
         " -random    : Run testcases within each test in random order\n"
         " -seed n    : Positive integer to be used as seed for -random. If not specified,\n"
         "              the current time will be used as seed.\n"
         " -eventdelay ms    : Set default delay for mouse and keyboard simulation to ms milliseconds\n"
         " -keydelay ms      : Set default delay for keyboard simulation to ms milliseconds\n"
         " -mousedelay ms    : Set default delay for mouse simulation to ms milliseconds\n"
         " -keyevent-verbose : Turn on verbose messages for keyboard simulation\n"
         " -maxwarnings n    : Sets the maximum amount of messages to output.\n"
         "                     0 means unlimited, default: 2000\n"
#if defined(Q_OS_UNIX) && !defined(Q_OS_SYMBIAN)
         " -nocrashhandler   : Disables the crash handler\n"
#endif
         "\n"
         " Benchmark related options:\n"
#ifdef QTESTLIB_USE_VALGRIND
        " -callgrind      : Use callgrind to time benchmarks\n"
#endif
#ifdef HAVE_TICK_COUNTER
        " -tickcounter    : Use CPU tick counters to time benchmarks\n"
#endif
        " -eventcounter   : Counts events received during benchmarks\n"
        " -minimumvalue n : Sets the minimum acceptable measurement value\n"
        " -iterations  n  : Sets the number of accumulation iterations.\n"
        " -median  n      : Sets the number of median iterations.\n"
        " -vb             : Print out verbose benchmarking information.\n"
         "\n"
        " -help      : This help\n";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0
            || strcmp(argv[i], "/?") == 0) {
            printf(" Usage: %s [options] [testfunction[:testdata]]...\n"
                   "    By default, all testfunctions will be run.\n\n"
                   "%s", argv[0], testOptions);
            exit(0);
        } else if (strcmp(argv[i], "-functions") == 0) {
            if (qml) {
                QTest::printAvailableFunctions = true;
            } else {
                qPrintTestSlots();
                exit(0);
            }
        } else if (strcmp(argv[i], "-datatags") == 0) {
            QTest::printAvailableTags = true;
            if (!qml) {
                qPrintDataTags();
                exit(0);
            }
        } else if(strcmp(argv[i], "-xunitxml") == 0){
            QTestLog::setLogMode(QTestLog::XunitXML);
        } else if (strcmp(argv[i], "-xml") == 0) {
            QTestLog::setLogMode(QTestLog::XML);
        } else if (strcmp(argv[i], "-lightxml") == 0) {
            QTestLog::setLogMode(QTestLog::LightXML);
        }else if(strcmp(argv[i], "-flush") == 0){
            QTestLog::setFlushMode(QTestLog::FLushOn);
        } else if (strcmp(argv[i], "-silent") == 0) {
            QTestLog::setVerboseLevel(-1);
        } else if (strcmp(argv[i], "-v1") == 0) {
            QTestLog::setVerboseLevel(1);
        } else if (strcmp(argv[i], "-v2") == 0) {
            QTestLog::setVerboseLevel(2);
        } else if (strcmp(argv[i], "-vs") == 0) {
            QSignalDumper::startDump();
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                printf("-o needs an extra parameter specifying the filename\n");
                exit(1);
            } else {
                QTestLog::redirectOutput(argv[++i]);
            }
        } else if (strcmp(argv[i], "-eventdelay") == 0) {
            if (i + 1 >= argc) {
                printf("-eventdelay needs an extra parameter to indicate the delay(ms)\n");
                exit(1);
            } else {
                QTest::eventDelay = qToInt(argv[++i]);
            }
        } else if (strcmp(argv[i], "-keydelay") == 0) {
            if (i + 1 >= argc) {
                printf("-keydelay needs an extra parameter to indicate the delay(ms)\n");
                exit(1);
            } else {
                QTest::keyDelay = qToInt(argv[++i]);
            }
        } else if (strcmp(argv[i], "-mousedelay") == 0) {
            if (i + 1 >= argc) {
                printf("-mousedelay needs an extra parameter to indicate the delay(ms)\n");
                exit(1);
            } else {
                QTest::mouseDelay = qToInt(argv[++i]);
            }
        } else if (strcmp(argv[i], "-maxwarnings") == 0) {
            if (i + 1 >= argc) {
                printf("-maxwarnings needs an extra parameter with the amount of warnings\n");
                exit(1);
            } else {
                QTestLog::setMaxWarnings(qToInt(argv[++i]));
            }
#if defined(Q_OS_UNIX) && !defined(Q_OS_SYMBIAN)
        } else if (strcmp(argv[i], "-nocrashhandler") == 0) {
            QTest::noCrashHandler = true;
#endif
        } else if (strcmp(argv[i], "-keyevent-verbose") == 0) {
            QTest::keyVerbose = 1;
#ifdef QTESTLIB_USE_VALGRIND
        } else if (strcmp(argv[i], "-callgrind") == 0) {
            if (QBenchmarkValgrindUtils::haveValgrind())
                if (QFileInfo(QDir::currentPath()).isWritable()) {
                    QBenchmarkGlobalData::current->setMode(QBenchmarkGlobalData::CallgrindParentProcess);
                } else {
                    printf("WARNING: Current directory not writable. Using the walltime measurer.\n");
                }
            else {
                printf("WARNING: Valgrind not found or too old. Make sure it is installed and in your path. "
                       "Using the walltime measurer.\n");
            }
        } else if (strcmp(argv[i], "-callgrindchild") == 0) { // "private" option
            QBenchmarkGlobalData::current->setMode(QBenchmarkGlobalData::CallgrindChildProcess);
            QBenchmarkGlobalData::current->callgrindOutFileBase =
                QBenchmarkValgrindUtils::outFileBase();
#endif
#ifdef HAVE_TICK_COUNTER
        } else if (strcmp(argv[i], "-tickcounter") == 0) {
            QBenchmarkGlobalData::current->setMode(QBenchmarkGlobalData::TickCounter);
#endif
        } else if (strcmp(argv[i], "-eventcounter") == 0) {
            QBenchmarkGlobalData::current->setMode(QBenchmarkGlobalData::EventCounter);
        } else if (strcmp(argv[i], "-random") == 0) {
            QTest::randomOrder = true;
        } else if (strcmp(argv[i], "-seed") == 0) {
            bool argumentOk = false;
            if (i + 1 < argc) {
                char * endpt = 0;
                long longSeed = strtol(argv[++i], &endpt, 10);
                argumentOk = (*endpt == '\0' && longSeed >= 0);
                QTest::seed = longSeed;
            }
            if (!argumentOk) {
                printf("-seed needs an extra positive integer parameter to specify the seed\n");
                exit(1);
            } else {
                QTest::seedSet = true;
            }
        } else if (strcmp(argv[i], "-minimumvalue") == 0) {
            if (i + 1 >= argc) {
                printf("-minimumvalue needs an extra parameter to indicate the minimum time(ms)\n");
                exit(1);
            } else {
                QBenchmarkGlobalData::current->walltimeMinimum = qToInt(argv[++i]);
            }
        } else if (strcmp(argv[i], "-iterations") == 0) {
            if (i + 1 >= argc) {
                printf("-iterations needs an extra parameter to indicate the number of iterations\n");
                exit(1);
            } else {
                QBenchmarkGlobalData::current->iterationCount = qToInt(argv[++i]);
            }
        } else if (strcmp(argv[i], "-median") == 0) {
            if (i + 1 >= argc) {
                printf("-median needs an extra parameter to indicate the number of median iterations\n");
                exit(1);
            } else {
                QBenchmarkGlobalData::current->medianIterationCount = qToInt(argv[++i]);
            }

        } else if (strcmp(argv[i], "-vb") == 0) {
            QBenchmarkGlobalData::current->verboseOutput = true;
        } else if (strcmp(argv[i], "-chart") == 0) {
            fprintf(stderr, "Warning: `-chart' option is not available\n");
        } else if (strcmp(argv[i], "-qws") == 0) {
            // do nothing
        } else if (strcmp(argv[i], "-graphicssystem") == 0) {
            // do nothing
            if (i + 1 >= argc) {
                printf("-graphicssystem needs an extra parameter specifying the graphics system\n");
                exit(1);
            } else {
                ++i;
            }
        } else if (argv[i][0] == '-') {
            printf("Unknown option: '%s'\n\n%s", argv[i], testOptions);
            exit(1);
        } else if (qml) {
            // We can't check the availability of test functions until
            // we load the QML files.  So just store the data for now.
            int colon = -1;
            int offset;
            for(offset = 0; *(argv[i]+offset); ++offset) {
                if (*(argv[i]+offset) == ':') {
                    if (*(argv[i]+offset+1) == ':') {
                        // "::" is used as a test name separator.
                        // e.g. "ClickTests::test_click:row1".
                        ++offset;
                    } else {
                        colon = offset;
                        break;
                    }
                }
            }
            if (colon == -1) {
                QTest::testFunctions += QString::fromLatin1(argv[i]);
                QTest::testTags += QString();
            } else {
                QTest::testFunctions +=
                    QString::fromLatin1(argv[i], colon);
                QTest::testTags +=
                    QString::fromLatin1(argv[i] + colon + 1);
            }
        } else {
			if (!QTest::testFuncs) {
		        QTest::testFuncs = new QTest::TestFunction[512];
			}

            int colon = -1;
            char buf[512], *data=0;
            int off;
            for(off = 0; *(argv[i]+off); ++off) {
                if (*(argv[i]+off) == ':') {
                    colon = off;
                    break;
                }
            }
            if(colon != -1) {
                data = qstrdup(argv[i]+colon+1);
            }
            QTest::qt_snprintf(buf, qMin(512, off + 1), "%s", argv[i]); // copy text before the ':' into buf
            QTest::qt_snprintf(buf + off, qMin(512 - off, 3), "()");    // append "()"
            int idx = QTest::currentTestObject->metaObject()->indexOfMethod(buf);
            if (idx < 0 || !isValidSlot(QTest::currentTestObject->metaObject()->method(idx))) {
                printf("Unknown testfunction: '%s'\n", buf);
                printf("Available testfunctions:\n");
                qPrintTestSlots();
                exit(1);
            }
            testFuncs[testFuncCount].set(idx, data);
			testFuncCount++;
			QTEST_ASSERT(QTest::testFuncCount < 512);
        }
    }

    if (QTest::seedSet && !QTest::randomOrder) {
        printf("-seed requires -random\n");
        exit(1);
    }
}

QBenchmarkResult qMedian(const QList<QBenchmarkResult> &container)
{
    const int count = container.count();
    if (count == 0)
        return QBenchmarkResult();

    if (count == 1)
        return container.at(0);

    QList<QBenchmarkResult> containerCopy = container;
    qSort(containerCopy);

    const int middle = count / 2;

    // ### handle even-sized containers here by doing an aritmetic mean of the two middle items.
    return containerCopy.at(middle);
}

struct QTestDataSetter
{
    QTestDataSetter(QTestData *data)
    {
        QTestResult::setCurrentTestData(data);
    }
    ~QTestDataSetter()
    {
        QTestResult::setCurrentTestData(0);
    }
};

static void qInvokeTestMethodDataEntry(char *slot)
{
    /* Benchmarking: for each median iteration*/

    int i = (QBenchmarkGlobalData::current->measurer->needsWarmupIteration()) ? -1 : 0;

    QList<QBenchmarkResult> results;
    do {
        QBenchmarkTestMethodData::current->beginDataRun();

        /* Benchmarking: for each accumulation iteration*/
        bool invokeOk;
        do {
            QTestResult::setCurrentTestLocation(QTestResult::InitFunc);
            invokeMethod(QTest::currentTestObject, "init()");
            if (QTestResult::skipCurrentTest())
                break;

            QTestResult::setCurrentTestLocation(QTestResult::Func);

            QBenchmarkTestMethodData::current->result = QBenchmarkResult();
            QBenchmarkTestMethodData::current->resultAccepted = false;

            QBenchmarkGlobalData::current->context.tag =
                QLatin1String(
                    QTestResult::currentDataTag()
                    ? QTestResult::currentDataTag() : "");

            invokeOk = QMetaObject::invokeMethod(QTest::currentTestObject, slot,
                                                 Qt::DirectConnection);
            if (!invokeOk)
                QTestResult::addFailure("Unable to execute slot", __FILE__, __LINE__);

            QTestResult::setCurrentTestLocation(QTestResult::CleanupFunc);
            invokeMethod(QTest::currentTestObject, "cleanup()");
            QTestResult::setCurrentTestLocation(QTestResult::NoWhere);

            // If this test method has a benchmark, repeat until all measurements are
            // acceptable.
            // The QBENCHMARK macro increases the number of iterations for each run until
            // this happens.
        } while (invokeOk
                 && QBenchmarkTestMethodData::current->isBenchmark()
                 && QBenchmarkTestMethodData::current->resultsAccepted() == false);

        QBenchmarkTestMethodData::current->endDataRun();
        if (i > -1)  // iteration -1 is the warmup iteration.
            results.append(QBenchmarkTestMethodData::current->result);

        if (QBenchmarkTestMethodData::current->isBenchmark() &&
            QBenchmarkGlobalData::current->verboseOutput) {
                if (i == -1) {
                    qDebug() << "warmup stage result      :" << QBenchmarkTestMethodData::current->result.value;
                } else {
                    qDebug() << "accumulation stage result:" << QBenchmarkTestMethodData::current->result.value;
                }
            }
    } while (QBenchmarkTestMethodData::current->isBenchmark()
             && (++i < QBenchmarkGlobalData::current->adjustMedianIterationCount()));

    if (QBenchmarkTestMethodData::current->isBenchmark()
        && QBenchmarkTestMethodData::current->resultsAccepted())
        QTestLog::addBenchmarkResult(qMedian(results));
}

/*!
    \internal

    Call init(), slot_data(), slot(), slot(), slot()..., cleanup()
    If data is set then it is the only test that is performed

    If the function was successfully called, true is returned, otherwise
    false.
 */
static bool qInvokeTestMethod(const char *slotName, const char *data=0)
{
    QTEST_ASSERT(slotName);

    QBenchmarkTestMethodData benchmarkData;
    QBenchmarkTestMethodData::current = &benchmarkData;

    QBenchmarkGlobalData::current->context.slotName = QLatin1String(slotName);

    char member[512];
    QTestTable table;

    char *slot = qstrdup(slotName);
    slot[strlen(slot) - 2] = '\0';
    QTestResult::setCurrentTestFunction(slot);

    const QTestTable *gTable = QTestTable::globalTestTable();
    const int globalDataCount = gTable->dataCount();
    int curGlobalDataIndex = 0;

    /* For each test function that has a *_data() table/function, do: */
    do {
        if (!gTable->isEmpty())
            QTestResult::setCurrentGlobalTestData(gTable->testData(curGlobalDataIndex));

        if (curGlobalDataIndex == 0) {
            QTestResult::setCurrentTestLocation(QTestResult::DataFunc);
            QTest::qt_snprintf(member, 512, "%s_data()", slot);
            invokeMethod(QTest::currentTestObject, member);

            // if we encounter a SkipAll in the _data slot, we skip the whole
            // testfunction, no matter how much global data exists
            if (QTestResult::skipCurrentTest()) {
                QTestResult::setCurrentGlobalTestData(0);
                break;
            }
        }

        bool foundFunction = false;
        if (!QTestResult::skipCurrentTest()) {
            int curDataIndex = 0;
            const int dataCount = table.dataCount();
            QTestResult::setSkipCurrentTest(false);

            // Data tag requested but none available?
            if (data && !dataCount) {
                // Let empty data tag through.
                if (!*data)
                    data = 0;
                else {
                    printf("Unknown testdata for function %s: '%s'\n", slotName, data);
                    printf("Function has no testdata.\n");
                    return false;
                }
            }

            /* For each entry in the data table, do: */
            do {
                if (!data || !qstrcmp(data, table.testData(curDataIndex)->dataTag())) {
                    foundFunction = true;
                    QTestDataSetter s(curDataIndex >= dataCount ? static_cast<QTestData *>(0)
                                                      : table.testData(curDataIndex));

                    qInvokeTestMethodDataEntry(slot);

                    if (QTestResult::skipCurrentTest())
                        // check whether SkipAll was requested
                        break;
                    if (data)
                        break;
                }
                ++curDataIndex;
            } while (curDataIndex < dataCount);
        }

        if (data && !foundFunction) {
            printf("Unknown testdata for function %s: '%s'\n", slotName, data);
            printf("Available testdata:\n");
            for(int i = 0; i < table.dataCount(); ++i)
                printf("%s\n", table.testData(i)->dataTag());
            return false;
        }

        QTestResult::setCurrentGlobalTestData(0);
        ++curGlobalDataIndex;
    } while (curGlobalDataIndex < globalDataCount);

    QTestResult::finishedCurrentTestFunction();
    QTestResult::setSkipCurrentTest(false);
    QTestResult::setCurrentTestData(0);
    delete[] slot;

    return true;
}

void *fetchData(QTestData *data, const char *tagName, int typeId)
{
    QTEST_ASSERT(typeId);
    QTEST_ASSERT_X(data, "QTest::fetchData()", "Test data requested, but no testdata available.");
    QTEST_ASSERT(data->parent());

    int idx = data->parent()->indexOf(tagName);

    if (idx == -1 || idx >= data->dataCount()) {
        qFatal("QFETCH: Requested testdata '%s' not available, check your _data function.",
                tagName);
    }

    if (typeId != data->parent()->elementTypeId(idx)) {
        qFatal("Requested type '%s' does not match available type '%s'.",
               QMetaType::typeName(typeId),
               QMetaType::typeName(data->parent()->elementTypeId(idx)));
    }

    return data->data(idx);
}

/*!
  \fn char* QTest::toHexRepresentation(const char *ba, int length)

  Returns a pointer to a string that is the string \a ba represented
  as a space-separated sequence of hex characters. If the input is
  considered too long, it is truncated. A trucation is indicated in
  the returned string as an ellipsis at the end.

  \a length is the length of the string \a ba.
 */
char *toHexRepresentation(const char *ba, int length)
{
    if(length == 0)
        return qstrdup("");

    /* We output at maximum about maxLen characters in order to avoid
     * running out of memory and flooding things when the byte array
     * is large.
     *
     * maxLen can't be for example 200 because QTestLib is sprinkled with fixed
     * size char arrays.
     * */
    const int maxLen = 50;
    const int len = qMin(maxLen, length);
    char *result = 0;

    if(length > maxLen) {
        const int size = len * 3 + 4;
        result = new char[size];

        char *const forElipsis = result + size - 5;
        forElipsis[0] = ' ';
        forElipsis[1] = '.';
        forElipsis[2] = '.';
        forElipsis[3] = '.';
        result[size - 1] = '\0';
    }
    else {
        const int size = len * 3;
        result = new char[size];
        result[size - 1] = '\0';
    }

    const char toHex[] = "0123456789ABCDEF";
    int i = 0;
    int o = 0;

    while(true) {
        const char at = ba[i];

        result[o] = toHex[(at >> 4) & 0x0F];
        ++o;
        result[o] = toHex[at & 0x0F];

        ++i;
        ++o;
        if(i == len)
            break;
        else {
            result[o] = ' ';
            ++o;
        }
    }

    return result;
}

static void qInvokeTestMethods(QObject *testObject)
{
    const QMetaObject *metaObject = testObject->metaObject();
    QTEST_ASSERT(metaObject);
    if (QTest::randomOrder) {
        QTestLog::startLogging(QTest::seed);
    } else {
        QTestLog::startLogging();
    }
    QTestResult::setCurrentTestFunction("initTestCase");
    QTestResult::setCurrentTestLocation(QTestResult::DataFunc);
    QTestTable::globalTestTable();
    invokeMethod(testObject, "initTestCase_data()");

    if (!QTestResult::skipCurrentTest() && !QTest::currentTestFailed()) {
        QTestResult::setCurrentTestLocation(QTestResult::InitFunc);
        invokeMethod(testObject, "initTestCase()");

        // finishedCurrentTestFunction() resets QTestResult::testFailed(), so use a local copy.
        const bool previousFailed = QTestResult::testFailed();
        QTestResult::finishedCurrentTestFunction();

        if(!QTestResult::skipCurrentTest() && !previousFailed) {

            if (QTest::testFuncs) {
                if (QTest::randomOrder)
                    randomizeList(QTest::testFuncs, QTest::testFuncCount);
                for (int i = 0; i != QTest::testFuncCount; i++) {
                    if (!qInvokeTestMethod(metaObject->method(QTest::testFuncs[i].function()).signature(),
                                                              QTest::testFuncs[i].data())) {
                        break;
                    }
                }
                testFuncCleaner.cleanup();
            } else {
                int methodCount = metaObject->methodCount();
                QMetaMethod *testMethods = new QMetaMethod[methodCount];
                for (int i = 0; i != methodCount; i++)
                    testMethods[i] = metaObject->method(i);
                if (QTest::randomOrder)
                    randomizeList(testMethods, methodCount);
                for (int i = 0; i != methodCount; i++) {
                    if (!isValidSlot(testMethods[i]))
                        continue;
                    if (!qInvokeTestMethod(testMethods[i].signature()))
                        break;
                }
                delete[] testMethods;
                testMethods = 0;
            }
        }

        QTestResult::setSkipCurrentTest(false);
        QTestResult::setCurrentTestFunction("cleanupTestCase");
        invokeMethod(testObject, "cleanupTestCase()");
    }
    QTestResult::finishedCurrentTestFunction();
    QTestResult::setCurrentTestFunction(0);
    QTestTable::clearGlobalTestTable();

    QTestLog::stopLogging();
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_SYMBIAN)
class FatalSignalHandler
{
public:
    FatalSignalHandler();
    ~FatalSignalHandler();

private:
    static void signal(int);
    sigset_t handledSignals;
};

void FatalSignalHandler::signal(int signum)
{
    qFatal("Received signal %d", signum);
#if defined(Q_OS_INTEGRITY)
    {
        struct sigaction act;
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = SIG_DFL;
        sigaction(signum, &act, NULL);
    }
#endif
}

FatalSignalHandler::FatalSignalHandler()
{
    sigemptyset(&handledSignals);

    const int fatalSignals[] = {
         SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGFPE, SIGSEGV, SIGPIPE, SIGTERM, 0 };

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = FatalSignalHandler::signal;

    // Remove the handler after it is invoked.
#if !defined(Q_OS_INTEGRITY)
    act.sa_flags = SA_RESETHAND;
#endif
    // Block all fatal signals in our signal handler so we don't try to close
    // the testlog twice.
    sigemptyset(&act.sa_mask);
    for (int i = 0; fatalSignals[i]; ++i)
        sigaddset(&act.sa_mask, fatalSignals[i]);

    struct sigaction oldact;

    for (int i = 0; fatalSignals[i]; ++i) {
        sigaction(fatalSignals[i], &act, &oldact);
#ifndef Q_WS_QWS
        // Don't overwrite any non-default handlers
        // however, we need to replace the default QWS handlers
        if (
#ifdef SA_SIGINFO
            oldact.sa_flags & SA_SIGINFO ||
#endif
            oldact.sa_handler != SIG_DFL) {
            sigaction(fatalSignals[i], &oldact, 0);
        } else
#endif
        {
            sigaddset(&handledSignals, fatalSignals[i]);
        }
    }
}


FatalSignalHandler::~FatalSignalHandler()
{
    // Unregister any of our remaining signal handlers
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;

    struct sigaction oldact;

    for (int i = 1; i < 32; ++i) {
        if (!sigismember(&handledSignals, i))
            continue;
        sigaction(i, &act, &oldact);

        // If someone overwrote it in the mean time, put it back
        if (oldact.sa_handler != FatalSignalHandler::signal)
            sigaction(i, &oldact, 0);
    }
}

#endif


} // namespace

/*!
    Executes tests declared in \a testObject. In addition, the private slots
    \c{initTestCase()}, \c{cleanupTestCase()}, \c{init()} and \c{cleanup()}
    are executed if they exist. See \l{Creating a Test} for more details.

    Optionally, the command line arguments \a argc and \a argv can be provided.
    For a list of recognized arguments, read \l {QTestLib Command Line Arguments}.

    For stand-alone tests, the convenience macro \l QTEST_MAIN() can
    be used to declare a main method that parses the command line arguments
    and executes the tests.

    Returns 0 if all tests passed. Returns a value other than 0 if tests failed
    or in case of unhandled exceptions. The return value from this function is
    also the exit code of the test application when the \l QTEST_MAIN() macro
    is used.

    The following example will run all tests in \c MyFirstTestObject and
    \c{MySecondTestObject}:

    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 18

    Note: This function is not reentrant, only one test can run at a time. A
    test that was executed with qExec() can't run another test via qExec() and
    threads are not allowed to call qExec() simultaneously.

    If you have programatically created the arguments, as opposed to getting them
    from the arguments in \c main(), it is likely of interest to use
    QTest::qExec(QObject *, const QStringList &) since it is Unicode safe.

    \sa QTEST_MAIN()
*/

int QTest::qExec(QObject *testObject, int argc, char **argv)
{
    QBenchmarkGlobalData benchmarkData;
    QBenchmarkGlobalData::current = &benchmarkData;

#ifdef QTESTLIB_USE_VALGRIND
    int callgrindChildExitCode = 0;
#endif

#ifdef Q_WS_MAC
     bool macNeedsActivate = qApp && (qstrcmp(qApp->metaObject()->className(), "QApplication") == 0);
#ifdef QT_MAC_USE_COCOA
     IOPMAssertionID powerID;
#endif
#endif
#ifndef QT_NO_EXCEPTIONS
    try {
#endif

 #if defined(Q_OS_WIN) && !defined(Q_OS_WINCE)
     SetErrorMode(SetErrorMode(0) | SEM_NOGPFAULTERRORBOX);
 #endif

#ifdef Q_WS_MAC
    // Starting with Qt 4.4, applications launched from the command line
    // no longer get focus automatically. Since some tests might depend
    // on this, call SetFrontProcess here to get the pre 4.4 behavior.
    if (macNeedsActivate) {
        ProcessSerialNumber psn = { 0, kCurrentProcess };
        SetFrontProcess(&psn);
#  ifdef QT_MAC_USE_COCOA
        IOReturn ok = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &powerID);
        if (ok != kIOReturnSuccess)
            macNeedsActivate = false; // no need to release the assertion on exit.
#  else
        UpdateSystemActivity(1); // Wake the display.
#  endif
    }
#endif

#if defined(Q_OS_SYMBIAN) && defined(Q_CC_NOKIAX86)
    // Delay execution of tests in Symbian emulator.
    // Needed to allow worst of other higher priority apps and services launched by emulator
    // to get out of the way before we run our test. Otherwise some of the timing sensitive tests
    // will not work properly.
    qSleep(3000);
#endif

    QTestResult::reset();

    QTEST_ASSERT(testObject);
    QTEST_ASSERT(!currentTestObject);
    currentTestObject = testObject;

    const QMetaObject *metaObject = testObject->metaObject();
    QTEST_ASSERT(metaObject);

    QTestResult::setCurrentTestObject(metaObject->className());
    if (argc > 0)
        QTestResult::setCurrentAppName(argv[0]);

    qtest_qParseArgs(argc, argv, false);
    if (QTest::randomOrder) {
        seedRandom();
    }
#ifdef QTESTLIB_USE_VALGRIND
    if (QBenchmarkGlobalData::current->mode() == QBenchmarkGlobalData::CallgrindParentProcess) {
        const QStringList origAppArgs(QCoreApplication::arguments());
        if (!QBenchmarkValgrindUtils::rerunThroughCallgrind(origAppArgs, callgrindChildExitCode))
            return -1;

        QBenchmarkValgrindUtils::cleanup();

    } else
#endif
    {
#if defined(Q_OS_UNIX) && !defined(Q_OS_SYMBIAN)
        QScopedPointer<FatalSignalHandler> handler;
        if (!noCrashHandler)
            handler.reset(new FatalSignalHandler);
#endif
        qInvokeTestMethods(testObject);
    }

#ifndef QT_NO_EXCEPTIONS
     } catch (...) {
         QTestResult::addFailure("Caught unhandled exception", __FILE__, __LINE__);
         if (QTestResult::currentTestFunction()) {
             QTestResult::finishedCurrentTestFunction();
             QTestResult::setCurrentTestFunction(0);
         }

        QTestLog::stopLogging();
#ifdef QT_MAC_USE_COCOA
         if (macNeedsActivate) {
             IOPMAssertionRelease(powerID);
         }
#endif
         currentTestObject = 0;

         // Rethrow exception to make debugging easier.
         throw;
         return 1;
     }
#  endif

    currentTestObject = 0;
#ifdef QT_MAC_USE_COCOA
     if (macNeedsActivate) {
         IOPMAssertionRelease(powerID);
     }
#endif

#if defined(QTEST_NOEXITCODE)
    return 0;
#else

#ifdef QTESTLIB_USE_VALGRIND
    if (QBenchmarkGlobalData::current->mode() == QBenchmarkGlobalData::CallgrindParentProcess)
        return callgrindChildExitCode;
#endif
    // make sure our exit code is never going above 127
    // since that could wrap and indicate 0 test fails
    return qMin(QTestResult::failCount(), 127);

#endif
}

/*!
  \overload
  \since 4.4

  Behaves identically to qExec(QObject *, int, char**) but takes a
  QStringList of \a arguments instead of a \c char** list.
 */
int QTest::qExec(QObject *testObject, const QStringList &arguments)
{
    const int argc = arguments.count();
    QVarLengthArray<char *> argv(argc);

    QVector<QByteArray> args;
    args.reserve(argc);

    for(int i = 0; i < argc; ++i)
    {
        args.append(arguments.at(i).toLocal8Bit().constData());
        argv[i] = args.last().data();
    }

    return qExec(testObject, argc, argv.data());
}

/*! \internal
 */
void QTest::qFail(const char *statementStr, const char *file, int line)
{
    QTestResult::addFailure(statementStr, file, line);
}

/*! \internal
 */
bool QTest::qVerify(bool statement, const char *statementStr, const char *description,
                   const char *file, int line)
{
    return QTestResult::verify(statement, statementStr, description, file, line);
}

/*! \fn void QTest::qSkip(const char *message, SkipMode mode, const char *file, int line)
\internal
 */
void QTest::qSkip(const char *message, QTest::SkipMode mode,
                 const char *file, int line)
{
    QTestResult::addSkip(message, mode, file, line);
    if (mode == QTest::SkipAll)
        QTestResult::setSkipCurrentTest(true);
}

/*! \fn bool QTest::qExpectFail(const char *dataIndex, const char *comment, TestFailMode mode, const char *file, int line)
\internal
 */
bool QTest::qExpectFail(const char *dataIndex, const char *comment,
                       QTest::TestFailMode mode, const char *file, int line)
{
    return QTestResult::expectFail(dataIndex, qstrdup(comment), mode, file, line);
}

/*! \internal
 */
void QTest::qWarn(const char *message)
{
    QTestLog::warn(message);
}

/*!
    Ignores messages created by qDebug() or qWarning(). If the \a message
    with the corresponding \a type is outputted, it will be removed from the
    test log. If the test finished and the \a message was not outputted,
    a test failure is appended to the test log.

    \bold {Note:} Invoking this function will only ignore one message.
    If the message you want to ignore is outputted twice, you have to
    call ignoreMessage() twice, too.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 19

    The example above tests that QDir::mkdir() outputs the right warning when invoked
    with an invalid file name.
*/
void QTest::ignoreMessage(QtMsgType type, const char *message)
{
    QTestResult::ignoreMessage(type, message);
}

/*! \internal
 */
void *QTest::qData(const char *tagName, int typeId)
{
    return fetchData(QTestResult::currentTestData(), tagName, typeId);
}

/*! \internal
 */
void *QTest::qGlobalData(const char *tagName, int typeId)
{
    return fetchData(QTestResult::currentGlobalTestData(), tagName, typeId);
}

/*! \internal
 */
void *QTest::qElementData(const char *tagName, int metaTypeId)
{
    QTEST_ASSERT(tagName);
    QTestData *data = QTestResult::currentTestData();
    QTEST_ASSERT(data);
    QTEST_ASSERT(data->parent());

    int idx = data->parent()->indexOf(tagName);
    QTEST_ASSERT(idx != -1);
    QTEST_ASSERT(data->parent()->elementTypeId(idx) == metaTypeId);

    return data->data(data->parent()->indexOf(tagName));
}

/*! \internal
 */
void QTest::addColumnInternal(int id, const char *name)
{
    QTestTable *tbl = QTestTable::currentTestTable();
    QTEST_ASSERT_X(tbl, "QTest::addColumn()", "Cannot add testdata outside of a _data slot.");

    tbl->addColumn(id, name);
}

/*!
    Appends a new row to the current test data. \a dataTag is the name of
    the testdata that will appear in the test output. Returns a QTestData reference
    that can be used to stream in data.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 20

    \bold {Note:} This macro can only be used in a test's data function
    that is invoked by the test framework.

    See \l {Chapter 2: Data Driven Testing}{Data Driven Testing} for
    a more extensive example.

    \sa addColumn(), QFETCH()
*/
QTestData &QTest::newRow(const char *dataTag)
{
    QTestTable *tbl = QTestTable::currentTestTable();
    QTEST_ASSERT_X(tbl, "QTest::addColumn()", "Cannot add testdata outside of a _data slot.");

    return *tbl->newData(dataTag);
}

/*! \fn void QTest::addColumn(const char *name, T *dummy = 0)

    Adds a column with type \c{T} to the current test data.
    \a name is the name of the column. \a dummy is a workaround
    for buggy compilers and can be ignored.

    To populate the column with values, newRow() can be used. Use
    \l QFETCH() to fetch the data in the actual test.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 21

    To add custom types to the testdata, the type must be registered with
    QMetaType via \l Q_DECLARE_METATYPE().

    \bold {Note:} This macro can only be used in a test's data function
    that is invoked by the test framework.

    See \l {Chapter 2: Data Driven Testing}{Data Driven Testing} for
    a more extensive example.

    \sa QTest::newRow(), QFETCH(), QMetaType
*/

/*!
    Returns the name of the binary that is currently executed.
*/
const char *QTest::currentAppName()
{
    return QTestResult::currentAppName();
}

/*!
    Returns the name of the test function that is currently executed.

    Example:

    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 22
*/
const char *QTest::currentTestFunction()
{
    return QTestResult::currentTestFunction();
}

/*!
    Returns the name of the current test data. If the test doesn't
    have any assigned testdata, the function returns 0.
*/
const char *QTest::currentDataTag()
{
    return QTestResult::currentDataTag();
}

/*!
    Returns true if the current test function failed, otherwise false.
*/
bool QTest::currentTestFailed()
{
    return QTestResult::currentTestFailed();
}

/*!
    Sleeps for \a ms milliseconds, blocking execution of the
    test. qSleep() will not do any event processing and leave your test
    unresponsive. Network communication might time out while
    sleeping. Use \l qWait() to do non-blocking sleeping.

    \a ms must be greater than 0.

    \bold {Note:} The qSleep() function calls either \c nanosleep() on
    unix or \c Sleep() on windows, so the accuracy of time spent in
    qSleep() depends on the operating system.

    Example:
    \snippet doc/src/snippets/code/src_qtestlib_qtestcase.cpp 23

    \sa qWait()
*/
void QTest::qSleep(int ms)
{
    QTEST_ASSERT(ms > 0);

#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
#endif
}

/*! \internal
 */
QObject *QTest::testObject()
{
    return currentTestObject;
}

/*! \internal
 */
bool QTest::compare_helper(bool success, const char *msg, const char *file, int line)
{
    return QTestResult::compare(success, msg, file, line);
}

/*! \internal
 */
bool QTest::compare_helper(bool success, const char *msg, char *val1, char *val2,
                    const char *actual, const char *expected, const char *file, int line)
{
    return QTestResult::compare(success, msg, val1, val2, actual, expected, file, line);
}

/*! \fn bool QTest::qCompare<float>(float const &t1, float const &t2, const char *actual, const char *expected, const char *file, int line)
\internal
 */
template <>
Q_TESTLIB_EXPORT bool QTest::qCompare<float>(float const &t1, float const &t2, const char *actual, const char *expected,
                    const char *file, int line)
{
    return qFuzzyCompare(t1, t2)
            ? compare_helper(true, "COMPARE()", file, line)
            : compare_helper(false, "Compared floats are not the same (fuzzy compare)",
                             toString(t1), toString(t2), actual, expected, file, line);
}

/*! \fn bool QTest::qCompare<double>(double const &t1, double const &t2, const char *actual, const char *expected, const char *file, int line)
\internal
 */
template <>
Q_TESTLIB_EXPORT bool QTest::qCompare<double>(double const &t1, double const &t2, const char *actual, const char *expected,
                    const char *file, int line)
{
    return qFuzzyCompare(t1, t2)
            ? compare_helper(true, "COMPARE()", file, line)
            : compare_helper(false, "Compared doubles are not the same (fuzzy compare)",
                             toString(t1), toString(t2), actual, expected, file, line);
}

#define COMPARE_IMPL2(TYPE, FORMAT) \
template <> Q_TESTLIB_EXPORT char *QTest::toString<TYPE >(const TYPE &t) \
{ \
    char *msg = new char[128]; \
    qt_snprintf(msg, 128, #FORMAT, t); \
    return msg; \
}

COMPARE_IMPL2(short, %hd)
COMPARE_IMPL2(ushort, %hu)
COMPARE_IMPL2(int, %d)
COMPARE_IMPL2(uint, %u)
COMPARE_IMPL2(long, %ld)
COMPARE_IMPL2(ulong, %lu)
#if defined(Q_OS_WIN)
COMPARE_IMPL2(qint64, %I64d)
COMPARE_IMPL2(quint64, %I64u)
#else
COMPARE_IMPL2(qint64, %lld)
COMPARE_IMPL2(quint64, %llu)
#endif
COMPARE_IMPL2(bool, %d)
COMPARE_IMPL2(char, %c)
COMPARE_IMPL2(float, %g)
COMPARE_IMPL2(double, %lg)

/*! \internal
 */
char *QTest::toString(const char *str)
{
    if (!str)
        return 0;
    char *msg = new char[strlen(str) + 1];
    return qstrcpy(msg, str);
}

/*! \internal
 */
char *QTest::toString(const void *p)
{
    char *msg = new char[128];
    qt_snprintf(msg, 128, "%p", p);
    return msg;
}

/*! \internal
 */
bool QTest::compare_string_helper(const char *t1, const char *t2, const char *actual,
                                  const char *expected, const char *file, int line)
{
    return (qstrcmp(t1, t2) == 0)
            ? compare_helper(true, "COMPARE()", file, line)
            : compare_helper(false, "Compared strings are not the same",
                             toString(t1), toString(t2), actual, expected, file, line);
}

/*! \fn bool QTest::compare_ptr_helper(const void *t1, const void *t2, const char *actual, const char *expected, const char *file, int line);
    \internal
*/

/*! \fn bool QTest::qCompare(T1 const &, T2 const &, const char *, const char *, const char *, int);
    \internal
*/


/*! \fn void QTest::mouseEvent(MouseAction action, QWidget *widget, Qt::MouseButton button, Qt::KeyboardModifiers stateKey, QPoint pos, int delay=-1)
    \internal
*/

/*! \fn bool QTest::qCompare(QIcon const &t1, QIcon const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QPixmap const &t1, QPixmap const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(T const &t1, T const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(const T *t1, const T *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(T *t1, T *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(const T1 *t1, const T2 *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(T1 *t1, T2 *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(const char *t1, const char *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(char *t1, char *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(char *t1, const char *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(const char *t1, char *t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QString const &t1, QLatin1String const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QLatin1String const &t1, QString const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QStringList const &t1, QStringList const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QFlags<T> const &t1, T const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(QFlags<T> const &t1, int const &t2, const char *actual, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn bool QTest::qCompare(bool const &t1, int const &t2, const char *actual, const char *expected, const char *file, int line)
  \internal
 */

/*! \fn bool QTest::qTest(const T& actual, const char *elementName, const char *actualStr, const char *expected, const char *file, int line)
    \internal
*/

/*! \fn void QTest::sendKeyEvent(KeyAction action, QWidget *widget, Qt::Key code, QString text, Qt::KeyboardModifiers modifier, int delay=-1)
    \internal
*/

/*! \fn void QTest::sendKeyEvent(KeyAction action, QWidget *widget, Qt::Key code, char ascii, Qt::KeyboardModifiers modifier, int delay=-1)
    \internal
*/

/*! \fn void QTest::simulateEvent(QWidget *widget, bool press, int code, Qt::KeyboardModifiers modifier, QString text, bool repeat, int delay=-1)
    \internal
*/

QT_END_NAMESPACE
