/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SecurityContext_h
#define SecurityContext_h

#include <memory>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class SecurityOrigin;
class SecurityOriginPolicy;
class ContentSecurityPolicy;
class URL;

enum SandboxFlag {
    // See http://www.whatwg.org/specs/web-apps/current-work/#attr-iframe-sandbox for a list of the sandbox flags.
    SandboxNone = 0,
    SandboxNavigation = 1,
    SandboxPlugins = 1 << 1,
    SandboxOrigin = 1 << 2,
    SandboxForms = 1 << 3,
    SandboxScripts = 1 << 4,
    SandboxTopNavigation = 1 << 5,
    SandboxPopups = 1 << 6, // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=12393
    SandboxAutomaticFeatures = 1 << 7,
    SandboxPointerLock = 1 << 8,
    SandboxAll = -1 // Mask with all bits set to 1.
};

typedef int SandboxFlags;

class SecurityContext {
public:
    SandboxFlags sandboxFlags() const { return m_sandboxFlags; }
    ContentSecurityPolicy* contentSecurityPolicy() { return m_contentSecurityPolicy.get(); }

    bool isSecureTransitionTo(const URL&) const;

    void enforceSandboxFlags(SandboxFlags mask);
    bool isSandboxed(SandboxFlags mask) const { return m_sandboxFlags & mask; }

    SecurityOriginPolicy* securityOriginPolicy() const { return m_securityOriginPolicy.get(); }

    // Explicitly override the security origin for this security context.
    // Note: It is dangerous to change the security origin of a script context
    //       that already contains content.
    void setSecurityOriginPolicy(RefPtr<SecurityOriginPolicy>&&);

    WEBCORE_EXPORT SecurityOrigin* securityOrigin() const;

    static SandboxFlags parseSandboxPolicy(const String& policy, String& invalidTokensErrorMessage);
    static bool isSupportedSandboxPolicy(StringView);

    bool foundMixedContent() const { return m_foundMixedContent; }
    void setFoundMixedContent() { m_foundMixedContent = true; }
    bool geolocationAccessed() const { return m_geolocationAccessed; }
    void setGeolocationAccessed() { m_geolocationAccessed = true; }

protected:
    SecurityContext();
    virtual ~SecurityContext();

    void setContentSecurityPolicy(std::unique_ptr<ContentSecurityPolicy>);

    void didFailToInitializeSecurityOrigin() { m_haveInitializedSecurityOrigin = false; }
    bool haveInitializedSecurityOrigin() const { return m_haveInitializedSecurityOrigin; }

private:
    bool m_haveInitializedSecurityOrigin;
    SandboxFlags m_sandboxFlags;
    RefPtr<SecurityOriginPolicy> m_securityOriginPolicy;
    std::unique_ptr<ContentSecurityPolicy> m_contentSecurityPolicy;
    bool m_foundMixedContent { false };
    bool m_geolocationAccessed { false };
};

} // namespace WebCore

#endif // SecurityContext_h
