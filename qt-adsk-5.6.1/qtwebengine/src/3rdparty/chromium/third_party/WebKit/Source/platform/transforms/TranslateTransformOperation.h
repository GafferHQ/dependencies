/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef TranslateTransformOperation_h
#define TranslateTransformOperation_h

#include "platform/Length.h"
#include "platform/LengthFunctions.h"
#include "platform/transforms/TransformOperation.h"

namespace blink {

class PLATFORM_EXPORT TranslateTransformOperation : public TransformOperation {
public:
    static PassRefPtr<TranslateTransformOperation> create(const Length& tx, const Length& ty, OperationType type)
    {
        return adoptRef(new TranslateTransformOperation(tx, ty, 0, type));
    }

    static PassRefPtr<TranslateTransformOperation> create(const Length& tx, const Length& ty, double tz, OperationType type)
    {
        return adoptRef(new TranslateTransformOperation(tx, ty, tz, type));
    }

    virtual bool canBlendWith(const TransformOperation& other) const;

    double x(const FloatSize& borderBoxSize) const { return floatValueForLength(m_x, borderBoxSize.width()); }
    double y(const FloatSize& borderBoxSize) const { return floatValueForLength(m_y, borderBoxSize.height()); }

    Length x() const { return m_x; }
    Length y() const { return m_y; }
    double z() const { return m_z; }

    void apply(TransformationMatrix& transform, const FloatSize& borderBoxSize) const override
    {
        transform.translate3d(x(borderBoxSize), y(borderBoxSize), z());
    }

    static bool isMatchingOperationType(OperationType type) { return type == Translate || type == TranslateX || type == TranslateY || type == TranslateZ || type == Translate3D; }

private:
    OperationType type() const override { return m_type; }

    bool operator==(const TransformOperation& o) const override
    {
        if (!isSameType(o))
            return false;
        const TranslateTransformOperation* t = static_cast<const TranslateTransformOperation*>(&o);
        return m_x == t->m_x && m_y == t->m_y && m_z == t->m_z;
    }

    PassRefPtr<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) override;

    bool dependsOnBoxSize() const override
    {
        return m_x.hasPercent() || m_y.hasPercent();
    }

    TranslateTransformOperation(const Length& tx, const Length& ty, double tz, OperationType type)
        : m_x(tx)
        , m_y(ty)
        , m_z(tz)
        , m_type(type)
    {
        ASSERT(isMatchingOperationType(type));
    }

    Length m_x;
    Length m_y;
    double m_z;
    OperationType m_type;
};

DEFINE_TRANSFORM_TYPE_CASTS(TranslateTransformOperation);

} // namespace blink

#endif // TranslateTransformOperation_h
