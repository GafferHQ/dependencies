/*  This file is part of the KDE project
    Copyright (C) 2005-2006 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), Nokia Corporation 
    (or its successors, if any) and the KDE Free Qt Foundation, which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public 
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "backendcapabilities.h"
#include "backendcapabilities_p.h"

#include "phonondefs_p.h"
#include "backendinterface.h"
#include "factory_p.h"
#include "globalconfig.h"
#include "globalstatic_p.h"
#include "objectdescription.h"

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE

PHONON_GLOBAL_STATIC(Phonon::BackendCapabilitiesPrivate, globalBCPrivate)

namespace Phonon
{

BackendCapabilities::Notifier *BackendCapabilities::notifier()
{
    return globalBCPrivate;
}

QStringList BackendCapabilities::availableMimeTypes()
{
    if (BackendInterface *backendIface = qobject_cast<BackendInterface *>(Factory::backend()))
        return backendIface->availableMimeTypes();
    else
        return QStringList();
}

bool BackendCapabilities::isMimeTypeAvailable(const QString &mimeType)
{
    QObject *m_backendObject = Factory::backend(false);
    if (!m_backendObject) {
        if (!Factory::isMimeTypeAvailable(mimeType)) {
            return false;
        }
        // without loading the backend we found out that the MIME type might be supported, now we
        // want to know for certain. For that we need to load the backend.
        m_backendObject = Factory::backend(true);
    }
    if (!m_backendObject) {
        // no backend == no MIME type supported at all
        return false;
    }
    return availableMimeTypes().contains(mimeType);
}

QList<AudioOutputDevice> BackendCapabilities::availableAudioOutputDevices()
{
    QList<AudioOutputDevice> ret;
#ifndef QT_NO_PHONON_SETTINGSGROUP
    const QList<int> deviceIndexes = GlobalConfig().audioOutputDeviceListFor(Phonon::NoCategory);
    for (int i = 0; i < deviceIndexes.count(); ++i) {
        ret.append(AudioOutputDevice::fromIndex(deviceIndexes.at(i)));
    }
#endif //QT_NO_PHONON_SETTINGSGROUP
    return ret;
}


#ifndef QT_NO_PHONON_AUDIOCAPTURE
QList<AudioCaptureDevice> BackendCapabilities::availableAudioCaptureDevices()
{
    QList<AudioCaptureDevice> ret;
    const QList<int> deviceIndexes = GlobalConfig().audioCaptureDeviceListFor(Phonon::NoCategory);
    for (int i = 0; i < deviceIndexes.count(); ++i) {
        ret.append(AudioCaptureDevice::fromIndex(deviceIndexes.at(i)));
    }
    return ret;
}
#endif //QT_NO_PHONON_AUDIOCAPTURE

#ifndef QT_NO_PHONON_EFFECT
QList<EffectDescription> BackendCapabilities::availableAudioEffects()
{
    BackendInterface *backendIface = qobject_cast<BackendInterface *>(Factory::backend());
    QList<EffectDescription> ret;
    if (backendIface) {
        const QList<int> deviceIndexes = backendIface->objectDescriptionIndexes(Phonon::EffectType);
        for (int i = 0; i < deviceIndexes.count(); ++i) {
            ret.append(EffectDescription::fromIndex(deviceIndexes.at(i)));
        }
    }
    return ret;
}
#endif //QT_NO_PHONON_EFFECT

} // namespace Phonon

QT_END_NAMESPACE

#include "moc_backendcapabilities.cpp"

// vim: sw=4 ts=4


