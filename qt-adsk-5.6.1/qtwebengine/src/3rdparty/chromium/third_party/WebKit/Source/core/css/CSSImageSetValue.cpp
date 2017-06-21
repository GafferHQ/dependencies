/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSImageSetValue.h"

#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/style/StyleFetchedImageSet.h"
#include "core/style/StylePendingImage.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(ImageSetClass, CommaSeparator)
    , m_accessedBestFitImage(false)
    , m_scaleFactor(1)
{
}

CSSImageSetValue::~CSSImageSetValue()
{
    if (m_imageSet && m_imageSet->isImageResourceSet())
        toStyleFetchedImageSet(m_imageSet)->clearImageSetValue();
}

void CSSImageSetValue::fillImageSet()
{
    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        CSSImageValue* imageValue = toCSSImageValue(item(i));
        String imageURL = imageValue->url();

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        CSSValue* scaleFactorValue = item(i);
        float scaleFactor = toCSSPrimitiveValue(scaleFactorValue)->getFloatValue();

        ImageWithScale image;
        image.imageURL = imageURL;
        image.referrer = SecurityPolicy::generateReferrer(imageValue->referrer().referrerPolicy, KURL(ParsedURLString, imageURL), imageValue->referrer().referrer);
        image.scaleFactor = scaleFactor;
        m_imagesInSet.append(image);
        ++i;
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::sort(m_imagesInSet.begin(), m_imagesInSet.end(), CSSImageSetValue::compareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::bestImageForScaleFactor()
{
    ImageWithScale image;
    size_t numberOfImages = m_imagesInSet.size();
    for (size_t i = 0; i < numberOfImages; ++i) {
        image = m_imagesInSet.at(i);
        if (image.scaleFactor >= m_scaleFactor)
            return image;
    }
    return image;
}

StyleFetchedImageSet* CSSImageSetValue::cachedImageSet(Document* document, float deviceScaleFactor, const ResourceLoaderOptions& options)
{
    ASSERT(document);

    m_scaleFactor = deviceScaleFactor;

    if (!m_imagesInSet.size())
        fillImageSet();

    if (!m_accessedBestFitImage) {
        // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
        // All forms of scale should be included: Page::pageScaleFactor(), LocalFrame::pageZoomFactor(),
        // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
        ImageWithScale image = bestImageForScaleFactor();
        FetchRequest request(ResourceRequest(document->completeURL(image.imageURL)), FetchInitiatorTypeNames::css, options);
        request.mutableResourceRequest().setHTTPReferrer(image.referrer);

        if (options.corsEnabled == IsCORSEnabled)
            request.setCrossOriginAccessControl(document->securityOrigin(), options.allowCredentials, options.credentialsRequested);

        if (ResourcePtr<ImageResource> cachedImage = ImageResource::fetch(request, document->fetcher())) {
            m_imageSet = StyleFetchedImageSet::create(cachedImage.get(), image.scaleFactor, this);
            m_accessedBestFitImage = true;
        }
    }

    return (m_imageSet && m_imageSet->isImageResourceSet()) ? toStyleFetchedImageSet(m_imageSet) : 0;
}

StyleFetchedImageSet* CSSImageSetValue::cachedImageSet(Document* document, float deviceScaleFactor)
{
    return cachedImageSet(document, deviceScaleFactor, ResourceFetcher::defaultResourceOptions());
}

StyleImage* CSSImageSetValue::cachedOrPendingImageSet(float deviceScaleFactor)
{
    if (!m_imageSet) {
        m_imageSet = StylePendingImage::create(this);
    } else if (!m_imageSet->isPendingImage()) {
        // If the deviceScaleFactor has changed, we may not have the best image loaded, so we have to re-assess.
        if (deviceScaleFactor != m_scaleFactor) {
            m_accessedBestFitImage = false;
            m_imageSet = StylePendingImage::create(this);
        }
    }

    return m_imageSet.get();
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("-webkit-image-set(");

    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        if (i > 0)
            result.appendLiteral(", ");

        const CSSValue* imageValue = item(i);
        result.append(imageValue->cssText());
        result.append(' ');

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        const CSSValue* scaleFactorValue = item(i);
        result.append(scaleFactorValue->cssText());
        // FIXME: Eventually the scale factor should contain it's own unit http://wkb.ug/100120.
        // For now 'x' is hard-coded in the parser, so we hard-code it here too.
        result.append('x');

        ++i;
    }

    result.append(')');
    return result.toString();
}

bool CSSImageSetValue::hasFailedOrCanceledSubresources() const
{
    if (!m_imageSet || !m_imageSet->isImageResourceSet())
        return false;
    if (Resource* cachedResource = toStyleFetchedImageSet(m_imageSet)->cachedImage())
        return cachedResource->loadFailedOrCanceled();
    return true;
}

} // namespace blink
