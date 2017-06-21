/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/css/CSSPrimitiveValue.h"

#include "core/css/CSSBasicShapes.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSHelper.h"
#include "core/css/CSSMarkup.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/Counter.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Node.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/FontMetrics.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringBuilder.h"

using namespace WTF;

namespace blink {

namespace {

// Max/min values for CSS, needs to slightly smaller/larger than the true max/min values to allow for rounding without overflowing.
// Subtract two (rather than one) to allow for values to be converted to float and back without exceeding the LayoutUnit::max.
const int maxValueForCssLength = INT_MAX / kFixedPointDenominator - 2;
const int minValueForCssLength = INT_MIN / kFixedPointDenominator + 2;

typedef HashMap<String, CSSPrimitiveValue::UnitType> StringToUnitTable;

StringToUnitTable createStringToUnitTable()
{
    StringToUnitTable table;
    table.set(String("em"), CSSPrimitiveValue::CSS_EMS);
    table.set(String("ex"), CSSPrimitiveValue::CSS_EXS);
    table.set(String("px"), CSSPrimitiveValue::CSS_PX);
    table.set(String("cm"), CSSPrimitiveValue::CSS_CM);
    table.set(String("mm"), CSSPrimitiveValue::CSS_MM);
    table.set(String("in"), CSSPrimitiveValue::CSS_IN);
    table.set(String("pt"), CSSPrimitiveValue::CSS_PT);
    table.set(String("pc"), CSSPrimitiveValue::CSS_PC);
    table.set(String("deg"), CSSPrimitiveValue::CSS_DEG);
    table.set(String("rad"), CSSPrimitiveValue::CSS_RAD);
    table.set(String("grad"), CSSPrimitiveValue::CSS_GRAD);
    table.set(String("ms"), CSSPrimitiveValue::CSS_MS);
    table.set(String("s"), CSSPrimitiveValue::CSS_S);
    table.set(String("hz"), CSSPrimitiveValue::CSS_HZ);
    table.set(String("khz"), CSSPrimitiveValue::CSS_KHZ);
    table.set(String("dpi"), CSSPrimitiveValue::CSS_DPI);
    table.set(String("dpcm"), CSSPrimitiveValue::CSS_DPCM);
    table.set(String("dppx"), CSSPrimitiveValue::CSS_DPPX);
    table.set(String("vw"), CSSPrimitiveValue::CSS_VW);
    table.set(String("vh"), CSSPrimitiveValue::CSS_VH);
    table.set(String("vmin"), CSSPrimitiveValue::CSS_VMIN);
    table.set(String("vmax"), CSSPrimitiveValue::CSS_VMAX);
    table.set(String("rem"), CSSPrimitiveValue::CSS_REMS);
    table.set(String("fr"), CSSPrimitiveValue::CSS_FR);
    table.set(String("turn"), CSSPrimitiveValue::CSS_TURN);
    table.set(String("ch"), CSSPrimitiveValue::CSS_CHS);
    table.set(String("__qem"), CSSPrimitiveValue::CSS_QEM);
    return table;
}

StringToUnitTable& unitTable()
{
    DEFINE_STATIC_LOCAL(StringToUnitTable, unitTable, (createStringToUnitTable()));
    return unitTable;
}

} // namespace

float CSSPrimitiveValue::clampToCSSLengthRange(double value)
{
    return clampTo<float>(value, minValueForCssLength, maxValueForCssLength);
}

void CSSPrimitiveValue::initUnitTable()
{
    // Make sure we initialize this during blink initialization
    // to avoid racy static local initialization.
    unitTable();
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::fromName(const String& unit)
{
    return unitTable().get(unit.lower());
}

CSSPrimitiveValue::UnitCategory CSSPrimitiveValue::unitCategory(UnitType type)
{
    switch (type) {
    case CSS_NUMBER:
        return CSSPrimitiveValue::UNumber;
    case CSS_PERCENTAGE:
        return CSSPrimitiveValue::UPercent;
    case CSS_PX:
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
        return CSSPrimitiveValue::ULength;
    case CSS_MS:
    case CSS_S:
        return CSSPrimitiveValue::UTime;
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_TURN:
        return CSSPrimitiveValue::UAngle;
    case CSS_HZ:
    case CSS_KHZ:
        return CSSPrimitiveValue::UFrequency;
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
        return CSSPrimitiveValue::UResolution;
    default:
        return CSSPrimitiveValue::UOther;
    }
}

bool CSSPrimitiveValue::colorIsDerivedFromElement() const
{
    int valueID = getValueID();
    switch (valueID) {
    case CSSValueWebkitText:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

typedef HashMap<const CSSPrimitiveValue*, String> CSSTextCache;
static CSSTextCache& cssTextCache()
{
    AtomicallyInitializedStaticReference(ThreadSpecific<CSSTextCache>, cache, new ThreadSpecific<CSSTextCache>());
    return *cache;
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::primitiveType() const
{
    if (m_primitiveUnitType == CSS_PROPERTY_ID || m_primitiveUnitType == CSS_VALUE_ID)
        return CSS_IDENT;

    if (m_primitiveUnitType != CSS_CALC)
        return static_cast<UnitType>(m_primitiveUnitType);

    switch (m_value.calc->category()) {
    case CalcAngle:
        return CSS_DEG;
    case CalcFrequency:
        return CSS_HZ;
    case CalcNumber:
        return CSS_NUMBER;
    case CalcPercent:
        return CSS_PERCENTAGE;
    case CalcLength:
        return CSS_PX;
    case CalcPercentNumber:
        return CSS_CALC_PERCENTAGE_WITH_NUMBER;
    case CalcPercentLength:
        return CSS_CALC_PERCENTAGE_WITH_LENGTH;
    case CalcTime:
        return CSS_MS;
    case CalcOther:
        return CSS_UNKNOWN;
    }
    return CSS_UNKNOWN;
}

static const AtomicString& propertyName(CSSPropertyID propertyID)
{
    return getPropertyNameAtomicString(propertyID);
}

static const AtomicString& valueName(CSSValueID valueID)
{
    ASSERT_ARG(valueID, valueID >= 0);
    ASSERT_ARG(valueID, valueID < numCSSValueKeywords);

    if (valueID < 0)
        return nullAtom;

    static AtomicString* keywordStrings = new AtomicString[numCSSValueKeywords]; // Leaked intentionally.
    AtomicString& keywordString = keywordStrings[valueID];
    if (keywordString.isNull())
        keywordString = getValueName(valueID);
    return keywordString;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSValueID valueID)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_VALUE_ID;
    m_value.valueID = valueID;
}

CSSPrimitiveValue::CSSPrimitiveValue(CSSPropertyID propertyID)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_PROPERTY_ID;
    m_value.propertyID = propertyID;
}

CSSPrimitiveValue::CSSPrimitiveValue(double num, UnitType type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    ASSERT(std::isfinite(num));
    m_value.num = num;
}

CSSPrimitiveValue::CSSPrimitiveValue(const String& str, UnitType type)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = type;
    m_value.string = str.impl();
    if (m_value.string)
        m_value.string->ref();
}

CSSPrimitiveValue::CSSPrimitiveValue(const LengthSize& lengthSize, const ComputedStyle& style)
    : CSSValue(PrimitiveClass)
{
    init(lengthSize, style);
}

CSSPrimitiveValue::CSSPrimitiveValue(RGBA32 color)
    : CSSValue(PrimitiveClass)
{
    m_primitiveUnitType = CSS_RGBCOLOR;
    m_value.rgbcolor = color;
}

CSSPrimitiveValue::CSSPrimitiveValue(const Length& length, float zoom)
    : CSSValue(PrimitiveClass)
{
    switch (length.type()) {
    case Auto:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueAuto;
        break;
    case Intrinsic:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueIntrinsic;
        break;
    case MinIntrinsic:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueMinIntrinsic;
        break;
    case MinContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueMinContent;
        break;
    case MaxContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueMaxContent;
        break;
    case FillAvailable:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitFillAvailable;
        break;
    case FitContent:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueWebkitFitContent;
        break;
    case ExtendToZoom:
        m_primitiveUnitType = CSS_VALUE_ID;
        m_value.valueID = CSSValueInternalExtendToZoom;
        break;
    case Percent:
        m_primitiveUnitType = CSS_PERCENTAGE;
        ASSERT(std::isfinite(length.percent()));
        m_value.num = length.percent();
        break;
    case Fixed:
        m_primitiveUnitType = CSS_PX;
        m_value.num = length.value() / zoom;
        break;
    case Calculated: {
        const CalculationValue& calc = length.calculationValue();
        if (calc.pixels() && calc.percent()) {
            init(CSSCalcValue::create(
                CSSCalcValue::createExpressionNode(calc.pixels() / zoom, calc.percent()),
                calc.isNonNegative() ? ValueRangeNonNegative : ValueRangeAll));
            break;
        }
        if (calc.percent()) {
            m_primitiveUnitType = CSS_PERCENTAGE;
            m_value.num = calc.percent();
        } else {
            m_primitiveUnitType = CSS_PX;
            m_value.num = calc.pixels() / zoom;
        }
        if (m_value.num < 0 && calc.isNonNegative())
            m_value.num = 0;
        break;
    }
    case DeviceWidth:
    case DeviceHeight:
    case MaxSizeNone:
        ASSERT_NOT_REACHED();
        break;
    }
}

void CSSPrimitiveValue::init(const LengthSize& lengthSize, const ComputedStyle& style)
{
    m_primitiveUnitType = CSS_PAIR;
    m_hasCachedCSSText = false;
    m_value.pair = Pair::create(create(lengthSize.width(), style.effectiveZoom()), create(lengthSize.height(), style.effectiveZoom()), Pair::KeepIdenticalValues).leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<Counter> c)
{
    m_primitiveUnitType = CSS_COUNTER;
    m_hasCachedCSSText = false;
    m_value.counter = c.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<Rect> r)
{
    m_primitiveUnitType = CSS_RECT;
    m_hasCachedCSSText = false;
    m_value.rect = r.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<Quad> quad)
{
    m_primitiveUnitType = CSS_QUAD;
    m_hasCachedCSSText = false;
    m_value.quad = quad.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<Pair> p)
{
    m_primitiveUnitType = CSS_PAIR;
    m_hasCachedCSSText = false;
    m_value.pair = p.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<CSSCalcValue> c)
{
    m_primitiveUnitType = CSS_CALC;
    m_hasCachedCSSText = false;
    m_value.calc = c.leakRef();
}

void CSSPrimitiveValue::init(PassRefPtrWillBeRawPtr<CSSBasicShape> shape)
{
    m_primitiveUnitType = CSS_SHAPE;
    m_hasCachedCSSText = false;
    m_value.shape = shape.leakRef();
}

CSSPrimitiveValue::~CSSPrimitiveValue()
{
    cleanup();
}

void CSSPrimitiveValue::cleanup()
{
    switch (static_cast<UnitType>(m_primitiveUnitType)) {
    case CSS_CUSTOM_IDENT:
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
        if (m_value.string)
            m_value.string->deref();
        break;
    case CSS_COUNTER:
        // We must not call deref() when oilpan is enabled because m_value.counter is traced.
#if !ENABLE(OILPAN)
        m_value.counter->deref();
#endif
        break;
    case CSS_RECT:
        // We must not call deref() when oilpan is enabled because m_value.rect is traced.
#if !ENABLE(OILPAN)
        m_value.rect->deref();
#endif
        break;
    case CSS_QUAD:
        // We must not call deref() when oilpan is enabled because m_value.quad is traced.
#if !ENABLE(OILPAN)
        m_value.quad->deref();
#endif
        break;
    case CSS_PAIR:
        // We must not call deref() when oilpan is enabled because m_value.pair is traced.
#if !ENABLE(OILPAN)
        m_value.pair->deref();
#endif
        break;
    case CSS_CALC:
        // We must not call deref() when oilpan is enabled because m_value.calc is traced.
#if !ENABLE(OILPAN)
        m_value.calc->deref();
#endif
        break;
    case CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSS_CALC_PERCENTAGE_WITH_LENGTH:
        ASSERT_NOT_REACHED();
        break;
    case CSS_SHAPE:
        // We must not call deref() when oilpan is enabled because m_value.shape is traced.
#if !ENABLE(OILPAN)
        m_value.shape->deref();
#endif
        break;
    case CSS_NUMBER:
    case CSS_INTEGER:
    case CSS_PERCENTAGE:
    case CSS_EMS:
    case CSS_QEM:
    case CSS_EXS:
    case CSS_REMS:
    case CSS_CHS:
    case CSS_PX:
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_MS:
    case CSS_S:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_TURN:
    case CSS_VW:
    case CSS_VH:
    case CSS_VMIN:
    case CSS_VMAX:
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
    case CSS_FR:
    case CSS_IDENT:
    case CSS_RGBCOLOR:
    case CSS_UNKNOWN:
    case CSS_PROPERTY_ID:
    case CSS_VALUE_ID:
        break;
    }
    m_primitiveUnitType = 0;
    if (m_hasCachedCSSText) {
        cssTextCache().remove(this);
        m_hasCachedCSSText = false;
    }
}

double CSSPrimitiveValue::computeSeconds()
{
    ASSERT(isTime() || (isCalculated() && cssCalcValue()->category() == CalcTime));
    UnitType currentType = isCalculated() ? cssCalcValue()->expressionNode()->primitiveType() : static_cast<UnitType>(m_primitiveUnitType);
    if (currentType == CSS_S)
        return getDoubleValue();
    if (currentType == CSS_MS)
        return getDoubleValue() / 1000;
    ASSERT_NOT_REACHED();
    return 0;
}

double CSSPrimitiveValue::computeDegrees() const
{
    ASSERT(isAngle() || (isCalculated() && cssCalcValue()->category() == CalcAngle));
    UnitType currentType = isCalculated() ? cssCalcValue()->expressionNode()->primitiveType() : static_cast<UnitType>(m_primitiveUnitType);
    switch (currentType) {
    case CSS_DEG:
        return getDoubleValue();
    case CSS_RAD:
        return rad2deg(getDoubleValue());
    case CSS_GRAD:
        return grad2deg(getDoubleValue());
    case CSS_TURN:
        return turn2deg(getDoubleValue());
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

template<> int CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return roundForImpreciseConversion<int>(computeLengthDouble(conversionData));
}

template<> unsigned CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return roundForImpreciseConversion<unsigned>(computeLengthDouble(conversionData));
}

template<> Length CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return Length(clampToCSSLengthRange(computeLengthDouble(conversionData)), Fixed);
}

template<> short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return roundForImpreciseConversion<short>(computeLengthDouble(conversionData));
}

template<> unsigned short CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return roundForImpreciseConversion<unsigned short>(computeLengthDouble(conversionData));
}

template<> float CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return static_cast<float>(computeLengthDouble(conversionData));
}

template<> double CSSPrimitiveValue::computeLength(const CSSToLengthConversionData& conversionData)
{
    return computeLengthDouble(conversionData);
}

double CSSPrimitiveValue::computeLengthDouble(const CSSToLengthConversionData& conversionData)
{
    // The logic in this function is duplicated in MediaValues::computeLength
    // because MediaValues::computeLength needs nearly identical logic, but we haven't found a way to make
    // CSSPrimitiveValue::computeLengthDouble more generic (to solve both cases) without hurting performance.
    if (m_primitiveUnitType == CSS_CALC)
        return m_value.calc->computeLengthPx(conversionData);

    double factor;

    switch (primitiveType()) {
        case CSS_EMS:
            factor = conversionData.emFontSize();
            break;
        case CSS_EXS:
            factor = conversionData.exFontSize();
            break;
        case CSS_REMS:
            factor = conversionData.remFontSize();
            break;
        case CSS_CHS:
            factor = conversionData.chFontSize();
            break;
        case CSS_PX:
            factor = 1.0;
            break;
        case CSS_CM:
            factor = cssPixelsPerCentimeter;
            break;
        case CSS_MM:
            factor = cssPixelsPerMillimeter;
            break;
        case CSS_IN:
            factor = cssPixelsPerInch;
            break;
        case CSS_PT:
            factor = cssPixelsPerPoint;
            break;
        case CSS_PC:
            factor = cssPixelsPerPica;
            break;
        case CSS_VW:
            factor = conversionData.viewportWidthPercent();
            break;
        case CSS_VH:
            factor = conversionData.viewportHeightPercent();
            break;
        case CSS_VMIN:
            factor = conversionData.viewportMinPercent();
            break;
        case CSS_VMAX:
            factor = conversionData.viewportMaxPercent();
            break;
        case CSS_CALC_PERCENTAGE_WITH_LENGTH:
        case CSS_CALC_PERCENTAGE_WITH_NUMBER:
            ASSERT_NOT_REACHED();
            return -1.0;
        default:
            ASSERT_NOT_REACHED();
            return -1.0;
    }

    // We do not apply the zoom factor when we are computing the value of the font-size property. The zooming
    // for font sizes is much more complicated, since we have to worry about enforcing the minimum font size preference
    // as well as enforcing the implicit "smart minimum."
    double result = getDoubleValue() * factor;
    if (isFontRelativeLength())
        return result;

    return result * conversionData.zoom();
}

void CSSPrimitiveValue::accumulateLengthArray(CSSLengthArray& lengthArray, CSSLengthTypeArray& lengthTypeArray, double multiplier) const
{
    ASSERT(lengthArray.size() == LengthUnitTypeCount);

    if (m_primitiveUnitType == CSS_CALC) {
        cssCalcValue()->accumulateLengthArray(lengthArray, lengthTypeArray, multiplier);
        return;
    }

    LengthUnitType lengthType;
    if (unitTypeToLengthUnitType(static_cast<UnitType>(m_primitiveUnitType), lengthType)) {
        lengthArray.at(lengthType) += m_value.num * conversionToCanonicalUnitsScaleFactor(static_cast<UnitType>(m_primitiveUnitType)) * multiplier;
        lengthTypeArray.set(lengthType);
    }
}

void CSSPrimitiveValue::accumulateLengthArray(CSSLengthArray& lengthArray, double multiplier) const
{
    CSSLengthTypeArray lengthTypeArray;
    lengthTypeArray.resize(CSSPrimitiveValue::LengthUnitTypeCount);
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i)
        lengthTypeArray.clear(i);
    return CSSPrimitiveValue::accumulateLengthArray(lengthArray, lengthTypeArray, multiplier);
}

double CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(UnitType unitType)
{
    double factor = 1.0;
    // FIXME: the switch can be replaced by an array of scale factors.
    switch (unitType) {
    // These are "canonical" units in their respective categories.
    case CSS_PX:
    case CSS_DEG:
    case CSS_MS:
    case CSS_HZ:
        break;
    case CSS_CM:
        factor = cssPixelsPerCentimeter;
        break;
    case CSS_DPCM:
        factor = 1 / cssPixelsPerCentimeter;
        break;
    case CSS_MM:
        factor = cssPixelsPerMillimeter;
        break;
    case CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSS_DPI:
        factor = 1 / cssPixelsPerInch;
        break;
    case CSS_PT:
        factor = cssPixelsPerPoint;
        break;
    case CSS_PC:
        factor = cssPixelsPerPica;
        break;
    case CSS_RAD:
        factor = 180 / piDouble;
        break;
    case CSS_GRAD:
        factor = 0.9;
        break;
    case CSS_TURN:
        factor = 360;
        break;
    case CSS_S:
    case CSS_KHZ:
        factor = 1000;
        break;
    default:
        break;
    }

    return factor;
}

Length CSSPrimitiveValue::convertToLength(const CSSToLengthConversionData& conversionData)
{
    if (isLength())
        return computeLength<Length>(conversionData);
    if (isPercentage())
        return Length(getDoubleValue(), Percent);
    ASSERT(isCalculated());
    return Length(cssCalcValue()->toCalcValue(conversionData));
}

double CSSPrimitiveValue::getDoubleValue() const
{
    return m_primitiveUnitType != CSS_CALC ? m_value.num : m_value.calc->doubleValue();
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::canonicalUnitTypeForCategory(UnitCategory category)
{
    // The canonical unit type is chosen according to the way CSSPropertyParser::validUnit() chooses the default unit
    // in each category (based on unitflags).
    switch (category) {
    case UNumber:
        return CSS_NUMBER;
    case ULength:
        return CSS_PX;
    case UPercent:
        return CSS_UNKNOWN; // Cannot convert between numbers and percent.
    case UTime:
        return CSS_MS;
    case UAngle:
        return CSS_DEG;
    case UFrequency:
        return CSS_HZ;
    case UResolution:
        return CSS_DPPX;
    default:
        return CSS_UNKNOWN;
    }
}

bool CSSPrimitiveValue::unitTypeToLengthUnitType(UnitType unitType, LengthUnitType& lengthType)
{
    switch (unitType) {
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        lengthType = UnitTypePixels;
        return true;
    case CSSPrimitiveValue::CSS_EMS:
        lengthType = UnitTypeFontSize;
        return true;
    case CSSPrimitiveValue::CSS_EXS:
        lengthType = UnitTypeFontXSize;
        return true;
    case CSSPrimitiveValue::CSS_REMS:
        lengthType = UnitTypeRootFontSize;
        return true;
    case CSSPrimitiveValue::CSS_CHS:
        lengthType = UnitTypeZeroCharacterWidth;
        return true;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        lengthType = UnitTypePercentage;
        return true;
    case CSSPrimitiveValue::CSS_VW:
        lengthType = UnitTypeViewportWidth;
        return true;
    case CSSPrimitiveValue::CSS_VH:
        lengthType = UnitTypeViewportHeight;
        return true;
    case CSSPrimitiveValue::CSS_VMIN:
        lengthType = UnitTypeViewportMin;
        return true;
    case CSSPrimitiveValue::CSS_VMAX:
        lengthType = UnitTypeViewportMax;
        return true;
    default:
        return false;
    }
}

CSSPrimitiveValue::UnitType CSSPrimitiveValue::lengthUnitTypeToUnitType(LengthUnitType type)
{
    switch (type) {
    case UnitTypePixels:
        return CSSPrimitiveValue::CSS_PX;
    case UnitTypeFontSize:
        return CSSPrimitiveValue::CSS_EMS;
    case UnitTypeFontXSize:
        return CSSPrimitiveValue::CSS_EXS;
    case UnitTypeRootFontSize:
        return CSSPrimitiveValue::CSS_REMS;
    case UnitTypeZeroCharacterWidth:
        return CSSPrimitiveValue::CSS_CHS;
    case UnitTypePercentage:
        return CSSPrimitiveValue::CSS_PERCENTAGE;
    case UnitTypeViewportWidth:
        return CSSPrimitiveValue::CSS_VW;
    case UnitTypeViewportHeight:
        return CSSPrimitiveValue::CSS_VH;
    case UnitTypeViewportMin:
        return CSSPrimitiveValue::CSS_VMIN;
    case UnitTypeViewportMax:
        return CSSPrimitiveValue::CSS_VMAX;
    case LengthUnitTypeCount:
        break;
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::CSS_UNKNOWN;
}

String CSSPrimitiveValue::getStringValue() const
{
    switch (m_primitiveUnitType) {
    case CSS_CUSTOM_IDENT:
    case CSS_STRING:
    case CSS_ATTR:
    case CSS_URI:
        return m_value.string;
    case CSS_VALUE_ID:
        return valueName(m_value.valueID);
    case CSS_PROPERTY_ID:
        return propertyName(m_value.propertyID);
    default:
        break;
    }

    return String();
}

static String formatNumber(double number, const char* suffix, unsigned suffixLength)
{
#if OS(WIN) && _MSC_VER < 1900
    unsigned oldFormat = _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
    String result = String::format("%.6g", number);
#if OS(WIN) && _MSC_VER < 1900
    _set_output_format(oldFormat);
#endif
    result.append(suffix, suffixLength);
    return result;
}

template <unsigned characterCount>
ALWAYS_INLINE static String formatNumber(double number, const char (&characters)[characterCount])
{
    return formatNumber(number, characters, characterCount - 1);
}

static String formatNumber(double number, const char* characters)
{
    return formatNumber(number, characters, strlen(characters));
}

const char* CSSPrimitiveValue::unitTypeToString(UnitType type)
{
    switch (type) {
    case CSS_NUMBER:
    case CSS_INTEGER:
        return "";
    case CSS_PERCENTAGE:
        return "%";
    case CSS_EMS:
        return "em";
    case CSS_EXS:
        return "ex";
    case CSS_REMS:
        return "rem";
    case CSS_CHS:
        return "ch";
    case CSS_PX:
        return "px";
    case CSS_CM:
        return "cm";
    case CSS_DPPX:
        return "dppx";
    case CSS_DPI:
        return "dpi";
    case CSS_DPCM:
        return "dpcm";
    case CSS_MM:
        return "mm";
    case CSS_IN:
        return "in";
    case CSS_PT:
        return "pt";
    case CSS_PC:
        return "pc";
    case CSS_DEG:
        return "deg";
    case CSS_RAD:
        return "rad";
    case CSS_GRAD:
        return "grad";
    case CSS_MS:
        return "ms";
    case CSS_S:
        return "s";
    case CSS_HZ:
        return "hz";
    case CSS_KHZ:
        return "khz";
    case CSS_TURN:
        return "turn";
    case CSS_FR:
        return "fr";
    case CSS_VW:
        return "vw";
    case CSS_VH:
        return "vh";
    case CSS_VMIN:
        return "vmin";
    case CSS_VMAX:
        return "vmax";
    case CSS_UNKNOWN:
    case CSS_CUSTOM_IDENT:
    case CSS_STRING:
    case CSS_URI:
    case CSS_VALUE_ID:
    case CSS_PROPERTY_ID:
    case CSS_ATTR:
    case CSS_COUNTER:
    case CSS_RECT:
    case CSS_QUAD:
    case CSS_RGBCOLOR:
    case CSS_PAIR:
    case CSS_CALC:
    case CSS_SHAPE:
    case CSS_IDENT:
    case CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSS_QEM:
        break;
    };
    ASSERT_NOT_REACHED();
    return "";
}

String CSSPrimitiveValue::customCSSText() const
{
    if (m_hasCachedCSSText) {
        ASSERT(cssTextCache().contains(this));
        return cssTextCache().get(this);
    }

    String text;
    switch (m_primitiveUnitType) {
        case CSS_UNKNOWN:
            // FIXME
            break;
        case CSS_INTEGER:
            text = String::format("%d", getIntValue());
            break;
        case CSS_NUMBER:
        case CSS_PERCENTAGE:
        case CSS_EMS:
        case CSS_EXS:
        case CSS_REMS:
        case CSS_CHS:
        case CSS_PX:
        case CSS_CM:
        case CSS_DPPX:
        case CSS_DPI:
        case CSS_DPCM:
        case CSS_MM:
        case CSS_IN:
        case CSS_PT:
        case CSS_PC:
        case CSS_DEG:
        case CSS_RAD:
        case CSS_GRAD:
        case CSS_MS:
        case CSS_S:
        case CSS_HZ:
        case CSS_KHZ:
        case CSS_TURN:
        case CSS_FR:
        case CSS_VW:
        case CSS_VH:
        case CSS_VMIN:
        case CSS_VMAX:
            text = formatNumber(m_value.num, unitTypeToString((UnitType)m_primitiveUnitType));
            break;
        case CSS_CUSTOM_IDENT:
            text = quoteCSSStringIfNeeded(m_value.string);
            break;
        case CSS_STRING: {
            text = serializeString(m_value.string);
            break;
        }
        case CSS_URI:
            text = "url(" + quoteCSSURLIfNeeded(m_value.string) + ")";
            break;
        case CSS_VALUE_ID:
            text = valueName(m_value.valueID);
            break;
        case CSS_PROPERTY_ID:
            text = propertyName(m_value.propertyID);
            break;
        case CSS_ATTR: {
            StringBuilder result;
            result.reserveCapacity(6 + m_value.string->length());
            result.appendLiteral("attr(");
            result.append(m_value.string);
            result.append(')');

            text = result.toString();
            break;
        }
        case CSS_COUNTER: {
            StringBuilder result;
            String separator = m_value.counter->separator();
            if (separator.isEmpty())
                result.appendLiteral("counter(");
            else
                result.appendLiteral("counters(");

            result.append(m_value.counter->identifier());
            if (!separator.isEmpty()) {
                result.appendLiteral(", ");
                result.append(serializeString(separator));
            }
            String listStyle = m_value.counter->listStyle();
            bool isDefaultListStyle = m_value.counter->listStyleIdent() == CSSValueDecimal;
            if (!listStyle.isEmpty() && !isDefaultListStyle) {
                result.appendLiteral(", ");
                result.append(listStyle);
            }
            result.append(')');

            text = result.toString();
            break;
        }
        case CSS_RECT:
            text = getRectValue()->cssText();
            break;
        case CSS_QUAD:
            text = getQuadValue()->cssText();
            break;
        case CSS_RGBCOLOR: {
            text = Color(m_value.rgbcolor).serializedAsCSSComponentValue();
            break;
        }
        case CSS_PAIR:
            text = getPairValue()->cssText();
            break;
        case CSS_CALC:
            text = m_value.calc->cssText();
            break;
        case CSS_SHAPE:
            text = m_value.shape->cssText();
            break;
    }

    ASSERT(!cssTextCache().contains(this));
    cssTextCache().set(this, text);
    m_hasCachedCSSText = true;
    return text;
}

bool CSSPrimitiveValue::equals(const CSSPrimitiveValue& other) const
{
    if (m_primitiveUnitType != other.m_primitiveUnitType)
        return false;

    switch (m_primitiveUnitType) {
    case CSS_UNKNOWN:
        return false;
    case CSS_NUMBER:
    case CSS_PERCENTAGE:
    case CSS_EMS:
    case CSS_EXS:
    case CSS_REMS:
    case CSS_PX:
    case CSS_CM:
    case CSS_DPPX:
    case CSS_DPI:
    case CSS_DPCM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_DEG:
    case CSS_RAD:
    case CSS_GRAD:
    case CSS_MS:
    case CSS_S:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_TURN:
    case CSS_VW:
    case CSS_VH:
    case CSS_VMIN:
    case CSS_VMAX:
    case CSS_FR:
        return m_value.num == other.m_value.num;
    case CSS_PROPERTY_ID:
        return m_value.propertyID == other.m_value.propertyID;
    case CSS_VALUE_ID:
        return m_value.valueID == other.m_value.valueID;
    case CSS_CUSTOM_IDENT:
    case CSS_STRING:
    case CSS_URI:
    case CSS_ATTR:
        return equal(m_value.string, other.m_value.string);
    case CSS_COUNTER:
        return m_value.counter && other.m_value.counter && m_value.counter->equals(*other.m_value.counter);
    case CSS_RECT:
        return m_value.rect && other.m_value.rect && m_value.rect->equals(*other.m_value.rect);
    case CSS_QUAD:
        return m_value.quad && other.m_value.quad && m_value.quad->equals(*other.m_value.quad);
    case CSS_RGBCOLOR:
        return m_value.rgbcolor == other.m_value.rgbcolor;
    case CSS_PAIR:
        return m_value.pair && other.m_value.pair && m_value.pair->equals(*other.m_value.pair);
    case CSS_CALC:
        return m_value.calc && other.m_value.calc && m_value.calc->equals(*other.m_value.calc);
    case CSS_SHAPE:
        return m_value.shape && other.m_value.shape && m_value.shape->equals(*other.m_value.shape);
    }
    return false;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPrimitiveValue)
{
#if ENABLE(OILPAN)
    switch (m_primitiveUnitType) {
    case CSS_COUNTER:
        visitor->trace(m_value.counter);
        break;
    case CSS_RECT:
        visitor->trace(m_value.rect);
        break;
    case CSS_QUAD:
        visitor->trace(m_value.quad);
        break;
    case CSS_PAIR:
        visitor->trace(m_value.pair);
        break;
    case CSS_CALC:
        visitor->trace(m_value.calc);
        break;
    case CSS_SHAPE:
        visitor->trace(m_value.shape);
        break;
    default:
        break;
    }
#endif
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink
