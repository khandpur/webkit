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

#ifndef WebPlaybackSessionManager_h
#define WebPlaybackSessionManager_h

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/WebPlaybackSessionInterface.h>
#include <WebCore/WebPlaybackSessionModelMediaElement.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Attachment;
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WebCore {
class Node;
}

namespace WebKit {

class WebPage;
class WebPlaybackSessionManager;

class WebPlaybackSessionInterfaceContext final
    : public RefCounted<WebPlaybackSessionInterfaceContext>
    , public WebCore::WebPlaybackSessionInterface
    , public WebCore::WebPlaybackSessionModelClient {
public:
    static Ref<WebPlaybackSessionInterfaceContext> create(WebPlaybackSessionManager& manager, uint64_t contextId)
    {
        return adoptRef(*new WebPlaybackSessionInterfaceContext(manager, contextId));
    }
    virtual ~WebPlaybackSessionInterfaceContext();

    void invalidate() { m_manager = nullptr; }

private:
    friend class WebVideoFullscreenInterfaceContext;

    // WebPlaybackSessionInterface
    void resetMediaState() final;

    // WebPlaybackModelClient
    void durationChanged(double) final;
    void currentTimeChanged(double currentTime, double anchorTime) final;
    void bufferedTimeChanged(double) final;
    void playbackStartedTimeChanged(double playbackStartedTime) final;
    void rateChanged(bool isPlaying, float playbackRate) final;
    void seekableRangesChanged(const WebCore::TimeRanges&) final;
    void canPlayFastReverseChanged(bool value) final;
    void audioMediaSelectionOptionsChanged(const Vector<String>& options, uint64_t selectedIndex) final;
    void legibleMediaSelectionOptionsChanged(const Vector<String>& options, uint64_t selectedIndex) final;
    void externalPlaybackChanged(bool enabled, WebCore::WebPlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) final;
    void wirelessVideoPlaybackDisabledChanged(bool) final;

    WebPlaybackSessionInterfaceContext(WebPlaybackSessionManager&, uint64_t contextId);

    WebPlaybackSessionManager* m_manager;
    uint64_t m_contextId;
};

class WebPlaybackSessionManager : public RefCounted<WebPlaybackSessionManager>, private IPC::MessageReceiver {
public:
    static Ref<WebPlaybackSessionManager> create(WebPage&);
    virtual ~WebPlaybackSessionManager();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&);
    void clearPlaybackControlsManager();
    uint64_t contextIdForMediaElement(WebCore::HTMLMediaElement&);

protected:
    friend class WebPlaybackSessionInterfaceContext;
    friend class WebVideoFullscreenManager;

    explicit WebPlaybackSessionManager(WebPage&);

    typedef std::tuple<RefPtr<WebCore::WebPlaybackSessionModelMediaElement>, RefPtr<WebPlaybackSessionInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebCore::WebPlaybackSessionModelMediaElement& ensureModel(uint64_t contextId);
    WebPlaybackSessionInterfaceContext& ensureInterface(uint64_t contextId);
    void removeContext(uint64_t contextId);
    void addClientForContext(uint64_t contextId);
    void removeClientForContext(uint64_t contextId);

    // Interface to WebPlaybackSessionInterfaceContext
    void resetMediaState(uint64_t contextId);
    void durationChanged(uint64_t contextId, double);
    void currentTimeChanged(uint64_t contextId, double currentTime, double anchorTime);
    void bufferedTimeChanged(uint64_t contextId, double bufferedTime);
    void playbackStartedTimeChanged(uint64_t contextId, double playbackStartedTime);
    void rateChanged(uint64_t contextId, bool isPlaying, float playbackRate);
    void seekableRangesChanged(uint64_t contextId, const WebCore::TimeRanges&);
    void canPlayFastReverseChanged(uint64_t contextId, bool value);
    void audioMediaSelectionOptionsChanged(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex);
    void externalPlaybackChanged(uint64_t contextId, bool enabled, WebCore::WebPlaybackSessionModel::ExternalPlaybackTargetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(uint64_t contextId, bool);

    // Messages from WebPlaybackSessionManagerProxy
    void play(uint64_t contextId);
    void pause(uint64_t contextId);
    void togglePlayState(uint64_t contextId);
    void beginScrubbing(uint64_t contextId);
    void endScrubbing(uint64_t contextId);
    void seekToTime(uint64_t contextId, double time);
    void fastSeek(uint64_t contextId, double time);
    void beginScanningForward(uint64_t contextId);
    void beginScanningBackward(uint64_t contextId);
    void endScanning(uint64_t contextId);
    void selectAudioMediaOption(uint64_t contextId, uint64_t index);
    void selectLegibleMediaOption(uint64_t contextId, uint64_t index);
    void handleControlledElementIDRequest(uint64_t contextId);

    WebPage* m_page;
    HashMap<WebCore::HTMLMediaElement*, uint64_t> m_mediaElements;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
    uint64_t m_controlsManagerContextId { 0 };
    HashMap<uint64_t, int> m_clientCounts;
};

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#endif // WebPlaybackSessionManager_h
