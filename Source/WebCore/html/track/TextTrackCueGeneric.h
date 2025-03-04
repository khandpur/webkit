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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#ifndef TextTrackCueGeneric_h
#define TextTrackCueGeneric_h

#if ENABLE(VIDEO_TRACK)

#include "Color.h"
#include "VTTCue.h"

namespace WebCore {

class GenericCueData;

// A "generic" cue is a non-WebVTT cue, so it is not positioned/sized with the WebVTT logic.
class TextTrackCueGeneric final : public VTTCue {
public:
    static Ref<TextTrackCueGeneric> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const String& content)
    {
        return adoptRef(*new TextTrackCueGeneric(context, start, end, content));
    }
    
    virtual ~TextTrackCueGeneric() { }

    PassRefPtr<VTTCueBox> createDisplayTree() override;

    void setLine(double, ExceptionCode&) override;
    void setPosition(double, ExceptionCode&) override;

    bool useDefaultPosition() const { return m_defaultPosition; }
    
    double baseFontSizeRelativeToVideoHeight() const { return m_baseFontSizeRelativeToVideoHeight; }
    void setBaseFontSizeRelativeToVideoHeight(double size) { m_baseFontSizeRelativeToVideoHeight = size; }

    double fontSizeMultiplier() const { return m_fontSizeMultiplier; }
    void setFontSizeMultiplier(double size) { m_fontSizeMultiplier = size; }

    String fontName() const { return m_fontName; }
    void setFontName(String name) { m_fontName = name; }

    const Color& foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(Color color) { m_foregroundColor = color; }
    
    const Color& backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(Color color) { m_backgroundColor = color; }
    
    const Color& highlightColor() const { return m_highlightColor; }
    void setHighlightColor(Color color) { m_highlightColor = color; }
    
    void setFontSize(int, const IntSize&, bool important) override;

    bool isEqual(const TextTrackCue&, CueMatchRules) const override;
    bool cueContentsMatch(const TextTrackCue&) const override;
    bool doesExtendCue(const TextTrackCue&) const override;

    TextTrackCue::CueType cueType() const override { return TextTrackCue::Generic; }

private:
    bool isOrderedBefore(const TextTrackCue*) const override;
    bool isPositionedAbove(const TextTrackCue*) const override;

    TextTrackCueGeneric(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const String&);
    
    Color m_foregroundColor;
    Color m_backgroundColor;
    Color m_highlightColor;
    double m_baseFontSizeRelativeToVideoHeight;
    double m_fontSizeMultiplier;
    String m_fontName;
    bool m_defaultPosition;
};

} // namespace WebCore

#endif
#endif
