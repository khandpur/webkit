/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
#include "StyleRuleImport.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "Document.h"
#include "MediaList.h"
#include "SecurityOrigin.h"
#include "StyleSheetContents.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

Ref<StyleRuleImport> StyleRuleImport::create(const String& href, Ref<MediaQuerySet>&& media)
{
    return adoptRef(*new StyleRuleImport(href, WTFMove(media)));
}

StyleRuleImport::StyleRuleImport(const String& href, Ref<MediaQuerySet>&& media)
    : StyleRuleBase(Import, 0)
    , m_parentStyleSheet(0)
    , m_styleSheetClient(this)
    , m_strHref(href)
    , m_mediaQueries(WTFMove(media))
    , m_cachedSheet(0)
    , m_loading(false)
{
    if (!m_mediaQueries)
        m_mediaQueries = MediaQuerySet::create(String());
}

StyleRuleImport::~StyleRuleImport()
{
    if (m_styleSheet)
        m_styleSheet->clearOwnerRule();
    if (m_cachedSheet)
        m_cachedSheet->removeClient(&m_styleSheetClient);
}

void StyleRuleImport::setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* cachedStyleSheet)
{
    if (m_styleSheet)
        m_styleSheet->clearOwnerRule();

    CSSParserContext context = m_parentStyleSheet ? m_parentStyleSheet->parserContext() : HTMLStandardMode;
    context.charset = charset;
    if (!baseURL.isNull())
        context.baseURL = baseURL;

    Document* document = m_parentStyleSheet ? m_parentStyleSheet->singleOwnerDocument() : nullptr;
    m_styleSheet = StyleSheetContents::create(this, href, context);
    m_styleSheet->parseAuthorStyleSheet(cachedStyleSheet, document ? document->securityOrigin() : nullptr);

    m_loading = false;

    if (m_parentStyleSheet) {
        m_parentStyleSheet->notifyLoadedSheet(cachedStyleSheet);
        m_parentStyleSheet->checkLoaded();
    }
}

bool StyleRuleImport::isLoading() const
{
    return m_loading || (m_styleSheet && m_styleSheet->isLoading());
}

void StyleRuleImport::requestStyleSheet()
{
    if (!m_parentStyleSheet)
        return;
    Document* document = m_parentStyleSheet->singleOwnerDocument();
    if (!document)
        return;

    URL absURL;
    if (!m_parentStyleSheet->baseURL().isNull())
        // use parent styleheet's URL as the base URL
        absURL = URL(m_parentStyleSheet->baseURL(), m_strHref);
    else
        absURL = document->completeURL(m_strHref);

    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    StyleSheetContents* rootSheet = m_parentStyleSheet;
    for (StyleSheetContents* sheet = m_parentStyleSheet; sheet; sheet = sheet->parentStyleSheet()) {
        if (equalIgnoringFragmentIdentifier(absURL, sheet->baseURL())
            || equalIgnoringFragmentIdentifier(absURL, document->completeURL(sheet->originalURL())))
            return;
        rootSheet = sheet;
    }

    // FIXME: Skip Content Security Policy check when stylesheet is in a user agent shadow tree.
    // See <https://bugs.webkit.org/show_bug.cgi?id=146663>.
    CachedResourceRequest request(ResourceRequest(absURL), m_parentStyleSheet->charset());
    request.setInitiator(cachedResourceRequestInitiators().css);
    if (m_cachedSheet)
        m_cachedSheet->removeClient(&m_styleSheetClient);
    if (m_parentStyleSheet->isUserStyleSheet()) {
        request.setOptions(ResourceLoaderOptions(DoNotSendCallbacks, SniffContent, BufferData, AllowStoredCredentials, ClientCredentialPolicy::MayAskClientForCredentials, FetchOptions::Credentials::Include, SkipSecurityCheck, FetchOptions::Mode::NoCors, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::SkipPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, CachingPolicy::AllowCaching));
        m_cachedSheet = document->cachedResourceLoader().requestUserCSSStyleSheet(WTFMove(request));
    } else
        m_cachedSheet = document->cachedResourceLoader().requestCSSStyleSheet(WTFMove(request));
    if (m_cachedSheet) {
        // if the import rule is issued dynamically, the sheet may be
        // removed from the pending sheet count, so let the doc know
        // the sheet being imported is pending.
        if (m_parentStyleSheet && m_parentStyleSheet->loadCompleted() && rootSheet == m_parentStyleSheet)
            m_parentStyleSheet->startLoadingDynamicSheet();
        m_loading = true;
        m_cachedSheet->addClient(&m_styleSheetClient);
    }
}

} // namespace WebCore
