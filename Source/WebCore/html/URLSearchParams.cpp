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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "URLSearchParams.h"

#include "DOMURL.h"
#include "URLParser.h"

namespace WebCore {

URLSearchParams::URLSearchParams(const String& init, DOMURL* associatedURL)
    : m_associatedURL(associatedURL)
    , m_pairs(init.startsWith('?') ? URLParser::parseURLEncodedForm(StringView(init).substring(1)) : URLParser::parseURLEncodedForm(init))
{
}

URLSearchParams::URLSearchParams(const Vector<std::pair<String, String>>& pairs)
    : m_pairs(pairs)
{
}

String URLSearchParams::get(const String& name) const
{
    for (const auto& pair : m_pairs) {
        if (pair.first == name)
            return pair.second;
    }
    return String();
}

bool URLSearchParams::has(const String& name) const
{
    for (const auto& pair : m_pairs) {
        if (pair.first == name)
            return true;
    }
    return false;
}

void URLSearchParams::set(const String& name, const String& value)
{
    for (auto& pair : m_pairs) {
        if (pair.first != name)
            continue;
        if (pair.second != value)
            pair.second = value;
        bool skippedFirstMatch = false;
        m_pairs.removeAllMatching([&] (const auto& pair) {
            bool nameMatches = pair.first == name;
            if (nameMatches) {
                if (skippedFirstMatch)
                    return true;
                skippedFirstMatch = true;
            }
            return false;
        });
        updateURL();
        return;
    }
    m_pairs.append({name, value});
    updateURL();
}

void URLSearchParams::append(const String& name, const String& value)
{
    m_pairs.append({name, value});
    updateURL();
}

Vector<String> URLSearchParams::getAll(const String& name) const
{
    Vector<String> values;
    values.reserveInitialCapacity(m_pairs.size());
    for (const auto& pair : m_pairs) {
        if (pair.first == name)
            values.uncheckedAppend(pair.second);
    }
    return values;
}

void URLSearchParams::remove(const String& name)
{
    if (m_pairs.removeAllMatching([&] (const auto& pair) { return pair.first == name; }))
        updateURL();
}

String URLSearchParams::toString() const
{
    return URLParser::serialize(m_pairs);
}

void URLSearchParams::updateURL()
{
    if (m_associatedURL)
        m_associatedURL->setQuery(URLParser::serialize(m_pairs));
}

void URLSearchParams::updateFromAssociatedURL()
{
    ASSERT(m_associatedURL);
    String search = m_associatedURL->search();
    m_pairs = search.startsWith('?') ? URLParser::parseURLEncodedForm(StringView(search).substring(1)) : URLParser::parseURLEncodedForm(search);
}
    
}
