/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#pragma once

#include "DashArray.h"
#include "FloatRect.h"
#include "FontCascade.h"
#include "Gradient.h"
#include "GraphicsTypes.h"
#include "Image.h"
#include "ImageOrientation.h"
#include "Pattern.h"
#include <wtf/Noncopyable.h>

#if USE(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif USE(CAIRO)
namespace WebCore {
class PlatformContextCairo;
}
typedef WebCore::PlatformContextCairo PlatformGraphicsContext;
#elif USE(WINGDI)
typedef struct HDC__ PlatformGraphicsContext;
#else
typedef void PlatformGraphicsContext;
#endif

#if PLATFORM(WIN)
#include "DIBPixelData.h"
typedef struct HDC__* HDC;
#if !USE(CG)
// UInt8 is defined in CoreFoundation/CFBase.h
typedef unsigned char UInt8;
#endif
#endif

namespace WebCore {

#if USE(WINGDI)
class SharedBitmap;
class Font;
class GlyphBuffer;
#endif

const int cMisspellingLineThickness = 3;
const int cMisspellingLinePatternWidth = 4;
const int cMisspellingLinePatternGapWidth = 1;

class AffineTransform;
class FloatRoundedRect;
class Gradient;
class GraphicsContextPlatformPrivate;
class ImageBuffer;
class IntRect;
class RoundedRect;
class URL;
class GraphicsContext3D;
class Path;
class TextRun;
class TransformationMatrix;

enum TextDrawingMode {
    TextModeFill = 1 << 0,
    TextModeStroke = 1 << 1,
#if ENABLE(LETTERPRESS)
    TextModeLetterpress = 1 << 2,
#endif
};
typedef unsigned TextDrawingModeFlags;

enum StrokeStyle {
    NoStroke,
    SolidStroke,
    DottedStroke,
    DashedStroke,
    DoubleStroke,
    WavyStroke,
};

namespace DisplayList {
class Recorder;
}

struct GraphicsContextState {
    GraphicsContextState()
        : shouldAntialias(true)
        , shouldSmoothFonts(true)
        , shouldSubpixelQuantizeFonts(true)
        , shadowsIgnoreTransforms(false)
#if USE(CG)
        // Core Graphics incorrectly renders shadows with radius > 8px (<rdar://problem/8103442>),
        // but we need to preserve this buggy behavior for canvas and -webkit-box-shadow.
        , shadowsUseLegacyRadius(false)
#endif
        , drawLuminanceMask(false)
    {
    }

    enum Change : uint32_t {
        NoChange                                = 0,
        StrokeGradientChange                    = 1 << 1,
        StrokePatternChange                     = 1 << 2,
        FillGradientChange                      = 1 << 3,
        FillPatternChange                       = 1 << 4,
        StrokeThicknessChange                   = 1 << 5,
        StrokeColorChange                       = 1 << 6,
        StrokeStyleChange                       = 1 << 7,
        FillColorChange                         = 1 << 8,
        FillRuleChange                          = 1 << 9,
        ShadowChange                            = 1 << 10,
        ShadowColorChange                       = 1 << 11,
        ShadowsIgnoreTransformsChange           = 1 << 12,
        AlphaChange                             = 1 << 13,
        CompositeOperationChange                = 1 << 14,
        BlendModeChange                         = 1 << 15,
        TextDrawingModeChange                   = 1 << 16,
        ShouldAntialiasChange                   = 1 << 17,
        ShouldSmoothFontsChange                 = 1 << 18,
        ShouldSubpixelQuantizeFontsChange       = 1 << 19,
        DrawLuminanceMaskChange                 = 1 << 20,
        ImageInterpolationQualityChange         = 1 << 21,
    };
    typedef uint32_t StateChangeFlags;

    RefPtr<Gradient> strokeGradient;
    RefPtr<Pattern> strokePattern;
    
    RefPtr<Gradient> fillGradient;
    RefPtr<Pattern> fillPattern;

    FloatSize shadowOffset;

    float strokeThickness { 0 };
    float shadowBlur { 0 };

    TextDrawingModeFlags textDrawingMode { TextModeFill };

    Color strokeColor { Color::black };
    Color fillColor { Color::black };
    Color shadowColor;

    StrokeStyle strokeStyle { SolidStroke };
    WindRule fillRule { RULE_NONZERO };

    float alpha { 1 };
    CompositeOperator compositeOperator { CompositeSourceOver };
    BlendMode blendMode { BlendModeNormal };
    InterpolationQuality imageInterpolationQuality { InterpolationDefault };

    bool shouldAntialias : 1;
    bool shouldSmoothFonts : 1;
    bool shouldSubpixelQuantizeFonts : 1;
    bool shadowsIgnoreTransforms : 1;
#if USE(CG)
    bool shadowsUseLegacyRadius : 1;
#endif
    bool drawLuminanceMask : 1;
};

struct ImagePaintingOptions {
    ImagePaintingOptions(CompositeOperator compositeOperator = CompositeSourceOver, BlendMode blendMode = BlendModeNormal, ImageOrientationDescription orientationDescription = ImageOrientationDescription(), InterpolationQuality interpolationQuality = InterpolationDefault)
        : m_compositeOperator(compositeOperator)
        , m_blendMode(blendMode)
        , m_orientationDescription(orientationDescription)
        , m_interpolationQuality(interpolationQuality)
    {
    }

    ImagePaintingOptions(ImageOrientationDescription orientationDescription, InterpolationQuality interpolationQuality = InterpolationDefault, CompositeOperator compositeOperator = CompositeSourceOver, BlendMode blendMode = BlendModeNormal)
        : m_compositeOperator(compositeOperator)
        , m_blendMode(blendMode)
        , m_orientationDescription(orientationDescription)
        , m_interpolationQuality(interpolationQuality)
    {
    }

    ImagePaintingOptions(InterpolationQuality interpolationQuality, ImageOrientationDescription orientationDescription = ImageOrientationDescription(), CompositeOperator compositeOperator = CompositeSourceOver, BlendMode blendMode = BlendModeNormal)
        : m_compositeOperator(compositeOperator)
        , m_blendMode(blendMode)
        , m_orientationDescription(orientationDescription)
        , m_interpolationQuality(interpolationQuality)
    {
    }
    
    bool usesDefaultInterpolation() const { return m_interpolationQuality == InterpolationDefault; }

    CompositeOperator m_compositeOperator;
    BlendMode m_blendMode;
    ImageOrientationDescription m_orientationDescription;
    InterpolationQuality m_interpolationQuality;
};

struct GraphicsContextStateChange {
    GraphicsContextStateChange() = default;
    GraphicsContextStateChange(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
        : m_state(state)
        , m_changeFlags(flags)
    {
    }

    GraphicsContextState::StateChangeFlags changesFromState(const GraphicsContextState&) const;

    void accumulate(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);
    void apply(GraphicsContext&) const;
    
    void dump(TextStream&) const;

    GraphicsContextState m_state;
    GraphicsContextState::StateChangeFlags m_changeFlags { GraphicsContextState::NoChange };
};

TextStream& operator<<(TextStream&, const GraphicsContextStateChange&);


class GraphicsContext {
    WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT GraphicsContext(PlatformGraphicsContext*);
    GraphicsContext() = default;
    WEBCORE_EXPORT ~GraphicsContext();
    
    enum class NonPaintingReasons {
        NoReasons,
        UpdatingControlTints
    };
    GraphicsContext(NonPaintingReasons);

    WEBCORE_EXPORT PlatformGraphicsContext* platformContext() const;

    bool paintingDisabled() const { return !m_data && !isRecording(); }
    bool updatingControlTints() const { return m_nonPaintingReasons == NonPaintingReasons::UpdatingControlTints; }

    void setDisplayListRecorder(DisplayList::Recorder* recorder) { m_displayListRecorder = recorder; }
    bool isRecording() const { return m_displayListRecorder; }

    void setStrokeThickness(float);
    float strokeThickness() const { return m_state.strokeThickness; }

    void setStrokeStyle(StrokeStyle);
    StrokeStyle strokeStyle() const { return m_state.strokeStyle; }

    WEBCORE_EXPORT void setStrokeColor(const Color&);
    const Color& strokeColor() const { return m_state.strokeColor; }

    void setStrokePattern(Ref<Pattern>&&);
    Pattern* strokePattern() const { return m_state.strokePattern.get(); }

    void setStrokeGradient(Ref<Gradient>&&);
    Gradient* strokeGradient() const { return m_state.strokeGradient.get(); }

    void setFillRule(WindRule);
    WindRule fillRule() const { return m_state.fillRule; }

    WEBCORE_EXPORT void setFillColor(const Color&);
    const Color& fillColor() const { return m_state.fillColor; }

    void setFillPattern(Ref<Pattern>&&);
    Pattern* fillPattern() const { return m_state.fillPattern.get(); }

    WEBCORE_EXPORT void setFillGradient(Ref<Gradient>&&);
    Gradient* fillGradient() const { return m_state.fillGradient.get(); }

    void setShadowsIgnoreTransforms(bool);
    bool shadowsIgnoreTransforms() const { return m_state.shadowsIgnoreTransforms; }

    WEBCORE_EXPORT void setShouldAntialias(bool);
    bool shouldAntialias() const { return m_state.shouldAntialias; }

    WEBCORE_EXPORT void setShouldSmoothFonts(bool);
    bool shouldSmoothFonts() const { return m_state.shouldSmoothFonts; }

    // Normally CG enables subpixel-quantization because it improves the performance of aligning glyphs.
    // In some cases we have to disable to to ensure a high-quality output of the glyphs.
    void setShouldSubpixelQuantizeFonts(bool);
    bool shouldSubpixelQuantizeFonts() const { return m_state.shouldSubpixelQuantizeFonts; }

    const GraphicsContextState& state() const { return m_state; }

#if USE(CG) || USE(CAIRO)
    WEBCORE_EXPORT void drawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal, ImageOrientation = ImageOrientation());
#endif

#if USE(CG)
    void applyStrokePattern();
    void applyFillPattern();
    void drawPath(const Path&);

    WEBCORE_EXPORT void setIsCALayerContext(bool);
    bool isCALayerContext() const;

    WEBCORE_EXPORT void setIsAcceleratedContext(bool);
#endif
    bool isAcceleratedContext() const;
    RenderingMode renderingMode() const { return isAcceleratedContext() ? Accelerated : Unaccelerated; }

    WEBCORE_EXPORT void save();
    WEBCORE_EXPORT void restore();

    // These draw methods will do both stroking and filling.
    // FIXME: ...except drawRect(), which fills properly but always strokes
    // using a 1-pixel stroke inset from the rect borders (of the correct
    // stroke color).
    void drawRect(const FloatRect&, float borderThickness = 1);
    void drawLine(const FloatPoint&, const FloatPoint&);

    void drawEllipse(const FloatRect&);
    void drawRaisedEllipse(const FloatRect&, const Color& ellipseColor, const Color& shadowColor);

    WEBCORE_EXPORT void fillPath(const Path&);
    void strokePath(const Path&);

    void fillEllipse(const FloatRect&);
    void strokeEllipse(const FloatRect&);

    WEBCORE_EXPORT void fillRect(const FloatRect&);
    WEBCORE_EXPORT void fillRect(const FloatRect&, const Color&);
    void fillRect(const FloatRect&, Gradient&);
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode = BlendModeNormal);
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode = BlendModeNormal);
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&);

    WEBCORE_EXPORT void clearRect(const FloatRect&);

    WEBCORE_EXPORT void strokeRect(const FloatRect&, float lineWidth);

    WEBCORE_EXPORT void drawImage(Image&, const FloatPoint& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    WEBCORE_EXPORT void drawImage(Image&, const FloatRect& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = ImagePaintingOptions());

    void drawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawTiledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor,
        Image::TileRule, Image::TileRule, const ImagePaintingOptions& = ImagePaintingOptions());

    WEBCORE_EXPORT void drawImageBuffer(ImageBuffer&, const FloatPoint& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawImageBuffer(ImageBuffer&, const FloatRect& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawImageBuffer(ImageBuffer&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = ImagePaintingOptions());

    void drawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendModeNormal);

    WEBCORE_EXPORT void drawConsumingImageBuffer(std::unique_ptr<ImageBuffer>, const FloatPoint& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawConsumingImageBuffer(std::unique_ptr<ImageBuffer>, const FloatRect& destination, const ImagePaintingOptions& = ImagePaintingOptions());
    void drawConsumingImageBuffer(std::unique_ptr<ImageBuffer>, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = ImagePaintingOptions());

    WEBCORE_EXPORT void setImageInterpolationQuality(InterpolationQuality);
    InterpolationQuality imageInterpolationQuality() const { return m_state.imageInterpolationQuality; }

    WEBCORE_EXPORT void clip(const FloatRect&);
    void clipRoundedRect(const FloatRoundedRect&);

    void clipOut(const FloatRect&);
    void clipOutRoundedRect(const FloatRoundedRect&);
    void clipPath(const Path&, WindRule = RULE_EVENODD);
    void clipToImageBuffer(ImageBuffer&, const FloatRect&);
    
    IntRect clipBounds() const;

    void setTextDrawingMode(TextDrawingModeFlags);
    TextDrawingModeFlags textDrawingMode() const { return m_state.textDrawingMode; }

    float drawText(const FontCascade&, const TextRun&, const FloatPoint&, unsigned from = 0, Optional<unsigned> to = Nullopt);
    void drawGlyphs(const FontCascade&, const Font&, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint&);
    void drawEmphasisMarks(const FontCascade&, const TextRun&, const AtomicString& mark, const FloatPoint&, unsigned from = 0, Optional<unsigned> to = Nullopt);
    void drawBidiText(const FontCascade&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction = FontCascade::DoNotPaintIfFontNotReady);

    void applyState(const GraphicsContextState&);

    enum RoundingMode {
        RoundAllSides,
        RoundOriginAndDimensions
    };
    FloatRect roundToDevicePixels(const FloatRect&, RoundingMode = RoundAllSides);

    FloatRect computeUnderlineBoundsForText(const FloatPoint&, float width, bool printing);
    WEBCORE_EXPORT void drawLineForText(const FloatPoint&, float width, bool printing, bool doubleLines = false, StrokeStyle = SolidStroke);
    void drawLinesForText(const FloatPoint&, const DashArray& widths, bool printing, bool doubleLines = false, StrokeStyle = SolidStroke);
    enum DocumentMarkerLineStyle {
#if PLATFORM(IOS)
        TextCheckingDictationPhraseWithAlternativesLineStyle,
#endif
        DocumentMarkerSpellingLineStyle,
        DocumentMarkerGrammarLineStyle,
        DocumentMarkerAutocorrectionReplacementLineStyle,
        DocumentMarkerDictationAlternativesLineStyle
    };
    static void updateDocumentMarkerResources();
    void drawLineForDocumentMarker(const FloatPoint&, float width, DocumentMarkerLineStyle);

    WEBCORE_EXPORT void beginTransparencyLayer(float opacity);
    WEBCORE_EXPORT void endTransparencyLayer();
    bool isInTransparencyLayer() const { return (m_transparencyCount > 0) && supportsTransparencyLayers(); }

    WEBCORE_EXPORT void setShadow(const FloatSize&, float blur, const Color&);
    // Legacy shadow blur radius is used for canvas, and -webkit-box-shadow.
    // It has different treatment of radii > 8px.
    void setLegacyShadow(const FloatSize&, float blur, const Color&);

    WEBCORE_EXPORT void clearShadow();
    bool getShadow(FloatSize&, float&, Color&) const;

    bool hasVisibleShadow() const { return m_state.shadowColor.isValid() && m_state.shadowColor.alpha(); }
    bool hasShadow() const { return hasVisibleShadow() && (m_state.shadowBlur || m_state.shadowOffset.width() || m_state.shadowOffset.height()); }
    bool hasBlurredShadow() const { return hasVisibleShadow() && m_state.shadowBlur; }

#if USE(CAIRO)
    bool mustUseShadowBlur() const;
#endif

    void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&);
    void drawFocusRing(const Path&, float width, float offset, const Color&);
#if PLATFORM(MAC)
    void drawFocusRing(const Path&, double timeOffset, bool& needsRedraw);
    void drawFocusRing(const Vector<FloatRect>&, double timeOffset, bool& needsRedraw);
#endif

    void setLineCap(LineCap);
    void setLineDash(const DashArray&, float dashOffset);
    void setLineJoin(LineJoin);
    void setMiterLimit(float);

    void setAlpha(float);
    float alpha() const { return m_state.alpha; }

    WEBCORE_EXPORT void setCompositeOperation(CompositeOperator, BlendMode = BlendModeNormal);
    CompositeOperator compositeOperation() const { return m_state.compositeOperator; }
    BlendMode blendModeOperation() const { return m_state.blendMode; }

    void setDrawLuminanceMask(bool);
    bool drawLuminanceMask() const { return m_state.drawLuminanceMask; }

    // This clip function is used only by <canvas> code. It allows
    // implementations to handle clipping on the canvas differently since
    // the discipline is different.
    void canvasClip(const Path&, WindRule = RULE_EVENODD);
    void clipOut(const Path&);

    WEBCORE_EXPORT void scale(const FloatSize&);
    void rotate(float angleInRadians);
    void translate(const FloatSize& size) { translate(size.width(), size.height()); }
    WEBCORE_EXPORT void translate(float x, float y);

    void setURLForRect(const URL&, const IntRect&);

    void concatCTM(const AffineTransform&);
    void setCTM(const AffineTransform&);

    enum IncludeDeviceScale { DefinitelyIncludeDeviceScale, PossiblyIncludeDeviceScale };
    AffineTransform getCTM(IncludeDeviceScale includeScale = PossiblyIncludeDeviceScale) const;

#if ENABLE(3D_TRANSFORMS) && USE(TEXTURE_MAPPER)
    // This is needed when using accelerated-compositing in software mode, like in TextureMapper.
    void concat3DTransform(const TransformationMatrix&);
    void set3DTransform(const TransformationMatrix&);
    TransformationMatrix get3DTransform() const;
#endif

    // This function applies the device scale factor to the context, making the context capable of
    // acting as a base-level context for a HiDPI environment.
    WEBCORE_EXPORT void applyDeviceScaleFactor(float);
    void platformApplyDeviceScaleFactor(float);
    FloatSize scaleFactor() const;

#if OS(WINDOWS)
    HDC getWindowsContext(const IntRect&, bool supportAlphaBlend, bool mayCreateBitmap); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
    void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend, bool mayCreateBitmap); // The passed in HDC should be the one handed back by getWindowsContext.
    HDC hdc() const;
#if PLATFORM(WIN)
#if USE(WINGDI)
    void setBitmap(PassRefPtr<SharedBitmap>);
    const AffineTransform& affineTransform() const;
    AffineTransform& affineTransform();
    void resetAffineTransform();
    void fillRect(const FloatRect&, const Gradient*);
    void drawText(const Font&, const GlyphBuffer&, int from, int numGlyphs, const FloatPoint&);
    void drawFrameControl(const IntRect& rect, unsigned type, unsigned state);
    void drawFocusRect(const IntRect& rect);
    void paintTextField(const IntRect& rect, unsigned state);
    void drawBitmap(SharedBitmap*, const IntRect& dstRect, const IntRect& srcRect, CompositeOperator, BlendMode);
    void drawBitmapPattern(SharedBitmap*, const FloatRect& tileRectIn, const AffineTransform& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& destRect, const IntSize& origSourceSize);
    void drawIcon(HICON icon, const IntRect& dstRect, UINT flags);
    void drawRoundCorner(bool newClip, RECT clipRect, RECT rectWin, HDC dc, int width, int height);
#else
    GraphicsContext(HDC, bool hasAlpha = false); // FIXME: To be removed.

    // When set to true, child windows should be rendered into this context
    // rather than allowing them just to render to the screen. Defaults to
    // false.
    // FIXME: This is a layering violation. GraphicsContext shouldn't know
    // what a "window" is. It would be much more appropriate for this flag
    // to be passed as a parameter alongside the GraphicsContext, but doing
    // that would require lots of changes in cross-platform code that we
    // aren't sure we want to make.
    void setShouldIncludeChildWindows(bool);
    bool shouldIncludeChildWindows() const;

    class WindowsBitmap {
        WTF_MAKE_NONCOPYABLE(WindowsBitmap);
    public:
        WindowsBitmap(HDC, const IntSize&);
        ~WindowsBitmap();

        HDC hdc() const { return m_hdc; }
        UInt8* buffer() const { return m_pixelData.buffer(); }
        unsigned bufferLength() const { return m_pixelData.bufferLength(); }
        const IntSize& size() const { return m_pixelData.size(); }
        unsigned bytesPerRow() const { return m_pixelData.bytesPerRow(); }
        unsigned short bitsPerPixel() const { return m_pixelData.bitsPerPixel(); }
        const DIBPixelData& windowsDIB() const { return m_pixelData; }

    private:
        HDC m_hdc;
        HBITMAP m_bitmap;
        DIBPixelData m_pixelData;
    };

    std::unique_ptr<WindowsBitmap> createWindowsBitmap(const IntSize&);
    // The bitmap should be non-premultiplied.
    void drawWindowsBitmap(WindowsBitmap*, const IntPoint&);
#endif
#else // PLATFORM(WIN)
    bool shouldIncludeChildWindows() const { return false; }
#endif // PLATFORM(WIN)
#endif // OS(WINDOWS)

#if USE(CAIRO)
    GraphicsContext(cairo_t*);
#endif

    static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);

private:
    void platformInit(PlatformGraphicsContext*);
    void platformDestroy();

#if PLATFORM(WIN) && !USE(WINGDI)
    void platformInit(HDC, bool hasAlpha = false);
#endif

    void savePlatformState();
    void restorePlatformState();

    void setPlatformTextDrawingMode(TextDrawingModeFlags);

    void setPlatformStrokeColor(const Color&);
    void setPlatformStrokeStyle(StrokeStyle);
    void setPlatformStrokeThickness(float);

    void setPlatformFillColor(const Color&);

    void setPlatformShouldAntialias(bool);
    void setPlatformShouldSmoothFonts(bool);
    void setPlatformImageInterpolationQuality(InterpolationQuality);

    void setPlatformShadow(const FloatSize&, float blur, const Color&);
    void clearPlatformShadow();

    void setPlatformAlpha(float);
    void setPlatformCompositeOperation(CompositeOperator, BlendMode = BlendModeNormal);

    void beginPlatformTransparencyLayer(float opacity);
    void endPlatformTransparencyLayer();
    static bool supportsTransparencyLayers();

    void fillEllipseAsPath(const FloatRect&);
    void strokeEllipseAsPath(const FloatRect&);

    void platformFillEllipse(const FloatRect&);
    void platformStrokeEllipse(const FloatRect&);

    void platformFillRoundedRect(const FloatRoundedRect&, const Color&);

    FloatRect computeLineBoundsAndAntialiasingModeForText(const FloatPoint&, float width, bool printing, Color&);

    GraphicsContextPlatformPrivate* m_data { nullptr };
    DisplayList::Recorder* m_displayListRecorder { nullptr };

    GraphicsContextState m_state;
    Vector<GraphicsContextState, 1> m_stack;

    const NonPaintingReasons m_nonPaintingReasons { NonPaintingReasons::NoReasons };
    unsigned m_transparencyCount { 0 };
};

class GraphicsContextStateSaver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContextStateSaver(GraphicsContext& context, bool saveAndRestore = true)
    : m_context(context)
    , m_saveAndRestore(saveAndRestore)
    {
        if (m_saveAndRestore)
            m_context.save();
    }
    
    ~GraphicsContextStateSaver()
    {
        if (m_saveAndRestore)
            m_context.restore();
    }
    
    void save()
    {
        ASSERT(!m_saveAndRestore);
        m_context.save();
        m_saveAndRestore = true;
    }

    void restore()
    {
        ASSERT(m_saveAndRestore);
        m_context.restore();
        m_saveAndRestore = false;
    }
    
    GraphicsContext* context() const { return &m_context; }

private:
    GraphicsContext& m_context;
    bool m_saveAndRestore;
};

class InterpolationQualityMaintainer {
public:
    explicit InterpolationQualityMaintainer(GraphicsContext& graphicsContext, InterpolationQuality interpolationQualityToUse)
        : m_graphicsContext(graphicsContext)
        , m_currentInterpolationQuality(graphicsContext.imageInterpolationQuality())
        , m_interpolationQualityChanged(interpolationQualityToUse != InterpolationDefault && m_currentInterpolationQuality != interpolationQualityToUse)
    {
        if (m_interpolationQualityChanged)
            m_graphicsContext.setImageInterpolationQuality(interpolationQualityToUse);
    }

    explicit InterpolationQualityMaintainer(GraphicsContext& graphicsContext, Optional<InterpolationQuality> interpolationQuality)
        : InterpolationQualityMaintainer(graphicsContext, interpolationQuality ? interpolationQuality.value() : graphicsContext.imageInterpolationQuality())
    {
    }

    ~InterpolationQualityMaintainer()
    {
        if (m_interpolationQualityChanged)
            m_graphicsContext.setImageInterpolationQuality(m_currentInterpolationQuality);
    }

private:
    GraphicsContext& m_graphicsContext;
    InterpolationQuality m_currentInterpolationQuality;
    bool m_interpolationQualityChanged;
};

} // namespace WebCore
