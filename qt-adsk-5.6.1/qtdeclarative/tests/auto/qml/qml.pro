TEMPLATE = subdirs

METATYPETESTS += \
    qqmlmetatype

PUBLICTESTS += \
    parserstress \
    qjsvalueiterator \
    qjsonbinding \
    qmlmin \
    qmlplugindump \
    qqmlcomponent \
    qqmlconsole \
    qqmlengine \
    qqmlerror \
    qqmlincubator \
    qqmlinfo \
    qqmllistreference \
    qqmllocale \
    qqmlmetaobject \
    qqmlmoduleplugin \
    qqmlnotifier \
    qqmlqt \
    qqmlxmlhttprequest \
    qtqmlmodules \
    qquickfolderlistmodel \
    qqmlapplicationengine \
    qqmlsettings \
    qqmlstatemachine

PRIVATETESTS += \
    animation \
    qqmlcpputils \
    qqmlecmascript \
    qqmlcontext \
    qqmlexpression \
    qqmldirparser \
    qqmlglobal \
    qqmllanguage \
    qqmlopenmetaobject \
    qqmlproperty \
    qqmlpropertycache \
    qqmlpropertymap \
    qqmlsqldatabase \
    qqmlvaluetypes \
    qqmlvaluetypeproviders \
    qqmlbinding \
    qqmlchangeset \
    qqmlconnections \
    qqmllistcompositor \
    qqmllistmodel \
    qqmllistmodelworkerscript \
    qqmlitemmodels \
    qqmltypeloader \
    qqmlparser \
    qquickworkerscript \
    qrcqml \
    qqmltimer \
    qqmlinstantiator \
    qv4debugger \
    qqmlenginecleanup \
    v4misc \
    qqmltranslation \
    qqmlimport \
    qqmlobjectmodel

qtHaveModule(widgets) {
    PUBLICTESTS += \
        qjsengine \
        qjsvalue
}

SUBDIRS += $$PUBLICTESTS \
    qqmlextensionplugin
SUBDIRS += $$METATYPETESTS
!winrt { # no QProcess on winrt
    !contains(QT_CONFIG, no-qml-debug): SUBDIRS += debugger
    SUBDIRS += qmllint
}

contains(QT_CONFIG, private_tests) {
    SUBDIRS += $$PRIVATETESTS
}

qtNomakeTools( \
    qmlplugindump \
)
