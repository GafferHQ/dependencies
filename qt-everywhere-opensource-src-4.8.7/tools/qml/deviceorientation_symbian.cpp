/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "deviceorientation.h"

#include <e32base.h>
#include <sensrvchannelfinder.h>
#include <sensrvdatalistener.h>
#include <sensrvchannel.h>
#include <sensrvorientationsensor.h>

class SymbianOrientation : public DeviceOrientation, public MSensrvDataListener
{
    Q_OBJECT
public:
    SymbianOrientation()
        : DeviceOrientation(), m_current(UnknownOrientation), m_sensorChannel(0), m_channelOpen(false)
    {
        TRAP_IGNORE(initL());
        if (!m_sensorChannel)
            qWarning("No valid sensors found.");
    }

    ~SymbianOrientation()
    {
        if (m_sensorChannel) {
            m_sensorChannel->StopDataListening();
            m_sensorChannel->CloseChannel();
            delete m_sensorChannel;
        }
    }

    void initL()
    {
        CSensrvChannelFinder *channelFinder = CSensrvChannelFinder::NewLC();
        RSensrvChannelInfoList channelInfoList;
        CleanupClosePushL(channelInfoList);

        TSensrvChannelInfo searchConditions;
        searchConditions.iChannelType = KSensrvChannelTypeIdOrientationData;
        channelFinder->FindChannelsL(channelInfoList, searchConditions);

        for (int i = 0; i < channelInfoList.Count(); ++i) {
            TRAPD(error, m_sensorChannel = CSensrvChannel::NewL(channelInfoList[i]));
            if (!error)
                TRAP(error, m_sensorChannel->OpenChannelL());
            if (!error) {
                TRAP(error, m_sensorChannel->StartDataListeningL(this, 1, 1, 0));
                m_channelOpen = true;
                break;
           }
            if (error) {
                delete m_sensorChannel;
                m_sensorChannel = 0;
            }
        }

        channelInfoList.Close();
        CleanupStack::Pop(&channelInfoList);
        CleanupStack::PopAndDestroy(channelFinder);
    }

    Orientation orientation() const
    {
        return m_current;
    }

   void setOrientation(Orientation) { }

private:
    DeviceOrientation::Orientation m_current;
    CSensrvChannel *m_sensorChannel;
    bool m_channelOpen;
    void pauseListening() {
        if (m_sensorChannel && m_channelOpen) {
            m_sensorChannel->StopDataListening();
            m_sensorChannel->CloseChannel();
            m_channelOpen = false;
        }
    }

    void resumeListening() {
        if (m_sensorChannel && !m_channelOpen) {
            TRAPD(error, m_sensorChannel->OpenChannelL());
            if (!error) {
                TRAP(error, m_sensorChannel->StartDataListeningL(this, 1, 1, 0));
                if (!error) {
                    m_channelOpen = true;
                }
            }
            if (error) {
                delete m_sensorChannel;
                m_sensorChannel = 0;
            }
        }
    }

    void DataReceived(CSensrvChannel &channel, TInt count, TInt dataLost)
    {
        Q_UNUSED(dataLost)
        if (channel.GetChannelInfo().iChannelType == KSensrvChannelTypeIdOrientationData) {
            TSensrvOrientationData data;
            for (int i = 0; i < count; ++i) {
                TPckgBuf<TSensrvOrientationData> dataBuf;
                channel.GetData(dataBuf);
                data = dataBuf();
                Orientation orientation = UnknownOrientation;
                switch (data.iDeviceOrientation) {
                case TSensrvOrientationData::EOrientationDisplayUp:
                    orientation = Portrait;
                    break;
                case TSensrvOrientationData::EOrientationDisplayRightUp:
                    orientation = Landscape;
                    break;
                case TSensrvOrientationData::EOrientationDisplayLeftUp:
                    orientation = LandscapeInverted;
                    break;
                case TSensrvOrientationData::EOrientationDisplayDown:
                    orientation = PortraitInverted;
                    break;
                case TSensrvOrientationData::EOrientationUndefined:
                case TSensrvOrientationData::EOrientationDisplayUpwards:
                case TSensrvOrientationData::EOrientationDisplayDownwards:
                default:
                    break;
                }

                if (m_current != orientation && orientation != UnknownOrientation) {
                    m_current = orientation;
                    emit orientationChanged();
                }
           }
        }
    }

   void DataError(CSensrvChannel& /* channel */, TSensrvErrorSeverity /* error */)
   {
   }

   void GetDataListenerInterfaceL(TUid /* interfaceUid */, TAny*& /* interface */)
   {
   }
};


DeviceOrientation* DeviceOrientation::instance()
{
    static SymbianOrientation *o = 0;
    if (!o)
        o = new SymbianOrientation;
    return o;
}

#include "deviceorientation_symbian.moc"
