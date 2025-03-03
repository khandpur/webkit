/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSNodeOrString.h"

#include "IDLTypes.h"
#include "JSDOMConvert.h"
#include "JSNode.h"
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/ThrowScope.h>

using namespace JSC;

namespace WebCore {

Vector<std::experimental::variant<Ref<Node>, String>> toNodeOrStringVector(ExecState& state)
{
    using NodeOrStringType = IDLUnion<IDLInterface<Node>, IDLDOMString>;
    using ConverterType = Converter<NodeOrStringType>;

    using InterfaceTypeList = typename ConverterType::InterfaceTypeList;
    using InterfaceType = brigand::front<InterfaceTypeList>;
    static_assert(std::is_same<InterfaceType, IDLInterface<Node>>::value, "");
    static_assert(brigand::size<InterfaceTypeList>::value == 1, "");

    using StringTypeList = typename ConverterType::StringTypeList;
    using StringType = typename ConverterType::StringType;
    static_assert(std::is_same<StringType, IDLDOMString>::value, "");
    static_assert(brigand::size<StringTypeList>::value == 1, "");

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t argumentCount = state.argumentCount();

    Vector<typename NodeOrStringType::ImplementationType> result;
    result.reserveInitialCapacity(argumentCount);

    for (size_t i = 0; i < argumentCount; ++i) {
         auto item = ConverterType::convert(state, state.uncheckedArgument(i));
         RETURN_IF_EXCEPTION(scope, { });
         result.uncheckedAppend(WTFMove(item));
    }

    return result;
}

} // namespace WebCore
