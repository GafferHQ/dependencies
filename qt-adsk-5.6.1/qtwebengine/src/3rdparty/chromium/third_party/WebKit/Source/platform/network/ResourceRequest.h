/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include "platform/network/FormData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/OwnPtr.h"

namespace blink {

enum ResourceRequestCachePolicy {
    UseProtocolCachePolicy, // normal load
    ReloadIgnoringCacheData, // reload
    ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
    ReturnCacheDataDontLoad, // results of a post - allow stale data and only use cache
    ReloadBypassingCache, // end-to-end reload
};

enum InputToLoadPerfMetricReportPolicy {
    NoReport, // Don't report metrics for this ResourceRequest.
    ReportLink, // Report metrics for this request as initiated by a link click.
    ReportIntent, // Report metrics for this request as initiated by an intent.
};

struct CrossThreadResourceRequestData;

class PLATFORM_EXPORT ResourceRequest {
    WTF_MAKE_FAST_ALLOCATED(ResourceRequest);
public:
    class ExtraData : public RefCounted<ExtraData> {
    public:
        virtual ~ExtraData() { }
    };

    ResourceRequest()
    {
        initialize(KURL());
    }

    ResourceRequest(const String& urlString)
    {
        initialize(KURL(ParsedURLString, urlString));
    }

    ResourceRequest(const KURL& url)
    {
        initialize(url);
    }

    static PassOwnPtr<ResourceRequest> adopt(PassOwnPtr<CrossThreadResourceRequestData>);

    // Gets a copy of the data suitable for passing to another thread.
    PassOwnPtr<CrossThreadResourceRequestData> copyData() const;

    bool isNull() const;
    bool isEmpty() const;

    const KURL& url() const;
    void setURL(const KURL& url);

    void removeCredentials();

    ResourceRequestCachePolicy cachePolicy() const;
    void setCachePolicy(ResourceRequestCachePolicy cachePolicy);

    double timeoutInterval() const; // May return 0 when using platform default.
    void setTimeoutInterval(double timeoutInterval);

    const KURL& firstPartyForCookies() const;
    void setFirstPartyForCookies(const KURL& firstPartyForCookies);

    PassRefPtr<SecurityOrigin> requestorOrigin() const;
    void setRequestorOrigin(PassRefPtr<SecurityOrigin>);

    const AtomicString& httpMethod() const;
    void setHTTPMethod(const AtomicString&);

    const HTTPHeaderMap& httpHeaderFields() const;
    const AtomicString& httpHeaderField(const AtomicString& name) const;
    const AtomicString& httpHeaderField(const char* name) const;
    void setHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void setHTTPHeaderField(const char* name, const AtomicString& value);
    void addHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void addHTTPHeaderFields(const HTTPHeaderMap& headerFields);
    void clearHTTPHeaderField(const AtomicString& name);

    const AtomicString& httpContentType() const { return httpHeaderField("Content-Type");  }
    void setHTTPContentType(const AtomicString& httpContentType) { setHTTPHeaderField("Content-Type", httpContentType); }

    bool didSetHTTPReferrer() const { return m_didSetHTTPReferrer; }
    const AtomicString& httpReferrer() const { return httpHeaderField("Referer"); }
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
    void setHTTPReferrer(const Referrer&);
    void clearHTTPReferrer();

    const AtomicString& httpOrigin() const { return httpHeaderField("Origin"); }
    void setHTTPOrigin(const AtomicString& httpOrigin) { setHTTPHeaderField("Origin", httpOrigin); }
    void clearHTTPOrigin();
    void addHTTPOriginIfNeeded(const AtomicString& origin);

    const AtomicString& httpUserAgent() const { return httpHeaderField("User-Agent"); }
    void setHTTPUserAgent(const AtomicString& httpUserAgent) { setHTTPHeaderField("User-Agent", httpUserAgent); }
    void clearHTTPUserAgent();

    const AtomicString& httpAccept() const { return httpHeaderField("Accept"); }
    void setHTTPAccept(const AtomicString& httpAccept) { setHTTPHeaderField("Accept", httpAccept); }

    FormData* httpBody() const;
    void setHTTPBody(PassRefPtr<FormData> httpBody);

    bool allowStoredCredentials() const;
    void setAllowStoredCredentials(bool allowCredentials);

    ResourceLoadPriority priority() const;
    void setPriority(ResourceLoadPriority, int intraPriorityValue = 0);

    bool isConditional() const;

    // Whether the associated ResourceHandleClient needs to be notified of
    // upload progress made for that resource.
    bool reportUploadProgress() const { return m_reportUploadProgress; }
    void setReportUploadProgress(bool reportUploadProgress) { m_reportUploadProgress = reportUploadProgress; }

    // Whether actual headers being sent/received should be collected and reported for the request.
    bool reportRawHeaders() const { return m_reportRawHeaders; }
    void setReportRawHeaders(bool reportRawHeaders) { m_reportRawHeaders = reportRawHeaders; }

    // Allows the request to be matched up with its requestor.
    int requestorID() const { return m_requestorID; }
    void setRequestorID(int requestorID) { m_requestorID = requestorID; }

    // The process id of the process from which this request originated. In
    // the case of out-of-process plugins, this allows to link back the
    // request to the plugin process (as it is processed through a render
    // view process).
    int requestorProcessID() const { return m_requestorProcessID; }
    void setRequestorProcessID(int requestorProcessID) { m_requestorProcessID = requestorProcessID; }

    // Allows the request to be matched up with its app cache host.
    int appCacheHostID() const { return m_appCacheHostID; }
    void setAppCacheHostID(int id) { m_appCacheHostID = id; }

    // True if request was user initiated.
    bool hasUserGesture() const { return m_hasUserGesture; }
    void setHasUserGesture(bool);

    // True if request should be downloaded to file.
    bool downloadToFile() const { return m_downloadToFile; }
    void setDownloadToFile(bool downloadToFile) { m_downloadToFile = downloadToFile; }

    // True if the requestor wants to receive a response body as
    // WebDataConsumerHandle.
    bool useStreamOnResponse() const { return m_useStreamOnResponse; }
    void setUseStreamOnResponse(bool useStreamOnResponse) { m_useStreamOnResponse = useStreamOnResponse; }

    // True if the request should not be handled by the ServiceWorker.
    bool skipServiceWorker() const { return m_skipServiceWorker; }
    void setSkipServiceWorker(bool skipServiceWorker) { m_skipServiceWorker = skipServiceWorker; }

    // True if corresponding AppCache group should be resetted.
    bool shouldResetAppCache() { return m_shouldResetAppCache; }
    void setShouldResetAppCache(bool shouldResetAppCache) { m_shouldResetAppCache = shouldResetAppCache; }

    // Extra data associated with this request.
    ExtraData* extraData() const { return m_extraData.get(); }
    void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

    WebURLRequest::RequestContext requestContext() const { return m_requestContext; }
    void setRequestContext(WebURLRequest::RequestContext context) { m_requestContext = context; }

    WebURLRequest::FrameType frameType() const { return m_frameType; }
    void setFrameType(WebURLRequest::FrameType frameType) { m_frameType = frameType; }

    WebURLRequest::FetchRequestMode fetchRequestMode() const { return m_fetchRequestMode; }
    void setFetchRequestMode(WebURLRequest::FetchRequestMode mode) { m_fetchRequestMode = mode; }

    WebURLRequest::FetchCredentialsMode fetchCredentialsMode() const { return m_fetchCredentialsMode; }
    void setFetchCredentialsMode(WebURLRequest::FetchCredentialsMode mode) { m_fetchCredentialsMode = mode; }

    bool cacheControlContainsNoCache() const;
    bool cacheControlContainsNoStore() const;
    bool hasCacheValidatorFields() const;

    static bool compare(const ResourceRequest&, const ResourceRequest&);

    bool checkForBrowserSideNavigation() const { return m_checkForBrowserSideNavigation; }
    void setCheckForBrowserSideNavigation(bool check) { m_checkForBrowserSideNavigation = check; }

    double uiStartTime() const { return m_uiStartTime; }
    void setUIStartTime(double uiStartTime) { m_uiStartTime = uiStartTime; }

    bool originatesFromReservedIPRange() const { return m_originatesFromReservedIPRange; }
    void setOriginatesFromReservedIPRange(bool value) { m_originatesFromReservedIPRange = value; }

    InputToLoadPerfMetricReportPolicy inputPerfMetricReportPolicy() const { return m_inputPerfMetricReportPolicy; }
    void setInputPerfMetricReportPolicy(InputToLoadPerfMetricReportPolicy inputPerfMetricReportPolicy) { m_inputPerfMetricReportPolicy = inputPerfMetricReportPolicy; }

    void setFollowedRedirect(bool followed) { m_followedRedirect = followed; }
    bool followedRedirect() const { return m_followedRedirect; }

private:
    void initialize(const KURL&);

    const CacheControlHeader& cacheControlHeader() const;

    KURL m_url;
    ResourceRequestCachePolicy m_cachePolicy;
    double m_timeoutInterval; // 0 is a magic value for platform default on platforms that have one.
    KURL m_firstPartyForCookies;
    RefPtr<SecurityOrigin> m_requestorOrigin;
    AtomicString m_httpMethod;
    HTTPHeaderMap m_httpHeaderFields;
    RefPtr<FormData> m_httpBody;
    bool m_allowStoredCredentials : 1;
    bool m_reportUploadProgress : 1;
    bool m_reportRawHeaders : 1;
    bool m_hasUserGesture : 1;
    bool m_downloadToFile : 1;
    bool m_useStreamOnResponse : 1;
    bool m_skipServiceWorker : 1;
    bool m_shouldResetAppCache : 1;
    ResourceLoadPriority m_priority;
    int m_intraPriorityValue;
    int m_requestorID;
    int m_requestorProcessID;
    int m_appCacheHostID;
    RefPtr<ExtraData> m_extraData;
    WebURLRequest::RequestContext m_requestContext;
    WebURLRequest::FrameType m_frameType;
    WebURLRequest::FetchRequestMode m_fetchRequestMode;
    WebURLRequest::FetchCredentialsMode m_fetchCredentialsMode;
    ReferrerPolicy m_referrerPolicy;
    bool m_didSetHTTPReferrer;
    bool m_checkForBrowserSideNavigation;
    double m_uiStartTime;
    bool m_originatesFromReservedIPRange;
    InputToLoadPerfMetricReportPolicy m_inputPerfMetricReportPolicy;

    mutable CacheControlHeader m_cacheControlHeaderCache;

    static double s_defaultTimeoutInterval;

    bool m_followedRedirect;
};

bool equalIgnoringHeaderFields(const ResourceRequest&, const ResourceRequest&);

inline bool operator==(const ResourceRequest& a, const ResourceRequest& b) { return ResourceRequest::compare(a, b); }
inline bool operator!=(ResourceRequest& a, const ResourceRequest& b) { return !(a == b); }

struct CrossThreadResourceRequestData {
    WTF_MAKE_NONCOPYABLE(CrossThreadResourceRequestData); WTF_MAKE_FAST_ALLOCATED(CrossThreadResourceRequestData);
public:
    CrossThreadResourceRequestData() { }
    KURL m_url;

    ResourceRequestCachePolicy m_cachePolicy;
    double m_timeoutInterval;
    KURL m_firstPartyForCookies;
    RefPtr<SecurityOrigin> m_requestorOrigin;

    String m_httpMethod;
    OwnPtr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
    RefPtr<FormData> m_httpBody;
    bool m_allowStoredCredentials;
    bool m_reportUploadProgress;
    bool m_hasUserGesture;
    bool m_downloadToFile;
    bool m_skipServiceWorker;
    bool m_useStreamOnResponse;
    bool m_shouldResetAppCache;
    ResourceLoadPriority m_priority;
    int m_intraPriorityValue;
    int m_requestorID;
    int m_requestorProcessID;
    int m_appCacheHostID;
    WebURLRequest::RequestContext m_requestContext;
    WebURLRequest::FrameType m_frameType;
    WebURLRequest::FetchRequestMode m_fetchRequestMode;
    WebURLRequest::FetchCredentialsMode m_fetchCredentialsMode;
    ReferrerPolicy m_referrerPolicy;
    bool m_didSetHTTPReferrer;
    bool m_checkForBrowserSideNavigation;
    double m_uiStartTime;
    bool m_originatesFromReservedIPRange;
    InputToLoadPerfMetricReportPolicy m_inputPerfMetricReportPolicy;
    bool m_followedRedirect;
};

} // namespace blink

#endif // ResourceRequest_h
