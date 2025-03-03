/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CachedResourceLoader_h
#define CachedResourceLoader_h

#include "CachePolicy.h"
#include "CachedResource.h"
#include "CachedResourceHandle.h"
#include "CachedResourceRequest.h"
#include "ResourceLoadPriority.h"
#include "ResourceTimingInformation.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CachedCSSStyleSheet;
class CachedSVGDocument;
class CachedFont;
class CachedImage;
class CachedRawResource;
class CachedScript;
class CachedTextTrack;
class CachedXSLStyleSheet;
class Document;
class DocumentLoader;
class Frame;
class ImageLoader;
class URL;

// The CachedResourceLoader provides a per-context interface to the MemoryCache
// and enforces a bunch of security checks and rules for resource revalidation.
// Its lifetime is roughly per-DocumentLoader, in that it is generally created
// in the DocumentLoader constructor and loses its ability to generate network
// requests when the DocumentLoader is destroyed. Documents also hold a 
// RefPtr<CachedResourceLoader> for their lifetime (and will create one if they
// are initialized without a Frame), so a Document can keep a CachedResourceLoader
// alive past detach if scripts still reference the Document.
class CachedResourceLoader : public RefCounted<CachedResourceLoader> {
    WTF_MAKE_NONCOPYABLE(CachedResourceLoader); WTF_MAKE_FAST_ALLOCATED;
friend class ImageLoader;
friend class ResourceCacheValidationSuppressor;

public:
    static Ref<CachedResourceLoader> create(DocumentLoader* documentLoader) { return adoptRef(*new CachedResourceLoader(documentLoader)); }
    ~CachedResourceLoader();

    CachedResourceHandle<CachedImage> requestImage(CachedResourceRequest&&);
    CachedResourceHandle<CachedCSSStyleSheet> requestCSSStyleSheet(CachedResourceRequest&&);
    CachedResourceHandle<CachedCSSStyleSheet> requestUserCSSStyleSheet(CachedResourceRequest&&);
    CachedResourceHandle<CachedScript> requestScript(CachedResourceRequest&&);
    CachedResourceHandle<CachedFont> requestFont(CachedResourceRequest&&, bool isSVG);
    CachedResourceHandle<CachedRawResource> requestMedia(CachedResourceRequest&&);
    CachedResourceHandle<CachedRawResource> requestRawResource(CachedResourceRequest&&);
    CachedResourceHandle<CachedRawResource> requestMainResource(CachedResourceRequest&&);
    CachedResourceHandle<CachedSVGDocument> requestSVGDocument(CachedResourceRequest&&);
#if ENABLE(XSLT)
    CachedResourceHandle<CachedXSLStyleSheet> requestXSLStyleSheet(CachedResourceRequest&&);
#endif
#if ENABLE(LINK_PREFETCH)
    CachedResourceHandle<CachedResource> requestLinkResource(CachedResource::Type, CachedResourceRequest&&);
#endif
#if ENABLE(VIDEO_TRACK)
    CachedResourceHandle<CachedTextTrack> requestTextTrack(CachedResourceRequest&&);
#endif

    // Logs an access denied message to the console for the specified URL.
    void printAccessDeniedMessage(const URL& url) const;

    CachedResource* cachedResource(const String& url) const;
    CachedResource* cachedResource(const URL& url) const;

    typedef HashMap<String, CachedResourceHandle<CachedResource>> DocumentResourceMap;
    const DocumentResourceMap& allCachedResources() const { return m_documentResources; }

    bool autoLoadImages() const { return m_autoLoadImages; }
    void setAutoLoadImages(bool);

    bool imagesEnabled() const { return m_imagesEnabled; }
    void setImagesEnabled(bool);

    bool shouldDeferImageLoad(const URL&) const;
    bool shouldPerformImageLoad(const URL&) const;
    
    CachePolicy cachePolicy(CachedResource::Type) const;
    
    Frame* frame() const; // Can be null
    Document* document() const { return m_document; } // Can be null
    void setDocument(Document* document) { m_document = document; }
    void clearDocumentLoader() { m_documentLoader = nullptr; }
    SessionID sessionID() const;

    void removeCachedResource(CachedResource&);

    void loadDone(CachedResource*, bool shouldPerformPostLoadActions = true);

    WEBCORE_EXPORT void garbageCollectDocumentResources();
    
    void incrementRequestCount(const CachedResource&);
    void decrementRequestCount(const CachedResource&);
    int requestCount() const { return m_requestCount; }

    WEBCORE_EXPORT bool isPreloaded(const String& urlString) const;
    void clearPreloads();
    void clearPendingPreloads();
    enum PreloadType { ImplicitPreload, ExplicitPreload };
    CachedResourceHandle<CachedResource> preload(CachedResource::Type, CachedResourceRequest&&, PreloadType);
    void checkForPendingPreloads();
    void printPreloadStats();

    bool canRequest(CachedResource::Type, const URL&, const ResourceLoaderOptions&, bool forPreload = false, bool didReceiveRedirectResponse = false);

    static const ResourceLoaderOptions& defaultCachedResourceOptions();

    void documentDidFinishLoadEvent();

#if ENABLE(WEB_TIMING)
    ResourceTimingInformation& resourceTimingInformation() { return m_resourceTimingInfo; }
#endif

    bool isAlwaysOnLoggingAllowed() const;

private:
    explicit CachedResourceLoader(DocumentLoader*);

    CachedResourceHandle<CachedResource> requestResource(CachedResource::Type, CachedResourceRequest&&);
    void prepareFetch(CachedResource::Type, CachedResourceRequest&);
    CachedResourceHandle<CachedResource> revalidateResource(CachedResourceRequest&&, CachedResource&);
    CachedResourceHandle<CachedResource> loadResource(CachedResource::Type, CachedResourceRequest&&);
    CachedResourceHandle<CachedResource> requestPreload(CachedResource::Type, CachedResourceRequest&&);

    enum RevalidationPolicy { Use, Revalidate, Reload, Load };
    RevalidationPolicy determineRevalidationPolicy(CachedResource::Type, CachedResourceRequest&, CachedResource* existingResource) const;

    bool shouldUpdateCachedResourceWithCurrentRequest(const CachedResource&, const CachedResourceRequest&);
    CachedResourceHandle<CachedResource> updateCachedResourceWithCurrentRequest(const CachedResource&, CachedResourceRequest&&);

    bool shouldContinueAfterNotifyingLoadedFromMemoryCache(const CachedResourceRequest&, CachedResource*);
    bool checkInsecureContent(CachedResource::Type, const URL&) const;
    bool allowedByContentSecurityPolicy(CachedResource::Type, const URL&, const ResourceLoaderOptions&, bool);

    void performPostLoadActions();

    bool clientDefersImage(const URL&) const;
    void reloadImagesIfNotDeferred();

    bool canRequestInContentDispositionAttachmentSandbox(CachedResource::Type, const URL&) const;
    
    HashSet<String> m_validatedURLs;
    mutable DocumentResourceMap m_documentResources;
    Document* m_document;
    DocumentLoader* m_documentLoader;
    
    int m_requestCount;
    
    std::unique_ptr<ListHashSet<CachedResource*>> m_preloads;
    struct PendingPreload {
        CachedResource::Type m_type;
        CachedResourceRequest m_request;
    };
    Deque<PendingPreload> m_pendingPreloads;

    Timer m_garbageCollectDocumentResourcesTimer;

#if ENABLE(WEB_TIMING)
    ResourceTimingInformation m_resourceTimingInfo;
#endif

    // 29 bits left
    bool m_autoLoadImages : 1;
    bool m_imagesEnabled : 1;
    bool m_allowStaleResources : 1;
};

class ResourceCacheValidationSuppressor {
    WTF_MAKE_NONCOPYABLE(ResourceCacheValidationSuppressor);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ResourceCacheValidationSuppressor(CachedResourceLoader& loader)
        : m_loader(loader)
        , m_previousState(m_loader.m_allowStaleResources)
    {
        m_loader.m_allowStaleResources = true;
    }
    ~ResourceCacheValidationSuppressor()
    {
        m_loader.m_allowStaleResources = m_previousState;
    }
private:
    CachedResourceLoader& m_loader;
    bool m_previousState;
};

} // namespace WebCore

#endif
