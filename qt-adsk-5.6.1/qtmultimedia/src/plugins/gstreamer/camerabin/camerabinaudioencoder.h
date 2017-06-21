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

#ifndef CAMERABINAUDIOENCODE_H
#define CAMERABINAUDIOENCODE_H

#include <qaudioencodersettingscontrol.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qset.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#ifdef HAVE_GST_ENCODING_PROFILES
#include <gst/pbutils/encoding-profile.h>
#include <private/qgstcodecsinfo_p.h>
#endif

#include <qaudioformat.h>

QT_BEGIN_NAMESPACE
class CameraBinSession;

class CameraBinAudioEncoder : public QAudioEncoderSettingsControl
{
    Q_OBJECT
public:
    CameraBinAudioEncoder(QObject *parent);
    virtual ~CameraBinAudioEncoder();

    QStringList supportedAudioCodecs() const;
    QString codecDescription(const QString &codecName) const;

    QStringList supportedEncodingOptions(const QString &codec) const;
    QVariant encodingOption(const QString &codec, const QString &name) const;
    void setEncodingOption(const QString &codec, const QString &name, const QVariant &value);

    QList<int> supportedSampleRates(const QAudioEncoderSettings &settings = QAudioEncoderSettings(),
                                    bool *isContinuous = 0) const;
    QList<int> supportedChannelCounts(const QAudioEncoderSettings &settings = QAudioEncoderSettings()) const;
    QList<int> supportedSampleSizes(const QAudioEncoderSettings &settings = QAudioEncoderSettings()) const;

    QAudioEncoderSettings audioSettings() const;
    void setAudioSettings(const QAudioEncoderSettings&);

    QAudioEncoderSettings actualAudioSettings() const;
    void setActualAudioSettings(const QAudioEncoderSettings&);
    void resetActualSettings();

#ifdef HAVE_GST_ENCODING_PROFILES
    GstEncodingProfile *createProfile();
#endif

    void applySettings(GstElement *element);

Q_SIGNALS:
    void settingsChanged();

private:
#ifdef HAVE_GST_ENCODING_PROFILES
    QGstCodecsInfo m_codecs;
#endif

    QAudioEncoderSettings m_actualAudioSettings;
    QAudioEncoderSettings m_audioSettings;
};

QT_END_NAMESPACE

#endif
