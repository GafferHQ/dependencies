/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include "qqmlsettings_p.h"
#include <qcoreevent.h>
#include <qsettings.h>
#include <qpointer.h>
#include <qdebug.h>
#include <qhash.h>

QT_BEGIN_NAMESPACE

/*!
    \qmlmodule Qt.labs.settings 1.0
    \title Qt Labs Settings QML Types
    \ingroup qmlmodules
    \brief Provides persistent platform-independent application settings.

    To use this module, import the module with the following line:

    \code
    import Qt.labs.settings 1.0
    \endcode
*/

/*!
    \qmltype Settings
    \instantiates QQmlSettings
    \inqmlmodule Qt.labs.settings
    \ingroup settings
    \brief Provides persistent platform-independent application settings.

    The Settings type provides persistent platform-independent application settings.

    \note This type is made available by importing the \b Qt.labs.settings module.
    \e {Types in the Qt.labs module are not guaranteed to remain compatible
    in future versions.}

    Users normally expect an application to remember its settings (window sizes
    and positions, options, etc.) across sessions. The Settings type enables you
    to save and restore such application settings with the minimum of effort.

    Individual setting values are specified by declaring properties within a
    Settings element. All \l {QML Basic Types}{basic type} properties are
    supported. The recommended approach is to use property aliases in order
    to get automatic property updates both ways. The following example shows
    how to use Settings to store and restore the geometry of a window.

    \qml
    import QtQuick.Window 2.1
    import Qt.labs.settings 1.0

    Window {
        id: window

        width: 800
        height: 600

        Settings {
            property alias x: window.x
            property alias y: window.y
            property alias width: window.width
            property alias height: window.height
        }
    }
    \endqml

    At first application startup, the window gets default dimensions specified
    as 800x600. Notice that no default position is specified - we let the window
    manager handle that. Later when the window geometry changes, new values will
    be automatically stored to the persistent settings. The second application
    run will get initial values from the persistent settings, bringing the window
    back to the previous position and size.

    A fully declarative syntax, achieved by using property aliases, comes at the
    cost of storing persistent settings whenever the values of aliased properties
    change. Normal properties can be used to gain more fine-grained control over
    storing the persistent settings. The following example illustrates how to save
    a setting on component destruction.

    \qml
    import QtQuick 2.1
    import Qt.labs.settings 1.0

    Item {
        id: page

        state: settings.state

        states: [
            State {
                name: "active"
                // ...
            },
            State {
                name: "inactive"
                // ...
            }
        ]

        Settings {
            id: settings
            property string state: "active"
        }

        Component.onDestruction: {
            settings.state = page.state
        }
    }
    \endqml

    Notice how the default value is now specified in the persistent setting property,
    and the actual property is bound to the setting in order to get the initial value
    from the persistent settings.

    \section1 Application Identifiers

    Application specific settings are identified by providing application
    \l {QCoreApplication::applicationName}{name},
    \l {QCoreApplication::organizationName}{organization} and
    \l {QCoreApplication::organizationDomain}{domain}.

    \code
    #include <QGuiApplication>
    #include <QQmlApplicationEngine>

    int main(int argc, char *argv[])
    {
        QGuiApplication app(argc, argv);
        app.setOrganizationName("Some Company");
        app.setOrganizationDomain("somecompany.com");
        app.setApplicationName("Amazing Application");

        QQmlApplicationEngine engine("main.qml");
        return app.exec();
    }
    \endcode

    These are typically specified in C++ in the beginning of \c main(),
    but can also be controlled in QML via the following properties:
    \list
        \li \l {Qt::application}{Qt.application.name},
        \li \l {Qt::application}{Qt.application.organization} and
        \li \l {Qt::application}{Qt.application.domain}.
    \endlist

    \section1 Categories

    Application settings may be divided into logical categories by specifying
    a category name via the \l category property. Using logical categories not
    only provides a cleaner settings structure, but also prevents possible
    conflicts between setting keys.

    \qml
    Item {
        id: panel

        visible: true

        Settings {
            category: "OutputPanel"
            property alias visible: panel.visible
            // ...
        }
    }
    \endqml

    Instead of ensuring that all settings in the application have unique names,
    the settings can be divided into unique categories that may then contain
    settings using the same names that are used in other categories - without
    a conflict.

    \section1 Notes

    The current implementation is based on \l QSettings. This imposes certain
    limitations, such as missing change notifications. Writing a setting value
    using one instance of Settings does not update the value in another Settings
    instance, even if they are referring to the same setting in the same category.

    The information is stored in the system registry on Windows, and in XML
    preferences files on OS X. On other Unix systems, in the absence of a
    standard, INI text files are used. See \l QSettings documentation for
    more details.

    \sa QSettings
*/

// #define SETTINGS_DEBUG

static const int settingsWriteDelay = 500;

class QQmlSettingsPrivate
{
    Q_DECLARE_PUBLIC(QQmlSettings)

public:
    QQmlSettingsPrivate();

    QSettings *instance() const;

    void init();
    void reset();

    void load();
    void store();

    void _q_propertyChanged();

    QQmlSettings *q_ptr;
    int timerId;
    bool initialized;
    QString category;
    mutable QPointer<QSettings> settings;
    QHash<const char *, QVariant> changedProperties;
};

QQmlSettingsPrivate::QQmlSettingsPrivate()
    : q_ptr(0), timerId(0), initialized(false)
{
}

QSettings *QQmlSettingsPrivate::instance() const
{
    if (!settings) {
        QQmlSettings *q = const_cast<QQmlSettings*>(q_func());
        settings = new QSettings(q);
        if (!category.isEmpty())
            settings->beginGroup(category);
        if (initialized)
            q->d_func()->load();
    }
    return settings;
}

void QQmlSettingsPrivate::init()
{
    if (!initialized) {
#ifdef SETTINGS_DEBUG
        qDebug() << "QQmlSettings: stored at" << instance()->fileName();
#endif
        load();
        initialized = true;
    }
}

void QQmlSettingsPrivate::reset()
{
    if (initialized && settings && !changedProperties.isEmpty())
        store();
    delete settings;
}

void QQmlSettingsPrivate::load()
{
    Q_Q(QQmlSettings);
    const QMetaObject *mo = q->metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        QMetaProperty property = mo->property(i);

        const QVariant previousValue = property.read(q);
        const QVariant currentValue = instance()->value(property.name(), previousValue);

        if (!currentValue.isNull() && (!previousValue.isValid()
                || (currentValue.canConvert(previousValue.type()) && previousValue != currentValue))) {
            property.write(q, currentValue);
#ifdef SETTINGS_DEBUG
            qDebug() << "QQmlSettings: load" << property.name() << "setting:" << currentValue << "default:" << previousValue;
#endif
        }

        // ensure that a non-existent setting gets written
        // even if the property wouldn't change later
        if (!instance()->contains(property.name()))
            _q_propertyChanged();

        // setup change notifications on first load
        if (!initialized && property.hasNotifySignal()) {
            static const int propertyChangedIndex = mo->indexOfSlot("_q_propertyChanged()");
            QMetaObject::connect(q, property.notifySignalIndex(), q, propertyChangedIndex);
        }
    }
}

void QQmlSettingsPrivate::store()
{
    QHash<const char *, QVariant>::const_iterator it = changedProperties.constBegin();
    while (it != changedProperties.constEnd()) {
        instance()->setValue(it.key(), it.value());
#ifdef SETTINGS_DEBUG
        qDebug() << "QQmlSettings: store" << it.key() << ":" << it.value();
#endif
        ++it;
    }
    changedProperties.clear();
}

void QQmlSettingsPrivate::_q_propertyChanged()
{
    Q_Q(QQmlSettings);
    const QMetaObject *mo = q->metaObject();
    const int offset = mo->propertyOffset();
    const int count = mo->propertyCount();
    for (int i = offset; i < count; ++i) {
        const QMetaProperty &property = mo->property(i);
        changedProperties.insert(property.name(), property.read(q));
#ifdef SETTINGS_DEBUG
        qDebug() << "QQmlSettings: cache" << property.name() << ":" << property.read(q);
#endif
    }
    if (timerId != 0)
        q->killTimer(timerId);
    timerId = q->startTimer(settingsWriteDelay);
}

QQmlSettings::QQmlSettings(QObject *parent)
    : QObject(parent), d_ptr(new QQmlSettingsPrivate)
{
    Q_D(QQmlSettings);
    d->q_ptr = this;
}

QQmlSettings::~QQmlSettings()
{
    Q_D(QQmlSettings);
    d->reset(); // flush pending changes
}

/*!
    \qmlproperty string Settings::category

    This property holds the name of the settings category.

    Categories can be used to group related settings together.
*/
QString QQmlSettings::category() const
{
    Q_D(const QQmlSettings);
    return d->category;
}

void QQmlSettings::setCategory(const QString &category)
{
    Q_D(QQmlSettings);
    if (d->category != category) {
        d->reset();
        d->category = category;
        if (d->initialized)
            d->load();
    }
}

void QQmlSettings::classBegin()
{
}

void QQmlSettings::componentComplete()
{
    Q_D(QQmlSettings);
    d->init();
}

void QQmlSettings::timerEvent(QTimerEvent *event)
{
    Q_D(QQmlSettings);
    if (event->timerId() == d->timerId) {
        killTimer(d->timerId);
        d->timerId = 0;

        d->store();
    }
    QObject::timerEvent(event);
}

QT_END_NAMESPACE

#include "moc_qqmlsettings_p.cpp"
