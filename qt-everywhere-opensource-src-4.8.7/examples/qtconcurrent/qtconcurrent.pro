TEMPLATE      = subdirs
SUBDIRS       = imagescaling \
                map \
                runfunction \
                wordcount

!wince* {
    SUBDIRS += progressdialog
}

# install
target.path = $$[QT_INSTALL_EXAMPLES]/qtconcurrent
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS qtconcurrent.pro README
sources.path = $$[QT_INSTALL_EXAMPLES]/qtconcurrent
INSTALLS += target sources

