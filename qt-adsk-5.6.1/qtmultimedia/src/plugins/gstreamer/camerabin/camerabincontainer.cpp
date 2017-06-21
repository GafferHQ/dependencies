/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "camerabincontainer.h"
#include <QtCore/qregexp.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

CameraBinContainer::CameraBinContainer(QObject *parent)
    :QMediaContainerControl(parent)
#ifdef HAVE_GST_ENCODING_PROFILES
    , m_supportedContainers(QGstCodecsInfo::Muxer)
#endif
{
    //extension for containers hard to guess from mimetype
    m_fileExtensions["video/x-matroska"] = "mkv";
    m_fileExtensions["video/quicktime"] = "mov";
    m_fileExtensions["video/x-msvideo"] = "avi";
    m_fileExtensions["video/msvideo"] = "avi";
    m_fileExtensions["audio/mpeg"] = "mp3";
    m_fileExtensions["application/x-shockwave-flash"] = "swf";
    m_fileExtensions["application/x-pn-realmedia"] = "rm";
}

QStringList CameraBinContainer::supportedContainers() const
{
#ifdef HAVE_GST_ENCODING_PROFILES
    return m_supportedContainers.supportedCodecs();
#else
    return QStringList();
#endif
}

QString CameraBinContainer::containerDescription(const QString &formatMimeType) const
{
#ifdef HAVE_GST_ENCODING_PROFILES
    return m_supportedContainers.codecDescription(formatMimeType);
#else
    Q_UNUSED(formatMimeType)
    return QString();
#endif
}

QString CameraBinContainer::containerFormat() const
{
    return m_format;
}

void CameraBinContainer::setContainerFormat(const QString &format)
{
#ifdef HAVE_GST_ENCODING_PROFILES
    if (m_format != format) {
        m_format = format;
        m_actualFormat = format;
        emit settingsChanged();
    }
#endif
}

QString CameraBinContainer::actualContainerFormat() const
{
    return m_actualFormat;
}

void CameraBinContainer::setActualContainerFormat(const QString &containerFormat)
{
#ifdef HAVE_GST_ENCODING_PROFILES
    m_actualFormat = containerFormat;
#endif
}

void CameraBinContainer::resetActualContainerFormat()
{
    m_actualFormat = m_format;
}

#ifdef HAVE_GST_ENCODING_PROFILES

GstEncodingContainerProfile *CameraBinContainer::createProfile()
{
    GstCaps *caps;

    if (m_actualFormat.isEmpty()) {
        return 0;
    } else {
        QString format = m_actualFormat;
        QStringList supportedFormats = m_supportedContainers.supportedCodecs();

        //if format is not in the list of supported gstreamer mime types,
        //try to find the mime type with matching extension
        if (!supportedFormats.contains(format)) {
            QString extension = suggestedFileExtension(m_actualFormat);
            foreach (const QString &formatCandidate, supportedFormats) {
                if (suggestedFileExtension(formatCandidate) == extension) {
                    format = formatCandidate;
                    break;
                }
            }
        }

        caps = gst_caps_from_string(format.toLatin1());
    }

    GstEncodingContainerProfile *profile = (GstEncodingContainerProfile *)gst_encoding_container_profile_new(
                "camerabin2_profile",
                (gchar *)"custom camera profile",
                caps,
                NULL); //preset

    gst_caps_unref(caps);

    return profile;
}

#endif

/*!
  Suggest file extension for current container mimetype.
 */
QString CameraBinContainer::suggestedFileExtension(const QString &containerFormat) const
{
    //for container names like avi instead of video/x-msvideo, use it as extension
    if (!containerFormat.contains('/'))
        return containerFormat;

    QString format = containerFormat.left(containerFormat.indexOf(','));
    QString extension = m_fileExtensions.value(format);

    if (!extension.isEmpty() || format.isEmpty())
        return extension;

    QRegExp rx("[-/]([\\w]+)$");

    if (rx.indexIn(format) != -1)
        extension = rx.cap(1);

    return extension;
}

QT_END_NAMESPACE
