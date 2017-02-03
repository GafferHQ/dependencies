HEADERS     = mainwindow.h \
              pieceslist.h \
              puzzlewidget.h
RESOURCES   = puzzle.qrc
SOURCES     = main.cpp \
              mainwindow.cpp \
              pieceslist.cpp \
              puzzlewidget.cpp

QMAKE_PROJECT_NAME = dndpuzzle

# install
target.path = $$[QT_INSTALL_EXAMPLES]/draganddrop/puzzle
sources.files = $$SOURCES $$HEADERS $$RESOURCES *.pro *.jpg
sources.path = $$[QT_INSTALL_EXAMPLES]/draganddrop/puzzle
INSTALLS += target sources

symbian:{
   TARGET.UID3 = 0xA000CF65
   include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
   addFile.files = example.jpg
   addFile.path = .
   DEPLOYMENT += addFile
}
wince*: {
   addFile.files = example.jpg
   addFile.path = .
   DEPLOYMENT += addFile
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
