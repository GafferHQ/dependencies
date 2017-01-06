/*  This file is part of the KDE project.

    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 2.1 or 3 of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef Phonon_QT7_MEDIANODEEVENT_H
#define Phonon_QT7_MEDIANODEEVENT_H

#include <QtCore/qnamespace.h>

QT_BEGIN_NAMESPACE

namespace Phonon
{
namespace QT7
{
    class QuickTimeVideoPlayer;

    class MediaNodeEvent
    {
        public:
            enum Type {
                AudioGraphAboutToBeDeleted,
                NewAudioGraph,
                AudioGraphInitialized,
                AudioGraphCannotPlay,
                AboutToRestartAudioStream,
                RestartAudioStreamRequest,
                VideoSinkAdded,
                VideoSinkRemoved,
                AudioSinkAdded,
                AudioSinkRemoved,
                VideoSourceAdded,
                VideoSourceRemoved,
                AudioSourceAdded,
                AudioSourceRemoved,
				VideoOutputCountChanged,
                VideoFrameSizeChanged,
                SetMediaObject,
                StartConnectionChange,
                EndConnectionChange,
				MediaPlaying
            };

            MediaNodeEvent(Type type, void *data = 0);
            virtual ~MediaNodeEvent();
            inline Type type() const{ return eventType; };
            inline void* data() const { return eventData; };

        private:
            Type eventType;
            void *eventData;
    };

}} // namespace Phonon::QT7

QT_END_NAMESPACE

#endif // Phonon_QT7_MEDIANODEEVENT_H
