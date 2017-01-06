/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#! [0]
make all
#! [0]


#! [1]
CONFIG += qt thread debug
#! [1]


#! [2]
CONFIG += qt
QT += network xml
#! [2]


#! [3]
QT = network xml # This will omit the core and gui modules.
#! [3]


#! [4]
QT -= gui # Only the core module is used.
#! [4]


#! [5]
CONFIG += link_pkgconfig
PKGCONFIG += ogg dbus-1
#! [5]


#! [6]
LIBS += -L/usr/local/lib -lmath
#! [6]


#! [7]
INCLUDEPATH = c:/msdev/include d:/stl/include
#! [7]


#! [8]
qmake [mode] [options] files
#! [8]


#! [9]
qmake -makefile [options] files
#! [9]


#! [10]
qmake -makefile -unix -o Makefile "CONFIG+=test" test.pro
#! [10]


#! [11]
qmake "CONFIG+=test" test.pro
#! [11]


#! [12]
qmake -project [options] files
#! [12]


#! [13]
qmake -spec macx-g++
#! [13]


#! [14]
QMAKE_LFLAGS += -F/path/to/framework/directory/
#! [14]


#! [15]
LIBS += -framework TheFramework
#! [15]


#! [16]
TEMPLATE = lib
CONFIG += lib_bundle
#! [16]


#! [17]
FRAMEWORK_HEADERS.version = Versions
FRAMEWORK_HEADERS.files = path/to/header_one.h path/to/header_two.h
FRAMEWORK_HEADERS.path = Headers
QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
#! [17]


#! [18]
CONFIG += x86 ppc
#! [18]


#! [19]
qmake -spec macx-xcode project.pro
#! [19]


#! [20]
qmake -tp vc
#! [20]


#! [21]
qmake -tp vc -r
#! [21]


#! [22]
CONFIG -= embed_manifest_exe
#! [22]


#! [23]
CONFIG -= embed_manifest_dll
#! [23]


#! [24]
make all
#! [24]


#! [25]
build_pass:CONFIG(debug, debug|release) {
    unix: TARGET = $$join(TARGET,,,_debug)
    else: TARGET = $$join(TARGET,,,d)
}
#! [25]


#! [26]
CONFIG += qt console newstuff
...
newstuff {
    SOURCES += new.cpp
    HEADERS += new.h
}
#! [26]


#! [27]
DEFINES += USE_MY_STUFF QT_DLL
#! [27]


#! [28]
myFiles.files = path\*.png
DEPLOYMENT += myFiles
#! [28]


#! [29]
myFiles.files = path\file1.ext1 path2\file2.ext1 path3\*
myFiles.path = \some\path\on\device
someother.files = C:\additional\files\*
someother.path = \myFiles\path2
DEPLOYMENT += myFiles someother
#! [29]


#! [30]
DESTDIR = ../../lib
#! [30]


#! [31]
DISTFILES += ../program.txt
#! [31]


#! [32]
FORMS = mydialog.ui \
    mywidget.ui \
        myconfig.ui
#! [32]


#! [33]
FORMS3 = my_uic3_dialog.ui \
     my_uic3_widget.ui \
         my_uic3_config.ui
#! [33]


#! [34]
HEADERS = myclass.h \
          login.h \
          mainwindow.h
#! [34]


#! [35]
INCLUDEPATH = c:/msdev/include d:/stl/include
#! [35]


#! [36]
target.path += $$[QT_INSTALL_PLUGINS]/imageformats
INSTALLS += target
#! [36]


#! [37]
LEXSOURCES = lexer.l
#! [37]


#! [38]
unix:LIBS += -L/usr/local/lib -lmath
win32:LIBS += c:/mylibs/math.lib
#! [38]


#! [39]
CONFIG += no_lflags_merge
#! [39]


#! [40]
unix:MOC_DIR = ../myproject/tmp
win32:MOC_DIR = c:/myproject/tmp
#! [40]


#! [41]
unix:OBJECTS_DIR = ../myproject/tmp
win32:OBJECTS_DIR = c:/myproject/tmp
#! [41]


#! [42]
app {
    # Conditional code for 'app' template here
}
#! [42]


#! [43]
FRAMEWORK_HEADERS.version = Versions
FRAMEWORK_HEADERS.files = path/to/header_one.h path/to/header_two.h
FRAMEWORK_HEADERS.path = Headers
QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
#! [43]


#! [44]
QMAKE_BUNDLE_EXTENSION = .myframework
#! [44]


#! [45]
QMAKE_RESOURCE_FLAGS += -threshold 0 -compress 9
#! [45]


#! [46]
QMAKE_UIC = uic -L /path/to/plugin
#! [46]


#! [47]
QT -= gui # Only the core module is used.
#! [47]


#! [48]
unix:RCC_DIR = ../myproject/resources
win32:RCC_DIR = c:/myproject/resources
#! [48]


#! [49]
SOURCES = myclass.cpp \
      login.cpp \
      mainwindow.cpp
#! [49]


#! [50]
SUBDIRS = kernel \
          tools \
          myapp
#! [50]


#! [51]
CONFIG += ordered
#! [51]


#! [52]
TEMPLATE = app
TARGET = myapp
SOURCES = main.cpp
#! [52]


#! [53]
TEMPLATE = lib
SOURCES = main.cpp
TARGET = mylib
#! [53]


#! [54]
unix:UI_DIR = ../myproject/ui
win32:UI_DIR = c:/myproject/ui
#! [54]


#! [55]
unix:UI_HEADERS_DIR = ../myproject/ui/include
win32:UI_HEADERS_DIR = c:/myproject/ui/include
#! [55]


#! [56]
unix:UI_SOURCES_DIR = ../myproject/ui/src
win32:UI_SOURCES_DIR = c:/myproject/ui/src
#! [56]


#! [57]
VERSION = 1.2.3
#! [57]


#! [58]
YACCSOURCES = moc.y
#! [58]


#! [59]
FILE = /etc/passwd
FILENAME = $$basename(FILE) #passwd
#! [59]


#! [60]
CONFIG = debug
CONFIG += release
CONFIG(release, debug|release):message(Release build!) #will print
CONFIG(debug, debug|release):message(Debug build!) #no print
#! [60]


#! [61]
contains( drivers, network ) {
    # drivers contains 'network'
    message( "Configuring for network build..." )
    HEADERS += network.h
    SOURCES += network.cpp
}
#! [61]


#! [62]
error(An error has occurred in the configuration process.)
#! [62]


#! [63]
exists( $(QTDIR)/lib/libqt-mt* ) {
      message( "Configuring for multi-threaded Qt..." )
      CONFIG += thread
}
#! [63]


#! [64]
MY_VAR = one two three four
MY_VAR2 = $$join(MY_VAR, " -L", -L) -Lfive
MY_VAR3 = $$member(MY_VAR, 2) $$find(MY_VAR, t.*)
#! [64]


#! [65]
LIST = 1 2 3
for(a, LIST):exists(file.$${a}):message(I see a file.$${a}!)
#! [65]


#! [66]
include( shared.pri )
OPTIONS = standard custom
!include( options.pri ) {
    message( "No custom build options specified" )
OPTIONS -= custom
}
#! [66]


#! [67]
isEmpty( CONFIG ) {
CONFIG += qt warn_on debug
}
#! [67]


#! [68]
message( "This is a message" )
#! [68]


#! [69]
!build_pass:message( "This is a message" )
#! [69]


#! [70]
This is a test.
#! [70]


#! [71]
system(ls /bin):HAS_BIN=FALSE
#! [71]


#! [72]
UNAME = $$system(uname -s)
contains( UNAME, [lL]inux ):message( This looks like Linux ($$UNAME) to me )
#! [72]


#! [73]
ARGS = 1 2 3 2 5 1
ARGS = $$unique(ARGS) #1 2 3 5
#! [73]


#! [74]
qmake -set VARIABLE VALUE
#! [74]


#! [75]
qmake -query VARIABLE
qmake -query #queries all current VARIABLE/VALUE pairs..
#! [75]


#! [76]
qmake -query "1.06a/VARIABLE"
#! [76]


#! [77]
qmake -query "QT_INSTALL_PREFIX"
#! [77]


#! [78]
QMAKE_VERS = $$[QMAKE_VERSION]
#! [78]


#! [79]
documentation.path = /usr/local/program/doc
documentation.files = docs/*
#! [79]


#! [80]
INSTALLS += documentation
#! [80]


#! [81]
unix:documentation.extra = create_docs; mv master.doc toc.doc
#! [81]


#! [82]
target.path = /usr/local/myprogram
INSTALLS += target
#! [82]


#! [83]
CONFIG += create_prl
#! [83]


#! [84]
CONFIG += link_prl
#! [84]


#! [85]
QMAKE_EXT_MOC = .mymoc
#! [85]


#! [86]
mytarget.target = .buildfile
mytarget.commands = touch $$mytarget.target
mytarget.depends = mytarget2

mytarget2.commands = @echo Building $$mytarget.target
#! [86]


#! [87]
QMAKE_EXTRA_TARGETS += mytarget mytarget2
#! [87]


#! [88]
new_moc.output  = moc_${QMAKE_FILE_BASE}.cpp
new_moc.commands = moc ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}
new_moc.depend_command = g++ -E -M ${QMAKE_FILE_NAME} | sed "s,^.*: ,,"
new_moc.input = NEW_HEADERS
QMAKE_EXTRA_COMPILERS += new_moc
#! [88]


#! [89]
TARGET = myapp
#! [89]


#! [90]
DEFINES += QT_DLL
#! [90]


#! [91]
DEFINES -= QT_DLL
#! [91]


#! [92]
DEFINES *= QT_DLL
#! [92]


#! [93]
DEFINES ~= s/QT_[DT].+/QT
#! [93]


#! [94]
EVERYTHING = $$SOURCES $$HEADERS
message("The project contains the following files:")
message($$EVERYTHING)
#! [94]


#! [95]
win32:DEFINES += QT_DLL
#! [95]


#! [96]
win32:xml {
    message(Building for Windows)
    SOURCES += xmlhandler_win.cpp
} else:xml {
    SOURCES += xmlhandler.cpp
} else {
    message("Unknown configuration")
}
#! [96]


#! [97]
MY_VARIABLE = value
#! [97]


#! [98]
MY_DEFINES = $$DEFINES
#! [98]


#! [99]
MY_DEFINES = $${DEFINES}
#! [99]


#! [100]
TARGET = myproject_$${TEMPLATE}
#! [100]


#! [101]
target.path = $$[QT_INSTALL_PLUGINS]/designer
INSTALLS += target
#! [101]


#! [102]
defineReplace(functionName){
    #function code
}
#! [102]


#! [103]
CONFIG += myfeatures
#! [103]


#! [105]
PRECOMPILED_HEADER = stable.h
#! [105]


#! [106]
precompile_header:!isEmpty(PRECOMPILED_HEADER) {
DEFINES += USING_PCH
}
#! [106]


#! [107]
PRECOMPILED_HEADER = window.h
SOURCES            = window.cpp
#! [107]


#! [108]
SOURCES += hello.cpp
#! [108]


#! [109]
SOURCES += hello.cpp
SOURCES += main.cpp
#! [109]


#! [110]
SOURCES = hello.cpp \
          main.cpp
#! [110]


#! [111]
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
#! [111]


#! [112]
TARGET = helloworld
#! [112]


#! [113]
CONFIG += qt
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
#! [113]


#! [114]
qmake -o Makefile hello.pro
#! [114]


#! [115]
qmake -tp vc hello.pro
#! [115]


#! [116]
CONFIG += qt debug
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
#! [116]


#! [117]
win32 {
    SOURCES += hellowin.cpp
}
#! [117]


#! [118]
CONFIG += qt debug
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
win32 {
    SOURCES += hellowin.cpp
}
unix {
    SOURCES += hellounix.cpp
}
#! [118]


#! [119]
!exists( main.cpp ) {
    error( "No main.cpp file found" )
}
#! [119]


#! [120]
CONFIG += qt debug
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
win32 {
    SOURCES += hellowin.cpp
}
unix {
    SOURCES += hellounix.cpp
}
!exists( main.cpp ) {
    error( "No main.cpp file found" )
}
#! [120]


#! [121]
win32 {
    debug {
        CONFIG += console
    }
}
#! [121]


#! [122]
CONFIG += qt debug
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
win32 {
    SOURCES += hellowin.cpp
}
unix {
    SOURCES += hellounix.cpp
}
!exists( main.cpp ) {
    error( "No main.cpp file found" )
}
win32:debug {
    CONFIG += console
}
#! [122]


#! [123]
TEMPLATE = app
DESTDIR  = c:/helloapp
HEADERS += hello.h
SOURCES += hello.cpp
SOURCES += main.cpp
DEFINES += QT_DLL
CONFIG  += qt warn_on release
#! [123]


#! [124]
make all
#! [124]


#! [125]
make
#! [125]


#! [126]
make install
#! [126]


#! [127]
CONFIG(debug, debug|release) {
    mac: TARGET = $$join(TARGET,,,_debug)
    win32: TARGET = $$join(TARGET,,d)
}
#! [127]

#! [128]
customplugin.files = customimageplugin.dll
customplugin.files += c:\myplugins\othercustomimageplugin.dll
customplugin.path = imageformats
dynamiclibrary.files = mylib.dll helper.exe
dynamiclibrary.path = \sys\bin
globalplugin.files = someglobalimageplugin.dll
globalplugin.path = \resource\qt\plugins\imageformats
DEPLOYMENT += customplugin dynamiclibrary globalplugin
#! [128]

#! [129]
TARGET.EPOCALLOWDLLDATA = 1
#! [129]

#! [130]
TARGET.EPOCHEAPSIZE = 10000 10000000
TARGET.EPOCSTACKSIZE = 0x8000
#! [130]

#! [131]
QMAKE_CXXFLAGS.CW += -O2
QMAKE_CXXFLAGS.ARMCC += -O0
#! [131]

#! [132]
TARGET.UID2 = 0x00000001
TARGET.UID3 = 0x00000002
TARGET.SID = 0x00000003
TARGET.VID = 0x00000004
#! [132]

#! [133]
TARGET.CAPABILITY += AllFiles
#! [133]

#! [134]
TARGET.CAPABILITY = ALL -TCB -DRM -AllFiles
#! [134]

#! [135]
TARGET.EPOCHEAPSIZE = 10000 10000000
#! [135]

#! [136]
TARGET.EPOCSTACKSIZE = 0x8000
#! [136]

#! [137]
MMP_RULES += "DEFFILE hello.def"
#! [137]

#! [138]
myBlock = \
"START RESOURCE foo.rss" \
"TARGET bar" \
"TARGETPATH private\10001234" \
"HEADER" \
"LANG 01" \
"UID 0x10002345 0x10003456" \
"END"

MMP_RULES += myBlock
#! [138]

#! [139]
myIfdefBlock = \
"$${LITERAL_HASH}ifdef WINSCW" \
"DEFFILE hello_winscw.def" \
"$${LITERAL_HASH}endif" 

MMP_RULES += myIfdefBlock
#! [139]

#! [140]
somelib.files = somelib.dll
somelib.path = \sys\bin
somelib.pkg_prerules = "(0x12345678), 2, 2, 0, {\"Some Package\"}" \
                  "(0x87654321), 1, *, * ~ 2, 2, 0, {\"Some Other Package\"}"
justdep.pkg_prerules = "(0xAAAABBBB), 0, 2, 0, {\"My Framework\"}"                  
DEPLOYMENT += somelib justdep
#! [140]

#! [141]
default_deployment.pkg_prerules -= pkg_platform_dependencies
my_deployment.pkg_prerules = "[0x11223344],0,0,0,{\"SomeSpecificDeviceID\"}"
DEPLOYMENT += my_deployment
#! [141]

#! [142]
DEPLOYMENT_PLUGIN += qjpeg
#! [142]

#! [143]
myextension = \
    "start extension myextension" \
    "$${LITERAL_HASH}if defined(WINSCW)" \
    "option MYOPTION foo" \
    "$${LITERAL_HASH}endif" \
    "option MYOPTION bar" \
    "end"
BLD_INF_RULES.prj_extensions += myextension
#! [143]

#! [144]
RSS_RULES += "hidden = KAppIsHidden;" 
#! [144]

#! [145]
myrssrules = \
    "hidden = KAppIsHidden;" \
    "launch = KAppLaunchInBackground;" \
RSS_RULES += myrssrules
#! [145]

#! [146]
DEPLOYMENT.installer_header = 0x12345678
#! [146]

#! [147]
DEPLOYMENT.installer_header = "$${LITERAL_HASH}{\"My Application Installer\"},(0x12345678),1,0,0"
#! [147]

#! [148]
# Set conditional libraries
LIB.MARM = "LIBRARY myarm.lib"
LIB.WINSCW = "LIBRARY mywinscw.lib"
LIB.default = "LIBRARY mydefault.lib"

# Add the conditional MMP rules
MYCONDITIONS = MARM WINSCW
MYVARIABLES = LIB

addMMPRules(MYCONDITIONS, MYVARIABLES)
#! [148]

#! [149]
SUBDIRS += my_executable my_library
my_executable.subdir = app
my_executable.depends = my_library
my_library.subdir = lib
#! [149]

#! [150]
symbian {
    SUBDIRS += emulator_dll
    emulator_dll.condition = WINSCW
}
#! [150]

#! [151]
RSS_RULES.service_list += "uid = 0x12345678; datatype_list = \{\}; opaque_data = r_my_icon;"
RSS_RULES.footer +="RESOURCE CAPTION_AND_ICON_INFO r_my_icon \{ icon_file =\"$$PWD/my_icon.svg\"; \}"
#! [151]

#! [152]
my_exports = \
    "foo.h /epoc32/include/mylib/foo.h" \
    "bar.h /epoc32/include/mylib/bar.h"
BLD_INF_RULES.prj_exports += my_exports
#! [152]

#! [153]
my_note.pkg_postrules.installer = "\"myinstallnote.txt\" - \"\", FILETEXT, TEXTCONTINUE"
DEPLOYMENT += my_note
#! [153]

#! [154]
DEPLOYMENT -= default_bin_deployment default_resource_deployment default_reg_deployment
#! [154]

#! [155]
default_bin_deployment.flags += FILERUN RUNINSTALL
dep_note.files = install_note.txt
dep_note.flags = FILETEXT TEXTEXIT
DEPLOYMENT += dep_note
#! [155]

#! [156]
DEPLOYMENT.display_name = My Qt App
#! [156]

#! [157]
packagesExist(sqlite3 QtNetwork QtDeclarative) {
    DEFINES += USE_FANCY_UI
}
#! [157]

#! [158]
#ifdef USE_FANCY_UI
    // Use the fancy UI, as we have extra packages available
#endif
#! [158]

#! [159]
RSS_RULES += "graphics_memory=12288;"
#! [159]
