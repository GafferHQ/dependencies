# Use subdirs template to suppress generation of unnecessary files
TEMPLATE = subdirs

symbian: {
    load(data_caging_paths)

    SUBDIRS=
    # WARNING: Changing TARGET name will break Symbian SISX upgrade functionality
    # DO NOT TOUCH TARGET VARIABLE IF YOU ARE NOT SURE WHAT YOU ARE DOING
    TARGET = "Qt$${QT_LIBINFIX}"

    isEmpty(QT_LIBINFIX) {
        TARGET.UID3 = 0x2001E61C
    } else {
        # Always use experimental UID for infixed configuration to avoid UID clash
        TARGET.UID3 = 0xE001E61C
    }
    VERSION=$${QT_MAJOR_VERSION}.$${QT_MINOR_VERSION}.$${QT_PATCH_VERSION}

    DESTDIR = $$QMAKE_LIBDIR_QT

    qtlibraries.files = \
        $$QMAKE_LIBDIR_QT/QtCore$${QT_LIBINFIX}.dll \
        $$QMAKE_LIBDIR_QT/QtXml$${QT_LIBINFIX}.dll \
        $$QMAKE_LIBDIR_QT/QtGui$${QT_LIBINFIX}.dll \
        $$QMAKE_LIBDIR_QT/QtNetwork$${QT_LIBINFIX}.dll \
        $$QMAKE_LIBDIR_QT/QtTest$${QT_LIBINFIX}.dll \
        $$QMAKE_LIBDIR_QT/QtSql$${QT_LIBINFIX}.dll

    # Symbian exports do not like absolute paths, so generate a relative path to original .pro file dir
    S60_INSTALLS_SOURCE_DIR = $$relativeProPath()

    symbian-abld|symbian-sbsv2 {
        pluginLocations = $${EPOCROOT}epoc32/release/$(PLATFORM)/$(TARGET)
        bearerPluginLocation = $${EPOCROOT}epoc32/release/$(PLATFORM)/$(TARGET)
        bearerStubZ = $${EPOCROOT}$${HW_ZDIR}$${QT_PLUGINS_BASE_DIR}/bearer/qsymbianbearer$${QT_LIBINFIX}.qtplugin
    } else {
        pluginLocations = $$QT_BUILD_TREE/plugins/s60
        bearerPluginLocation = $$QT_BUILD_TREE/plugins/bearer
        bearerStubZ = $${PWD}/qsymbianbearer.qtplugin
    }

    contains(S60_VERSION, 5.0) {
        qts60plugindeployment = \
            "IF package(0x2003A678) OR package(0x20022E6D)" \
            "   \"$$bearerPluginLocation/qsymbianbearer$${QT_LIBINFIX}.dll\" - \"c:\\sys\\bin\\qsymbianbearer$${QT_LIBINFIX}.dll\"" \
            "ELSEIF package(0x1028315F)" \
            "   \"$$bearerPluginLocation/qsymbianbearer$${QT_LIBINFIX}_3_2.dll\" - \"c:\\sys\\bin\\qsymbianbearer$${QT_LIBINFIX}.dll\"" \
            "ELSE" \
            "   \"$$bearerPluginLocation/qsymbianbearer$${QT_LIBINFIX}.dll\" - \"c:\\sys\\bin\\qsymbianbearer$${QT_LIBINFIX}.dll\"" \
            "ENDIF" \
            "   \"$$bearerStubZ\" - \"c:$$replace(QT_PLUGINS_BASE_DIR,/,\\)\\bearer\\qsymbianbearer$${QT_LIBINFIX}.qtplugin\""
        qtlibraries.pkg_postrules += qts60plugindeployment
        BLD_INF_RULES.prj_exports += \
            "$$S60_INSTALLS_SOURCE_DIR/qsymbianbearer.qtplugin /$${HW_ZDIR}$${QT_PLUGINS_BASE_DIR}/bearer/qsymbianbearer$${QT_LIBINFIX}.qtplugin" \
            "$$S60_INSTALLS_SOURCE_DIR/qsymbianbearer.qtplugin /epoc32/winscw/c$${QT_PLUGINS_BASE_DIR}/bearer/qsymbianbearer$${QT_LIBINFIX}.qtplugin"
    } else {
        # No need to deploy plugins for older platform versions when building on Symbian3 or later
        bearer_plugin.files = $$QT_BUILD_TREE/plugins/bearer/qsymbianbearer$${QT_LIBINFIX}.dll
        bearer_plugin.path = c:$$QT_PLUGINS_BASE_DIR/bearer
        DEPLOYMENT += bearer_plugin
    }

    qtlibraries.path = c:/sys/bin

    vendorinfo = \
        "; Localised Vendor name" \
        "%{\"Nokia\"}" \
        " " \
        "; Unique Vendor name" \
        ":\"Nokia, Qt\"" \
        " "

    ru_header = "$${LITERAL_HASH}{\"$${TARGET}\"}, ($$TARGET.UID3), $${QT_MAJOR_VERSION},$${QT_MINOR_VERSION},$${QT_PATCH_VERSION}, TYPE=SA,RU"

    qtlibraries.pkg_prerules = ru_header vendorinfo
    qtlibraries.pkg_prerules += "; Dependencies of Qt libraries"

    # It is expected that Symbian^3 and newer phones will have sufficiently new OpenC already installed
    contains(S60_VERSION, 5.0) {
        qtlibraries.pkg_prerules += "(0x20013851), 1, 5, 1, {\"PIPS Installer\"}"
        contains(QT_CONFIG, openssl) | contains(QT_CONFIG, openssl-linked) {
            qtlibraries.pkg_prerules += "(0x200110CB), 1, 5, 1, {\"Open C LIBSSL Common\"}"
        }
        contains(CONFIG, stl) {
            qtlibraries.pkg_prerules += "(0x2000F866), 1, 0, 0, {\"Standard C++ Library Common\"}"
        }
    }
    qtlibraries.pkg_prerules += "(0x2002af5f), 0, 5, 0, {\"sqlite3\"}"

    !contains(QT_CONFIG, no-jpeg): imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qjpeg$${QT_LIBINFIX}.dll
    !contains(QT_CONFIG, no-gif):  imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qgif$${QT_LIBINFIX}.dll
    !contains(QT_CONFIG, no-mng):  imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qmng$${QT_LIBINFIX}.dll
    !contains(QT_CONFIG, no-tiff): imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qtiff$${QT_LIBINFIX}.dll
    !contains(QT_CONFIG, no-ico):  imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qico$${QT_LIBINFIX}.dll
    imageformats_plugins.path = c:$$QT_PLUGINS_BASE_DIR/imageformats

    codecs_plugins.files = $$QT_BUILD_TREE/plugins/codecs/qcncodecs$${QT_LIBINFIX}.dll $$QT_BUILD_TREE/plugins/codecs/qjpcodecs$${QT_LIBINFIX}.dll $$QT_BUILD_TREE/plugins/codecs/qtwcodecs$${QT_LIBINFIX}.dll $$QT_BUILD_TREE/plugins/codecs/qkrcodecs$${QT_LIBINFIX}.dll
    codecs_plugins.path = c:$$QT_PLUGINS_BASE_DIR/codecs

    contains(QT_CONFIG, phonon-backend) {
        phonon_backend_plugins.files += $$QT_BUILD_TREE/plugins/phonon_backend/phonon_mmf$${QT_LIBINFIX}.dll

        phonon_backend_plugins.path = c:$$QT_PLUGINS_BASE_DIR/phonon_backend
        DEPLOYMENT += phonon_backend_plugins
    }

    # Support backup & restore for Qt libraries
    qtbackup.files = backup_registration.xml
    qtbackup.path = c:/private/10202d56/import/packages/$$lower($$replace(TARGET.UID3, 0x,))

    DEPLOYMENT += qtlibraries \
                  qtbackup \
                  imageformats_plugins \
                  codecs_plugins \
                  graphicssystems_plugins

    contains(QT_CONFIG, svg): {
       qtlibraries.files += $$QMAKE_LIBDIR_QT/QtSvg$${QT_LIBINFIX}.dll
       imageformats_plugins.files += $$QT_BUILD_TREE/plugins/imageformats/qsvg$${QT_LIBINFIX}.dll
       iconengines_plugins.files = $$QT_BUILD_TREE/plugins/iconengines/qsvgicon$${QT_LIBINFIX}.dll
       iconengines_plugins.path = c:$$QT_PLUGINS_BASE_DIR/iconengines
       DEPLOYMENT += iconengines_plugins
    }

    contains(QT_CONFIG, phonon): {
       qtlibraries.files += $$QMAKE_LIBDIR_QT/phonon$${QT_LIBINFIX}.dll
    }

    contains(QT_CONFIG, script): {
        qtlibraries.files += $$QMAKE_LIBDIR_QT/QtScript$${QT_LIBINFIX}.dll
    }

    contains(QT_CONFIG, xmlpatterns): {
       qtlibraries.files += $$QMAKE_LIBDIR_QT/QtXmlPatterns$${QT_LIBINFIX}.dll
    }

    contains(QT_CONFIG, declarative): {
        qtlibraries.files += $$QMAKE_LIBDIR_QT/QtDeclarative$${QT_LIBINFIX}.dll

        folderlistmodelImport.sources = $$QT_BUILD_TREE/imports/Qt/labs/folderlistmodel/qmlfolderlistmodelplugin$${QT_LIBINFIX}.dll
        gesturesImport.sources = $$QT_BUILD_TREE/imports/Qt/labs/gestures/qmlgesturesplugin$${QT_LIBINFIX}.dll
        particlesImport.sources = $$QT_BUILD_TREE/imports/Qt/labs/particles/qmlparticlesplugin$${QT_LIBINFIX}.dll

        folderlistmodelImport.sources += $$QT_SOURCE_TREE/src/imports/folderlistmodel/qmldir
        gesturesImport.sources += $$QT_SOURCE_TREE/src/imports/gestures/qmldir
        particlesImport.sources += $$QT_SOURCE_TREE/src/imports/particles/qmldir

        folderlistmodelImport.path = c:$$QT_IMPORTS_BASE_DIR/Qt/labs/folderlistmodel
        gesturesImport.path = c:$$QT_IMPORTS_BASE_DIR/Qt/labs/gestures
        particlesImport.path = c:$$QT_IMPORTS_BASE_DIR/Qt/labs/particles

        DEPLOYMENT += folderlistmodelImport gesturesImport particlesImport

        contains(QT_CONFIG, opengl) {
            shadersImport.sources = $$QT_BUILD_TREE/imports/Qt/labs/shaders/qmlshadersplugin$${QT_LIBINFIX}.dll \
                                    $$QT_SOURCE_TREE/src/imports/shaders/qmldir
            shadersImport.path = c:$$QT_IMPORTS_BASE_DIR/Qt/labs/shaders
            DEPLOYMENT += shadersImport
        }
    }

    graphicssystems_plugins.path = c:$$QT_PLUGINS_BASE_DIR/graphicssystems
    contains(QT_CONFIG, openvg) {
        qtlibraries.files += $$QMAKE_LIBDIR_QT/QtOpenVG$${QT_LIBINFIX}.dll
        graphicssystems_plugins.files += $$QT_BUILD_TREE/plugins/graphicssystems/qvggraphicssystem$${QT_LIBINFIX}.dll
    }

    contains(QT_CONFIG, opengl) {
        qtlibraries.files += $$QMAKE_LIBDIR_QT/QtOpenGL$${QT_LIBINFIX}.dll
        graphicssystems_plugins.files += $$QT_BUILD_TREE/plugins/graphicssystems/qglgraphicssystem$${QT_LIBINFIX}.dll
    }

    contains(QT_CONFIG, multimedia){
        qtlibraries.files += $$QMAKE_LIBDIR_QT/QtMultimedia$${QT_LIBINFIX}.dll
    }

    BLD_INF_RULES.prj_exports += "$$S60_INSTALLS_SOURCE_DIR/qt.iby $$CORE_MW_LAYER_IBY_EXPORT_PATH(qt.iby)"
    BLD_INF_RULES.prj_exports += "$$S60_INSTALLS_SOURCE_DIR/qt_resources.iby $$LANGUAGE_MW_LAYER_IBY_EXPORT_PATH(qt_resources.iby)"
}
