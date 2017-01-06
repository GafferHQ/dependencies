TEMPLATE      = subdirs
SUBDIRS       = analogclock \
                applicationicon \
                calculator \
                calendarwidget \
                charactermap \
                codeeditor \
                digitalclock \
                elidedlabel \
                groupbox \
                icons \
                imageviewer \
                lineedits \
                movie \
                orientation \
                scribble \
                shapedclock \
                sliders \
                spinboxes \
                stylesheet \
                tablet \
                tetrix \
                tooltips \
                validators \
                wiggly \
                windowflags \

symbian: SUBDIRS = \
                analogclock \
                calculator \
                calendarwidget \
                lineedits \
                shapedclock \
		symbianvibration \
                tetrix \
                wiggly \
                softkeys

maemo5: SUBDIRS += maemovibration

contains(styles, motif): SUBDIRS += styles

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS widgets.pro README
sources.path = $$[QT_INSTALL_EXAMPLES]/widgets
INSTALLS += target sources

