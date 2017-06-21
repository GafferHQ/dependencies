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
#include <qapplication.h>
#include <QPushButton>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QStatusBar>
#include <QListWidget>
#include <QWidgetAction>
#include <QDesktopWidget>
#include <QScreen>
#include <qdialog.h>

#include <qmenu.h>
#include <qstyle.h>
#include <QTimer>
#include <qdebug.h>

Q_DECLARE_METATYPE(Qt::Key);
Q_DECLARE_METATYPE(Qt::KeyboardModifiers);

static inline void centerOnScreen(QWidget *w, const QSize &size)
{
    const QPoint offset = QPoint(size.width() / 2, size.height() / 2);
    w->move(QGuiApplication::primaryScreen()->availableGeometry().center() - offset);
}

static inline void centerOnScreen(QWidget *w)
{
    centerOnScreen(w, w->geometry().size());
}

class tst_QMenu : public QObject
{
    Q_OBJECT

public:
    tst_QMenu();

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
private slots:
    void getSetCheck();
    void addActionsAndClear();
    void addActionsConnect();

    void keyboardNavigation_data();
    void keyboardNavigation();
    void focus();
    void overrideMenuAction();
    void statusTip();
    void widgetActionFocus();
#ifndef Q_OS_WINCE
    void mouseActivation();
#endif
    void tearOff();
    void layoutDirection();

    void task208001_stylesheet();
    void activeSubMenuPosition();
    void activeSubMenuPositionExec();
    void task242454_sizeHint();
    void task176201_clear();
    void task250673_activeMultiColumnSubMenuPosition();
    void task256918_setFont();
    void menuSizeHint();
#ifndef Q_OS_WINCE
    void task258920_mouseBorder();
#endif
    void setFixedWidth();
    void deleteActionInTriggered();
    void pushButtonPopulateOnAboutToShow();
    void QTBUG7907_submenus_autoselect();
    void QTBUG7411_submenus_activate();
    void QTBUG30595_rtl_submenu();
    void QTBUG20403_nested_popup_on_shortcut_trigger();
#ifndef QT_NO_CURSOR
    void QTBUG47515_widgetActionEnterLeave();
#endif
    void QTBUG_10735_crashWithDialog();
#ifdef Q_OS_MAC
    void QTBUG_37933_ampersands_data();
    void QTBUG_37933_ampersands();
#endif
protected slots:
    void onActivated(QAction*);
    void onHighlighted(QAction*);
    void onStatusMessageChanged(const QString &);
    void onStatusTipTimer();
    void deleteAction(QAction *a) { delete a; }
private:
    void createActions();
    QMenu *menus[2], *lastMenu;
    enum { num_builtins = 10 };
    QAction *activated, *highlighted, *builtins[num_builtins];
    QString statustip;
    bool m_onStatusTipTimerExecuted;
};

// Testing get/set functions
void tst_QMenu::getSetCheck()
{
    QMenu obj1;
    // QAction * QMenu::defaultAction()
    // void QMenu::setDefaultAction(QAction *)
    QAction *var1 = new QAction(0);
    obj1.setDefaultAction(var1);
    QCOMPARE(var1, obj1.defaultAction());
    obj1.setDefaultAction((QAction *)0);
    QCOMPARE((QAction *)0, obj1.defaultAction());
    delete var1;

    // QAction * QMenu::activeAction()
    // void QMenu::setActiveAction(QAction *)
    QAction *var2 = new QAction(0);
    obj1.setActiveAction(var2);
    QCOMPARE(var2, obj1.activeAction());
    obj1.setActiveAction((QAction *)0);
    QCOMPARE((QAction *)0, obj1.activeAction());
    delete var2;
}

tst_QMenu::tst_QMenu()
    : m_onStatusTipTimerExecuted(false)
{
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
}

void tst_QMenu::initTestCase()
{
    for (int i = 0; i < num_builtins; i++)
        builtins[i] = 0;
    for (int i = 0; i < 2; i++) {
        menus[i] = new QMenu;
        QObject::connect(menus[i], SIGNAL(triggered(QAction*)), this, SLOT(onActivated(QAction*)));
        QObject::connect(menus[i], SIGNAL(hovered(QAction*)), this, SLOT(onHighlighted(QAction*)));
    }
}

void tst_QMenu::cleanupTestCase()
{
    for (int i = 0; i < 2; i++)
        menus[i]->clear();
    for (int i = 0; i < num_builtins; i++) {
        bool menuAction = false;
        for (int j = 0; j < 2; ++j)
            if (menus[j]->menuAction() == builtins[i])
                menuAction = true;
        if (!menuAction)
            delete builtins[i];
    }
    delete menus[0];
    delete menus[1];
}

void tst_QMenu::init()
{
    activated = highlighted = 0;
    lastMenu = 0;
    m_onStatusTipTimerExecuted = false;
}

void tst_QMenu::createActions()
{
    if (!builtins[0])
        builtins[0] = new QAction("New", 0);
    menus[0]->addAction(builtins[0]);

    if (!builtins[1]) {
        builtins[1] = new QAction(0);
        builtins[1]->setSeparator(true);
    }
    menus[0]->addAction(builtins[1]);

    if (!builtins[2]) {
        builtins[2] = menus[1]->menuAction();
        builtins[2]->setText("&Open..");
        builtins[8] = new QAction("Close", 0);
        menus[1]->addAction(builtins[8]);
        builtins[9] = new QAction("Quit", 0);
        menus[1]->addAction(builtins[9]);
    }
    menus[0]->addAction(builtins[2]);

    if (!builtins[3])
        builtins[3] = new QAction("Open &as..", 0);
    menus[0]->addAction(builtins[3]);

    if (!builtins[4]) {
        builtins[4] = new QAction("Save", 0);
        builtins[4]->setEnabled(false);
    }
    menus[0]->addAction(builtins[4]);

    if (!builtins[5])
        builtins[5] = new QAction("Sa&ve as..", 0);
    menus[0]->addAction(builtins[5]);

    if (!builtins[6]) {
        builtins[6] = new QAction(0);
        builtins[6]->setSeparator(true);
    }
    menus[0]->addAction(builtins[6]);

    if (!builtins[7])
        builtins[7] = new QAction("Prin&t", 0);
    menus[0]->addAction(builtins[7]);
}

void tst_QMenu::onHighlighted(QAction *action)
{
    highlighted = action;
    lastMenu = qobject_cast<QMenu*>(sender());
}

void tst_QMenu::onActivated(QAction *action)
{
    activated = action;
    lastMenu = qobject_cast<QMenu*>(sender());
}

void tst_QMenu::onStatusMessageChanged(const QString &s)
{
    statustip=s;
}

void tst_QMenu::addActionsAndClear()
{
    QCOMPARE(menus[0]->actions().count(), 0);
    createActions();
    QCOMPARE(menus[0]->actions().count(), 8);
    menus[0]->clear();
    QCOMPARE(menus[0]->actions().count(), 0);
}

static void testFunction() { }

void tst_QMenu::addActionsConnect()
{
    QMenu menu;
    const QString text = QLatin1String("bla");
    const QIcon icon;
    menu.addAction(text, &menu, SLOT(deleteLater()));
    menu.addAction(text, &menu, &QMenu::deleteLater);
    menu.addAction(text, testFunction);
    menu.addAction(text, &menu, testFunction);
    menu.addAction(icon, text, &menu, SLOT(deleteLater()));
    menu.addAction(icon, text, &menu, &QMenu::deleteLater);
    menu.addAction(icon, text, testFunction);
    menu.addAction(icon, text, &menu, testFunction);
#ifndef QT_NO_SHORTCUT
    const QKeySequence keySequence(Qt::CTRL + Qt::Key_C);
    menu.addAction(text, &menu, SLOT(deleteLater()), keySequence);
    menu.addAction(text, &menu, &QMenu::deleteLater, keySequence);
    menu.addAction(text, testFunction, keySequence);
    menu.addAction(text, &menu, testFunction, keySequence);
    menu.addAction(icon, text, &menu, SLOT(deleteLater()), keySequence);
    menu.addAction(icon, text, &menu, &QMenu::deleteLater, keySequence);
    menu.addAction(icon, text, testFunction, keySequence);
    menu.addAction(icon, text, &menu, testFunction, keySequence);
#endif // !QT_NO_SHORTCUT
}

// We have a separate mouseActivation test for Windows mobile
#ifndef Q_OS_WINCE
void tst_QMenu::mouseActivation()
{
    QWidget topLevel;
    topLevel.resize(300, 200);
    centerOnScreen(&topLevel);
    QMenu menu(&topLevel);
    topLevel.show();
    menu.addAction("Menu Action");
    menu.move(topLevel.geometry().topRight() + QPoint(50, 0));
    menu.show();
    QTest::mouseClick(&menu, Qt::LeftButton, 0, menu.rect().center(), 300);
    QVERIFY(!menu.isVisible());

    //context menus can always be accessed with right click except on windows
    menu.show();
    QTest::mouseClick(&menu, Qt::RightButton, 0, menu.rect().center(), 300);
    QVERIFY(!menu.isVisible());

#ifdef Q_OS_WIN
    //on windows normal mainwindow menus Can only be accessed with left mouse button
    QMenuBar menubar;
    QMenu submenu("Menu");
    submenu.addAction("action");
    QAction *action = menubar.addMenu(&submenu);
    menubar.move(topLevel.geometry().topRight() + QPoint(300, 0));
    menubar.show();


    QTest::mouseClick(&menubar, Qt::LeftButton, 0, menubar.actionGeometry(action).center(), 300);
    QVERIFY(submenu.isVisible());
    QTest::mouseClick(&submenu, Qt::LeftButton, 0, QPoint(5, 5), 300);
    QVERIFY(!submenu.isVisible());

    QTest::mouseClick(&menubar, Qt::LeftButton, 0, menubar.actionGeometry(action).center(), 300);
    QVERIFY(submenu.isVisible());
    QTest::mouseClick(&submenu, Qt::RightButton, 0, QPoint(5, 5), 300);
    QVERIFY(submenu.isVisible());
#endif
}
#endif

void tst_QMenu::keyboardNavigation_data()
{
    QTest::addColumn<Qt::Key>("key");
    QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
    QTest::addColumn<int>("expected_action");
    QTest::addColumn<int>("expected_menu");
    QTest::addColumn<bool>("init");
    QTest::addColumn<bool>("expected_activated");
    QTest::addColumn<bool>("expected_highlighted");

    //test up and down (order is important here)
    QTest::newRow("data0") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 0 << 0 << true << false << true;
    QTest::newRow("data1") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 2 << 0 << false << false << true; //skips the separator
    QTest::newRow("data2") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 3 << 0 << false << false << true;

    if (QApplication::style()->styleHint(QStyle::SH_Menu_AllowActiveAndDisabled))
        QTest::newRow("data3_noMac") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 4 << 0 << false << false << true;
    else
        QTest::newRow("data3_Mac") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 5 << 0 << false << false << true;
    QTest::newRow("data4") << Qt::Key(Qt::Key_Up) << Qt::KeyboardModifiers(Qt::NoModifier) << 3 << 0 << false << false << true;
    QTest::newRow("data5") << Qt::Key(Qt::Key_Up) << Qt::KeyboardModifiers(Qt::NoModifier) << 2 << 0 << false << false << true;
    QTest::newRow("data6") << Qt::Key(Qt::Key_Right) << Qt::KeyboardModifiers(Qt::NoModifier) << 8 << 1 << false << false << true;
    QTest::newRow("data7") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 9 << 1 << false << false << true;
    QTest::newRow("data8") << Qt::Key(Qt::Key_Escape) << Qt::KeyboardModifiers(Qt::NoModifier) << 2 << 0 << false << false << false;
    QTest::newRow("data9") << Qt::Key(Qt::Key_Down) << Qt::KeyboardModifiers(Qt::NoModifier) << 3 << 0 << false << false<< true;
    QTest::newRow("data10") << Qt::Key(Qt::Key_Return) << Qt::KeyboardModifiers(Qt::NoModifier) << 3 << 0 << false << true << false;

    if (QGuiApplication::platformName().compare(QLatin1String("xcb"), Qt::CaseInsensitive)) {
        // Test shortcuts.
        QTest::newRow("shortcut0") << Qt::Key(Qt::Key_V) << Qt::KeyboardModifiers(Qt::AltModifier) << 5 << 0 << true << true << false;
    }
}

void tst_QMenu::keyboardNavigation()
{
    QFETCH(Qt::Key, key);
    QFETCH(Qt::KeyboardModifiers, modifiers);
    QFETCH(int, expected_action);
    QFETCH(int, expected_menu);
    QFETCH(bool, init);
    QFETCH(bool, expected_activated);
    QFETCH(bool, expected_highlighted);

    if (init) {
        lastMenu = menus[0];
        lastMenu->clear();
        createActions();
        lastMenu->popup(QPoint(0, 0));
    }

    QTest::keyClick(lastMenu, key, modifiers);
    if (expected_activated) {
#ifdef Q_OS_MAC
        QEXPECT_FAIL("shortcut0", "Shortcut navication fails, see QTBUG-23684", Continue);
#endif
        QCOMPARE(activated, builtins[expected_action]);
#ifndef Q_OS_MAC
        QEXPECT_FAIL("shortcut0", "QTBUG-22449: QMenu doesn't remove highlight if a menu item is activated by a shortcut", Abort);
#endif
        QCOMPARE(menus[expected_menu]->activeAction(), (QAction *)0);
    } else {
        QCOMPARE(menus[expected_menu]->activeAction(), builtins[expected_action]);
    }

    if (expected_highlighted)
        QCOMPARE(menus[expected_menu]->activeAction(), highlighted);
    else
        QCOMPARE(highlighted, (QAction *)0);
}

#ifdef Q_OS_MAC
QT_BEGIN_NAMESPACE
extern bool qt_tab_all_widgets(); // from qapplication.cpp
QT_END_NAMESPACE
#endif

void tst_QMenu::focus()
{
    QMenu menu;
    menu.addAction("One");
    menu.addAction("Two");
    menu.addAction("Three");

#ifdef Q_OS_MAC
    if (!qt_tab_all_widgets())
        QSKIP("Computer is currently set up to NOT tab to all widgets,"
             " this test assumes you can tab to all widgets");
#endif

    QWidget window;
    window.resize(300, 200);
    QPushButton button("Push me", &window);
    centerOnScreen(&window);
    window.show();
    qApp->setActiveWindow(&window);

    QVERIFY(button.hasFocus());
    QCOMPARE(QApplication::focusWidget(), (QWidget *)&button);
    QCOMPARE(QApplication::activeWindow(), &window);
    menu.move(window.geometry().topRight() + QPoint(50, 0));
    menu.show();
    QVERIFY(button.hasFocus());
    QCOMPARE(QApplication::focusWidget(), (QWidget *)&button);
    QCOMPARE(QApplication::activeWindow(), &window);
    menu.hide();
    QVERIFY(button.hasFocus());
    QCOMPARE(QApplication::focusWidget(), (QWidget *)&button);
    QCOMPARE(QApplication::activeWindow(), &window);
}

void tst_QMenu::overrideMenuAction()
{
    //test the override menu action by first creating an action to which we set its menu
    QMainWindow w;
    w.resize(300, 200);
    centerOnScreen(&w);

    QAction *aFileMenu = new QAction("&File", &w);
    w.menuBar()->addAction(aFileMenu);

    QMenu *m = new QMenu(&w);
    QAction *menuaction = m->menuAction();
    connect(m, SIGNAL(triggered(QAction*)), SLOT(onActivated(QAction*)));
    aFileMenu->setMenu(m); //this sets the override menu action for the QMenu
    QCOMPARE(m->menuAction(), aFileMenu);

    // On Mac and Windows CE, we need to create native key events to test menu
    // action activation, so skip this part of the test.
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINCE)
    QAction *aQuit = new QAction("Quit", &w);
    aQuit->setShortcut(QKeySequence("Ctrl+X"));
    m->addAction(aQuit);

    w.show();
    QApplication::setActiveWindow(&w);
    w.setFocus();
    QVERIFY(QTest::qWaitForWindowActive(&w));
    QVERIFY(w.hasFocus());

    //test of the action inside the menu
    QTest::keyClick(&w, Qt::Key_X, Qt::ControlModifier);
    QTRY_COMPARE(activated, aQuit);

    //test if the menu still pops out
    QTest::keyClick(&w, Qt::Key_F, Qt::AltModifier);
    QTRY_VERIFY(m->isVisible());
#endif

    delete aFileMenu;

    //after the deletion of the override menu action,
    //the menu should have its default menu action back
    QCOMPARE(m->menuAction(), menuaction);
}

void tst_QMenu::statusTip()
{
    //check that the statustip of actions inserted into the menu are displayed
    QMainWindow w;
    w.resize(300, 200);
    centerOnScreen(&w);
    connect(w.statusBar(), SIGNAL(messageChanged(QString)), SLOT(onStatusMessageChanged(QString)));; //creates the status bar
    QToolBar tb;
    QAction a("main action", &tb);
    a.setStatusTip("main action");
    QMenu m(&tb);
    QAction subact("sub action", &m);
    subact.setStatusTip("sub action");
    m.addAction(&subact);
    a.setMenu(&m);
    tb.addAction(&a);

    w.addToolBar(&tb);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QRect rect1 = tb.actionGeometry(&a);
    QToolButton *btn = qobject_cast<QToolButton*>(tb.childAt(rect1.center()));

    QVERIFY(btn != NULL);

    //because showMenu calls QMenu::exec, we need to use a singleshot
    //to continue the test
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &tst_QMenu::onStatusTipTimer);
    timer.setInterval(200);
    timer.start();
    btn->showMenu();
    QVERIFY(m_onStatusTipTimerExecuted);
    QVERIFY(statustip.isEmpty());
}

//2nd part of the test
void tst_QMenu::onStatusTipTimer()
{
    QMenu *menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    QVERIFY(menu != 0);
    QVERIFY(menu->isVisible());
    QTest::keyClick(menu, Qt::Key_Down);

    //we store the statustip to press escape in any case
    //otherwise, if the test fails it blocks (never gets out of QMenu::exec
    const QString st=statustip;

    menu->close(); //goes out of the menu

    QCOMPARE(st, QString("sub action"));
    QVERIFY(!menu->isVisible());
    m_onStatusTipTimerExecuted = true;
}

void tst_QMenu::widgetActionFocus()
{
    //test if the focus is correctly handled with a QWidgetAction
    QMenu m;
    QListWidget *l = new QListWidget(&m);
    for (int i = 1; i<3 ; i++)
        l->addItem(QString("item%1").arg(i));
    QWidgetAction *wa = new QWidgetAction(&m);
    wa->setDefaultWidget(l);
    m.addAction(wa);
    m.setActiveAction(wa);
    l->setFocus(); //to ensure it has primarily the focus
    QAction *menuitem1=m.addAction("menuitem1");
    QAction *menuitem2=m.addAction("menuitem2");

    m.popup(QPoint());

    QVERIFY(m.isVisible());
    QVERIFY(l->hasFocus());
    QVERIFY(l->currentItem());
    QCOMPARE(l->currentItem()->text(), QString("item1"));

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
    QVERIFY(l->currentItem());
    QCOMPARE(l->currentItem()->text(), QString("item2"));

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
    QVERIFY(m.hasFocus());
    QCOMPARE(m.activeAction(), menuitem1);

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Down);
    QVERIFY(m.hasFocus());
    QCOMPARE(m.activeAction(), menuitem2);

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
    QVERIFY(m.hasFocus());
    QCOMPARE(m.activeAction(), menuitem1);

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up);
    QVERIFY(l->hasFocus());
    QCOMPARE(m.activeAction(), (QAction *)wa);
}

static QMenu *getTornOffMenu()
{
    foreach (QWidget *w, QApplication::allWidgets()) {
        if (w->isVisible() && w->inherits("QTornOffMenu"))
            return static_cast<QMenu *>(w);
    }
    return Q_NULLPTR;
}

void tst_QMenu::tearOff()
{
    QWidget widget;
    QScopedPointer<QMenu> menu(new QMenu(&widget));
    QVERIFY(!menu->isTearOffEnabled()); //default value
    menu->setTearOffEnabled(true);
    menu->addAction("aaa");
    menu->addAction("bbb");
    QVERIFY(menu->isTearOffEnabled());

    widget.resize(300, 200);
    centerOnScreen(&widget);
    widget.show();
    widget.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&widget));
    menu->popup(widget.geometry().topRight() + QPoint(50, 0));
    QVERIFY(QTest::qWaitForWindowActive(menu.data()));
    QVERIFY(!menu->isTearOffMenuVisible());

    QTest::mouseClick(menu.data(), Qt::LeftButton, 0, QPoint(3, 3), 10);
    QTRY_VERIFY(menu->isTearOffMenuVisible());
    QPointer<QMenu> torn = getTornOffMenu();
    QVERIFY(torn);
    QVERIFY(torn->isVisible());

    menu->hideTearOffMenu();
    QVERIFY(!menu->isTearOffMenuVisible());
    QVERIFY(!torn->isVisible());

#ifndef QT_NO_CURSOR
    // Test under-mouse positioning
    menu->showTearOffMenu();
    torn = getTornOffMenu();
    QVERIFY(torn);
    QVERIFY(torn->isVisible());
    QVERIFY(menu->isTearOffMenuVisible());
    // Some platforms include the window title bar in its geometry.
    QTRY_COMPARE(torn->windowHandle()->position(), QCursor::pos());

    menu->hideTearOffMenu();
    QVERIFY(!menu->isTearOffMenuVisible());
    QVERIFY(!torn->isVisible());

    // Test custom positioning
    const QPoint &pos = QCursor::pos() / 2 + QPoint(10, 10);
    menu->showTearOffMenu(pos);
    torn = getTornOffMenu();
    QVERIFY(torn);
    QVERIFY(torn->isVisible());
    QVERIFY(menu->isTearOffMenuVisible());
    // Some platforms include the window title bar in its geometry.
    QTRY_COMPARE(torn->windowHandle()->position(), pos);
#endif // QT_NO_CURSOR
}

void tst_QMenu::layoutDirection()
{
    QMainWindow win;
    win.setLayoutDirection(Qt::RightToLeft);
    win.resize(300, 200);
    centerOnScreen(&win);

    QMenu menu(&win);
    menu.addAction("foo");
    menu.move(win.geometry().topRight() + QPoint(50, 0));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QCOMPARE(menu.layoutDirection(), Qt::RightToLeft);
    menu.close();

    menu.setParent(0);
    menu.move(win.geometry().topRight() + QPoint(50, 0));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QCOMPARE(menu.layoutDirection(), QApplication::layoutDirection());
    menu.close();

    //now the menubar
    QAction *action = win.menuBar()->addMenu(&menu);
    win.menuBar()->setActiveAction(action);
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QCOMPARE(menu.layoutDirection(), Qt::RightToLeft);
}

void tst_QMenu::task208001_stylesheet()
{
    //test if it crash
    QMainWindow main;
    main.setStyleSheet("QMenu [title =\"File\"] { color: red;}");
    main.menuBar()->addMenu("File");
}

void tst_QMenu::activeSubMenuPosition()
{
    QPushButton lab("subMenuPosition test");

    QMenu *sub = new QMenu("Submenu", &lab);
    sub->addAction("Sub-Item1");
    QAction *subAction = sub->addAction("Sub-Item2");

    QMenu *main = new QMenu("Menu-Title", &lab);
    (void)main->addAction("Item 1");
    QAction *menuAction = main->addMenu(sub);
    (void)main->addAction("Item 3");
    (void)main->addAction("Item 4");

    main->setActiveAction(menuAction);
    sub->setActiveAction(subAction);
    main->popup(QPoint(200,200));

    QVERIFY(main->isVisible());
    QCOMPARE(main->activeAction(), menuAction);
    QVERIFY(sub->isVisible());
    QVERIFY(sub->pos() != QPoint(0,0));
    // well, it's enough to check the pos is not (0,0) but it's more safe
    // to check that submenu is to the right of the main menu too.
#ifndef Q_OS_WINCE_WM
    QVERIFY(sub->pos().x() > main->pos().x());
    QCOMPARE(sub->activeAction(), subAction);
#endif
}

// QTBUG-49588, QTBUG-48396: activeSubMenuPositionExec() is the same as
// activeSubMenuPosition(), but uses QMenu::exec(), which produces a different
// sequence of events. Verify that the sub menu is positioned to the right of the
// main menu.
class SubMenuPositionExecMenu : public QMenu
{
    Q_OBJECT
public:
    SubMenuPositionExecMenu() : QMenu("Menu-Title"), m_timerId(-1), m_timerTick(0)
    {
        addAction("Item 1");
        m_subMenu = addMenu("Submenu");
        m_subAction = m_subMenu->addAction("Sub-Item1");
        setActiveAction(m_subMenu->menuAction());
    }

protected:
    void showEvent(QShowEvent *e) Q_DECL_OVERRIDE
    {
        QVERIFY(m_subMenu->isVisible());
        QVERIFY2(m_subMenu->x() > x(),
                 (QByteArray::number(m_subMenu->x()) + ' ' + QByteArray::number(x())).constData());
        m_timerId = startTimer(50);
        QMenu::showEvent(e);
    }

    void timerEvent(QTimerEvent *e) Q_DECL_OVERRIDE
    {
        if (e->timerId() == m_timerId) {
            switch (m_timerTick++) {
            case 0:
                m_subMenu->close();
                break;
            case 1:
                close();
                break;
            }
        }
    }

private:
    int m_timerId;
    int m_timerTick;
    QMenu *m_subMenu;
    QAction *m_subAction;
};

void tst_QMenu::activeSubMenuPositionExec()
{
#ifndef Q_OS_WINCE
    SubMenuPositionExecMenu menu;
    menu.exec(QGuiApplication::primaryScreen()->availableGeometry().center());
#endif // !Q_OS_WINCE
}

void tst_QMenu::task242454_sizeHint()
{
    QMenu menu;
    QString s = QLatin1String("foo\nfoo\nfoo\nfoo");
    menu.addAction(s);
    QVERIFY(menu.sizeHint().width() > menu.fontMetrics().boundingRect(QRect(), Qt::TextSingleLine, s).width());
}

class Menu : public QMenu
{
    Q_OBJECT
public slots:
    void clear()
    {
        QMenu::clear();
    }
};

void tst_QMenu::task176201_clear()
{
    //this test used to crash
    Menu menu;
    QAction *action = menu.addAction("test");
    menu.connect(action, SIGNAL(triggered()), SLOT(clear()));
    menu.popup(QPoint());
    QTest::mouseClick(&menu, Qt::LeftButton, 0, menu.rect().center());
}

void tst_QMenu::task250673_activeMultiColumnSubMenuPosition()
{
    class MyMenu : public QMenu
    {
    public:
        int columnCount() const { return QMenu::columnCount(); }
    };

    QMenu sub;

    if (sub.style()->styleHint(QStyle::SH_Menu_Scrollable, 0, &sub)) {
        //the style prevents the menus from getting columns
        QSKIP("the style doesn't support multiple columns, it makes the menu scrollable");
    }

    sub.addAction("Sub-Item1");
    QAction *subAction = sub.addAction("Sub-Item2");

    MyMenu main;
    main.addAction("Item 1");
    QAction *menuAction = main.addMenu(&sub);
    main.popup(QPoint(200,200));

    uint i = 2;
    while (main.columnCount() < 2) {
        main.addAction(QString("Item %1").arg(i));
        ++i;
        QVERIFY(i<1000);
    }
    main.setActiveAction(menuAction);
    sub.setActiveAction(subAction);

    QVERIFY(main.isVisible());
    QCOMPARE(main.activeAction(), menuAction);
    QVERIFY(sub.isVisible());
    QVERIFY(sub.pos().x() > main.pos().x());

    const int subMenuOffset = main.style()->pixelMetric(QStyle::PM_SubMenuOverlap, 0, &main);
    QVERIFY((sub.geometry().left() - subMenuOffset + 5) < main.geometry().right());
}

void tst_QMenu::task256918_setFont()
{
    QMenu menu;
    QAction *action = menu.addAction("foo");
    QFont f;
    f.setPointSize(30);
    action->setFont(f);
    centerOnScreen(&menu, QSize(120, 40));
    menu.show(); //ensures that the actiongeometry are calculated
    QVERIFY(menu.actionGeometry(action).height() > f.pointSize());
}

void tst_QMenu::menuSizeHint()
{
    QMenu menu;
    //this is a list of arbitrary strings so that we check the geometry
    QStringList list = QStringList() << "trer" << "ezrfgtgvqd" << "sdgzgzerzerzer" << "eerzertz"  << "er";
    foreach (QString str, list)
        menu.addAction(str);

    int left, top, right, bottom;
    menu.getContentsMargins(&left, &top, &right, &bottom);
    const int panelWidth = menu.style()->pixelMetric(QStyle::PM_MenuPanelWidth, 0, &menu);
    const int hmargin = menu.style()->pixelMetric(QStyle::PM_MenuHMargin, 0, &menu),
    vmargin = menu.style()->pixelMetric(QStyle::PM_MenuVMargin, 0, &menu);

    int maxWidth =0;
    QRect result;
    foreach (QAction *action, menu.actions()) {
        maxWidth = qMax(maxWidth, menu.actionGeometry(action).width());
        result |= menu.actionGeometry(action);
        QCOMPARE(result.x(), left + hmargin + panelWidth);
        QCOMPARE(result.y(), top + vmargin + panelWidth);
    }

    QStyleOption opt(0);
    opt.rect = menu.rect();
    opt.state = QStyle::State_None;

    QSize resSize = QSize(result.x(), result.y()) + result.size() + QSize(hmargin + right + panelWidth, vmargin + top + panelWidth);

    resSize = menu.style()->sizeFromContents(QStyle::CT_Menu, &opt,
                                    resSize.expandedTo(QApplication::globalStrut()), &menu);

    QCOMPARE(resSize, menu.sizeHint());
}

class Menu258920 : public QMenu
{
    Q_OBJECT
public slots:
    void paintEvent(QPaintEvent *e)
    {
        QMenu::paintEvent(e);
        painted = true;
    }

public:
    bool painted;
};

// Mouse move related signals for Windows Mobile unavailable
#ifndef Q_OS_WINCE
void tst_QMenu::task258920_mouseBorder()
{
    Menu258920 menu;
    // For styles which inherit from QWindowsStyle, styleHint(QStyle::SH_Menu_MouseTracking) is true.
    menu.setMouseTracking(true);
    QAction *action = menu.addAction("test");

    const QPoint center = QApplication::desktop()->availableGeometry().center();
#ifndef QT_NO_CURSOR
    QCursor::setPos(center - QPoint(100, 100)); // Mac: Ensure cursor is outside
#endif
    menu.popup(center);
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QTest::qWait(100);
    QRect actionRect = menu.actionGeometry(action);
    const QPoint actionCenter = actionRect.center();
    QTest::mouseMove(&menu, actionCenter - QPoint(-10, 0));
    QTest::qWait(30);
    QTest::mouseMove(&menu, actionCenter);
    QTest::qWait(30);
    QTest::mouseMove(&menu, actionCenter + QPoint(10, 0));
    QTRY_COMPARE(action, menu.activeAction());
    menu.painted = false;
    QTest::mouseMove(&menu, QPoint(actionRect.center().x(), actionRect.bottom() + 1));
    QTest::qWait(30);
    QTRY_COMPARE(static_cast<QAction*>(0), menu.activeAction());
    QTRY_VERIFY(menu.painted);
}
#endif

void tst_QMenu::setFixedWidth()
{
    QMenu menu;
    menu.addAction("action");
    menu.setFixedWidth(300);
    //the sizehint should reflect the minimumwidth because the action will try to
    //get as much space as possible
    QCOMPARE(menu.sizeHint().width(), menu.minimumWidth());
}

void tst_QMenu::deleteActionInTriggered()
{
    // should not crash
    QMenu m;
    QObject::connect(&m, SIGNAL(triggered(QAction*)), this, SLOT(deleteAction(QAction*)));
    QPointer<QAction> a = m.addAction("action");
    a.data()->trigger();
    QVERIFY(!a);
}

class PopulateOnAboutToShowTestMenu : public QMenu {
    Q_OBJECT
public:
    explicit PopulateOnAboutToShowTestMenu(QWidget *parent = 0);

public slots:
    void populateMenu();
};

PopulateOnAboutToShowTestMenu::PopulateOnAboutToShowTestMenu(QWidget *parent) : QMenu(parent)
{
    connect(this, SIGNAL(aboutToShow()), this, SLOT(populateMenu()));
}

void PopulateOnAboutToShowTestMenu::populateMenu()
{
    // just adds 3 dummy actions and a separator.
    addAction("Foo");
    addAction("Bar");
    addAction("FooBar");
    addSeparator();
}

static inline QByteArray msgGeometryIntersects(const QRect &r1, const QRect &r2)
{
    QString result;
    QDebug(&result) << r1 << "intersects" << r2;
    return result.toLocal8Bit();
}

void tst_QMenu::pushButtonPopulateOnAboutToShow()
{
    QPushButton b("Test PushButton");
    b.setWindowFlags(Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint);

    QMenu *buttonMenu= new PopulateOnAboutToShowTestMenu(&b);
    b.setMenu(buttonMenu);
    const int scrNumber = QApplication::desktop()->screenNumber(&b);
    b.show();
    const QRect screen = QApplication::desktop()->screenGeometry(scrNumber);

    QRect desiredGeometry = b.geometry();
    desiredGeometry.moveTopLeft(QPoint(screen.x() + 10, screen.bottom() - b.height() - 5));

    b.setGeometry(desiredGeometry);
    QVERIFY(QTest::qWaitForWindowExposed(&b));

    if (b.geometry() != desiredGeometry) {
        // We are trying to put the button very close to the edge of the screen,
        // explicitly to test behavior when the popup menu goes off the screen.
        // However a modern window manager is quite likely to reject this requested geometry
        // (kwin in kde4 does, for example, since the button would probably appear behind
        // or partially behind the taskbar).
        // Your best bet is to run this test _without_ a WM.
        QSKIP("Your window manager won't allow a window against the bottom of the screen");
    }

    QTimer::singleShot(300, buttonMenu, SLOT(hide()));
    QTest::mouseClick(&b, Qt::LeftButton, Qt::NoModifier, b.rect().center());
    QVERIFY2(!buttonMenu->geometry().intersects(b.geometry()), msgGeometryIntersects(buttonMenu->geometry(), b.geometry()));

    // note: we're assuming that, if we previously got the desired geometry, we'll get it here too
    b.move(10, screen.bottom()-buttonMenu->height()-5);
    QTimer::singleShot(300, buttonMenu, SLOT(hide()));
    QTest::mouseClick(&b, Qt::LeftButton, Qt::NoModifier, b.rect().center());
    QVERIFY2(!buttonMenu->geometry().intersects(b.geometry()), msgGeometryIntersects(buttonMenu->geometry(), b.geometry()));
}

void tst_QMenu::QTBUG7907_submenus_autoselect()
{
    QMenu menu("Test Menu");
    QMenu set1("Setting1");
    QMenu set2("Setting2");
    QMenu subset("Subsetting");
    subset.addAction("Values");
    set1.addMenu(&subset);
    menu.addMenu(&set1);
    menu.addMenu(&set2);
    centerOnScreen(&menu, QSize(120, 100));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, QPoint(5,5) );
    QTest::qWait(500);
    QVERIFY(!subset.isVisible());
}

void tst_QMenu::QTBUG7411_submenus_activate()
{
    QMenu menu("Test Menu");
    QAction *act = menu.addAction("foo");
    QMenu sub1("&sub1");
    sub1.addAction("foo");
    sub1.setTitle("&sub1");
    QAction *act1 = menu.addMenu(&sub1);
    centerOnScreen(&menu, QSize(120, 100));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    menu.setActiveAction(act);
    QTest::keyPress(&menu, Qt::Key_Down);
    QCOMPARE(menu.activeAction(), act1);
    QVERIFY(!sub1.isVisible());
    QTest::keyPress(&menu, Qt::Key_S);
    QTRY_VERIFY(sub1.isVisible());
}

void tst_QMenu::QTBUG30595_rtl_submenu()
{
    QMenu menu("Test Menu");
    menu.setLayoutDirection(Qt::RightToLeft);
    QMenu sub("&sub");
    sub.addAction("bar");
    sub.setTitle("&sub");
    menu.addMenu(&sub);
    centerOnScreen(&menu, QSize(120, 40));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, QPoint(5,5) );
    QTRY_VERIFY(sub.isVisible());
    QVERIFY2(sub.pos().x() < menu.pos().x(), QByteArray::number(sub.pos().x()) + QByteArrayLiteral(" not less than ") + QByteArray::number(menu.pos().x()));
}

void tst_QMenu::QTBUG20403_nested_popup_on_shortcut_trigger()
{
    QMenu menu("Test Menu");
    QMenu sub1("&sub1");
    QMenu subsub1("&subsub1");
    subsub1.addAction("foo");
    sub1.addMenu(&subsub1);
    menu.addMenu(&sub1);
    centerOnScreen(&menu, QSize(120, 100));
    menu.show();
    QVERIFY(QTest::qWaitForWindowExposed(&menu));
    QTest::keyPress(&menu, Qt::Key_S);
    QTest::qWait(100); // 20ms delay with previous behavior
    QTRY_VERIFY(sub1.isVisible());
    QVERIFY(!subsub1.isVisible());
}

class MyWidget : public QWidget
{
public:
    MyWidget(QWidget *parent) :
        QWidget(parent),
        move(0), enter(0), leave(0)
    {
        setMinimumSize(100, 100);
        setMouseTracking(true);
    }

    bool event(QEvent *e) Q_DECL_OVERRIDE
    {
        switch (e->type()) {
        case QEvent::MouseMove:
            ++move;
            break;
        case QEvent::Enter:
            ++enter;
            break;
        case QEvent::Leave:
            ++leave;
            break;
        default:
            break;
        }
        return QWidget::event(e);
    }

    int move, enter, leave;
};

#ifndef QT_NO_CURSOR
void tst_QMenu::QTBUG47515_widgetActionEnterLeave()
{
    if (QGuiApplication::platformName() == QLatin1String("cocoa"))
        QSKIP("This test fails on OS X on CI");

    const QPoint center = QGuiApplication::primaryScreen()->availableGeometry().center();
    const QPoint cursorPos = center - QPoint(100, 100);

    QScopedPointer<QMenu> menu1(new QMenu("Menu1"));
    QScopedPointer<QMenu> menu2(new QMenu("Menu2"));

    QWidgetAction *wA1 = new QWidgetAction(menu1.data());
    MyWidget *w1 = new MyWidget(menu1.data());
    wA1->setDefaultWidget(w1);

    QWidgetAction *wA2 = new QWidgetAction(menu2.data());
    MyWidget *w2 = new MyWidget(menu2.data());
    wA2->setDefaultWidget(w2);

    QAction *nextMenuAct = menu1->addMenu(menu2.data());

    menu1->addAction(wA1);
    menu2->addAction(wA2);

    // Root menu
    {
        QCursor::setPos(cursorPos);
        QCoreApplication::processEvents();

        menu1->popup(center);
        QVERIFY(QTest::qWaitForWindowExposed(menu1.data()));

        QCursor::setPos(w1->mapToGlobal(w1->rect().center()));
        QVERIFY(w1->isVisible());
        QTRY_COMPARE(w1->leave, 0);
        QTRY_COMPARE(w1->enter, 1);

        // Check whether leave event is not delivered on mouse move
        w1->move = 0;
        QCursor::setPos(w1->mapToGlobal(w1->rect().center()) + QPoint(1, 1));
        QTRY_COMPARE(w1->move, 1);
        QTRY_COMPARE(w1->leave, 0);
        QTRY_COMPARE(w1->enter, 1);

        QCursor::setPos(cursorPos);
        QTRY_COMPARE(w1->leave, 1);
        QTRY_COMPARE(w1->enter, 1);
    }

    // Submenu
    {
        menu1->setActiveAction(nextMenuAct);
        QVERIFY(QTest::qWaitForWindowExposed(menu2.data()));

        QCursor::setPos(w2->mapToGlobal(w2->rect().center()));
        QVERIFY(w2->isVisible());
        QTRY_COMPARE(w2->leave, 0);
        QTRY_COMPARE(w2->enter, 1);

        // Check whether leave event is not delivered on mouse move
        w2->move = 0;
        QCursor::setPos(w2->mapToGlobal(w2->rect().center()) + QPoint(1, 1));
        QTRY_COMPARE(w2->move, 1);
        QTRY_COMPARE(w2->leave, 0);
        QTRY_COMPARE(w2->enter, 1);

        QCursor::setPos(cursorPos);
        QTRY_COMPARE(w2->leave, 1);
        QTRY_COMPARE(w2->enter, 1);
    }
}
#endif // !QT_NO_CURSOR

class MyMenu : public QMenu
{
    Q_OBJECT
public:
    MyMenu() : m_currentIndex(0)
    {
        for (int i = 0; i < 2; ++i)
            dialogActions[i] = addAction( QString("dialog %1").arg(i), dialogs + i, SLOT(exec()));
    }

    void activateAction(int index)
    {
        m_currentIndex = index;
        popup(QPoint());
        QVERIFY(QTest::qWaitForWindowExposed(this));
        setActiveAction(dialogActions[index]);
        QTimer::singleShot(500, this, SLOT(checkVisibility()));
        QTest::keyClick(this, Qt::Key_Enter); //activation
    }

public slots:
    void activateLastAction()
    {
        activateAction(1);
    }

    void checkVisibility()
    {
        QTRY_VERIFY(dialogs[m_currentIndex].isVisible());
        if (m_currentIndex == 1) {
            QApplication::closeAllWindows(); //this is the end of the test
        }
    }

private:
    QAction *dialogActions[2];
    QDialog dialogs[2];
    int m_currentIndex;
};

void tst_QMenu::QTBUG_10735_crashWithDialog()
{
    MyMenu menu;

    QTimer::singleShot(1000, &menu, SLOT(activateLastAction()));
    menu.activateAction(0);
}

#ifdef Q_OS_MAC
void tst_QMenu::QTBUG_37933_ampersands_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("visibleTitle");
    QTest::newRow("simple") << QString("Test") << QString("Test");
    QTest::newRow("ampersand") << QString("&Test") << QString("Test");
    QTest::newRow("double_ampersand") << QString("&Test && more") << QString("Test & more");
    QTest::newRow("ampersand_in_parentheses") << QString("Test(&T) (&&) more") << QString("Test (&) more");
    QTest::newRow("ampersand_in_parentheses_after_space") << QString("Test (&T)") << QString("Test");
    QTest::newRow("ampersand_in_parentheses_after_spaces") << QString("Test  (&T)") << QString("Test");
    QTest::newRow("ampersand_in_parentheses_before_space") << QString("Test(&T) ") << QString("Test ");
    QTest::newRow("only_ampersand_in_parentheses") << QString("(&T)") << QString("");
    QTest::newRow("only_ampersand_in_parentheses_after_space") << QString(" (&T)") << QString("");
    QTest::newRow("parentheses_after_space") << QString(" (Dummy)") << QString(" (Dummy)");
    QTest::newRow("ampersand_after_space") << QString("About &Qt Project") << QString("About Qt Project");
}

void tst_qmenu_QTBUG_37933_ampersands();

void tst_QMenu::QTBUG_37933_ampersands()
{
    // external in .mm file
    tst_qmenu_QTBUG_37933_ampersands();
}
#endif

QTEST_MAIN(tst_QMenu)
#include "tst_qmenu.moc"
