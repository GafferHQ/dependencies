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

#ifndef SOURCERESOLVER_H
#define SOURCERESOLVER_H

#include "mfstream.h"
#include "qmediaresource.h"

class SourceResolver: public QObject, public IMFAsyncCallback
{
    Q_OBJECT
public:
    SourceResolver();

    ~SourceResolver();

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult *pAsyncResult);

    HRESULT STDMETHODCALLTYPE GetParameters(DWORD*, DWORD*);

    void load(QMediaResourceList& resources, QIODevice* stream);

    void cancel();

    void shutdown();

    IMFMediaSource* mediaSource() const;

Q_SIGNALS:
    void error(long hr);
    void mediaSourceReady();

private:
    class State : public IUnknown
    {
    public:
        State(IMFSourceResolver *sourceResolver, bool fromStream);
        ~State();

        STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject);

        STDMETHODIMP_(ULONG) AddRef(void);

        STDMETHODIMP_(ULONG) Release(void);

        IMFSourceResolver* sourceResolver() const;
        bool fromStream() const;

    private:
        long m_cRef;
        IMFSourceResolver *m_sourceResolver;
        bool m_fromStream;
    };

    long              m_cRef;
    IUnknown          *m_cancelCookie;
    IMFSourceResolver *m_sourceResolver;
    IMFMediaSource    *m_mediaSource;
    MFStream          *m_stream;
    QMutex            m_mutex;
};

#endif
