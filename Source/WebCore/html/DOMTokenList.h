/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Element.h"

namespace WebCore {

class DOMTokenList {
    WTF_MAKE_NONCOPYABLE(DOMTokenList); WTF_MAKE_FAST_ALLOCATED;
public:
    DOMTokenList(Element&, const QualifiedName& attributeName, WTF::Function<bool(StringView)>&& isSupportedToken = { });

    void associatedAttributeValueChanged(const AtomicString&);

    void ref() { m_element.ref(); }
    void deref() { m_element.deref(); }

    unsigned length() const;
    const AtomicString& item(unsigned index) const;

    WEBCORE_EXPORT bool contains(const AtomicString&) const;
    void add(const Vector<String>&, ExceptionCode&);
    void add(const AtomicString&, ExceptionCode&);
    void remove(const Vector<String>&, ExceptionCode&);
    void remove(const AtomicString&, ExceptionCode&);
    WEBCORE_EXPORT bool toggle(const AtomicString&, Optional<bool> force, ExceptionCode&);
    void replace(const AtomicString& token, const AtomicString& newToken, ExceptionCode&);
    bool supports(StringView token, ExceptionCode&);

    Element& element() const { return m_element; }

    WEBCORE_EXPORT void setValue(const String&);
    WEBCORE_EXPORT const AtomicString& value() const;

private:
    void updateTokensFromAttributeValue(const String&);
    void updateAssociatedAttributeFromTokens();

    WEBCORE_EXPORT Vector<AtomicString>& tokens();
    const Vector<AtomicString>& tokens() const { return const_cast<DOMTokenList&>(*this).tokens(); }

    static bool validateToken(const String&, ExceptionCode&);
    static bool validateTokens(const String* tokens, size_t length, ExceptionCode&);
    void addInternal(const String* tokens, size_t length, ExceptionCode&);
    void removeInternal(const String* tokens, size_t length, ExceptionCode&);

    Element& m_element;
    const WebCore::QualifiedName& m_attributeName;
    bool m_inUpdateAssociatedAttributeFromTokens { false };
    bool m_tokensNeedUpdating { true };
    Vector<AtomicString> m_tokens;
    WTF::Function<bool(StringView)> m_isSupportedToken;
};

inline unsigned DOMTokenList::length() const
{
    return tokens().size();
}

inline const AtomicString& DOMTokenList::item(unsigned index) const
{
    auto& tokens = this->tokens();
    return index < tokens.size() ? tokens[index] : nullAtom;
}

} // namespace WebCore
