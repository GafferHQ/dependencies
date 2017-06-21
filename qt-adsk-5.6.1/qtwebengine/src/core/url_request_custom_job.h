/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef URL_REQUEST_CUSTOM_JOB_H_
#define URL_REQUEST_CUSTOM_JOB_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

#include <QtCore/qglobal.h>
#include <QtCore/QMutex>
#include <QtCore/QPointer>

QT_FORWARD_DECLARE_CLASS(QIODevice)

namespace QtWebEngineCore {

class BrowserContextAdapter;
class URLRequestCustomJobDelegate;
class URLRequestCustomJobShared;

// A request job that handles reading custom URL schemes
class URLRequestCustomJob : public net::URLRequestJob {
public:
    URLRequestCustomJob(net::URLRequest *request, net::NetworkDelegate *networkDelegate, const std::string &scheme, QWeakPointer<const BrowserContextAdapter> adapter);
    virtual void Start() Q_DECL_OVERRIDE;
    virtual void Kill() Q_DECL_OVERRIDE;
    virtual bool ReadRawData(net::IOBuffer *buf, int bufSize, int *bytesRead) Q_DECL_OVERRIDE;
    virtual bool GetMimeType(std::string *mimeType) const Q_DECL_OVERRIDE;
    virtual bool GetCharset(std::string *charset) Q_DECL_OVERRIDE;
    virtual bool IsRedirectResponse(GURL* location, int* http_status_code) Q_DECL_OVERRIDE;

protected:
    virtual ~URLRequestCustomJob();

private:
    std::string m_scheme;
    QWeakPointer<const BrowserContextAdapter> m_adapter;
    URLRequestCustomJobShared *m_shared;

    friend class URLRequestCustomJobShared;

    DISALLOW_COPY_AND_ASSIGN(URLRequestCustomJob);
};

// A shared state between URLRequestCustomJob living on the IO thread
// and URLRequestCustomJobDelegate living on the UI thread.
class URLRequestCustomJobShared {
public:
    URLRequestCustomJobShared(URLRequestCustomJob *job);
    ~URLRequestCustomJobShared();

    void setReplyMimeType(const std::string &);
    void setReplyCharset(const std::string &);
    void setReplyDevice(QIODevice *);

    void redirect(const GURL &url);
    void fail(int);
    void abort();

    void killJob();
    void unsetJobDelegate();

    void startAsync();
    void notifyStarted();
    void notifyFailure();
    void notifyCanceled();

    GURL requestUrl();
    std::string requestMethod();

    QMutex m_mutex;
    QPointer<QIODevice> m_device;
    URLRequestCustomJob *m_job;
    URLRequestCustomJobDelegate *m_delegate;
    std::string m_mimeType;
    std::string m_charset;
    int m_error;
    GURL m_redirect;
    bool m_started;
    bool m_asyncInitialized;
    base::WeakPtrFactory<URLRequestCustomJobShared> m_weakFactory;
};

} // namespace QtWebEngineCore

#endif // URL_REQUEST_CUSTOM_JOB_H_
