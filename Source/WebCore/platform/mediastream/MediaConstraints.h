/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaConstraints_h
#define MediaConstraints_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSupportedConstraints.h"
#include <cstdlib>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MediaConstraint {
public:
    enum class DataType { None, Integer, Double, Boolean, String };

    virtual ~MediaConstraint() { };

    virtual bool isEmpty() const { return true; }
    virtual bool isMandatory() const { return false; }
    virtual void merge(const MediaConstraint&) { }

    bool isInt() const { return m_dataType == DataType::Integer; }
    bool isDouble() const { return m_dataType == DataType::Double; }
    bool isBoolean() const { return m_dataType == DataType::Boolean; }
    bool isString() const { return m_dataType == DataType::String; }

    DataType dataType() const { return m_dataType; }
    MediaConstraintType constraintType() const { return m_constraintType; }
    const String& name() const { return m_name; }

protected:
    explicit MediaConstraint(const AtomicString& name, MediaConstraintType constraintType, DataType dataType)
        : m_name(name)
        , m_constraintType(constraintType)
        , m_dataType(dataType)
    {
    }


private:
    AtomicString m_name;
    MediaConstraintType m_constraintType;
    DataType m_dataType;
};

template<class ValueType>
class NumericConstraint : public MediaConstraint {
public:
    void setMin(ValueType value) { m_min = value; }
    void setMax(ValueType value) { m_max = value; }
    void setExact(ValueType value) { m_exact = value; }
    void setIdeal(ValueType value) { m_ideal = value; }

    bool getMin(ValueType& min) const
    {
        if (!m_min)
            return false;

        min = m_min.value();
        return true;
    }

    bool getMax(ValueType& max) const
    {
        if (!m_max)
            return false;

        max = m_max.value();
        return true;
    }

    bool getExact(ValueType& exact) const
    {
        if (!m_exact)
            return false;

        exact = m_exact.value();
        return true;
    }

    bool getIdeal(ValueType& ideal) const
    {
        if (!m_ideal)
            return false;

        ideal = m_ideal.value();
        return true;
    }

    bool nearlyEqual(double a, double b) const
    {
        // Don't require strict equality when comparing constraints, or many floating point constraint values,
        // e.g. "aspectRatio: 1.333", will never match.
        const double epsilon = 0.00001;
        return std::abs(a - b) <= epsilon;
    }

    double fitnessDistance(ValueType rangeMin, ValueType rangeMax) const
    {
        // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints
        // 1. If the constraint is not supported by the browser, the fitness distance is 0.
        if (isEmpty())
            return 0;

        // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
        //    dictionary's value for the constraint does not satisfy the constraint, the
        //    fitness distance is positive infinity.
        bool valid = validForRange(rangeMin, rangeMax);
        if (m_exact && !valid)
            return std::numeric_limits<double>::infinity();

        if (m_min && !valid)
            return std::numeric_limits<double>::infinity();

        if (m_max && !valid)
            return std::numeric_limits<double>::infinity();

        // 3. If no ideal value is specified, the fitness distance is 0.
        if (!m_ideal)
            return 0;

        // 4. For all positive numeric non-required constraints (such as height, width, frameRate,
        //    aspectRatio, sampleRate and sampleSize), the fitness distance is the result of the formula
        //
        //         (actual == ideal) ? 0 : |actual - ideal| / max(|actual|,|ideal|)
        ValueType ideal = m_ideal.value();
        if (ideal >= rangeMin && ideal <= rangeMax)
            return 0;

        ideal = ideal > std::max(rangeMin, rangeMax) ? rangeMax : rangeMin;
        return static_cast<double>(std::abs(ideal - m_ideal.value())) / std::max(std::abs(ideal), std::abs(m_ideal.value()));
    }

    bool validForRange(ValueType rangeMin, ValueType rangeMax) const
    {
        if (isEmpty())
            return false;

        if (m_exact) {
            const ValueType exact = m_exact.value();
            if (exact < rangeMin && !nearlyEqual(exact, rangeMin))
                return false;
            if (exact > rangeMax && !nearlyEqual(exact, rangeMax))
                return false;
        }

        if (m_min) {
            const ValueType constraintMin = m_min.value();
            if (constraintMin > rangeMax && !nearlyEqual(constraintMin, rangeMax))
                return false;
        }


        if (m_max) {
            const ValueType constraintMax = m_max.value();
            if (constraintMax < rangeMin && !nearlyEqual(constraintMax, rangeMin))
                return false;
        }

        return true;
    }

    ValueType find(std::function<bool(ValueType)> function) const
    {
        if (m_min && function(m_min.value()))
            return m_min.value();

        if (m_max && function(m_max.value()))
            return m_max.value();

        if (m_exact && function(m_exact.value()))
            return m_exact.value();

        if (m_ideal && function(m_ideal.value()))
            return m_ideal.value();

        return 0;
    }

    bool isEmpty() const override { return !m_min && !m_max && !m_exact && !m_ideal; }
    bool isMandatory() const override { return m_min || m_max || m_exact; }

protected:
    explicit NumericConstraint(const AtomicString& name, MediaConstraintType type, DataType dataType)
        : MediaConstraint(name, type, dataType)
    {
    }

    void innerMerge(const NumericConstraint& other)
    {
        if (other.isEmpty())
            return;

        ValueType value;
        if (other.getExact(value))
            m_exact = value;

        if (other.getMin(value))
            m_min = value;

        if (other.getMax(value))
            m_max = value;

        // https://w3c.github.io/mediacapture-main/#constrainable-interface
        // When processing advanced constraints:
        //   ... the User Agent must attempt to apply, individually, any 'ideal' constraints or
        //   a constraint given as a bare value for the property. Of these properties, it must
        //   satisfy the largest number that it can, in any order.
        if (other.getIdeal(value)) {
            if (!m_ideal || value > m_ideal.value())
                m_ideal = value;
        }
    }

    Optional<ValueType> m_min;
    Optional<ValueType> m_max;
    Optional<ValueType> m_exact;
    Optional<ValueType> m_ideal;
};

class IntConstraint final : public NumericConstraint<int> {
public:
    explicit IntConstraint(const AtomicString& name, MediaConstraintType type)
        : NumericConstraint<int>(name, type, DataType::Integer)
    {
    }

    void merge(const MediaConstraint& other) final {
        ASSERT(other.isInt());
        NumericConstraint::innerMerge(downcast<const IntConstraint>(other));
    }
};

class DoubleConstraint final : public NumericConstraint<double> {
public:
    explicit DoubleConstraint(const AtomicString& name, MediaConstraintType type)
        : NumericConstraint<double>(name, type, DataType::Double)
    {
    }

    void merge(const MediaConstraint& other) final {
        ASSERT(other.isDouble());
        NumericConstraint::innerMerge(downcast<DoubleConstraint>(other));
    }
};

class BooleanConstraint final : public MediaConstraint {
public:
    explicit BooleanConstraint(const AtomicString& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::Boolean)
    {
    }

    void setExact(bool value) { m_exact = value; }
    void setIdeal(bool value) { m_ideal = value; }

    bool getExact(bool&) const;
    bool getIdeal(bool&) const;

    double fitnessDistance(bool value) const
    {
        // https://w3c.github.io/mediacapture-main/#dfn-applyconstraints
        // 1. If the constraint is not supported by the browser, the fitness distance is 0.
        if (isEmpty())
            return 0;

        // 2. If the constraint is required ('min', 'max', or 'exact'), and the settings
        //    dictionary's value for the constraint does not satisfy the constraint, the
        //    fitness distance is positive infinity.
        if (m_exact && value != m_exact.value())
            return std::numeric_limits<double>::infinity();

        // 3. If no ideal value is specified, the fitness distance is 0.
        if (!m_ideal || m_ideal.value() == value)
            return 0;

        // 5. For all string and enum non-required constraints (deviceId, groupId, facingMode,
        // echoCancellation), the fitness distance is the result of the formula
        //        (actual == ideal) ? 0 : 1
        return 1;
    }

    void merge(const MediaConstraint& other) final {
        ASSERT(other.isBoolean());
        const BooleanConstraint& typedOther = downcast<BooleanConstraint>(other);

        if (typedOther.isEmpty())
            return;

        bool value;
        if (typedOther.getExact(value))
            m_exact = value;

        if (typedOther.getIdeal(value)) {
            if (!m_ideal || (value && !m_ideal.value()))
                m_ideal = value;
        }
    }

    bool isEmpty() const final { return !m_exact && !m_ideal; };
    bool isMandatory() const final { return bool(m_exact); }

private:
    Optional<bool> m_exact;
    Optional<bool> m_ideal;
};

class StringConstraint final : public MediaConstraint {
public:
    explicit StringConstraint(const AtomicString& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::String)
    {
    }

    void setExact(const String&);
    void appendExact(const String&);
    void setIdeal(const String&);
    void appendIdeal(const String&);

    bool getExact(Vector<String>&) const;
    bool getIdeal(Vector<String>&) const;

    double fitnessDistance(const String&) const;
    double fitnessDistance(const Vector<String>&) const;

    const String& find(std::function<bool(const String&)>) const;
    void merge(const MediaConstraint&) final;

    bool isEmpty() const final { return m_exact.isEmpty() && m_ideal.isEmpty(); }
    bool isMandatory() const final { return !m_exact.isEmpty(); }

private:
    Vector<String> m_exact;
    Vector<String> m_ideal;
};

class UnknownConstraint final : public MediaConstraint {
public:
    explicit UnknownConstraint(const AtomicString& name, MediaConstraintType type)
        : MediaConstraint(name, type, DataType::None)
    {
    }

private:
    bool isEmpty() const final { return true; }
    bool isMandatory() const final { return false; }
    void merge(const MediaConstraint&) final { }
};

class MediaTrackConstraintSetMap {
public:
    void forEach(std::function<void(const MediaConstraint&)>) const;
    void filter(std::function<bool(const MediaConstraint&)>) const;
    bool isEmpty() const;

    void set(MediaConstraintType, Optional<IntConstraint>&&);
    void set(MediaConstraintType, Optional<DoubleConstraint>&&);
    void set(MediaConstraintType, Optional<BooleanConstraint>&&);
    void set(MediaConstraintType, Optional<StringConstraint>&&);

private:
    Optional<IntConstraint> m_width;
    Optional<IntConstraint> m_height;
    Optional<IntConstraint> m_sampleRate;
    Optional<IntConstraint> m_sampleSize;

    Optional<DoubleConstraint> m_aspectRatio;
    Optional<DoubleConstraint> m_frameRate;
    Optional<DoubleConstraint> m_volume;

    Optional<BooleanConstraint> m_echoCancellation;

    Optional<StringConstraint> m_facingMode;
    Optional<StringConstraint> m_deviceId;
    Optional<StringConstraint> m_groupId;
};

class FlattenedConstraint {
public:

    void set(const MediaConstraint&);
    void merge(const MediaConstraint&);
    void append(const MediaConstraint&);
    bool isEmpty() const { return m_variants.isEmpty(); }

    class iterator {
    public:
        iterator(const FlattenedConstraint* constraint, size_t index)
            : m_constraint(constraint)
            , m_index(index)
#ifndef NDEBUG
            , m_generation(constraint->m_generation)
#endif
        {
        }

        MediaConstraint& operator*() const
        {
            return m_constraint->m_variants.at(m_index).constraint();
        }

        iterator& operator++()
        {
#ifndef NDEBUG
            ASSERT(m_generation == m_constraint->m_generation);
#endif
            m_index++;
            return *this;
        }

        bool operator==(const iterator& other) const { return m_index == other.m_index; }
        bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        const FlattenedConstraint* m_constraint { nullptr };
        size_t m_index { 0 };
#ifndef NDEBUG
        int m_generation { 0 };
#endif
    };

    const iterator begin() const { return iterator(this, 0); }
    const iterator end() const { return iterator(this, m_variants.size()); }

private:
    class ConstraintHolder {
    public:
        static ConstraintHolder& create(const MediaConstraint& value) { return *new ConstraintHolder(value); }

        ~ConstraintHolder()
        {
            switch (dataType()) {
            case MediaConstraint::DataType::Integer:
                delete m_value.asInteger;
                break;
            case MediaConstraint::DataType::Double:
                delete m_value.asDouble;
                break;
            case MediaConstraint::DataType::Boolean:
                delete m_value.asBoolean;
                break;
            case MediaConstraint::DataType::String:
                delete m_value.asString;
                break;
            case MediaConstraint::DataType::None:
                ASSERT_NOT_REACHED();
                break;
            }

#ifndef NDEBUG
            m_value.asRaw = reinterpret_cast<MediaConstraint*>(0xbbadbeef);
#endif
        }

        MediaConstraint& constraint() const { return *m_value.asRaw; }
        MediaConstraint::DataType dataType() const { return constraint().dataType(); }
        MediaConstraintType constraintType() const { return constraint().constraintType(); }

    private:
        explicit ConstraintHolder(const MediaConstraint& value)
        {
            switch (value.dataType()) {
            case MediaConstraint::DataType::Integer:
                m_value.asInteger = new IntConstraint(downcast<const IntConstraint>(value));
                break;
            case MediaConstraint::DataType::Double:
                m_value.asDouble = new DoubleConstraint(downcast<DoubleConstraint>(value));
                break;
            case MediaConstraint::DataType::Boolean:
                m_value.asBoolean = new BooleanConstraint(downcast<BooleanConstraint>(value));
                break;
            case MediaConstraint::DataType::String:
                m_value.asString = new StringConstraint(downcast<StringConstraint>(value));
                break;
            case MediaConstraint::DataType::None:
                ASSERT_NOT_REACHED();
                break;
            }
        }
        
        union {
            MediaConstraint* asRaw;
            IntConstraint* asInteger;
            DoubleConstraint* asDouble;
            BooleanConstraint* asBoolean;
            StringConstraint* asString;
        } m_value;
    };

    Vector<ConstraintHolder> m_variants;
#ifndef NDEBUG
    int m_generation { 0 };
#endif
};

class MediaConstraints : public RefCounted<MediaConstraints> {
public:
    virtual ~MediaConstraints() { }

    virtual const MediaTrackConstraintSetMap& mandatoryConstraints() const = 0;
    virtual const Vector<MediaTrackConstraintSetMap>& advancedConstraints() const = 0;
    virtual bool isValid() const = 0;

protected:
    MediaConstraints() { }
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(ConstraintType, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ConstraintType) \
static bool isType(const WebCore::MediaConstraint& constraint) { return constraint.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(IntConstraint, isInt())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(DoubleConstraint, isDouble())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(StringConstraint, isString())
SPECIALIZE_TYPE_TRAITS_MEDIACONSTRAINT(BooleanConstraint, isBoolean())

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaConstraints_h
