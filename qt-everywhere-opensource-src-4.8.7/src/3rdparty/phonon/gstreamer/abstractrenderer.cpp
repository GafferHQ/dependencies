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

#include "abstractrenderer.h"

#ifndef QT_NO_PHONON_VIDEO
QT_BEGIN_NAMESPACE

namespace Phonon
{
namespace Gstreamer
{


AbstractRenderer::~AbstractRenderer()
{
    if (m_videoSink) {
        gst_object_unref (GST_OBJECT (m_videoSink)); //Take ownership
        m_videoSink = 0;
    }
}

void AbstractRenderer::aspectRatioChanged(Phonon::VideoWidget::AspectRatio aspectRatio)
{
    Q_UNUSED(aspectRatio);
}

void AbstractRenderer::scaleModeChanged(Phonon::VideoWidget::ScaleMode scaleMode)
{
    Q_UNUSED(scaleMode);
}

void AbstractRenderer::movieSizeChanged(const QSize &size)
{
    Q_UNUSED(size);
}

}
} //namespace Phonon::Gstreamer

QT_END_NAMESPACE
#endif //QT_NO_PHONON_VIDEO

