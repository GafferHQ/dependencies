/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2012 Thorbjørn Lund Martsum - tmartsum[at]gmail.com
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
#include <QStandardItemModel>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QProxyStyle>

#include <qabstractitemmodel.h>
#include <qapplication.h>
#include <qheaderview.h>
#include <private/qheaderview_p.h>
#include <qitemdelegate.h>
#include <qtreewidget.h>
#include <qdebug.h>

typedef QList<int> IntList;

typedef QList<bool> BoolList;

class TestStyle : public QProxyStyle
{
public:
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        if (element == CE_HeaderSection) {
            if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option))
                lastPosition = header->position;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }
    mutable QStyleOptionHeader::SectionPosition lastPosition;
};

class protected_QHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    protected_QHeaderView(Qt::Orientation orientation) : QHeaderView(orientation) {
        resizeSections();
    };

    void testEvent();
    void testhorizontalOffset();
    void testverticalOffset();
    void testVisualRegionForSelection();
    friend class tst_QHeaderView;
};

class XResetModel : public QStandardItemModel
{
    virtual bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex())
    {
        blockSignals(true);
        bool r = QStandardItemModel::removeRows(row, count, parent);
        blockSignals(false);
        emit reset();
        return r;
    }
    virtual bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex())
    {
        blockSignals(true);
        bool r = QStandardItemModel::insertRows(row, count, parent);
        blockSignals(false);
        emit reset();
        return r;
    }
};

class tst_QHeaderView : public QObject
{
    Q_OBJECT

public:
    tst_QHeaderView();
    virtual ~tst_QHeaderView();

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void getSetCheck();
    void visualIndex();

    void visualIndexAt_data();
    void visualIndexAt();

    void noModel();
    void emptyModel();
    void removeRows();
    void removeCols();

    void clickable();
    void movable();
    void hidden();
    void stretch();

    void sectionSize_data();
    void sectionSize();

    void length();
    void offset();
    void sectionSizeHint();
    void logicalIndex();
    void logicalIndexAt();
    void swapSections();

    void moveSection_data();
    void moveSection();

    void resizeMode();

    void resizeSection_data();
    void resizeSection();

    void resizeAndMoveSection_data();
    void resizeAndMoveSection();
    void resizeHiddenSection_data();
    void resizeHiddenSection();
    void resizeAndInsertSection_data();
    void resizeAndInsertSection();
    void resizeWithResizeModes_data();
    void resizeWithResizeModes();
    void moveAndInsertSection_data();
    void moveAndInsertSection();
    void highlightSections();
    void showSortIndicator();
    void sortIndicatorTracking();
    void removeAndInsertRow();
    void unhideSection();
    void testEvent();
    void headerDataChanged();
    void currentChanged();
    void horizontalOffset();
    void verticalOffset();
    void stretchSectionCount();
    void hiddenSectionCount();
    void focusPolicy();
    void moveSectionAndReset();
    void moveSectionAndRemove();
    void saveRestore();
    void defaultSectionSizeTest();
    void defaultSectionSizeTestStyles();

    void defaultAlignment_data();
    void defaultAlignment();

    void globalResizeMode_data();
    void globalResizeMode();

    void sectionPressedSignal_data();
    void sectionPressedSignal();
    void sectionClickedSignal_data() { sectionPressedSignal_data(); }
    void sectionClickedSignal();

    void defaultSectionSize_data();
    void defaultSectionSize();

    void oneSectionSize();

    void hideAndInsert_data();
    void hideAndInsert();

    void removeSection();
    void preserveHiddenSectionWidth();
    void invisibleStretchLastSection();
    void noSectionsWithNegativeSize();

    void emptySectionSpan();
    void task236450_hidden_data();
    void task236450_hidden();
    void task248050_hideRow();
    void QTBUG6058_reset();
    void QTBUG7833_sectionClicked();
    void QTBUG8650_crashOnInsertSections();
    void QTBUG12268_hiddenMovedSectionSorting();
    void QTBUG14242_hideSectionAutoSize();
    void QTBUG50171_visualRegionForSwappedItems();
    void ensureNoIndexAtLength();
    void offsetConsistent();

    void initialSortOrderRole();

    void logicalIndexAtTest_data()   { setupTestData(); }
    void visualIndexAtTest_data()    { setupTestData(); }
    void hideShowTest_data()         { setupTestData(); }
    void swapSectionsTest_data()     { setupTestData(); }
    void moveSectionTest_data()      { setupTestData(); }
    void defaultSizeTest_data()      { setupTestData(); }
    void removeTest_data()           { setupTestData(true); }
    void insertTest_data()           { setupTestData(true); }
    void mixedTests_data()           { setupTestData(true); }
    void resizeToContentTest_data()  { setupTestData(); }
    void logicalIndexAtTest();
    void visualIndexAtTest();
    void hideShowTest();
    void swapSectionsTest();
    void moveSectionTest();
    void defaultSizeTest();
    void removeTest();
    void insertTest();
    void mixedTests();
    void resizeToContentTest();
    void testStreamWithHide();
    void testStylePosition();

    void sizeHintCrash();

protected:
    void setupTestData(bool use_reset_model = false);
    void additionalInit();
    void calculateAndCheck(int cppline, const int precalced_comparedata[]);

    QWidget *topLevel;
    QHeaderView *view;
    QStandardItemModel *model;
    QTableView *m_tableview;
    bool m_using_reset_model;
    QElapsedTimer timer;
};

class QtTestModel: public QAbstractTableModel
{

Q_OBJECT

public:
    QtTestModel(QObject *parent = 0): QAbstractTableModel(parent),
       cols(0), rows(0), wrongIndex(false) {}
    int rowCount(const QModelIndex&) const { return rows; }
    int columnCount(const QModelIndex&) const { return cols; }
    bool isEditable(const QModelIndex &) const { return true; }

    QVariant data(const QModelIndex &idx, int) const
    {
        if (idx.row() < 0 || idx.column() < 0 || idx.column() >= cols || idx.row() >= rows) {
            wrongIndex = true;
            qWarning("Invalid modelIndex [%d,%d,%p]", idx.row(), idx.column(), idx.internalPointer());
        }
        return QString("[%1,%2,%3]").arg(idx.row()).arg(idx.column()).arg(0);//idx.data());
    }

    void insertOneColumn(int col)
    {
        beginInsertColumns(QModelIndex(), col, col);
        --cols;
        endInsertColumns();
    }

    void removeLastRow()
    {
        beginRemoveRows(QModelIndex(), rows - 1, rows - 1);
        --rows;
        endRemoveRows();
    }

    void removeAllRows()
    {
        beginRemoveRows(QModelIndex(), 0, rows - 1);
        rows = 0;
        endRemoveRows();
    }

    void removeOneColumn(int col)
    {
        beginRemoveColumns(QModelIndex(), col, col);
        --cols;
        endRemoveColumns();
    }

    void removeLastColumn()
    {
        beginRemoveColumns(QModelIndex(), cols - 1, cols - 1);
        --cols;
        endRemoveColumns();
    }

    void removeAllColumns()
    {
        beginRemoveColumns(QModelIndex(), 0, cols - 1);
        cols = 0;
        endRemoveColumns();
    }

    void cleanup()
    {
        cols = 3;
        rows = 3;
        emit layoutChanged();
    }

    int cols, rows;
    mutable bool wrongIndex;
};

// Testing get/set functions
void tst_QHeaderView::getSetCheck()
{
    protected_QHeaderView obj1(Qt::Horizontal);
    // bool QHeaderView::highlightSections()
    // void QHeaderView::setHighlightSections(bool)
    obj1.setHighlightSections(false);
    QCOMPARE(false, obj1.highlightSections());
    obj1.setHighlightSections(true);
    QCOMPARE(true, obj1.highlightSections());

    // bool QHeaderView::stretchLastSection()
    // void QHeaderView::setStretchLastSection(bool)
    obj1.setStretchLastSection(false);
    QCOMPARE(false, obj1.stretchLastSection());
    obj1.setStretchLastSection(true);
    QCOMPARE(true, obj1.stretchLastSection());

    // int QHeaderView::defaultSectionSize()
    // void QHeaderView::setDefaultSectionSize(int)
    obj1.setDefaultSectionSize(-1);
    QVERIFY(obj1.defaultSectionSize() >= 0);
    obj1.setDefaultSectionSize(0);
    QCOMPARE(0, obj1.defaultSectionSize());
    obj1.setDefaultSectionSize(99999);
    QCOMPARE(99999, obj1.defaultSectionSize());

    // int QHeaderView::minimumSectionSize()
    // void QHeaderView::setMinimumSectionSize(int)
    obj1.setMinimumSectionSize(-1);
    QVERIFY(obj1.minimumSectionSize() >= 0);
    obj1.setMinimumSectionSize(0);
    QCOMPARE(0, obj1.minimumSectionSize());
    obj1.setMinimumSectionSize(99999);
    QCOMPARE(99999, obj1.minimumSectionSize());
    obj1.setMinimumSectionSize(-1);
    QVERIFY(obj1.minimumSectionSize() < 100);

    // int QHeaderView::offset()
    // void QHeaderView::setOffset(int)
    obj1.setOffset(0);
    QCOMPARE(0, obj1.offset());
    obj1.setOffset(INT_MIN);
    QCOMPARE(INT_MIN, obj1.offset());
    obj1.setOffset(INT_MAX);
    QCOMPARE(INT_MAX, obj1.offset());

}

tst_QHeaderView::tst_QHeaderView()
{
    qRegisterMetaType<int>("Qt::SortOrder");
}

tst_QHeaderView::~tst_QHeaderView()
{
}

void tst_QHeaderView::initTestCase()
{
#ifdef Q_OS_WINCE //disable magic for WindowsCE
    qApp->setAutoMaximizeThreshold(-1);
#endif
    m_tableview = new QTableView();
}

void tst_QHeaderView::cleanupTestCase()
{
    delete m_tableview;
}

void tst_QHeaderView::init()
{
    topLevel = new QWidget();
    view = new QHeaderView(Qt::Vertical,topLevel);
    // Some initial value tests before a model is added
    QCOMPARE(view->length(), 0);
    QCOMPARE(view->sizeHint(), QSize(0,0));
    QCOMPARE(view->sectionSizeHint(0), -1);

    /*
    model = new QStandardItemModel(1, 1);
    view->setModel(model);
    //qDebug() << view->count();
    view->sizeHint();
    */

    int rows = 4;
    int columns = 4;
    model = new QStandardItemModel(rows, columns);
    /*
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            QModelIndex index = model->index(row, column, QModelIndex());
            model->setData(index, QVariant((row+1) * (column+1)));
        }
    }
    */

    QSignalSpy spy(view, SIGNAL(sectionCountChanged(int,int)));
    view->setModel(model);
    QCOMPARE(spy.count(), 1);
    view->resize(200,200);
}

void tst_QHeaderView::cleanup()
{
    m_tableview->setUpdatesEnabled(true);
    if (view && view->parent() != m_tableview)
        delete view;
    view = 0;
    delete model;
    model = 0;
    delete topLevel;
    topLevel = 0;
}

void tst_QHeaderView::noModel()
{
    QHeaderView emptyView(Qt::Vertical);
    QCOMPARE(emptyView.count(), 0);
}

void tst_QHeaderView::emptyModel()
{
    QtTestModel testmodel;
    view->setModel(&testmodel);
    QVERIFY(!testmodel.wrongIndex);
    QCOMPARE(view->count(), testmodel.rows);
    view->setModel(model);
}

void tst_QHeaderView::removeRows()
{
    QtTestModel model;
    model.rows = model.cols = 10;

    QHeaderView vertical(Qt::Vertical);
    QHeaderView horizontal(Qt::Horizontal);

    vertical.setModel(&model);
    horizontal.setModel(&model);
    vertical.show();
    horizontal.show();
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);

    model.removeLastRow();
    QVERIFY(!model.wrongIndex);
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);

    model.removeAllRows();
    QVERIFY(!model.wrongIndex);
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);
}


void tst_QHeaderView::removeCols()
{
    QtTestModel model;
    model.rows = model.cols = 10;

    QHeaderView vertical(Qt::Vertical);
    QHeaderView horizontal(Qt::Horizontal);
    vertical.setModel(&model);
    horizontal.setModel(&model);
    vertical.show();
    horizontal.show();
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);

    model.removeLastColumn();
    QVERIFY(!model.wrongIndex);
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);

    model.removeAllColumns();
    QVERIFY(!model.wrongIndex);
    QCOMPARE(vertical.count(), model.rows);
    QCOMPARE(horizontal.count(), model.cols);
}

void tst_QHeaderView::movable()
{
    QCOMPARE(view->sectionsMovable(), false);
    view->setSectionsMovable(false);
    QCOMPARE(view->sectionsMovable(), false);
    view->setSectionsMovable(true);
    QCOMPARE(view->sectionsMovable(), true);
}

void tst_QHeaderView::clickable()
{
    QCOMPARE(view->sectionsClickable(), false);
    view->setSectionsClickable(false);
    QCOMPARE(view->sectionsClickable(), false);
    view->setSectionsClickable(true);
    QCOMPARE(view->sectionsClickable(), true);
}

void tst_QHeaderView::hidden()
{
    //hideSection() & showSection call setSectionHidden
    // Test bad arguments
    QCOMPARE(view->isSectionHidden(-1), false);
    QCOMPARE(view->isSectionHidden(view->count()), false);
    QCOMPARE(view->isSectionHidden(999999), false);

    view->setSectionHidden(-1, true);
    view->setSectionHidden(view->count(), true);
    view->setSectionHidden(999999, true);
    view->setSectionHidden(-1, false);
    view->setSectionHidden(view->count(), false);
    view->setSectionHidden(999999, false);

    // Hidden sections shouldn't have visual properties (except position)
    int pos = view->defaultSectionSize();
    view->setSectionHidden(1, true);
    QCOMPARE(view->sectionSize(1), 0);
    QCOMPARE(view->sectionPosition(1), pos);
    view->resizeSection(1, 100);
    QCOMPARE(view->sectionViewportPosition(1), pos);
    QCOMPARE(view->sectionSize(1), 0);
    view->setSectionHidden(1, false);
    QCOMPARE(view->isSectionHidden(0), false);
    QCOMPARE(view->sectionSize(0), view->defaultSectionSize());
}

void tst_QHeaderView::stretch()
{
    // Show before resize and setStrechLastSection
#if defined(Q_OS_WINCE)
    QSize viewSize(200,300);
#else
    QSize viewSize(500, 500);
#endif
    view->resize(viewSize);
    view->setStretchLastSection(true);
    QCOMPARE(view->stretchLastSection(), true);
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));
    QCOMPARE(view->width(), viewSize.width());
    QCOMPARE(view->visualIndexAt(view->viewport()->height() - 5), 3);

    view->setSectionHidden(3, true);
    QCOMPARE(view->visualIndexAt(view->viewport()->height() - 5), 2);

    view->setStretchLastSection(false);
    QCOMPARE(view->stretchLastSection(), false);
}

void tst_QHeaderView::oneSectionSize()
{
    //this ensures that if there is only one section, it gets a correct width (more than 0)
    QHeaderView view (Qt::Vertical);
    QtTestModel model;
    model.cols = 1;
    model.rows = 1;

    view.setSectionResizeMode(QHeaderView::Interactive);
    view.setModel(&model);

    view.show();

    QVERIFY(view.sectionSize(0) > 0);
}


void tst_QHeaderView::sectionSize_data()
{
    QTest::addColumn<QList<int> >("boundsCheck");
    QTest::addColumn<QList<int> >("defaultSizes");
    QTest::addColumn<int>("initialDefaultSize");
    QTest::addColumn<int>("lastVisibleSectionSize");
    QTest::addColumn<int>("persistentSectionSize");

    QTest::newRow("data set one")
        << (QList<int>() << -1 << 0 << 4 << 9999)
        << (QList<int>() << 10 << 30 << 30)
        << 30
        << 300
        << 20;
}

void tst_QHeaderView::sectionSize()
{
#if defined Q_OS_QNX
    QSKIP("The section size is dpi dependent on QNX");
#endif
    QFETCH(QList<int>, boundsCheck);
    QFETCH(QList<int>, defaultSizes);
    QFETCH(int, initialDefaultSize);
    QFETCH(int, lastVisibleSectionSize);
    QFETCH(int, persistentSectionSize);

#ifdef Q_OS_WINCE
    // We test on a device with doubled pixels. Therefore we need to specify
    // different boundaries.
    initialDefaultSize = qMax(view->minimumSectionSize(), 30);
#endif

    // bounds check
    foreach (int val, boundsCheck)
        view->sectionSize(val);

    // default size
    QCOMPARE(view->defaultSectionSize(), initialDefaultSize);
    foreach (int def, defaultSizes) {
        view->setDefaultSectionSize(def);
        QCOMPARE(view->defaultSectionSize(), def);
    }

    view->setDefaultSectionSize(initialDefaultSize);
    for (int s = 0; s < view->count(); ++s)
        QCOMPARE(view->sectionSize(s), initialDefaultSize);
    view->doItemsLayout();

    // stretch last section
    view->setStretchLastSection(true);
    int lastSection = view->count() - 1;

    //test that when hiding the last column,
    //resizing the new last visible columns still works
    view->hideSection(lastSection);
    view->resizeSection(lastSection - 1, lastVisibleSectionSize);
    QCOMPARE(view->sectionSize(lastSection - 1), lastVisibleSectionSize);
    view->showSection(lastSection);

    // turn off stretching
    view->setStretchLastSection(false);
    QCOMPARE(view->sectionSize(lastSection), initialDefaultSize);

    // test persistence
    int sectionCount = view->count();
    for (int i = 0; i < sectionCount; ++i)
        view->resizeSection(i, persistentSectionSize);
    QtTestModel model;
    model.cols = sectionCount * 2;
    model.rows = sectionCount * 2;
    view->setModel(&model);
    for (int j = 0; j < sectionCount; ++j)
        QCOMPARE(view->sectionSize(j), persistentSectionSize);
    for (int k = sectionCount; k < view->count(); ++k)
        QCOMPARE(view->sectionSize(k), initialDefaultSize);
}

void tst_QHeaderView::visualIndex()
{
    // Test bad arguments
    QCOMPARE(view->visualIndex(999999), -1);
    QCOMPARE(view->visualIndex(-1), -1);
    QCOMPARE(view->visualIndex(1), 1);
    view->setSectionHidden(1, true);
    QCOMPARE(view->visualIndex(1), 1);
    QCOMPARE(view->visualIndex(2), 2);

    view->setSectionHidden(1, false);
    QCOMPARE(view->visualIndex(1), 1);
    QCOMPARE(view->visualIndex(2), 2);
}

void tst_QHeaderView::visualIndexAt_data()
{
    QTest::addColumn<QList<int> >("hidden");
    QTest::addColumn<QList<int> >("from");
    QTest::addColumn<QList<int> >("to");
    QTest::addColumn<QList<int> >("coordinate");
    QTest::addColumn<QList<int> >("visual");

    QList<int> coordinateList;
#ifndef Q_OS_WINCE
    coordinateList << -1 << 0 << 31 << 91 << 99999;
#else
    // We test on a device with doubled pixels. Therefore we need to specify
    // different boundaries.
    coordinateList << -1 << 0 << 33 << 97 << 99999;
#endif

    QTest::newRow("no hidden, no moved sections")
        << QList<int>()
        << QList<int>()
        << QList<int>()
        << coordinateList
        << (QList<int>() << -1 << 0 << 1 << 3 << -1);

    QTest::newRow("no hidden, moved sections")
        << QList<int>()
        << (QList<int>() << 0)
        << (QList<int>() << 1)
        << coordinateList
        << (QList<int>() << -1 << 0 << 1 << 3 << -1);

    QTest::newRow("hidden, no moved sections")
        << (QList<int>() << 0)
        << QList<int>()
        << QList<int>()
        << coordinateList
        << (QList<int>() << -1 << 1 << 2 << 3 << -1);
}

void tst_QHeaderView::visualIndexAt()
{
#if defined Q_OS_QNX
    QSKIP("The section size is dpi dependent on QNX");
#endif
    QFETCH(QList<int>, hidden);
    QFETCH(QList<int>, from);
    QFETCH(QList<int>, to);
    QFETCH(QList<int>, coordinate);
    QFETCH(QList<int>, visual);

    view->setStretchLastSection(true);
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));

    for (int i = 0; i < hidden.count(); ++i)
        view->setSectionHidden(hidden.at(i), true);

    for (int j = 0; j < from.count(); ++j)
        view->moveSection(from.at(j), to.at(j));

    QTest::qWait(100);

    for (int k = 0; k < coordinate.count(); ++k)
        QCOMPARE(view->visualIndexAt(coordinate.at(k)), visual.at(k));
}

void tst_QHeaderView::length()
{
#if defined(Q_OS_WINCE)
    QFont font(QLatin1String("Tahoma"), 7);
    view->setFont(font);
#endif
    view->setStretchLastSection(true);
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));

    //minimumSectionSize should be the size of the last section of the widget is not tall enough
    int length = view->minimumSectionSize();
    for (int i=0; i < view->count()-1; i++) {
        length += view->sectionSize(i);
    }

    length = qMax(length, view->viewport()->height());
    QCOMPARE(length, view->length());

    view->setStretchLastSection(false);
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));

    QVERIFY(length != view->length());

    // layoutChanged might mean rows have been removed
    QtTestModel model;
    model.cols = 10;
    model.rows = 10;
    view->setModel(&model);
    int oldLength = view->length();
    model.cleanup();
    QCOMPARE(model.rows, view->count());
    QVERIFY(oldLength != view->length());
}

void tst_QHeaderView::offset()
{
    QCOMPARE(view->offset(), 0);
    view->setOffset(10);
    QCOMPARE(view->offset(), 10);
    view->setOffset(0);
    QCOMPARE(view->offset(), 0);

    // Test odd arguments
    view->setOffset(-1);
}

void tst_QHeaderView::sectionSizeHint()
{
    // Test bad arguments
    view->sectionSizeHint(-1);
    view->sectionSizeHint(99999);

    // TODO how to test the return value?
}

void tst_QHeaderView::logicalIndex()
{
    // Test bad arguments
    QCOMPARE(view->logicalIndex(-1), -1);
    QCOMPARE(view->logicalIndex(99999), -1);
}

void tst_QHeaderView::logicalIndexAt()
{
    // Test bad arguments
    view->logicalIndexAt(-1);
    view->logicalIndexAt(99999);
    QCOMPARE(view->logicalIndexAt(0), 0);
    QCOMPARE(view->logicalIndexAt(1), 0);

    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));
    view->setStretchLastSection(true);
    // First item
    QCOMPARE(view->logicalIndexAt(0), 0);
    QCOMPARE(view->logicalIndexAt(view->sectionSize(0)-1), 0);
    QCOMPARE(view->logicalIndexAt(view->sectionSize(0)+1), 1);
    // Last item
    int last = view->length() - 1;//view->viewport()->height() - 10;
    QCOMPARE(view->logicalIndexAt(last), 3);
    // Not in widget
    int outofbounds = view->length() + 1;//view->viewport()->height() + 1;
    QCOMPARE(view->logicalIndexAt(outofbounds), -1);

    view->moveSection(0,1);
    // First item
    QCOMPARE(view->logicalIndexAt(0), 1);
    QCOMPARE(view->logicalIndexAt(view->sectionSize(0)-1), 1);
    QCOMPARE(view->logicalIndexAt(view->sectionSize(0)+1), 0);
    // Last item
    QCOMPARE(view->logicalIndexAt(last), 3);
    view->moveSection(1,0);

}

void tst_QHeaderView::swapSections()
{
    view->swapSections(-1, 1);
    view->swapSections(99999, 1);
    view->swapSections(1, -1);
    view->swapSections(1, 99999);

    QVector<int> logical = (QVector<int>() << 0 << 1 << 2 << 3);

    QSignalSpy spy1(view, SIGNAL(sectionMoved(int,int,int)));

    QCOMPARE(view->sectionsMoved(), false);
    view->swapSections(1, 1);
    QCOMPARE(view->sectionsMoved(), false);
    view->swapSections(1, 2);
    QCOMPARE(view->sectionsMoved(), true);
    view->swapSections(2, 1);
    QCOMPARE(view->sectionsMoved(), true);
    for (int i = 0; i < view->count(); ++i)
        QCOMPARE(view->logicalIndex(i), logical.at(i));
    QCOMPARE(spy1.count(), 4);

    logical = (QVector<int>()  << 3 << 1 << 2 << 0);
    view->swapSections(3, 0);
    QCOMPARE(view->sectionsMoved(), true);
    for (int j = 0; j < view->count(); ++j)
        QCOMPARE(view->logicalIndex(j), logical.at(j));
    QCOMPARE(spy1.count(), 6);
}

void tst_QHeaderView::moveSection_data()
{
    QTest::addColumn<QList<int> >("hidden");
    QTest::addColumn<QList<int> >("from");
    QTest::addColumn<QList<int> >("to");
    QTest::addColumn<QList<bool> >("moved");
    QTest::addColumn<QList<int> >("logical");
    QTest::addColumn<int>("count");

    QTest::newRow("bad args, no hidden")
        << QList<int>()
        << (QList<int>() << -1 << 1 << 99999 << 1)
        << (QList<int>() << 1 << -1 << 1 << 99999)
        << (QList<bool>() << false << false << false << false)
        << (QList<int>() << 0 << 1 << 2 << 3)
        << 0;

    QTest::newRow("good args, no hidden")
        << QList<int>()
        << (QList<int>() << 1 << 1 << 2 << 1)
        << (QList<int>() << 1 << 2 << 1 << 2)
        << (QList<bool>() << false << true << true << true)
        << (QList<int>() << 0 << 2 << 1 << 3)
        << 3;

    QTest::newRow("hidden sections")
        << (QList<int>() << 0 << 3)
        << (QList<int>() << 1 << 1 << 2 << 1)
        << (QList<int>() << 1 << 2 << 1 << 2)
        << (QList<bool>() << false << true << true << true)
        << (QList<int>() << 0 << 2 << 1 << 3)
        << 3;
}

void tst_QHeaderView::moveSection()
{
    QFETCH(QList<int>, hidden);
    QFETCH(QList<int>, from);
    QFETCH(QList<int>, to);
    QFETCH(QList<bool>, moved);
    QFETCH(QList<int>, logical);
    QFETCH(int, count);

    QCOMPARE(from.count(), to.count());
    QCOMPARE(from.count(), moved.count());
    QCOMPARE(view->count(), logical.count());

    QSignalSpy spy1(view, SIGNAL(sectionMoved(int,int,int)));
    QCOMPARE(view->sectionsMoved(), false);

    for (int h = 0; h < hidden.count(); ++h)
        view->setSectionHidden(hidden.at(h), true);

    for (int i = 0; i < from.count(); ++i) {
        view->moveSection(from.at(i), to.at(i));
        QCOMPARE(view->sectionsMoved(), moved.at(i));
    }

    for (int j = 0; j < view->count(); ++j)
        QCOMPARE(view->logicalIndex(j), logical.at(j));

    QCOMPARE(spy1.count(), count);
}

void tst_QHeaderView::resizeAndMoveSection_data()
{
    QTest::addColumn<IntList>("logicalIndexes");
    QTest::addColumn<IntList>("sizes");
    QTest::addColumn<int>("logicalFrom");
    QTest::addColumn<int>("logicalTo");

    QTest::newRow("resizeAndMove-1")
        << (IntList() << 0 << 1)
        << (IntList() << 20 << 40)
        << 0 << 1;

    QTest::newRow("resizeAndMove-2")
        << (IntList() << 0 << 1 << 2 << 3)
        << (IntList() << 20 << 60 << 10 << 80)
        << 0 << 2;

    QTest::newRow("resizeAndMove-3")
        << (IntList() << 0 << 1 << 2 << 3)
        << (IntList() << 100 << 60 << 40 << 10)
        << 0 << 3;

    QTest::newRow("resizeAndMove-4")
        << (IntList() << 0 << 1 << 2 << 3)
        << (IntList() << 10 << 40 << 80 << 30)
        << 1 << 2;

    QTest::newRow("resizeAndMove-5")
        << (IntList() << 2 << 3)
        << (IntList() << 100 << 200)
        << 3 << 2;
}

void tst_QHeaderView::resizeAndMoveSection()
{
    QFETCH(IntList, logicalIndexes);
    QFETCH(IntList, sizes);
    QFETCH(int, logicalFrom);
    QFETCH(int, logicalTo);

    // Save old visual indexes and sizes
    IntList oldVisualIndexes;
    IntList oldSizes;
    foreach (int logical, logicalIndexes) {
        oldVisualIndexes.append(view->visualIndex(logical));
        oldSizes.append(view->sectionSize(logical));
    }

    // Resize sections
    for (int i = 0; i < logicalIndexes.size(); ++i) {
        int logical = logicalIndexes.at(i);
        view->resizeSection(logical, sizes.at(i));
    }

    // Move sections
    int visualFrom = view->visualIndex(logicalFrom);
    int visualTo = view->visualIndex(logicalTo);
    view->moveSection(visualFrom, visualTo);
    QCOMPARE(view->visualIndex(logicalFrom), visualTo);

    // Check that sizes are still correct
    for (int i = 0; i < logicalIndexes.size(); ++i) {
        int logical = logicalIndexes.at(i);
        QCOMPARE(view->sectionSize(logical), sizes.at(i));
    }

    // Move sections back
    view->moveSection(visualTo, visualFrom);

    // Check that sizes are still correct
    for (int i = 0; i < logicalIndexes.size(); ++i) {
        int logical = logicalIndexes.at(i);
        QCOMPARE(view->sectionSize(logical), sizes.at(i));
    }

    // Put everything back as it was
    for (int i = 0; i < logicalIndexes.size(); ++i) {
        int logical = logicalIndexes.at(i);
        view->resizeSection(logical, oldSizes.at(i));
        QCOMPARE(view->visualIndex(logical), oldVisualIndexes.at(i));
    }
}

void tst_QHeaderView::resizeHiddenSection_data()
{
    QTest::addColumn<int>("section");
    QTest::addColumn<int>("initialSize");
    QTest::addColumn<int>("finalSize");

    QTest::newRow("section 0 resize 50 to 20")
        << 0 << 50 << 20;

    QTest::newRow("section 1 resize 50 to 20")
        << 1 << 50 << 20;

    QTest::newRow("section 2 resize 50 to 20")
        << 2 << 50 << 20;

    QTest::newRow("section 3 resize 50 to 20")
        << 3 << 50 << 20;
}

void tst_QHeaderView::resizeHiddenSection()
{
    QFETCH(int, section);
    QFETCH(int, initialSize);
    QFETCH(int, finalSize);

    view->resizeSection(section, initialSize);
    view->setSectionHidden(section, true);
    QCOMPARE(view->sectionSize(section), 0);

    view->resizeSection(section, finalSize);
    QCOMPARE(view->sectionSize(section), 0);

    view->setSectionHidden(section, false);
    QCOMPARE(view->sectionSize(section), finalSize);
}

void tst_QHeaderView::resizeAndInsertSection_data()
{
    QTest::addColumn<int>("section");
    QTest::addColumn<int>("size");
    QTest::addColumn<int>("insert");
    QTest::addColumn<int>("compare");
    QTest::addColumn<int>("expected");

    QTest::newRow("section 0 size 50 insert 0")
        << 0 << 50 << 0 << 1 << 50;

    QTest::newRow("section 1 size 50 insert 1")
        << 0 << 50 << 1 << 0 << 50;

    QTest::newRow("section 1 size 50 insert 0")
        << 1 << 50 << 0 << 2 << 50;

}

void tst_QHeaderView::resizeAndInsertSection()
{
    QFETCH(int, section);
    QFETCH(int, size);
    QFETCH(int, insert);
    QFETCH(int, compare);
    QFETCH(int, expected);

    view->setStretchLastSection(false);

    view->resizeSection(section, size);
    QCOMPARE(view->sectionSize(section), size);

    model->insertRow(insert);

    QCOMPARE(view->sectionSize(compare), expected);
}

void tst_QHeaderView::resizeWithResizeModes_data()
{
    QTest::addColumn<int>("size");
    QTest::addColumn<QList<int> >("sections");
    QTest::addColumn<QList<int> >("modes");
    QTest::addColumn<QList<int> >("expected");

    QTest::newRow("stretch first section")
        << 600
        << (QList<int>() << 100 << 100 << 100 << 100)
        << (QList<int>() << ((int)QHeaderView::Stretch)
                         << ((int)QHeaderView::Interactive)
                         << ((int)QHeaderView::Interactive)
                         << ((int)QHeaderView::Interactive))
        << (QList<int>() << 300 << 100 << 100 << 100);
}

void  tst_QHeaderView::resizeWithResizeModes()
{
    QFETCH(int, size);
    QFETCH(QList<int>, sections);
    QFETCH(QList<int>, modes);
    QFETCH(QList<int>, expected);

    view->setStretchLastSection(false);
    for (int i = 0; i < sections.count(); ++i) {
        view->resizeSection(i, sections.at(i));
        view->setSectionResizeMode(i, (QHeaderView::ResizeMode)modes.at(i));
    }
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));
    view->resize(size, size);
    for (int j = 0; j < expected.count(); ++j)
        QCOMPARE(view->sectionSize(j), expected.at(j));
}

void tst_QHeaderView::moveAndInsertSection_data()
{
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<int>("insert");
    QTest::addColumn<QList<int> >("mapping");

    QTest::newRow("move from 1 to 3, insert 0")
        << 1 << 3 << 0 <<(QList<int>() << 0 << 1 << 3 << 4 << 2);

}

void tst_QHeaderView::moveAndInsertSection()
{
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(int, insert);
    QFETCH(QList<int>, mapping);

    view->setStretchLastSection(false);

    view->moveSection(from, to);

    model->insertRow(insert);

    for (int i = 0; i < mapping.count(); ++i)
        QCOMPARE(view->logicalIndex(i), mapping.at(i));
}

void tst_QHeaderView::resizeMode()
{
    // resizeMode must not be called with an invalid index
    int last = view->count() - 1;
    view->setSectionResizeMode(QHeaderView::Interactive);
    QCOMPARE(view->sectionResizeMode(last), QHeaderView::Interactive);
    QCOMPARE(view->sectionResizeMode(1), QHeaderView::Interactive);
    view->setSectionResizeMode(QHeaderView::Stretch);
    QCOMPARE(view->sectionResizeMode(last), QHeaderView::Stretch);
    QCOMPARE(view->sectionResizeMode(1), QHeaderView::Stretch);
    view->setSectionResizeMode(QHeaderView::Custom);
    QCOMPARE(view->sectionResizeMode(last), QHeaderView::Custom);
    QCOMPARE(view->sectionResizeMode(1), QHeaderView::Custom);

    // test when sections have been moved
    view->setStretchLastSection(false);
    for (int i=0; i < (view->count() - 1); ++i)
        view->setSectionResizeMode(i, QHeaderView::Interactive);
    int logicalIndex = view->count() / 2;
    view->setSectionResizeMode(logicalIndex, QHeaderView::Stretch);
    view->moveSection(view->visualIndex(logicalIndex), 0);
    for (int i=0; i < (view->count() - 1); ++i) {
        if (i == logicalIndex)
            QCOMPARE(view->sectionResizeMode(i), QHeaderView::Stretch);
        else
            QCOMPARE(view->sectionResizeMode(i), QHeaderView::Interactive);
    }
}

void tst_QHeaderView::resizeSection_data()
{
    QTest::addColumn<int>("initial");
    QTest::addColumn<QList<int> >("logical");
    QTest::addColumn<QList<int> >("size");
    QTest::addColumn<QList<int> >("mode");
    QTest::addColumn<int>("resized");
    QTest::addColumn<QList<int> >("expected");

    QTest::newRow("bad args")
        << 100
        << (QList<int>() << -1 << -1 << 99999 << 99999 << 4)
        << (QList<int>() << -1 << 0 << 99999 << -1 << -1)
        << (QList<int>()
            << int(QHeaderView::Interactive)
            << int(QHeaderView::Interactive)
            << int(QHeaderView::Interactive)
            << int(QHeaderView::Interactive))
        << 0
        << (QList<int>() << 0 << 0 << 0 << 0 << 0);
}

void tst_QHeaderView::resizeSection()
{

    QFETCH(int, initial);
    QFETCH(QList<int>, logical);
    QFETCH(QList<int>, size);
    QFETCH(QList<int>, mode);
    QFETCH(int, resized);
    QFETCH(QList<int>, expected);

    view->resize(400, 400);

    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));
    view->setSectionsMovable(true);
    view->setStretchLastSection(false);

    for (int i = 0; i < logical.count(); ++i)
        if (logical.at(i) > -1 && logical.at(i) < view->count()) // for now
            view->setSectionResizeMode(logical.at(i), (QHeaderView::ResizeMode)mode.at(i));

    for (int j = 0; j < logical.count(); ++j)
        view->resizeSection(logical.at(j), initial);

    QSignalSpy spy(view, SIGNAL(sectionResized(int,int,int)));

    for (int k = 0; k < logical.count(); ++k)
        view->resizeSection(logical.at(k), size.at(k));

    QCOMPARE(spy.count(), resized);

    for (int l = 0; l < logical.count(); ++l)
        QCOMPARE(view->sectionSize(logical.at(l)), expected.at(l));
}

void tst_QHeaderView::highlightSections()
{
    view->setHighlightSections(true);
    QCOMPARE(view->highlightSections(), true);
    view->setHighlightSections(false);
    QCOMPARE(view->highlightSections(), false);
}

void tst_QHeaderView::showSortIndicator()
{
    view->setSortIndicatorShown(true);
    QCOMPARE(view->isSortIndicatorShown(), true);
    QCOMPARE(view->sortIndicatorOrder(), Qt::DescendingOrder);
    view->setSortIndicator(1, Qt::AscendingOrder);
    QCOMPARE(view->sortIndicatorOrder(), Qt::AscendingOrder);
    view->setSortIndicator(1, Qt::DescendingOrder);
    QCOMPARE(view->sortIndicatorOrder(), Qt::DescendingOrder);
    view->setSortIndicatorShown(false);
    QCOMPARE(view->isSortIndicatorShown(), false);

    view->setSortIndicator(999999, Qt::DescendingOrder);
    // Don't segfault baby :)
    view->setSortIndicatorShown(true);

    view->setSortIndicator(0, Qt::DescendingOrder);
    // Don't assert baby :)
}

void tst_QHeaderView::sortIndicatorTracking()
{
    QtTestModel model;
    model.rows = model.cols = 10;

    QHeaderView hv(Qt::Horizontal);

    hv.setModel(&model);
    hv.show();
    hv.setSortIndicatorShown(true);
    hv.setSortIndicator(1, Qt::DescendingOrder);

    model.removeOneColumn(8);
    QCOMPARE(hv.sortIndicatorSection(), 1);

    model.removeOneColumn(2);
    QCOMPARE(hv.sortIndicatorSection(), 1);

    model.insertOneColumn(2);
    QCOMPARE(hv.sortIndicatorSection(), 1);

    model.insertOneColumn(1);
    QCOMPARE(hv.sortIndicatorSection(), 2);

    model.removeOneColumn(0);
    QCOMPARE(hv.sortIndicatorSection(), 1);

    model.removeOneColumn(1);
    QCOMPARE(hv.sortIndicatorSection(), -1);
}

void tst_QHeaderView::removeAndInsertRow()
{
    // Check if logicalIndex returns the correct value after we have removed a row
    // we might as well te
    for (int i = 0; i < model->rowCount(); ++i) {
        QCOMPARE(i, view->logicalIndex(i));
    }

    while (model->removeRow(0)) {
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
    }

    int pass = 0;
    for (pass = 0; pass < 5; pass++) {
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
        model->insertRow(0);
    }

    while (model->removeRows(0, 2)) {
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
    }

    for (pass = 0; pass < 3; pass++) {
        model->insertRows(0, 2);
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
    }

    for (pass = 0; pass < 3; pass++) {
        model->insertRows(3, 2);
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
    }

    // Insert at end
    for (pass = 0; pass < 3; pass++) {
        int rowCount = model->rowCount();
        model->insertRows(rowCount, 1);
        for (int i = 0; i < rowCount; ++i) {
            QCOMPARE(i, view->logicalIndex(i));
        }
    }

}
void tst_QHeaderView::unhideSection()
{
    // You should not necessarily expect the same size back again, so the best test we can do is to test if it is larger than 0 after a unhide.
    QCOMPARE(view->sectionsHidden(), false);
    view->setSectionHidden(0, true);
    QCOMPARE(view->sectionsHidden(), true);
    QCOMPARE(view->sectionSize(0), 0);
    view->setSectionResizeMode(QHeaderView::Interactive);
    view->setSectionHidden(0, false);
    QVERIFY(view->sectionSize(0) > 0);

    view->setSectionHidden(0, true);
    QCOMPARE(view->sectionSize(0), 0);
    view->setSectionHidden(0, true);
    QCOMPARE(view->sectionSize(0), 0);
    view->setSectionResizeMode(QHeaderView::Stretch);
    view->setSectionHidden(0, false);
    QVERIFY(view->sectionSize(0) > 0);

}

void tst_QHeaderView::testEvent()
{
    protected_QHeaderView x(Qt::Vertical);
    x.testEvent();
    protected_QHeaderView y(Qt::Horizontal);
    y.testEvent();
}


void protected_QHeaderView::testEvent()
{
    // No crashy please
    QHoverEvent enterEvent(QEvent::HoverEnter, QPoint(), QPoint());
    event(&enterEvent);
    QHoverEvent eventLeave(QEvent::HoverLeave, QPoint(), QPoint());
    event(&eventLeave);
    QHoverEvent eventMove(QEvent::HoverMove, QPoint(), QPoint());
    event(&eventMove);
}

void tst_QHeaderView::headerDataChanged()
{
    // This shouldn't asserver because view is Vertical
    view->headerDataChanged(Qt::Horizontal, -1, -1);
#if 0
    // This will assert
    view->headerDataChanged(Qt::Vertical, -1, -1);
#endif

    // No crashing please
    view->headerDataChanged(Qt::Horizontal, 0, 1);
    view->headerDataChanged(Qt::Vertical, 0, 1);
}

void tst_QHeaderView::currentChanged()
{
    view->setCurrentIndex(QModelIndex());
}

void tst_QHeaderView::horizontalOffset()
{
    protected_QHeaderView x(Qt::Vertical);
    x.testhorizontalOffset();
    protected_QHeaderView y(Qt::Horizontal);
    y.testhorizontalOffset();
}

void tst_QHeaderView::verticalOffset()
{
    protected_QHeaderView x(Qt::Vertical);
    x.testverticalOffset();
    protected_QHeaderView y(Qt::Horizontal);
    y.testverticalOffset();
}

void  protected_QHeaderView::testhorizontalOffset()
{
    if(orientation() == Qt::Horizontal){
        QCOMPARE(horizontalOffset(), 0);
        setOffset(10);
        QCOMPARE(horizontalOffset(), 10);
    }
    else
        QCOMPARE(horizontalOffset(), 0);

}

void  protected_QHeaderView::testverticalOffset()
{
    if(orientation() == Qt::Vertical){
        QCOMPARE(verticalOffset(), 0);
        setOffset(10);
        QCOMPARE(verticalOffset(), 10);
    }
    else
        QCOMPARE(verticalOffset(), 0);
}

void tst_QHeaderView::stretchSectionCount()
{
    view->setStretchLastSection(false);
    QCOMPARE(view->stretchSectionCount(), 0);
    view->setStretchLastSection(true);
    QCOMPARE(view->stretchSectionCount(), 0);

    view->setSectionResizeMode(0, QHeaderView::Stretch);
    QCOMPARE(view->stretchSectionCount(), 1);
}

void tst_QHeaderView::hiddenSectionCount()
{
    model->clear();
    model->insertRows(0, 10);
    // Hide every other one
    for (int i=0; i<10; i++)
        view->setSectionHidden(i, (i & 1) == 0);

    QCOMPARE(view->hiddenSectionCount(), 5);

    view->setSectionResizeMode(QHeaderView::Stretch);
    QCOMPARE(view->hiddenSectionCount(), 5);

    // Remove some rows and make sure they are now still counted
    model->removeRow(9);
    model->removeRow(8);
    model->removeRow(7);
    model->removeRow(6);
    QCOMPARE(view->count(), 6);
    QCOMPARE(view->hiddenSectionCount(), 3);
    model->removeRows(0,5);
    QCOMPARE(view->count(), 1);
    QCOMPARE(view->hiddenSectionCount(), 0);
    QVERIFY(view->count() >=  view->hiddenSectionCount());
}

void tst_QHeaderView::focusPolicy()
{
    QHeaderView view(Qt::Horizontal);
    QCOMPARE(view.focusPolicy(), Qt::NoFocus);

    QTreeWidget widget;
    QCOMPARE(widget.header()->focusPolicy(), Qt::NoFocus);
    QVERIFY(!widget.focusProxy());
    QVERIFY(!widget.hasFocus());
    QVERIFY(!widget.header()->focusProxy());
    QVERIFY(!widget.header()->hasFocus());

    widget.show();
    widget.setFocus(Qt::OtherFocusReason);
    QApplication::setActiveWindow(&widget);
    widget.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&widget));
    QVERIFY(widget.hasFocus());
    QVERIFY(!widget.header()->hasFocus());

    widget.setFocusPolicy(Qt::NoFocus);
    widget.clearFocus();
    QTRY_VERIFY(!widget.hasFocus());
    QVERIFY(!widget.header()->hasFocus());

    QTest::keyPress(&widget, Qt::Key_Tab);

    qApp->processEvents();
    qApp->processEvents();

    QVERIFY(!widget.hasFocus());
    QVERIFY(!widget.header()->hasFocus());
}

class SimpleModel : public QAbstractItemModel
{
    Q_OBJECT
public:

    SimpleModel( QObject* parent=0)
        : QAbstractItemModel(parent),
        m_col_count(3) {}

    QModelIndex parent(const QModelIndex &/*child*/) const
    {
        return QModelIndex();
    }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const
    {
        return hasIndex(row, column, parent) ? createIndex(row, column) : QModelIndex();
    }
    int rowCount(const QModelIndex & /* parent */) const
    {
        return 8;
    }
    int columnCount(const QModelIndex &/*parent= QModelIndex()*/) const
    {
        return m_col_count;
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid())
        {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return QString::fromLatin1("%1,%2").arg(index.row()).arg(index.column());
        }
        return QVariant();
    }

    void setColumnCount( int c )
    {
        m_col_count = c;
    }

private:
    int m_col_count;
};

void tst_QHeaderView::moveSectionAndReset()
{
    SimpleModel m;
    QHeaderView v(Qt::Horizontal);
    v.setModel(&m);
    int cc = 2;
    for (cc = 2; cc < 4; ++cc) {
        m.setColumnCount(cc);
        int movefrom = 0;
        int moveto;
        for (moveto = 1; moveto < cc; ++moveto) {
            v.moveSection(movefrom, moveto);
            m.setColumnCount(cc - 1);
            v.reset();
            for (int i = 0; i < cc - 1; ++i) {
                QCOMPARE(v.logicalIndex(v.visualIndex(i)), i);
            }
        }
    }
}

void tst_QHeaderView::moveSectionAndRemove()
{
    QStandardItemModel m;
    QHeaderView v(Qt::Horizontal);

    v.setModel(&m);
    v.model()->insertColumns(0, 3);
    v.moveSection(0, 1);

    QCOMPARE(v.count(), 3);
    v.model()->removeColumns(0, v.model()->columnCount());
    QCOMPARE(v.count(), 0);
}

void tst_QHeaderView::saveRestore()
{
    SimpleModel m;
    QHeaderView h1(Qt::Horizontal);
    h1.setModel(&m);
    h1.swapSections(0, 2);
    h1.resizeSection(1, 10);
    h1.setSortIndicatorShown(true);
    h1.setSortIndicator(1,Qt::DescendingOrder);
    QByteArray s1 = h1.saveState();

    QHeaderView h2(Qt::Vertical);
    QSignalSpy spy(&h2, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)));

    h2.setModel(&m);
    h2.restoreState(s1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);

    QCOMPARE(h2.logicalIndex(0), 2);
    QCOMPARE(h2.logicalIndex(2), 0);
    QCOMPARE(h2.sectionSize(1), 10);
    QCOMPARE(h2.sortIndicatorSection(), 1);
    QCOMPARE(h2.sortIndicatorOrder(), Qt::DescendingOrder);
    QCOMPARE(h2.isSortIndicatorShown(), true);

    QByteArray s2 = h2.saveState();

    QCOMPARE(s1, s2);
    QVERIFY(!h2.restoreState(QByteArrayLiteral("Garbage")));

    // QTBUG-40462
    // Setting from Qt4, where information about multiple sections were grouped together in one
    // sectionItem object
    QByteArray settings_qt4 =
      QByteArray::fromHex("000000ff00000000000000010000000100000000010000000000000000000000000000"
                          "0000000003e80000000a0101000100000000000000000000000064ffffffff00000081"
                          "0000000000000001000003e80000000a00000000");
    QVERIFY(h2.restoreState(settings_qt4));
    int sectionItemsLengthTotal = 0;
    for (int i = 0; i < h2.count(); ++i)
        sectionItemsLengthTotal += h2.sectionSize(i);
    QCOMPARE(sectionItemsLengthTotal, h2.length());

    // Buggy setting where sum(sectionItems) != length. Check false is returned and this corrupted
    // state isn't restored
    QByteArray settings_buggy_length =
      QByteArray::fromHex("000000ff000000000000000100000000000000050100000000000000000000000a4000"
                          "000000010000000600000258000000fb0000000a010100010000000000000000000000"
                          "0064ffffffff00000081000000000000000a000000d30000000100000000000000c800"
                          "000001000000000000008000000001000000000000005c00000001000000000000003c"
                          "0000000100000000000002580000000100000000000000000000000100000000000002"
                          "580000000100000000000002580000000100000000000003c000000001000000000000"
                          "03e8");
    int old_length = h2.length();
    QByteArray old_state = h2.saveState();
    // Check setting is correctly recognized as corrupted
    QVERIFY(!h2.restoreState(settings_buggy_length));
    // Check nothing has been actually restored
    QCOMPARE(h2.length(), old_length);
    QCOMPARE(h2.saveState(), old_state);
}

void tst_QHeaderView::defaultSectionSizeTest()
{
    // Setup
    QTableView qtv;
    QHeaderView *hv = qtv.verticalHeader();
    hv->setDefaultSectionSize(99); // Set it to a value different from defaultSize.
    QStandardItemModel amodel(4, 4);
    qtv.setModel(&amodel);
    QCOMPARE(hv->sectionSize(0), 99);
    QCOMPARE(hv->visualIndexAt(50), 0); // <= also make sure that indexes are calculated
    hv->setDefaultSectionSize(40); // Set it to a value different from defaultSize.
    QCOMPARE(hv->visualIndexAt(50), 1);

    const int defaultSize = 26;
    hv->setDefaultSectionSize(defaultSize + 1); // Set it to a value different from defaultSize.

    // no hidden Sections
    hv->resizeSection(1, 0);
    hv->setDefaultSectionSize(defaultSize);
    QCOMPARE(hv->sectionSize(1), defaultSize);

    // with hidden sections
    hv->resizeSection(1, 0);
    hv->hideSection(2);
    hv->setDefaultSectionSize(defaultSize);

    QVERIFY(hv->sectionSize(0) == defaultSize); // trivial case.
    QVERIFY(hv->sectionSize(1) == defaultSize); // just sized 0. Now it should be 10
    QVERIFY(hv->sectionSize(2) == 0); // section is hidden. It should not be resized.
}

class TestHeaderViewStyle : public QProxyStyle
{
public:
    TestHeaderViewStyle() : horizontalSectionSize(100) {}
    int pixelMetric(PixelMetric metric, const QStyleOption *option = 0, const QWidget *widget = 0) const Q_DECL_OVERRIDE
    {
        if (metric == QStyle::PM_HeaderDefaultSectionSizeHorizontal)
            return horizontalSectionSize;
        else
            return QProxyStyle::pixelMetric(metric, option, widget);
    }
    int horizontalSectionSize;
};

void tst_QHeaderView::defaultSectionSizeTestStyles()
{
    TestHeaderViewStyle style1;
    TestHeaderViewStyle style2;
    style1.horizontalSectionSize = 100;
    style2.horizontalSectionSize = 200;

    QHeaderView hv(Qt::Horizontal);
    hv.setStyle(&style1);
    QCOMPARE(hv.defaultSectionSize(), style1.horizontalSectionSize);
    hv.setStyle(&style2);
    QCOMPARE(hv.defaultSectionSize(), style2.horizontalSectionSize);
    hv.setDefaultSectionSize(70);
    QCOMPARE(hv.defaultSectionSize(), 70);
    hv.setStyle(&style1);
    QCOMPARE(hv.defaultSectionSize(), 70);
    hv.resetDefaultSectionSize();
    QCOMPARE(hv.defaultSectionSize(), style1.horizontalSectionSize);
    hv.setStyle(&style2);
    QCOMPARE(hv.defaultSectionSize(), style2.horizontalSectionSize);
}

void tst_QHeaderView::defaultAlignment_data()
{
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("initial");
    QTest::addColumn<int>("alignment");

    QTest::newRow("horizontal right aligned")
        << int(Qt::Horizontal)
        << int(Qt::AlignCenter)
        << int(Qt::AlignRight);

    QTest::newRow("horizontal left aligned")
        << int(Qt::Horizontal)
        << int(Qt::AlignCenter)
        << int(Qt::AlignLeft);

    QTest::newRow("vertical right aligned")
        << int(Qt::Vertical)
        << int(Qt::AlignLeft|Qt::AlignVCenter)
        << int(Qt::AlignRight);

    QTest::newRow("vertical left aligned")
        << int(Qt::Vertical)
        << int(Qt::AlignLeft|Qt::AlignVCenter)
        << int(Qt::AlignLeft);
}

void tst_QHeaderView::defaultAlignment()
{
    QFETCH(int, direction);
    QFETCH(int, initial);
    QFETCH(int, alignment);

    SimpleModel m;

    QHeaderView header((Qt::Orientation)direction);
    header.setModel(&m);

    QCOMPARE(header.defaultAlignment(), (Qt::Alignment)initial);
    header.setDefaultAlignment((Qt::Alignment)alignment);
    QCOMPARE(header.defaultAlignment(), (Qt::Alignment)alignment);
}

void tst_QHeaderView::globalResizeMode_data()
{
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("insert");

    QTest::newRow("horizontal ResizeToContents 0")
        << int(Qt::Horizontal)
        << int(QHeaderView::ResizeToContents)
        << 0;
}

void tst_QHeaderView::globalResizeMode()
{
    QFETCH(int, direction);
    QFETCH(int, mode);
    QFETCH(int, insert);

    QStandardItemModel m(4, 4);
    QHeaderView h((Qt::Orientation)direction);
    h.setModel(&m);

    h.setSectionResizeMode((QHeaderView::ResizeMode)mode);
    m.insertRow(insert);
    for (int i = 0; i < h.count(); ++i)
        QCOMPARE(h.sectionResizeMode(i), (QHeaderView::ResizeMode)mode);
}


void tst_QHeaderView::sectionPressedSignal_data()
{
    QTest::addColumn<int>("direction");
    QTest::addColumn<bool>("clickable");
    QTest::addColumn<int>("count");

    QTest::newRow("horizontal unclickable 0")
        << int(Qt::Horizontal)
        << false
        << 0;

    QTest::newRow("horizontal clickable 1")
        << int(Qt::Horizontal)
        << true
        << 1;
}

void tst_QHeaderView::sectionPressedSignal()
{
    QFETCH(int, direction);
    QFETCH(bool, clickable);
    QFETCH(int, count);

    QStandardItemModel m(4, 4);
    QHeaderView h((Qt::Orientation)direction);

    h.setModel(&m);
    h.show();
    h.setSectionsClickable(clickable);

    QSignalSpy spy(&h, SIGNAL(sectionPressed(int)));

    QCOMPARE(spy.count(), 0);
    QTest::mousePress(h.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QCOMPARE(spy.count(), count);
}

void tst_QHeaderView::sectionClickedSignal()
{
    QFETCH(int, direction);
    QFETCH(bool, clickable);
    QFETCH(int, count);

    QStandardItemModel m(4, 4);
    QHeaderView h((Qt::Orientation)direction);

    h.setModel(&m);
    h.show();
    h.setSectionsClickable(clickable);
    h.setSortIndicatorShown(true);

    QSignalSpy spy(&h, SIGNAL(sectionClicked(int)));
    QSignalSpy spy2(&h, SIGNAL(sortIndicatorChanged(int,Qt::SortOrder)));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QTest::mouseClick(h.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QCOMPARE(spy.count(), count);
    QCOMPARE(spy2.count(), count);

    //now let's try with the sort indicator hidden (the result should be the same
    spy.clear();
    spy2.clear();
    h.setSortIndicatorShown(false);
    QTest::mouseClick(h.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QCOMPARE(spy.count(), count);
    QCOMPARE(spy2.count(), count);
}

void tst_QHeaderView::defaultSectionSize_data()
{
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("oldDefaultSize");
    QTest::addColumn<int>("newDefaultSize");

    //QTest::newRow("horizontal,-5") << int(Qt::Horizontal) << 100 << -5;
    QTest::newRow("horizontal, 0") << int(Qt::Horizontal) << 100 << 0;
    QTest::newRow("horizontal, 5") << int(Qt::Horizontal) << 100 << 5;
    QTest::newRow("horizontal,25") << int(Qt::Horizontal) << 100 << 5;
}

void tst_QHeaderView::defaultSectionSize()
{
    QFETCH(int, direction);
    QFETCH(int, oldDefaultSize);
    QFETCH(int, newDefaultSize);

    QStandardItemModel m(4, 4);
    QHeaderView h((Qt::Orientation)direction);

    h.setModel(&m);

    QCOMPARE(h.defaultSectionSize(), oldDefaultSize);
    h.setDefaultSectionSize(newDefaultSize);
    QCOMPARE(h.defaultSectionSize(), newDefaultSize);
    h.reset();
    for (int i = 0; i < h.count(); ++i)
        QCOMPARE(h.sectionSize(i), newDefaultSize);
}

void tst_QHeaderView::hideAndInsert_data()
{
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("hide");
    QTest::addColumn<int>("insert");
    QTest::addColumn<int>("hidden");

    QTest::newRow("horizontal, 0, 0") << int(Qt::Horizontal) << 0 << 0 << 1;
}

void tst_QHeaderView::hideAndInsert()
{
    QFETCH(int, direction);
    QFETCH(int, hide);
    QFETCH(int, insert);
    QFETCH(int, hidden);

    QStandardItemModel m(4, 4);
    QHeaderView h((Qt::Orientation)direction);

    h.setModel(&m);

    h.setSectionHidden(hide, true);

    if (direction == Qt::Vertical)
        m.insertRow(insert);
    else
        m.insertColumn(insert);

    for (int i = 0; i < h.count(); ++i)
        if (i != hidden)
            QCOMPARE(h.isSectionHidden(i), false);
        else
            QCOMPARE(h.isSectionHidden(i), true);
}

void tst_QHeaderView::removeSection()
{
//test that removing a hidden section gives the expected result: the next row should be hidden
//(see task
    const int hidden = 3; //section that will be hidden
    const QStringList list = QStringList() << "0" << "1" << "2" << "3" << "4" << "5" << "6";

    QStringListModel model( list );
    QHeaderView view(Qt::Vertical);
    view.setModel(&model);
    view.hideSection(hidden);
    view.hideSection(1);
    model.removeRow(1);
    view.show();

    for(int i = 0; i < view.count(); i++) {
        if (i == (hidden-1)) { //-1 because we removed a row in the meantime
            QCOMPARE(view.sectionSize(i), 0);
            QVERIFY(view.isSectionHidden(i));
        } else {
            QCOMPARE(view.sectionSize(i), view.defaultSectionSize() );
            QVERIFY(!view.isSectionHidden(i));
        }
    }
}

void tst_QHeaderView::preserveHiddenSectionWidth()
{
    const QStringList list = QStringList() << "0" << "1" << "2" << "3";

    QStringListModel model( list );
    QHeaderView view(Qt::Vertical);
    view.setModel(&model);
    view.resizeSection(0, 100);
    view.resizeSection(1, 10);
    view.resizeSection(2, 50);
    view.setSectionResizeMode(3, QHeaderView::Stretch);
    view.show();

    view.hideSection(2);
    model.removeRow(1);
    view.showSection(1);
    QCOMPARE(view.sectionSize(0), 100);
    QCOMPARE(view.sectionSize(1), 50);

    view.hideSection(1);
    model.insertRow(1);
    view.showSection(2);
    QCOMPARE(view.sectionSize(0), 100);
    QCOMPARE(view.sectionSize(1), view.defaultSectionSize());
    QCOMPARE(view.sectionSize(2), 50);
}

void tst_QHeaderView::invisibleStretchLastSection()
{
    int count = 6;
    QStandardItemModel model(1, count);
    QHeaderView view(Qt::Horizontal);
    view.setModel(&model);
    int height = view.height();

    view.resize(view.defaultSectionSize() * (count / 2), height); // don't show all sections
    view.show();
    view.setStretchLastSection(true);
    // stretch section is not visible; it should not be stretched
    for (int i = 0; i < count; ++i)
        QCOMPARE(view.sectionSize(i), view.defaultSectionSize());

    view.resize(view.defaultSectionSize() * (count + 1), height); // give room to stretch

    // stretch section is visible; it should be stretched
    for (int i = 0; i < count - 1; ++i)
        QCOMPARE(view.sectionSize(i), view.defaultSectionSize());
    QCOMPARE(view.sectionSize(count - 1), view.defaultSectionSize() * 2);
}

void tst_QHeaderView::noSectionsWithNegativeSize()
{
    QStandardItemModel m(4, 4);
    QHeaderView h(Qt::Horizontal);
    h.setModel(&m);
    h.resizeSection(1, -5);
    QVERIFY(h.sectionSize(1) >= 0); // Sections with negative sizes not well defined.
}

void tst_QHeaderView::emptySectionSpan()
{
    QHeaderViewPrivate::SectionItem section;
    QCOMPARE(section.sectionSize(), 0);
}

void tst_QHeaderView::task236450_hidden_data()
{
    QTest::addColumn<QList<int> >("hide1");
    QTest::addColumn<QList<int> >("hide2");

    QTest::newRow("set 1") << (QList<int>() << 1 << 3)
                           << (QList<int>() << 1 << 5);

    QTest::newRow("set 2") << (QList<int>() << 2 << 3)
                           << (QList<int>() << 1 << 5);

    QTest::newRow("set 3") << (QList<int>() << 0 << 2 << 4)
                           << (QList<int>() << 2 << 3 << 5);

}

void tst_QHeaderView::task236450_hidden()
{
    QFETCH(QList<int>, hide1);
    QFETCH(QList<int>, hide2);
    const QStringList list = QStringList() << "0" << "1" << "2" << "3" << "4" << "5";

    QStringListModel model( list );
    protected_QHeaderView view(Qt::Vertical);
    view.setModel(&model);
    view.show();

    foreach (int i, hide1)
        view.hideSection(i);

    QCOMPARE(view.hiddenSectionCount(), hide1.count());
    for (int i = 0; i < 6; i++) {
        QCOMPARE(!view.isSectionHidden(i), !hide1.contains(i));
    }

    view.setDefaultSectionSize(2);
    view.scheduleDelayedItemsLayout();
    view.executeDelayedItemsLayout(); //force to do a relayout

    QCOMPARE(view.hiddenSectionCount(), hide1.count());
    for (int i = 0; i < 6; i++) {
        QCOMPARE(!view.isSectionHidden(i), !hide1.contains(i));
        view.setSectionHidden(i, hide2.contains(i));
    }

    QCOMPARE(view.hiddenSectionCount(), hide2.count());
    for (int i = 0; i < 6; i++) {
        QCOMPARE(!view.isSectionHidden(i), !hide2.contains(i));
    }

}

void tst_QHeaderView::task248050_hideRow()
{
    //this is the sequence of events that make the task fail
    protected_QHeaderView header(Qt::Vertical);
    QStandardItemModel model(0, 1);
    header.setStretchLastSection(false);
    header.setDefaultSectionSize(17);
    header.setModel(&model);
    header.doItemsLayout();

    model.setRowCount(3);

    QCOMPARE(header.sectionPosition(2), 17*2);

    header.hideSection(1);
    QCOMPARE(header.sectionPosition(2), 17);

    QTest::qWait(100);
    //the size of the section shouldn't have changed
    QCOMPARE(header.sectionPosition(2), 17);
}


//returns 0 if everything is fine.
static int checkHeaderViewOrder(QHeaderView *view, const QVector<int> &expected)
{
    if (view->count() != expected.count())
        return 1;

    for (int i = 0; i < expected.count(); i++) {
        if (view->logicalIndex(i) != expected.at(i))
            return i + 10;
        if (view->visualIndex(expected.at(i)) != i)
            return i + 100;
    }
    return 0;
}


void tst_QHeaderView::QTBUG6058_reset()
{
    QStringListModel model1( QStringList() << "0" << "1" << "2" << "3" << "4" << "5" );
    QStringListModel model2( QStringList() << "a" << "b" << "c" );
    QSortFilterProxyModel proxy;

    QHeaderView view(Qt::Vertical);
    view.setModel(&proxy);
    view.show();
    QTest::qWait(20);

    proxy.setSourceModel(&model1);
    QApplication::processEvents();
    view.swapSections(0,2);
    view.swapSections(1,4);
    QApplication::processEvents();
    QCOMPARE(checkHeaderViewOrder(&view, QVector<int>() << 2 << 4 << 0 << 3 << 1 << 5) , 0);

    proxy.setSourceModel(&model2);
    QApplication::processEvents();
    QCOMPARE(checkHeaderViewOrder(&view, QVector<int>() << 2 << 0 << 1 ) , 0);

    proxy.setSourceModel(&model1);
    QApplication::processEvents();
    QCOMPARE(checkHeaderViewOrder(&view, QVector<int>() << 2 << 0 << 1 << 3 << 4 << 5 ) , 0);
}

void tst_QHeaderView::QTBUG7833_sectionClicked()
{




    QTableView tv;
    QStandardItemModel *sim = new QStandardItemModel(&tv);
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(&tv);
    proxyModel->setSourceModel(sim);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    QList<QStandardItem *> row;
    for (int i = 0; i < 12; i++)
        row.append(new QStandardItem(QString(QLatin1Char('A' + i))));
    sim->appendRow(row);
    row.clear();
    for (int i = 12; i > 0; i--)
        row.append(new QStandardItem(QString(QLatin1Char('A' + i))));
    sim->appendRow(row);

    tv.setSortingEnabled(true);
    tv.horizontalHeader()->setSortIndicatorShown(true);
    tv.horizontalHeader()->setSectionsClickable(true);
    tv.horizontalHeader()->setStretchLastSection(true);
    tv.horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);

    tv.setModel(proxyModel);
    tv.setColumnHidden(5, true);
    tv.setColumnHidden(6, true);
    tv.horizontalHeader()->swapSections(8, 10);
    tv.sortByColumn(1, Qt::AscendingOrder);

    QSignalSpy clickedSpy(tv.horizontalHeader(), SIGNAL(sectionClicked(int)));
    QSignalSpy pressedSpy(tv.horizontalHeader(), SIGNAL(sectionPressed(int)));


    QTest::mouseClick(tv.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(tv.horizontalHeader()->sectionViewportPosition(11) + tv.horizontalHeader()->sectionSize(11)/2, 5));
    QCOMPARE(clickedSpy.count(), 1);
    QCOMPARE(pressedSpy.count(), 1);
    QCOMPARE(clickedSpy.at(0).at(0).toInt(), 11);
    QCOMPARE(pressedSpy.at(0).at(0).toInt(), 11);

    QTest::mouseClick(tv.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(tv.horizontalHeader()->sectionViewportPosition(8) + tv.horizontalHeader()->sectionSize(0)/2, 5));

    QCOMPARE(clickedSpy.count(), 2);
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(clickedSpy.at(1).at(0).toInt(), 8);
    QCOMPARE(pressedSpy.at(1).at(0).toInt(), 8);

    QTest::mouseClick(tv.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(tv.horizontalHeader()->sectionViewportPosition(0) + tv.horizontalHeader()->sectionSize(0)/2, 5));

    QCOMPARE(clickedSpy.count(), 3);
    QCOMPARE(pressedSpy.count(), 3);
    QCOMPARE(clickedSpy.at(2).at(0).toInt(), 0);
    QCOMPARE(pressedSpy.at(2).at(0).toInt(), 0);
}

void tst_QHeaderView::QTBUG8650_crashOnInsertSections()
{
    QStringList headerLabels;
    QHeaderView view(Qt::Horizontal);
    QStandardItemModel model(2,2);
    view.setModel(&model);
    view.moveSection(1, 0);
    view.hideSection(0);

    QList<QStandardItem *> items;
    items << new QStandardItem("c");
    model.insertColumn(0, items);
}

void tst_QHeaderView::QTBUG12268_hiddenMovedSectionSorting()
{
    QTableView view; // ### this test fails on QTableView &view = *m_tableview; !? + shadowing view member
    QStandardItemModel *model = new QStandardItemModel(4,3, &view);
    for (int i = 0; i< model->rowCount(); ++i)
        for (int j = 0; j< model->columnCount(); ++j)
            model->setData(model->index(i,j), QString("item [%1,%2]").arg(i).arg(j));
    view.setModel(model);
    view.horizontalHeader()->setSectionsMovable(true);
    view.setSortingEnabled(true);
    view.sortByColumn(1, Qt::AscendingOrder);
    view.horizontalHeader()->moveSection(0,2);
    view.setColumnHidden(1, true);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QCOMPARE(view.horizontalHeader()->hiddenSectionCount(), 1);
    QTest::mouseClick(view.horizontalHeader()->viewport(), Qt::LeftButton);
    QCOMPARE(view.horizontalHeader()->hiddenSectionCount(), 1);
}

void tst_QHeaderView::QTBUG14242_hideSectionAutoSize()
{
    QTableView qtv;
    QStandardItemModel amodel(4, 4);
    qtv.setModel(&amodel);
    QHeaderView *hv = qtv.verticalHeader();
    hv->setDefaultSectionSize(25);
    hv->setSectionResizeMode(QHeaderView::ResizeToContents);
    qtv.show();

    hv->hideSection(0);
    int afterlength = hv->length();

    int calced_length = 0;
    for (int u = 0; u < hv->count(); ++u)
        calced_length += hv->sectionSize(u);

    QCOMPARE(calced_length, afterlength);
}

void tst_QHeaderView::QTBUG50171_visualRegionForSwappedItems()
{
    protected_QHeaderView headerView(Qt::Horizontal);
    QtTestModel model;
    model.rows = 2;
    model.cols = 3;
    headerView.setModel(&model);
    headerView.swapSections(1, 2);
    headerView.hideSection(0);
    headerView.testVisualRegionForSelection();
}

void protected_QHeaderView::testVisualRegionForSelection()
{
    QRegion r = visualRegionForSelection(QItemSelection(model()->index(1, 0), model()->index(1, 2)));
    QCOMPARE(r.boundingRect().contains(QRect(1, 1, length() - 2, 1)), true);
}

void tst_QHeaderView::ensureNoIndexAtLength()
{
    QTableView qtv;
    QStandardItemModel amodel(4, 4);
    qtv.setModel(&amodel);
    QHeaderView *hv = qtv.verticalHeader();
    QCOMPARE(hv->visualIndexAt(hv->length()), -1);
    hv->resizeSection(hv->count() - 1, 0);
    QCOMPARE(hv->visualIndexAt(hv->length()), -1);
}

void tst_QHeaderView::offsetConsistent()
{
    // Ensure that a hidden section 'far away'
    // does not affect setOffsetToSectionPosition ..
    const int sectionToHide = 513;
    QTableView qtv;
    QStandardItemModel amodel(1000, 4);
    qtv.setModel(&amodel);
    QHeaderView *hv = qtv.verticalHeader();
    for (int u = 0; u < 100; u += 2)
        hv->resizeSection(u, 0);
    hv->setOffsetToSectionPosition(150);
    int offset1 = hv->offset();
    hv->hideSection(sectionToHide);
    hv->setOffsetToSectionPosition(150);
    int offset2 = hv->offset();
    QCOMPARE(offset1, offset2);
    // Ensure that hidden indexes (still) is considered.
    hv->resizeSection(sectionToHide, hv->sectionSize(200) * 2);
    hv->setOffsetToSectionPosition(800);
    offset1 = hv->offset();
    hv->showSection(sectionToHide);
    hv->setOffsetToSectionPosition(800);
    offset2 = hv->offset();
    QVERIFY(offset2 > offset1);
}

void tst_QHeaderView::initialSortOrderRole()
{
    QTableView view; // ### Shadowing member view (of type QHeaderView)
    QStandardItemModel *model = new QStandardItemModel(4, 3, &view);
    for (int i = 0; i< model->rowCount(); ++i)
        for (int j = 0; j< model->columnCount(); ++j)
            model->setData(model->index(i,j), QString("item [%1,%2]").arg(i).arg(j));
    QStandardItem *ascendingItem = new QStandardItem();
    QStandardItem *descendingItem = new QStandardItem();
    ascendingItem->setData(Qt::AscendingOrder, Qt::InitialSortOrderRole);
    descendingItem->setData(Qt::DescendingOrder, Qt::InitialSortOrderRole);
    model->setHorizontalHeaderItem(1, ascendingItem);
    model->setHorizontalHeaderItem(2, descendingItem);
    view.setModel(model);
    view.setSortingEnabled(true);
    view.sortByColumn(0, Qt::AscendingOrder);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    const int column1Pos = view.horizontalHeader()->sectionViewportPosition(1) + 5; // +5 not to be on the handle
    QTest::mouseClick(view.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(column1Pos, 0));
    QCOMPARE(view.horizontalHeader()->sortIndicatorSection(), 1);
    QCOMPARE(view.horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);

    const int column2Pos = view.horizontalHeader()->sectionViewportPosition(2) + 5; // +5 not to be on the handle
    QTest::mouseClick(view.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(column2Pos, 0));
    QCOMPARE(view.horizontalHeader()->sortIndicatorSection(), 2);
    QCOMPARE(view.horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);

    const int column0Pos = view.horizontalHeader()->sectionViewportPosition(0) + 5; // +5 not to be on the handle
    QTest::mouseClick(view.horizontalHeader()->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(column0Pos, 0));
    QCOMPARE(view.horizontalHeader()->sortIndicatorSection(), 0);
    QCOMPARE(view.horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);
}

const bool block_some_signals = true; // The test should also work with this set to false
const int rowcount = 500;
const int colcount = 10;

QString istr(int n, bool comma = true)
{
    QString s;
    s.setNum(n);
    if (comma)
        s += ", ";
    return s;
}

void tst_QHeaderView::calculateAndCheck(int cppline, const int precalced_comparedata[])
{
    qint64 endtimer = timer.elapsed();
    const bool silentmode = true;
    if (!silentmode)
        qDebug().nospace() << "(Time:" << endtimer << ")";

    QString sline;
    sline.setNum(cppline - 1);

    const int p1 = 3133777;      // just a prime (maybe not that random ;) )
    const int p2 = 135928393;    // just a random large prime - a bit less than signed 32-bit

    int sum_visual = 0;
    int sum_logical = 0;
    int sum_size = 0;
    int sum_hidden_size = 0;
    int sum_lookup_visual = 0;
    int sum_lookup_logical = 0;

    int chk_visual = 1;
    int chk_logical = 1;
    int chk_sizes = 1;
    int chk_hidden_size = 1;
    int chk_lookup_visual = 1;
    int chk_lookup_logical = 1;

    int header_lenght = 0;
    int lastindex = view->count() - 1;

    // calculate information based on index
    for (int i = 0; i <= lastindex; ++i) {
        int visual = view->visualIndex(i);
        int logical = view->logicalIndex(i);
        int ssize = view->sectionSize(i);

        sum_visual += visual;
        sum_logical += logical;
        sum_size += ssize;

        if (visual >= 0) {
            chk_visual %= p2;
            chk_visual *= (visual + 1) * (i + 1) * p1;
        }

        if (logical >= 0) {
            chk_logical %= p2;
            chk_logical *= (logical + 1) * (i + 1 + (logical != i) ) * p1;
        }

        if (ssize >= 0) {
            chk_sizes %= p2;
            chk_sizes *= ( (ssize + 2) * (i + 1) * p1);
        }

        if (view->isSectionHidden(i)) {
            view->showSection(i);
            int hiddensize = view->sectionSize(i);
            sum_hidden_size += hiddensize;
            chk_hidden_size %= p2;
            chk_hidden_size += ( (hiddensize + 1) * (i + 1) * p1);
            // (hiddensize + 1) in the above to differ between hidden and size 0
            // Though it can be changed (why isn't sections with size 0 hidden?)

            view->hideSection(i);
        }
    }

    // lookup indexes by pixel position
    const int max_lookup_count = 500;
    int lookup_to = view->height() + 1;
    if (lookup_to > max_lookup_count)
        lookup_to = max_lookup_count; // We limit this lookup - not to spend years when testing.
                                      // Notice that lookupTest also has its own extra test
    for (int u = 0; u < max_lookup_count; ++u) {
        int visu = view->visualIndexAt(u);
        int logi = view->logicalIndexAt(u);
        sum_lookup_visual += logi;
        sum_lookup_logical += visu;
        chk_lookup_visual %= p2;
        chk_lookup_visual *= ( (u + 1) * p1 * (visu + 2));
        chk_lookup_logical %= p2;
        chk_lookup_logical *=  ( (u + 1) * p1 * (logi + 2));
    }
    header_lenght = view->length();

    // visual and logical indexes.
    int sum_to_last_index = (lastindex * (lastindex + 1)) / 2; // == 0 + 1 + 2 + 3 + ... + lastindex

    const bool write_calced_data = false;  // Do not write calculated output (unless the test fails)
    if (write_calced_data) {
        qDebug().nospace() << "(" << cppline - 1 << ")"  // << " const int precalced[] = "
                           << " {" << chk_visual << ", " << chk_logical << ", " << chk_sizes << ", " << chk_hidden_size
                           << ", " << chk_lookup_visual << ", " << chk_lookup_logical << ", " << header_lenght << "};";
    }

    const bool sanity_checks = true;
    if (sanity_checks) {
        QString msg = QString("sanity problem at ") + sline;
        char *verifytext = QTest::toString(msg);

        QVERIFY2(m_tableview->model()->rowCount() == view->count() , verifytext);
        QVERIFY2(view->visualIndex(lastindex + 1) <= 0, verifytext);       // there is no such index in model
        QVERIFY2(view->logicalIndex(lastindex + 1) <= 0, verifytext);      // there is no such index in model.
        QVERIFY2(view->logicalIndex(lastindex + 1) <= 0, verifytext);      // there is no such index in model.
        QVERIFY2(lastindex < 0 || view->visualIndex(0) >= 0, verifytext);   // no rows or legal index
        QVERIFY2(lastindex < 0 || view->logicalIndex(0) >= 0, verifytext);  // no rows or legal index
        QVERIFY2(lastindex < 0 || view->visualIndex(lastindex) >= 0, verifytext);  // no rows or legal index
        QVERIFY2(lastindex < 0 || view->logicalIndex(lastindex) >= 0, verifytext); // no rows or legal index
        QVERIFY2(view->visualIndexAt(-1) == -1, verifytext);
        QVERIFY2(view->logicalIndexAt(-1) == -1, verifytext);
        QVERIFY2(view->visualIndexAt(view->length()) == -1, verifytext);
        QVERIFY2(view->logicalIndexAt(view->length()) == -1, verifytext);
        QVERIFY2(sum_visual == sum_logical, verifytext);
        QVERIFY2(sum_to_last_index == sum_logical, verifytext);
    }

    // Semantic test
    const bool check_semantics = true; // Otherwise there is no 'real' test
    if (!check_semantics)
        return;

    const int *x = precalced_comparedata;

    QString msg = "semantic problem at " + QString(__FILE__) + " (" + sline + ")";
    msg += "\nThe *expected* result was : {" + istr(x[0]) + istr(x[1]) + istr(x[2]) + istr(x[3])
        + istr(x[4]) + istr(x[5]) + istr(x[6], false) + "}";
    msg += "\nThe calculated result was : {";
    msg += istr(chk_visual) + istr(chk_logical) + istr(chk_sizes) + istr(chk_hidden_size)
        + istr(chk_lookup_visual) + istr(chk_lookup_logical) + istr(header_lenght, false) + "};";

    char *verifytext = QTest::toString(msg);

    QVERIFY2(chk_visual            == x[0], verifytext);
    QVERIFY2(chk_logical           == x[1], verifytext);
    QVERIFY2(chk_sizes             == x[2], verifytext);
    QVERIFY2(chk_hidden_size       == x[3], verifytext);
    QVERIFY2(chk_lookup_visual     == x[4], verifytext);
    QVERIFY2(chk_lookup_logical    == x[5], verifytext);
    QVERIFY2(header_lenght         == x[6], verifytext);
}

void tst_QHeaderView::setupTestData(bool also_use_reset_model)
{
    QTest::addColumn<bool>("updates_enabled");
    QTest::addColumn<bool>("special_prepare");
    QTest::addColumn<bool>("reset_model");

    if (also_use_reset_model) {
        QTest::newRow("no_updates+normal+reset")  << false << false << true;
        QTest::newRow("hasupdates+normal+reset")  << true << false << true;
        QTest::newRow("no_updates+special+reset") << false << true << true;
        QTest::newRow("hasupdates+special+reset") << true << true << true;
    }

    QTest::newRow("no_updates+normal")  << false << false << false;
    QTest::newRow("hasupdates+normal")  << true << false << false;
    QTest::newRow("no_updates+special") << false << true << false;
    QTest::newRow("hasupdates+special") << true << true << false;
}

void tst_QHeaderView::additionalInit()
{
    m_tableview->setVerticalHeader(view);

    QFETCH(bool, updates_enabled);
    QFETCH(bool, special_prepare);
    QFETCH(bool, reset_model);

    m_using_reset_model = reset_model;

    if (m_using_reset_model) {
        XResetModel *m = new XResetModel();
        m_tableview->setModel(m);
        delete model;
        model = m;
    } else {
        m_tableview->setModel(model);
    }

    const int default_section_size = 25;
    view->setDefaultSectionSize(default_section_size); // Important - otherwise there will be semantic changes

    model->clear();

    if (special_prepare) {

        for (int u = 0; u <= rowcount; ++u) // ensures fragmented spans in e.g Qt 4.7
            model->setRowCount(u);

        model->setColumnCount(colcount);
        view->swapSections(0, rowcount - 1);
        view->swapSections(0, rowcount - 1); // No real change - setup visual and log-indexes
        view->hideSection(rowcount - 1);
        view->showSection(rowcount - 1);  // No real change - (maybe init hidden vector)
    } else {
        model->setColumnCount(colcount);
        model->setRowCount(rowcount);
    }

    QString s;
    for (int i = 0; i < model->rowCount(); ++i) {
        model->setData(model->index(i, 0), QVariant(i));
        s.setNum(i);
        s += ".";
        s += 'a' + (i % 25);
        model->setData(model->index(i, 1), QVariant(s));
    }
    m_tableview->setUpdatesEnabled(updates_enabled);
    view->blockSignals(block_some_signals);
    timer.start();
}

void tst_QHeaderView::logicalIndexAtTest()
{
    additionalInit();

    view->swapSections(4, 9); // Make sure that visual and logical Indexes are not just the same.

    int check1 = 0;
    int check2 = 0;
    for (int u = 0; u < model->rowCount(); ++u) {
        view->resizeSection(u, 10 + u % 30);
        int v = view->visualIndexAt(u * 29);
        view->visualIndexAt(u * 29);
        check1 += v;
        check2 += u * v;
    }
    view->resizeSection(0, 0); // Make sure that we have a 0 size section - before the result set
    view->setSectionHidden(6, true); // Make sure we have a real hidden section before result set

    //qDebug() << "logicalIndexAtTest" << check1 << check2;
    const int precalced_check1 = 106327;
    const int precalced_check2 = 29856418;
    QCOMPARE(precalced_check1, check1);
    QCOMPARE(precalced_check2, check2);

    const int precalced_results[] = { 1145298384, -1710423344, -650981936, 372919464, -1544372176, -426463328, 12124 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::visualIndexAtTest()
{
    additionalInit();

    view->swapSections(4, 9); // Make sure that visual and logical Indexes are not just the same.
    int check1 = 0;
    int check2 = 0;

    for (int u = 0; u < model->rowCount(); ++u) {
        view->resizeSection(u, 3 + u % 17);
        int v = view->visualIndexAt(u * 29);
        check1 += v;
        check2 += u * v;
    }

    view->resizeSection(1, 0); // Make sure that we have a 0 size section - before the result set
    view->setSectionHidden(5, true); // Make sure we have a real hidden section before result set

    //qDebug() << "visualIndexAtTest" << check1 << check2;
    const int precalced_check1 = 72665;
    const int precalced_check2 = 14015890;
    QCOMPARE(precalced_check1, check1);
    QCOMPARE(precalced_check2, check2);

    const int precalced_results[] = { 1145298384, -1710423344, -1457520212, 169223959, 557466160, -324939600, 5453 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::hideShowTest()
{
    additionalInit();

    for (int u = model->rowCount(); u >= 0; --u)
        if (u % 8 != 0)
            view->hideSection(u);

    for (int u = model->rowCount(); u >= 0; --u)
        if (u % 3 == 0)
            view->showSection(u);

    view->setSectionHidden(model->rowCount(), true); // invalid hide (should be ignored)
    view->setSectionHidden(-1, true); // invalid hide (should be ignored)

    const int precalced_results[] = { -1523279360, -1523279360, -1321506816, 2105322423, 1879611280, 1879611280, 5225 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::swapSectionsTest()
{
    additionalInit();

    for (int u = 0; u < rowcount / 2; ++u)
        view->swapSections(u, rowcount - u - 1);

    for (int u = 0; u < rowcount; u += 2)
        view->swapSections(u, u + 1);

    view->swapSections(0, model->rowCount()); // invalid swapsection (should be ignored)

    const int precalced_results[] = { -1536450048, -1774641430, -1347156568, 1, 1719705216, -240077576, 12500 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::moveSectionTest()
{
    additionalInit();

    for (int u = 1; u < 5; ++u)
        view->moveSection(u, model->rowCount() - u);

    view->moveSection(2, model->rowCount() / 2);
    view->moveSection(0, 10);
    view->moveSection(0, model->rowCount() - 10);

    view->moveSection(0, model->rowCount()); // invalid move (should be ignored)

    const int precalced_results[] = { 645125952, 577086896, -1347156568, 1, 1719705216, 709383416, 12500 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::defaultSizeTest()
{
    additionalInit();

    view->hideSection(rowcount / 2);
    int restore_to = view->defaultSectionSize();
    view->setDefaultSectionSize(restore_to + 5);

    const int precalced_results[] = { -1523279360, -1523279360, -1739688320, -1023807777, 997629696, 997629696, 14970 };
    calculateAndCheck(__LINE__, precalced_results);

    view->setDefaultSectionSize(restore_to);
}

void tst_QHeaderView::removeTest()
{
    additionalInit();

    view->swapSections(0, 5);
    model->removeRows(0, 1);   // remove one row
    model->removeRows(4, 10);
    model->setRowCount(model->rowCount() / 2 - 1);

    if (m_using_reset_model) {
        const int precalced_results[] = { 1741224292, -135269187, -569519837, 1, 1719705216, -1184395000, 6075 };
        calculateAndCheck(__LINE__, precalced_results);
    } else {
        const int precalced_results[] = { 289162397, 289162397, -569519837, 1, 1719705216, 1719705216, 6075 };
        calculateAndCheck(__LINE__, precalced_results);
    }
}

void tst_QHeaderView::insertTest()
{
    additionalInit();

    view->swapSections(0, model->rowCount() - 1);
    model->insertRows(0, 1);   // insert one row
    model->insertRows(4, 10);
    model->setRowCount(model->rowCount() * 2 - 1);

    if (m_using_reset_model) {
        const int precalced_results[] = { 2040508069, -1280266538, -150350734, 1, 1719705216, 1331312784, 25525 };
        calculateAndCheck(__LINE__, precalced_results);
    } else {
        const int precalced_results[] = { -1909447021, 339092083, -150350734, 1, 1719705216, -969712728, 25525 };
        calculateAndCheck(__LINE__, precalced_results);
    }
}

void tst_QHeaderView::mixedTests()
{
    additionalInit();

    model->setRowCount(model->rowCount() + 10);

    for (int u = 0; u < model->rowCount(); u += 2)
        view->swapSections(u, u + 1);

    view->moveSection(0, 5);

    for (int u = model->rowCount(); u >= 0; --u) {
        if (u % 5 != 0)
            view->hideSection(u);
        if (u % 3 != 0)
            view->showSection(u);
    }

    model->insertRows(3, 7);
    model->removeRows(8, 3);
    model->setRowCount(model->rowCount() - 10);

    if (m_using_reset_model) {
        const int precalced_results[] = { 898296472, 337096378, -543340640, 1, -1251526424, -568618976, 9250 };
        calculateAndCheck(__LINE__, precalced_results);
    } else {
        const int precalced_results[] = { 1911338224, 1693514365, -613398968, -1912534953, 1582159424, -1851079000, 9300 };
        calculateAndCheck(__LINE__, precalced_results);
    }
}

void tst_QHeaderView::resizeToContentTest()
{
    additionalInit();

    QModelIndex idx = model->index(2, 2);
    model->setData(idx, QVariant("A normal string"));

    idx = model->index(1, 3);
    model->setData(idx, QVariant("A normal longer string to test resize"));

    QHeaderView *hh = m_tableview->horizontalHeader();
    hh->resizeSections(QHeaderView::ResizeToContents);
    QVERIFY(hh->sectionSize(3) > hh->sectionSize(2));

    for (int u = 0; u < 10; ++u)
        view->resizeSection(u, 1);

    view->resizeSections(QHeaderView::ResizeToContents);
    QVERIFY(view->sectionSize(1) > 1);
    QVERIFY(view->sectionSize(2) > 1);

    // Check minimum section size
    hh->setMinimumSectionSize(150);
    model->setData(idx, QVariant("i"));
    hh->resizeSections(QHeaderView::ResizeToContents);
    QCOMPARE(hh->sectionSize(3), 150);
    hh->setMinimumSectionSize(-1);

    // Check maximumSection size
    hh->setMaximumSectionSize(200);
    model->setData(idx, QVariant("This is a even longer string that is expected to be more than 200 pixels"));
    hh->resizeSections(QHeaderView::ResizeToContents);
    QCOMPARE(hh->sectionSize(3), 200);
    hh->setMaximumSectionSize(-1);

    view->setDefaultSectionSize(25); // To make sure our precalced data are correct. We do not know font height etc.

    const int precalced_results[] =  { -1523279360, -1523279360, -1347156568, 1, 1719705216, 1719705216, 12500 };
    calculateAndCheck(__LINE__, precalced_results);
}

void tst_QHeaderView::testStreamWithHide()
{
#ifndef QT_NO_DATASTREAM
    m_tableview->setVerticalHeader(view);
    m_tableview->setModel(model);
    view->setDefaultSectionSize(25);
    view->hideSection(2);
    view->swapSections(1, 2);

    QByteArray s = view->saveState();
    view->swapSections(1, 2);
    view->setDefaultSectionSize(30); // To make sure our precalced data are correct.
    view->restoreState(s);

    const int precalced_results[] =  { -1116614432, -1528653200, -1914165644, 244434607, -1111214068, 750357900, 75};
    calculateAndCheck(__LINE__, precalced_results);
#else
    QSKIP("Datastream required for testStreamWithHide. Skipping this test.");
#endif
}

void tst_QHeaderView::testStylePosition()
{
    topLevel->show();
    QVERIFY(QTest::qWaitForWindowExposed(topLevel));

    protected_QHeaderView *header = static_cast<protected_QHeaderView *>(view);

    TestStyle proxy;
    header->setStyle(&proxy);

    QImage image(1, 1, QImage::Format_ARGB32);
    QPainter p(&image);

    // 0, 1, 2, 3
    header->paintSection(&p, view->rect(), 0);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Beginning);
    header->paintSection(&p, view->rect(), 1);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Middle);
    header->paintSection(&p, view->rect(), 2);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Middle);
    header->paintSection(&p, view->rect(), 3);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::End);

    // (0),2,1,3
    view->setSectionHidden(0, true);
    view->swapSections(1, 2);
    header->paintSection(&p, view->rect(), 1);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Middle);
    header->paintSection(&p, view->rect(), 2);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Beginning);
    header->paintSection(&p, view->rect(), 3);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::End);

    // (1),2,0,(3)
    view->setSectionHidden(3, true);
    view->setSectionHidden(0, false);
    view->setSectionHidden(1, true);
    view->swapSections(0, 1);
    header->paintSection(&p, view->rect(), 0);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::End);
    header->paintSection(&p, view->rect(), 2);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::Beginning);

    // (1),2,(0),(3)
    view->setSectionHidden(0, true);
    header->paintSection(&p, view->rect(), 2);
    QCOMPARE(proxy.lastPosition, QStyleOptionHeader::OnlyOneSection);
}

void tst_QHeaderView::sizeHintCrash()
{
    QTreeView treeView;
    QStandardItemModel *model = new QStandardItemModel(&treeView);
    model->appendRow(new QStandardItem("QTBUG-48543"));
    treeView.setModel(model);
    treeView.header()->sizeHintForColumn(0);
    treeView.header()->sizeHintForRow(0);
}

QTEST_MAIN(tst_QHeaderView)
#include "tst_qheaderview.moc"
