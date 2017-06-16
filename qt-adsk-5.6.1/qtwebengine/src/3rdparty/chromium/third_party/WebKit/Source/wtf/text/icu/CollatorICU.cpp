/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "wtf/text/Collator.h"

#include "wtf/Assertions.h"
#include "wtf/StringExtras.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include <stdlib.h>
#include <string.h>
#include <unicode/ucol.h>

namespace WTF {

static UCollator* cachedCollator;
static char cachedEquivalentLocale[Collator::ulocFullnameCapacity];
static Mutex& cachedCollatorMutex()
{
    AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
    return mutex;
}

Collator::Collator(const char* locale)
    : m_collator(0)
    , m_locale(locale ? strdup(locale) : 0)
    , m_lowerFirst(false)
{
    setEquivalentLocale(m_locale, m_equivalentLocale);
}

PassOwnPtr<Collator> Collator::userDefault()
{
    return adoptPtr(new Collator(0));
}

Collator::~Collator()
{
    releaseCollator();
    free(m_locale);
}

void Collator::setOrderLowerFirst(bool lowerFirst)
{
    m_lowerFirst = lowerFirst;
}

Collator::Result Collator::collate(const UChar* lhs, size_t lhsLength, const UChar* rhs, size_t rhsLength) const
{
    if (!m_collator)
        createCollator();

    return static_cast<Result>(ucol_strcoll(m_collator, lhs, lhsLength, rhs, rhsLength));
}

void Collator::createCollator() const
{
    ASSERT(!m_collator);
    UErrorCode status = U_ZERO_ERROR;

    {
        Locker<Mutex> lock(cachedCollatorMutex());
        if (cachedCollator) {
            UColAttributeValue cachedCollatorLowerFirst = ucol_getAttribute(cachedCollator, UCOL_CASE_FIRST, &status);
            ASSERT(U_SUCCESS(status));

            if (0 == strcmp(cachedEquivalentLocale, m_equivalentLocale)
                && ((UCOL_LOWER_FIRST == cachedCollatorLowerFirst && m_lowerFirst) || (UCOL_UPPER_FIRST == cachedCollatorLowerFirst && !m_lowerFirst))) {
                m_collator = cachedCollator;
                cachedCollator = 0;
                cachedEquivalentLocale[0] = 0;
                return;
            }
        }
    }

    m_collator = ucol_open(m_locale, &status);
    if (U_FAILURE(status)) {
        status = U_ZERO_ERROR;
        m_collator = ucol_open("", &status); // Fallback to Unicode Collation Algorithm.
    }
    ASSERT(U_SUCCESS(status));

    ucol_setAttribute(m_collator, UCOL_CASE_FIRST, m_lowerFirst ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST, &status);
    ASSERT(U_SUCCESS(status));

    ucol_setAttribute(m_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ASSERT(U_SUCCESS(status));
}

void Collator::releaseCollator()
{
    {
        Locker<Mutex> lock(cachedCollatorMutex());
        if (cachedCollator)
            ucol_close(cachedCollator);
        cachedCollator = m_collator;
        strncpy(cachedEquivalentLocale, m_equivalentLocale, ulocFullnameCapacity);
        m_collator  = 0;
    }
    m_collator = 0;
}

void Collator::setEquivalentLocale(const char* locale, char* equivalentLocale)
{
    UErrorCode status = U_ZERO_ERROR;
    UBool isAvailable;
    ucol_getFunctionalEquivalent(equivalentLocale, ulocFullnameCapacity, "collation", locale, &isAvailable, &status);
    if (U_FAILURE(status))
        strcpy(equivalentLocale, "root");
}

} // namespace WTF
