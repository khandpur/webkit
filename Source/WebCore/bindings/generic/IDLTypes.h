/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Brigand.h>
#include <wtf/StdLibExtras.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

template <typename Value> class DOMPromise;

template<typename T>
struct IDLType {
    using ImplementationType = T;
    using NullableType = Optional<ImplementationType>;
};

struct IDLDummy;

// IDLUnsupportedType is a special type that serves as a base class for currently unsupported types.
struct IDLUnsupportedType : IDLType<void> { };

// IDLNull is a special type for use as a subtype in an IDLUnion that is nullable.
struct IDLNull : IDLType<std::nullptr_t> { };

struct IDLAny : IDLType<JSC::JSValue> { };

struct IDLBoolean : IDLType<bool> { };

template<typename NumericType> struct IDLNumber : IDLType<NumericType> { };

template<typename IntegerType> struct IDLInteger : IDLNumber<IntegerType> { };
struct IDLByte : IDLInteger<int8_t> { };
struct IDLOctet : IDLInteger<uint8_t> { };
struct IDLShort : IDLInteger<int16_t> { };
struct IDLUnsignedShort : IDLInteger<uint16_t> { };
struct IDLLong : IDLInteger<int32_t> { };
struct IDLUnsignedLong : IDLInteger<uint32_t> { };
struct IDLLongLong : IDLInteger<int64_t> { };
struct IDLUnsignedLongLong : IDLInteger<uint64_t> { };

template<typename FloatingPointType, bool unrestricted = false> struct IDLFloatingPoint : IDLNumber<FloatingPointType> {
    static constexpr bool isUnrestricted = unrestricted;
};
struct IDLFloat : IDLFloatingPoint<float> { };
struct IDLUnrestrictedFloat : IDLFloatingPoint<float, true> { };
struct IDLDouble : IDLFloatingPoint<double> { };
struct IDLUnrestrictedDouble : IDLFloatingPoint<double, true> { };

struct IDLString : IDLType<String> {
    using NullableType = String;
};
struct IDLDOMString : IDLString { };
struct IDLByteString : IDLUnsupportedType { };
struct IDLUSVString : IDLString { };

struct IDLObject : IDLUnsupportedType { };

template<typename T> struct IDLInterface : IDLType<Ref<T>> {
    using RawType = T;
    using NullableType = RefPtr<T>;
};

template<typename T> struct IDLDictionary : IDLType<T> { };
template<typename T> struct IDLEnumeration : IDLType<T> { };
template<typename T> struct IDLCallbackFunction : IDLUnsupportedType { };

template<typename T> struct IDLNullable : IDLType<typename T::NullableType> {
    using InnerType = T;
};

template<typename T> struct IDLSequence : IDLType<Vector<typename T::ImplementationType>> {
    using InnerType = T;
};

template<typename T> struct IDLFrozenArray : IDLType<Vector<typename T::ImplementationType>> {
    using InnerType = T;
};

template<typename T> struct IDLPromise : IDLType<DOMPromise<T>> {
    using InnerType = T;
};

struct IDLRegExp : IDLUnsupportedType { };
struct IDLError : IDLUnsupportedType { };
struct IDLDOMException : IDLUnsupportedType { };

template<typename... Ts>
struct IDLUnion : IDLType<std::experimental::variant<typename Ts::ImplementationType...>> {
    using TypeList = brigand::list<Ts...>;
};


// Helper predicates

template <typename T>
struct IsIDLInterface : public std::integral_constant<bool, WTF::IsTemplate<T, IDLInterface>::value> { };

template <typename T>
struct IsIDLDictionary : public std::integral_constant<bool, WTF::IsTemplate<T, IDLDictionary>::value> { };

template <typename T>
struct IsIDLNumber : public std::integral_constant<bool, WTF::IsBaseOfTemplate<IDLNumber, T>::value> { };

} // namespace WebCore
