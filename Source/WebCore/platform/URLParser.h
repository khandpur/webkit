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

#include "TextEncoding.h"
#include "URL.h"
#include <wtf/Forward.h>

namespace WebCore {

template<typename CharacterType> class CodePointIterator;

class URLParser {
public:
    WEBCORE_EXPORT URLParser(const String&, const URL& = { }, const TextEncoding& = UTF8Encoding());
    URL result() { return m_url; }

    WEBCORE_EXPORT static bool allValuesEqual(const URL&, const URL&);
    WEBCORE_EXPORT static bool internalValuesConsistent(const URL&);

    WEBCORE_EXPORT static bool enabled();
    WEBCORE_EXPORT static void setEnabled(bool);
    
    typedef Vector<std::pair<String, String>> URLEncodedForm;
    static URLEncodedForm parseURLEncodedForm(StringView);
    static String serialize(const URLEncodedForm&);

private:
    URL m_url;
    Vector<LChar> m_asciiBuffer;
    Vector<UChar> m_unicodeFragmentBuffer;
    bool m_didSeeUnicodeFragmentCodePoint { false };
    bool m_urlIsSpecial { false };
    bool m_hostHasPercentOrNonASCII { false };
    String m_inputString;
    const void* m_inputBegin { nullptr };

    bool m_didSeeSyntaxViolation { false };

    template<typename CharacterType> void parse(const CharacterType*, const unsigned length, const URL&, const TextEncoding&);
    template<typename CharacterType> void parseAuthority(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parseHostAndPort(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parsePort(CodePointIterator<CharacterType>&);

    void failure();
    enum class ReportSyntaxViolation { No, Yes };
    template<typename CharacterType, ReportSyntaxViolation reportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>& iterator) { advance<CharacterType, reportSyntaxViolation>(iterator, iterator); }
    template<typename CharacterType, ReportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>&, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    template<typename CharacterType> void syntaxViolation(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> void fragmentSyntaxViolation(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> bool isPercentEncodedDot(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isWindowsDriveLetter(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isSingleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isDoubleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool shouldCopyFileURL(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool checkLocalhostCodePoint(CodePointIterator<CharacterType>&, UChar32);
    template<typename CharacterType> bool isAtLocalhost(CodePointIterator<CharacterType>);
    bool isLocalhost(StringView);
    template<typename CharacterType> void consumeSingleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void consumeDoubleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void appendWindowsDriveLetter(CodePointIterator<CharacterType>&);
    template<typename CharacterType> size_t currentPosition(const CodePointIterator<CharacterType>&);
    template<typename UnsignedIntegerType> void appendNumberToASCIIBuffer(UnsignedIntegerType);
    template<bool(*isInCodeSet)(UChar32), typename CharacterType> void utf8PercentEncode(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> void utf8QueryEncode(const CodePointIterator<CharacterType>&);
    void percentEncodeByte(uint8_t);
    void appendToASCIIBuffer(UChar32);
    void appendToASCIIBuffer(const char*, size_t);
    void appendToASCIIBuffer(const LChar* characters, size_t size) { appendToASCIIBuffer(reinterpret_cast<const char*>(characters), size); }
    template<typename CharacterType> void encodeQuery(const Vector<UChar>& source, const TextEncoding&, CodePointIterator<CharacterType>);
    void copyASCIIStringUntil(const String&, size_t lengthIf8Bit, size_t lengthIf16Bit);
    StringView parsedDataView(size_t start, size_t length);

    using IPv4Address = uint32_t;
    void serializeIPv4(IPv4Address);
    template<typename CharacterType> Optional<IPv4Address> parseIPv4Host(CodePointIterator<CharacterType>);
    template<typename CharacterType> Optional<uint32_t> parseIPv4Number(CodePointIterator<CharacterType>&, bool& syntaxViolation);
    using IPv6Address = std::array<uint16_t, 8>;
    template<typename CharacterType> Optional<IPv6Address> parseIPv6Host(CodePointIterator<CharacterType>);
    void serializeIPv6Piece(uint16_t piece);
    void serializeIPv6(IPv6Address);

    enum class URLPart;
    template<typename CharacterType> void copyURLPartsUntil(const URL& base, URLPart, const CodePointIterator<CharacterType>&);
    static size_t urlLengthUntilPart(const URL&, URLPart);
    void popPath();
};

}
