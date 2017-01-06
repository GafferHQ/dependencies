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

#include "iodevicereader.h"
#include "qasyncreader.h"

#include "mediagraph.h"

#include <phonon/streaminterface.h>

#include <dshow.h>
#include <initguid.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_PHONON_ABSTRACTMEDIASTREAM

namespace Phonon
{
    namespace DS9
    {
        //these mediatypes define a stream, its type will be autodetected by DirectShow
        static QVector<AM_MEDIA_TYPE> getMediaTypes()
        {
            //the order here is important because otherwise,
            //directshow might not be able to detect the stream type correctly

            AM_MEDIA_TYPE mt = { MEDIATYPE_Stream, MEDIASUBTYPE_Avi, TRUE, FALSE, 1, GUID_NULL, 0, 0, 0};

            QVector<AM_MEDIA_TYPE> ret;
            //AVI stream
            ret << mt;
            //WAVE stream
            mt.subtype = MEDIASUBTYPE_WAVE;
            ret << mt;
            //normal auto-detect stream (must be at the end!)
            mt.subtype = MEDIASUBTYPE_NULL;
            ret << mt;
            return ret;
        }

        class StreamReader : public QAsyncReader, public Phonon::StreamInterface
        {
        public:
            StreamReader(QBaseFilter *parent, const Phonon::MediaSource &source, const MediaGraph *mg) : 
              QAsyncReader(parent, getMediaTypes()),
                  m_seekable(false), m_pos(0), m_size(-1), m_mediaGraph(mg)
              {
                  connectToSource(source);
              }

              //for Phonon::StreamInterface
              void writeData(const QByteArray &data)
              {
                  m_pos += data.size();
                  m_buffer += data;
              }

              void endOfData()
              {
              }

              void setStreamSize(qint64 newSize)
              {
                  QMutexLocker locker(&m_mutex);
                  m_size = newSize;
              }

              void setStreamSeekable(bool s)
              {
                  QMutexLocker locker(&m_mutex);
                  m_seekable = s;
              }

              //virtual pure members

              //implementation from IAsyncReader
              STDMETHODIMP Length(LONGLONG *total, LONGLONG *available)
              {
                  QMutexLocker locker(&m_mutex);
                  if (total) {
                      *total = m_size;
                  }

                  if (available) {
                      *available = m_size;
                  }

                  return S_OK;
              }


              HRESULT read(LONGLONG pos, LONG length, BYTE *buffer, LONG *actual)
              {
                  Q_ASSERT(!m_mutex.tryLock());
                  if (m_mediaGraph->isStopping()) {
                      return VFW_E_WRONG_STATE;
                  }

                  if(m_size != 1 && pos + length > m_size) {
                      //it tries to read outside of the boundaries
                      return E_FAIL;
                  }

                  if (m_pos - m_buffer.size() != pos) {
                      if (!m_seekable) {
                          return S_FALSE;
                      }
                      m_pos = pos;
                      seekStream(pos);
                      m_buffer.clear();
                  }

                  int oldSize = m_buffer.size();
                  while (m_buffer.size() < int(length)) {
                      needData();

                      if (oldSize == m_buffer.size()) {
                          break; //we didn't get any data
                      }
                      oldSize = m_buffer.size();
                  }

                  int bytesRead = qMin(m_buffer.size(), int(length));
                  qMemCopy(buffer, m_buffer.data(), bytesRead);
                  //truncate the buffer
                  m_buffer = m_buffer.mid(bytesRead);

                  if (actual) {
                      *actual = bytesRead; //initialization
                  }

                  return bytesRead == length ? S_OK : S_FALSE;
              }

        public:
            //for Phonon::StreamInterface
            QByteArray m_buffer;
            bool m_seekable;
            qint64 m_pos;
            qint64 m_size;

            const MediaGraph *m_mediaGraph;
        };

        IODeviceReader::IODeviceReader(const Phonon::MediaSource &source, const MediaGraph *mg) :
        QBaseFilter(CLSID_NULL)
        {
            //create the output pin
            m_streamReader = new StreamReader(this, source, mg);
        }

        IODeviceReader::~IODeviceReader()
        {
        }
    }
}

#endif //QT_NO_PHONON_ABSTRACTMEDIASTREAM

QT_END_NAMESPACE
