/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "platform/Length.h"

#include "platform/CalculationValue.h"
#include "platform/animation/AnimationUtilities.h"
#include "wtf/ASCIICType.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/WTFString.h"

using namespace WTF;

namespace blink {

template<typename CharType>
static unsigned splitLength(const CharType* data, unsigned length, unsigned& intLength, unsigned& doubleLength)
{
    ASSERT(length);

    unsigned i = 0;
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;
    if (i < length && (data[i] == '+' || data[i] == '-'))
        ++i;
    while (i < length && isASCIIDigit(data[i]))
        ++i;
    intLength = i;
    while (i < length && (isASCIIDigit(data[i]) || data[i] == '.'))
        ++i;
    doubleLength = i;

    // IE quirk: Skip whitespace between the number and the % character (20 % => 20%).
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;

    return i;
}

template<typename CharType>
static Length parseHTMLAreaCoordinate(const CharType* data, unsigned length)
{
    unsigned intLength;
    unsigned doubleLength;
    splitLength(data, length, intLength, doubleLength);

    return Length(charactersToIntStrict(data, intLength), Fixed);
}

// FIXME: Per HTML5, this should follow the "rules for parsing a list of integers".
Vector<Length> parseHTMLAreaElementCoords(const String& string)
{
    unsigned length = string.length();
    StringBuffer<LChar> spacified(length);
    for (unsigned i = 0; i < length; i++) {
        UChar cc = string[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '.'))
            spacified[i] = ' ';
        else
            spacified[i] = cc;
    }
    RefPtr<StringImpl> str = spacified.release();
    str = str->simplifyWhiteSpace();
    ASSERT(str->is8Bit());

    if (!str->length())
        return Vector<Length>();

    unsigned len = str->count(' ') + 1;
    Vector<Length> r(len);

    unsigned i = 0;
    unsigned pos = 0;
    size_t pos2;

    while ((pos2 = str->find(' ', pos)) != kNotFound) {
        r[i++] = parseHTMLAreaCoordinate(str->characters8() + pos, pos2 - pos);
        pos = pos2 + 1;
    }
    r[i] = parseHTMLAreaCoordinate(str->characters8() + pos, str->length() - pos);

    ASSERT(i == len - 1);

    return r;
}

class CalculationValueHandleMap {
    WTF_MAKE_FAST_ALLOCATED(CalculationValueHandleMap);
public:
    CalculationValueHandleMap()
        : m_index(1)
    {
    }

    int insert(PassRefPtr<CalculationValue> calcValue)
    {
        ASSERT(m_index);
        // FIXME calc(): https://bugs.webkit.org/show_bug.cgi?id=80489
        // This monotonically increasing handle generation scheme is potentially wasteful
        // of the handle space. Consider reusing empty handles.
        while (m_map.contains(m_index))
            m_index++;

        m_map.set(m_index, calcValue);

        return m_index;
    }

    void remove(int index)
    {
        ASSERT(m_map.contains(index));
        m_map.remove(index);
    }

    CalculationValue& get(int index)
    {
        ASSERT(m_map.contains(index));
        return *m_map.get(index);
    }

    void decrementRef(int index)
    {
        ASSERT(m_map.contains(index));
        CalculationValue* value = m_map.get(index);
        if (value->hasOneRef()) {
            // Force the CalculationValue destructor early to avoid a potential recursive call inside HashMap remove().
            m_map.set(index, nullptr);
            m_map.remove(index);
        } else {
            value->deref();
        }
    }

private:
    int m_index;
    HashMap<int, RefPtr<CalculationValue>> m_map;
};

static CalculationValueHandleMap& calcHandles()
{
    DEFINE_STATIC_LOCAL(CalculationValueHandleMap, handleMap, ());
    return handleMap;
}

Length::Length(PassRefPtr<CalculationValue> calc)
    : m_quirk(false)
    , m_type(Calculated)
    , m_isFloat(false)
{
    m_intValue = calcHandles().insert(calc);
}

Length Length::blendMixedTypes(const Length& from, double progress, ValueRange range) const
{
    ASSERT(from.isSpecified());
    ASSERT(isSpecified());
    PixelsAndPercent fromPixelsAndPercent = from.pixelsAndPercent();
    PixelsAndPercent toPixelsAndPercent = pixelsAndPercent();
    const float pixels = blink::blend(fromPixelsAndPercent.pixels, toPixelsAndPercent.pixels, progress);
    const float percent = blink::blend(fromPixelsAndPercent.percent, toPixelsAndPercent.percent, progress);
    return Length(CalculationValue::create(PixelsAndPercent(pixels, percent), range));
}

PixelsAndPercent Length::pixelsAndPercent() const
{
    switch (type()) {
    case Fixed:
        return PixelsAndPercent(value(), 0);
    case Percent:
        return PixelsAndPercent(0, value());
    case Calculated:
        return calculationValue().pixelsAndPercent();
    default:
        ASSERT_NOT_REACHED();
        return PixelsAndPercent(0, 0);
    }
}

Length Length::subtractFromOneHundredPercent() const
{
    PixelsAndPercent result = pixelsAndPercent();
    result.pixels = -result.pixels;
    result.percent = 100 - result.percent;
    if (result.pixels && result.percent)
        return Length(CalculationValue::create(result, ValueRangeAll));
    if (result.percent)
        return Length(result.percent, Percent);
    return Length(result.pixels, Fixed);
}

CalculationValue& Length::calculationValue() const
{
    ASSERT(isCalculated());
    return calcHandles().get(calculationHandle());
}

void Length::incrementCalculatedRef() const
{
    ASSERT(isCalculated());
    calculationValue().ref();
}

void Length::decrementCalculatedRef() const
{
    ASSERT(isCalculated());
    calcHandles().decrementRef(calculationHandle());
}

float Length::nonNanCalculatedValue(LayoutUnit maxValue) const
{
    ASSERT(isCalculated());
    float result = calculationValue().evaluate(maxValue.toFloat());
    if (std::isnan(result))
        return 0;
    return result;
}

bool Length::isCalculatedEqual(const Length& o) const
{
    return isCalculated() && (&calculationValue() == &o.calculationValue() || calculationValue() == o.calculationValue());
}

struct SameSizeAsLength {
    int32_t value;
    int32_t metaData;
};
static_assert(sizeof(Length) == sizeof(SameSizeAsLength), "length should stay small");

} // namespace blink
