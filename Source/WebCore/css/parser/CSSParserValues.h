/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelector.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSValue;
class QualifiedName;

// This should be a StringView but currently it can't because it's used as an element of a union in CSSParserValue.
struct CSSParserString {
    void init(LChar* characters, unsigned length)
    {
        m_data.characters8 = characters;
        m_length = length;
        m_is8Bit = true;
    }

    void init(UChar* characters, unsigned length)
    {
        m_data.characters16 = characters;
        m_length = length;
        m_is8Bit = false;
    }

    void init(const String& string)
    {
        m_length = string.length();
        if (!m_length || string.is8Bit()) {
            m_data.characters8 = const_cast<LChar*>(string.characters8());
            m_is8Bit = true;
        } else {
            m_data.characters16 = const_cast<UChar*>(string.characters16());
            m_is8Bit = false;
        }
    }

    void clear()
    {
        m_data.characters8 = 0;
        m_length = 0;
        m_is8Bit = true;
    }

    bool is8Bit() const { return m_is8Bit; }
    LChar* characters8() const { ASSERT(is8Bit()); return m_data.characters8; }
    UChar* characters16() const { ASSERT(!is8Bit()); return m_data.characters16; }
    template<typename CharacterType> CharacterType* characters() const;

    unsigned length() const { return m_length; }
    void setLength(unsigned length) { m_length = length; }

    void convertToASCIILowercaseInPlace();

    UChar operator[](unsigned i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(i < m_length);
        if (is8Bit())
            return m_data.characters8[i];
        return m_data.characters16[i];
    }

    operator String() const { return is8Bit() ? String(m_data.characters8, m_length) : String(m_data.characters16, m_length); }
    operator AtomicString() const { return is8Bit() ? AtomicString(m_data.characters8, m_length) : AtomicString(m_data.characters16, m_length); }
    StringView toStringView() const { return is8Bit() ? StringView(m_data.characters8, m_length) : StringView(m_data.characters16, m_length); }

    union {
        LChar* characters8;
        UChar* characters16;
    } m_data;
    unsigned m_length;
    bool m_is8Bit;
};

template<unsigned length> bool equalLettersIgnoringASCIICase(const CSSParserString&, const char (&lowercaseLetters)[length]);

struct CSSParserFunction;
struct CSSParserVariable;

struct CSSParserValue {
    CSSValueID id;
    bool isInt;
    union {
        double fValue;
        int iValue;
        CSSParserString string;
        CSSParserFunction* function;
        CSSParserVariable* variable;
        CSSParserValueList* valueList;
    };
    enum {
        Operator  = 0x100000,
        Function  = 0x100001,
        ValueList = 0x100002,
        Q_EMS     = 0x100003,
        Variable  = 0x100004
    };
    int unit;

    void setFromValueList(std::unique_ptr<CSSParserValueList>);

    RefPtr<CSSValue> createCSSValue();
};

void destroy(const CSSParserValue&);

class CSSParserValueList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserValueList()
        : m_current(0)
    {
    }
    ~CSSParserValueList();

    void addValue(const CSSParserValue&);
    void insertValueAt(unsigned, const CSSParserValue&);
    void extend(CSSParserValueList&);

    unsigned size() const { return m_values.size(); }
    unsigned currentIndex() { return m_current; }
    CSSParserValue* current() { return m_current < m_values.size() ? &m_values[m_current] : 0; }
    CSSParserValue* next() { ++m_current; return current(); }
    CSSParserValue* previous()
    {
        if (!m_current)
            return 0;
        --m_current;
        return current();
    }
    void setCurrentIndex(unsigned index)
    {
        ASSERT(index < m_values.size());
        if (index < m_values.size())
            m_current = index;
    }

    CSSParserValue* valueAt(unsigned i) { return i < m_values.size() ? &m_values[i] : 0; }

    void clear() { m_values.clear(); }
    
    String toString();
    
    bool containsVariables() const;

private:
    unsigned m_current;
    Vector<CSSParserValue, 4> m_values;
};

struct CSSParserFunction {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserString name;
    std::unique_ptr<CSSParserValueList> args;
};

struct CSSParserVariable {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSParserString name; // The custom property name
    std::unique_ptr<CSSParserValueList> args; // The fallback args
};

enum class CSSParserSelectorCombinator {
    Child,
    DescendantSpace,
#if ENABLE(CSS_SELECTORS_LEVEL4)
    DescendantDoubleChild,
#endif
    DirectAdjacent,
    IndirectAdjacent,
    ShadowPseudo, // Special case of shadow DOM pseudo elements / shadow pseudo element
    ShadowDeep, // /deep/ combinator
    ShadowSlot // slotted to <slot> e
};

class CSSParserSelector {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME-NEWPARSER: Remove the CSSParserString-based parsing functions once the old parser is gone.
    static CSSParserSelector* parsePagePseudoSelector(const CSSParserString& pseudoTypeString);
    static CSSParserSelector* parsePseudoElementSelector(CSSParserString& pseudoTypeString);
    static CSSParserSelector* parsePseudoElementCueFunctionSelector(const CSSParserString& functionIdentifier, Vector<std::unique_ptr<CSSParserSelector>>*);
    static CSSParserSelector* parsePseudoElementSlottedFunctionSelector(const CSSParserString& functionIdentifier, CSSParserSelector*);
    static CSSParserSelector* parsePseudoClassHostFunctionSelector(const CSSParserString& functionIdentifier, CSSParserSelector*);
    static CSSParserSelector* parsePseudoClassAndCompatibilityElementSelector(CSSParserString& pseudoTypeString);

    static CSSParserSelector* parsePseudoClassSelectorFromStringView(StringView&);
    static CSSParserSelector* parsePseudoElementSelectorFromStringView(StringView&);
    static CSSParserSelector* parsePagePseudoSelector(const AtomicString&);
    
    CSSParserSelector();
    explicit CSSParserSelector(const QualifiedName&);
    ~CSSParserSelector();

    std::unique_ptr<CSSSelector> releaseSelector() { return WTFMove(m_selector); }

    void setValue(const AtomicString& value) { m_selector->setValue(value); }
    void setAttribute(const QualifiedName& value, bool isCaseInsensitive) { m_selector->setAttribute(value, isCaseInsensitive); }
    void setArgument(const AtomicString& value) { m_selector->setArgument(value); }
    void setAttributeValueMatchingIsCaseInsensitive(bool isCaseInsensitive) { m_selector->setAttributeValueMatchingIsCaseInsensitive(isCaseInsensitive); }
    void setMatch(CSSSelector::Match value) { m_selector->setMatch(value); }
    void setRelation(CSSSelector::Relation value) { m_selector->setRelation(value); }
    void setForPage() { m_selector->setForPage(); }

    CSSSelector::Match match() const { return m_selector->match(); }
    CSSSelector::PseudoElementType pseudoElementType() const { return m_selector->pseudoElementType(); }
    const CSSSelectorList* selectorList() const { return m_selector->selectorList(); }

    void adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>& selectorVector);
    void setLangArgumentList(const Vector<CSSParserString>& stringVector);
    void setSelectorList(std::unique_ptr<CSSSelectorList>);

    void setPseudoClassValue(const CSSParserString& pseudoClassString);
    CSSSelector::PseudoClassType pseudoClassType() const { return m_selector->pseudoClassType(); }
    bool isCustomPseudoElement() const { return m_selector->isCustomPseudoElement(); }

    bool isPseudoElementCueFunction() const
    {
#if ENABLE(VIDEO_TRACK)
        return m_selector->match() == CSSSelector::PseudoElement && m_selector->pseudoElementType() == CSSSelector::PseudoElementCue;
#else
        return false;
#endif
    }

    bool hasShadowDescendant() const;
    bool matchesPseudoElement() const;

    bool isHostPseudoSelector() const;

    // FIXME-NEWPARSER: Missing "shadow"
    bool needsImplicitShadowCombinatorForMatching() const { return pseudoElementType() == CSSSelector::PseudoElementWebKitCustom || pseudoElementType() == CSSSelector::PseudoElementUserAgentCustom || pseudoElementType() == CSSSelector::PseudoElementWebKitCustomLegacyPrefixed || pseudoElementType() == CSSSelector::PseudoElementCue || pseudoElementType() == CSSSelector::PseudoElementSlotted; }

    CSSParserSelector* tagHistory() const { return m_tagHistory.get(); }
    void setTagHistory(std::unique_ptr<CSSParserSelector> selector) { m_tagHistory = WTFMove(selector); }
    void clearTagHistory() { m_tagHistory.reset(); }
    void insertTagHistory(CSSSelector::Relation before, std::unique_ptr<CSSParserSelector>, CSSSelector::Relation after);
    void appendTagHistory(CSSSelector::Relation, std::unique_ptr<CSSParserSelector>);
    void appendTagHistory(CSSParserSelectorCombinator, std::unique_ptr<CSSParserSelector>);
    void prependTagSelector(const QualifiedName&, bool tagIsForNamespaceRule = false);
    std::unique_ptr<CSSParserSelector> releaseTagHistory();

private:
#if ENABLE(CSS_SELECTORS_LEVEL4)
    void setDescendantUseDoubleChildSyntax() { m_selector->setDescendantUseDoubleChildSyntax(); }
#endif

    std::unique_ptr<CSSSelector> m_selector;
    std::unique_ptr<CSSParserSelector> m_tagHistory;
};

inline bool CSSParserSelector::hasShadowDescendant() const
{
    return m_selector->relation() == CSSSelector::ShadowDescendant;
}

inline void CSSParserValue::setFromValueList(std::unique_ptr<CSSParserValueList> valueList)
{
    id = CSSValueInvalid;
    this->valueList = valueList.release();
    unit = ValueList;
}

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const CSSParserString& string, const char (&lowercaseLetters)[length])
{
    return WTF::equalLettersIgnoringASCIICaseCommon(string, lowercaseLetters);
}

}
