/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#import "WebFrameNetworkingContext.h"

#import "WebFrameInternal.h"
#import "WebViewPrivate.h"
#import <WebCore/CFNetworkSPI.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/Page.h>
#import <WebCore/ResourceError.h>
#import <WebCore/SessionID.h>
#import <WebCore/Settings.h>
#import <wtf/NeverDestroyed.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThread.h>
#import <WebKitLegacy/WebFrameLoadDelegate.h>
#endif

using namespace WebCore;

NetworkStorageSession& WebFrameNetworkingContext::ensurePrivateBrowsingSession()
{
    ASSERT(isMainThread());

    NetworkStorageSession::ensurePrivateBrowsingSession(SessionID::legacyPrivateSessionID(), [[NSBundle mainBundle] bundleIdentifier]);

    return *NetworkStorageSession::storageSession(SessionID::legacyPrivateSessionID());
}

void WebFrameNetworkingContext::destroyPrivateBrowsingSession()
{
    ASSERT(isMainThread());

    NetworkStorageSession::destroySession(SessionID::legacyPrivateSessionID());
}

bool WebFrameNetworkingContext::localFileContentSniffingEnabled() const
{
    return frame() && frame()->settings().localFileContentSniffingEnabled();
}

SchedulePairHashSet* WebFrameNetworkingContext::scheduledRunLoopPairs() const
{
    if (!frame() || !frame()->page())
        return 0;
    return frame()->page()->scheduledRunLoopPairs();
}

RetainPtr<CFDataRef> WebFrameNetworkingContext::sourceApplicationAuditData() const
{
    if (!frame() || !frame()->page())
        return 0;
    
    WebView *webview = kit(frame()->page());
    
    if (!webview)
        return 0;
    return reinterpret_cast<CFDataRef>(webview._sourceApplicationAuditData);
}

String WebFrameNetworkingContext::sourceApplicationIdentifier() const
{
    return emptyString();
}

ResourceError WebFrameNetworkingContext::blockedError(const ResourceRequest& request) const
{
    return frame()->loader().client().blockedError(request);
}

NetworkStorageSession& WebFrameNetworkingContext::storageSession() const
{
    ASSERT(isMainThread());
    if (frame() && frame()->page()->sessionID().isEphemeral()) {
        if (NetworkStorageSession* session = NetworkStorageSession::storageSession(SessionID::legacyPrivateSessionID()))
            return *session;
        // Some requests may still be coming shortly before WebCore updates the session ID and after WebKit destroys the private browsing session.
        LOG_ERROR("Invalid session ID. Please file a bug unless you just disabled private browsing, in which case it's an expected race.");
    }
    return NetworkStorageSession::defaultStorageSession();
}
