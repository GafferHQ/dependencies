/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef Length_h
#define Length_h

#include "platform/PlatformExport.h"
#include "platform/animation/AnimationUtilities.h"
#include "wtf/Assertions.h"
#include "wtf/FastAllocBase.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/MathExtras.h"
#include "wtf/Vector.h"
#include <cstring>

namespace blink {

// FIXME: This enum makes it hard to tell in general what values may be
// appropriate for any given Length.
enum LengthType {
    Auto, Percent, Fixed,
    Intrinsic, MinIntrinsic,
    MinContent, MaxContent, FillAvailable, FitContent,
    Calculated,
    ExtendToZoom, DeviceWidth, DeviceHeight,
    MaxSizeNone
};

enum ValueRange {
    ValueRangeAll,
    ValueRangeNonNegative
};

struct PixelsAndPercent {
    PixelsAndPercent(float pixels, float percent)
        : pixels(pixels)
        , percent(percent)
    {
    }
    float pixels;
    float percent;
};

class CalculationValue;

class PLATFORM_EXPORT Length {
    WTF_MAKE_FAST_ALLOCATED(Length);
public:
    Length()
        :  m_intValue(0), m_quirk(false), m_type(Auto), m_isFloat(false)
    {
    }

    Length(LengthType t)
        : m_intValue(0), m_quirk(false), m_type(t), m_isFloat(false)
    {
        ASSERT(t != Calculated);
    }

    Length(int v, LengthType t, bool q = false)
        : m_intValue(v), m_quirk(q), m_type(t), m_isFloat(false)
    {
        ASSERT(t != Calculated);
    }

    Length(LayoutUnit v, LengthType t, bool q = false)
        : m_floatValue(v.toFloat()), m_quirk(q), m_type(t), m_isFloat(true)
    {
        ASSERT(t != Calculated);
    }

    Length(float v, LengthType t, bool q = false)
        : m_floatValue(v), m_quirk(q), m_type(t), m_isFloat(true)
    {
        ASSERT(t != Calculated);
    }

    Length(double v, LengthType t, bool q = false)
        : m_quirk(q), m_type(t), m_isFloat(true)
    {
        m_floatValue = static_cast<float>(v);
    }

    explicit Length(PassRefPtr<CalculationValue>);

    Length(const Length& length)
    {
        memcpy(this, &length, sizeof(Length));
        if (isCalculated())
            incrementCalculatedRef();
    }

    Length& operator=(const Length& length)
    {
        if (length.isCalculated())
            length.incrementCalculatedRef();
        if (isCalculated())
            decrementCalculatedRef();
        memcpy(this, &length, sizeof(Length));
        return *this;
    }

    ~Length()
    {
        if (isCalculated())
            decrementCalculatedRef();
    }

    bool operator==(const Length& o) const { return (m_type == o.m_type) && (m_quirk == o.m_quirk) && (isMaxSizeNone() || (getFloatValue() == o.getFloatValue()) || isCalculatedEqual(o)); }
    bool operator!=(const Length& o) const { return !(*this == o); }

    const Length& operator*=(float v)
    {
        if (isCalculated()) {
            ASSERT_NOT_REACHED();
            return *this;
        }

        if (m_isFloat)
            m_floatValue = static_cast<float>(m_floatValue * v);
        else
            m_intValue = static_cast<int>(m_intValue * v);

        return *this;
    }

    // FIXME: Make this private (if possible) or at least rename it (http://crbug.com/432707).
    inline float value() const
    {
        return getFloatValue();
    }

    int intValue() const
    {
        if (isCalculated()) {
            ASSERT_NOT_REACHED();
            return 0;
        }
        return getIntValue();
    }

    float percent() const
    {
        ASSERT(type() == Percent);
        return getFloatValue();
    }
    PixelsAndPercent pixelsAndPercent() const;

    CalculationValue& calculationValue() const;

    LengthType type() const { return static_cast<LengthType>(m_type); }
    bool quirk() const { return m_quirk; }

    void setQuirk(bool quirk)
    {
        m_quirk = quirk;
    }

    void setValue(LengthType t, int value)
    {
        m_type = t;
        m_intValue = value;
        m_isFloat = false;
    }

    void setValue(int value)
    {
        if (isCalculated()) {
            ASSERT_NOT_REACHED();
            return;
        }
        setValue(Fixed, value);
    }

    void setValue(LengthType t, float value)
    {
        m_type = t;
        m_floatValue = value;
        m_isFloat = true;
    }

    void setValue(LengthType t, LayoutUnit value)
    {
        m_type = t;
        m_floatValue = value.toFloat();
        m_isFloat = true;
    }

    void setValue(float value)
    {
        *this = Length(value, Fixed);
    }

    bool isMaxSizeNone() const { return type() == MaxSizeNone; }

    // FIXME calc: https://bugs.webkit.org/show_bug.cgi?id=80357. A calculated Length
    // always contains a percentage, and without a maxValue passed to these functions
    // it's impossible to determine the sign or zero-ness. We assume all calc values
    // are positive and non-zero for now.
    bool isZero() const
    {
        ASSERT(!isMaxSizeNone());
        if (isCalculated())
            return false;

        return m_isFloat ? !m_floatValue : !m_intValue;
    }
    bool isPositive() const
    {
        if (isMaxSizeNone())
            return false;
        if (isCalculated())
            return true;

        return getFloatValue() > 0;
    }
    bool isNegative() const
    {
        if (isMaxSizeNone() || isCalculated())
            return false;

        return getFloatValue() < 0;
    }

    bool isAuto() const { return type() == Auto; }
    bool isFixed() const { return type() == Fixed; }
    bool isIntrinsicOrAuto() const { return type() == Auto || isLegacyIntrinsic() || isIntrinsic(); }
    bool isLegacyIntrinsic() const { return type() == Intrinsic || type() == MinIntrinsic; }
    bool isIntrinsic() const { return type() == MinContent || type() == MaxContent || type() == FillAvailable || type() == FitContent; }
    bool isSpecified() const { return type() == Fixed || type() == Percent || type() == Calculated; }
    bool isSpecifiedOrIntrinsic() const { return isSpecified() || isIntrinsic(); }
    bool isCalculated() const { return type() == Calculated; }
    bool isCalculatedEqual(const Length&) const;
    bool isMinContent() const { return type() == MinContent; }
    bool isMaxContent() const { return type() == MaxContent; }
    bool isFillAvailable() const { return type() == FillAvailable; }
    bool isFitContent() const { return type() == FitContent; }
    bool hasPercent() const { return type() == Percent || type() == Calculated; }

    Length blend(const Length& from, double progress, ValueRange range) const
    {
        ASSERT(isSpecified() && from.isSpecified());

        if (progress == 0.0)
            return from;

        if (progress == 1.0)
            return *this;

        if (from.type() == Calculated || type() == Calculated)
            return blendMixedTypes(from, progress, range);

        if (!from.isZero() && !isZero() && from.type() != type())
            return blendMixedTypes(from, progress, range);

        if (from.isZero() && isZero())
            return *this;

        LengthType resultType = type();
        if (isZero())
            resultType = from.type();

        float blendedValue = blink::blend(from.value(), value(), progress);
        if (range == ValueRangeNonNegative)
            blendedValue = clampTo<float>(blendedValue, 0);
        return Length(blendedValue, resultType);
    }

    float getFloatValue() const
    {
        ASSERT(!isMaxSizeNone());
        return m_isFloat ? m_floatValue : m_intValue;
    }
    float nonNanCalculatedValue(LayoutUnit maxValue) const;

    Length subtractFromOneHundredPercent() const;

private:
    int getIntValue() const
    {
        ASSERT(!isMaxSizeNone());
        return m_isFloat ? static_cast<int>(m_floatValue) : m_intValue;
    }

    Length blendMixedTypes(const Length& from, double progress, ValueRange) const;

    int calculationHandle() const
    {
        ASSERT(isCalculated());
        return getIntValue();
    }
    void incrementCalculatedRef() const;
    void decrementCalculatedRef() const;

    union {
        int m_intValue;
        float m_floatValue;
    };
    bool m_quirk;
    unsigned char m_type;
    bool m_isFloat;
};

PLATFORM_EXPORT Vector<Length> parseHTMLAreaElementCoords(const String&);

} // namespace blink

#endif // Length_h
