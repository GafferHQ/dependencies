I wrote down my PySide2 porting notes and hopefully it's helpful for others who need to port PySide2.   

Contents:   
1. Handle both PySide and PySide2   
2. Shiboken  
3. QHBoxLayout QVBoxLayout  
4. QHeaderView   
5. QApplication.palette()  

### Handle both PySide and PySide2  
Background:  Qt5 add a new module QtWidgets. Compared with Qt4, Qt5 moved some classes to QtWidgets module instead of QtGui. For example, Qt4: QtGui.QWidget(). But it should be QtWidgets.QWidget() in Qt5.   
Tricky part: some classes are still in QtGui. So we can't replace all QtGui with QtWidgets simply.   
 
Solution: to both test Qt4 and Qt5 in the transition stage, I'm trying to remove module name from python scripts. The fixed code will be available for both Pyside(Qt4) and PySide2(Qt5).    
After the change, you could use QWidget() instead of QtGui.QWidget().   

    try:
        from PySide import QtGui, QtCore
        from PySide.QtGui import *
        from PySide.QtCore import *
    except:
        from PySide2 import QtGui, QtCore, QtWidgets
        from PySide2.QtGui import *
        from PySide2.QtCore import *
        from PySide2.QtWidgets import *

If you don't want to strip “QtGui” prefix, below is another solution:   
   
    try:
        from PySide import QtGui, QtCore
        import PySide.QtGui as QtWidgets
    except:
        from PySide2 import QtGui, QtCore, QtWidgets

And the following code will work for both PySide and PySide2   
QtWidget.QLabel()   
QtGui.QIcon()   
 
### Shiboken   
It doesn't work if I "import shiboken2" (windows).  It complained that ImportError: dynamic module does not define init function (initeshiboken2) .    
Logged issue: https://github.com/PySide/pyside2/issues/61   
    
Workaround: Please rename shiboken2.pyd to shiboken.pyd.  import shiboken as before (Qt4 and PySide)   

### QHBoxLayout QVBoxLayout   
Background:   
Qt4 (PySide):  QLayout::setAlignment(Qt::Alignment alignment)   
http://doc.qt.io/qt-4.8/qlayout.html#setAlignment-2   
Qt5 (PySide2): removed the above method   
http://doc.qt.io/qt-5/qlayout.html#setAlignment   
To upgrade to PySIde2, we have to make the following changes for QHBoxLayout::setAlignment(Qt::Alignment alignment)    QVBoxLayout::setAlignment(Qt::Alignment alignment)   

Qt4 (PySide):   
    layout = QHBoxLayout()   
    layout.setAlignment(Qt.AlignLeft)   
 
Qt5(PySide2) : --solution    
    layout = QHBoxLayout()   
    QLayoutItem.setAlignment(layout, Qt.AlignLeft)   

### QHeaderView    
Qt4 (PySide): http://doc.qt.io/qt-4.8/qheaderview.html   
    void	setResizeMode(ResizeMode mode)   
    void	setResizeMode(int logicalIndex, ResizeMode mode)   
Qt5(PySide2) : http://doc.qt.io/qt-5/qheaderview.html   
    void	setSectionResizeMode(ResizeMode mode)   
    void	setSectionResizeMode(int logicalIndex, ResizeMode mode)   
 
Solution:    

    pysideVersion = '-1'    
    try:    
        import PySide    
        pysideVersion = PySide.__version__    
    except:    
        import PySide2    
        pysideVersion = PySide2.__version__     
    if pysideVersion == '1.2.0' :   
        treeUI.header().setResizeMode(QHeaderView.Fixed)   
        treeUI.header().setResizeMode(TREE_COLUMN_NAME, QHeaderView.Interactive)   
        treeUI.header().setResizeMode(TREE_COLUMN_WEIGHT, QHeaderView.Stretch)   
    else:   
        treeUI.header().setSectionResizeMode(QHeaderView.Fixed)   
        treeUI.header().setSectionResizeMode(TREE_COLUMN_NAME, QHeaderView.Interactive)   
        treeUI.header().setSectionResizeMode(TREE_COLUMN_WEIGHT, QHeaderView.Stretch)   
 
### QApplication.palette()   
Background:   
Qt4:   
   QPalette QApplication::palette()   
   http://doc.qt.io/qt-4.8/qapplication.html#palette   
 
Qt5:   
   QPalette QGuiApplication::palette()   
   http://doc.qt.io/qt-5/qguiapplication.html#palette   
Solution:   
   PySide(Qt4): palette = QApplication.palette()   
   PySide2 (Qt5): palette = self.palette()  #self is widget   