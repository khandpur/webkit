/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Inc.  All rights reserved.
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

#ifndef Image_h
#define Image_h

#include "Color.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsTypes.h"
#include "ImageOrientation.h"
#include "NativeImage.h"
#include <wtf/Optional.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

#if USE(APPKIT)
OBJC_CLASS NSImage;
#endif

#if USE(CG)
struct CGContext;
#endif

#if PLATFORM(WIN)
typedef struct tagSIZE SIZE;
typedef SIZE* LPSIZE;
typedef struct HBITMAP__ *HBITMAP;
#endif

#if PLATFORM(GTK)
typedef struct _GdkPixbuf GdkPixbuf;
#endif

namespace WebCore {

class AffineTransform;
class FloatPoint;
class FloatSize;
class GraphicsContext;
class SharedBuffer;
struct Length;

// This class gets notified when an image creates or destroys decoded frames and when it advances animation frames.
class ImageObserver;

class Image : public RefCounted<Image> {
    friend class GraphicsContext;
public:
    virtual ~Image();
    
    static PassRefPtr<Image> create(ImageObserver* = nullptr);
    WEBCORE_EXPORT static PassRefPtr<Image> loadPlatformResource(const char* name);
    WEBCORE_EXPORT static bool supportsType(const String&);

    virtual bool isBitmapImage() const { return false; }
    virtual bool isGeneratedImage() const { return false; }
    virtual bool isCrossfadeGeneratedImage() const { return false; }
    virtual bool isNamedImageGeneratedImage() const { return false; }
    virtual bool isGradientImage() const { return false; }
    virtual bool isSVGImage() const { return false; }
    virtual bool isPDFDocumentImage() const { return false; }

    virtual bool currentFrameKnownToBeOpaque() const = 0;
    virtual bool isAnimated() const { return false; }

    // Derived classes should override this if they can assure that 
    // the image contains only resources from its own security origin.
    virtual bool hasSingleSecurityOrigin() const { return false; }

    WEBCORE_EXPORT static Image* nullImage();
    bool isNull() const { return size().isEmpty(); }

    virtual void setContainerSize(const FloatSize&) { }
    virtual bool usesContainerSize() const { return false; }
    virtual bool hasRelativeWidth() const { return false; }
    virtual bool hasRelativeHeight() const { return false; }
    virtual void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    virtual FloatSize size() const = 0;
    FloatRect rect() const { return FloatRect(FloatPoint(), size()); }
    float width() const { return size().width(); }
    float height() const { return size().height(); }
    virtual Optional<IntPoint> hotSpot() const { return Nullopt; }

#if PLATFORM(IOS)
    virtual FloatSize originalSize() const { return size(); }
#endif

    WEBCORE_EXPORT bool setData(RefPtr<SharedBuffer>&& data, bool allDataReceived);
    virtual bool dataChanged(bool /*allDataReceived*/) { return false; }
    
    virtual String filenameExtension() const { return String(); } // null string if unknown

    virtual void destroyDecodedData(bool destroyAll = true) = 0;

    SharedBuffer* data() { return m_encodedImageData.get(); }
    const SharedBuffer* data() const { return m_encodedImageData.get(); }

    // Animation begins whenever someone draws the image, so startAnimation() is not normally called.
    // It will automatically pause once all observers no longer want to render the image anywhere.
    enum CatchUpAnimation { DoNotCatchUp, CatchUp };
    virtual void startAnimation(CatchUpAnimation = CatchUp) { }
    virtual void stopAnimation() {}
    virtual void resetAnimation() {}
    
    // Typically the CachedImage that owns us.
    ImageObserver* imageObserver() const { return m_imageObserver; }
    void setImageObserver(ImageObserver* observer) { m_imageObserver = observer; }

    enum TileRule { StretchTile, RoundTile, SpaceTile, RepeatTile };

    virtual NativeImagePtr nativeImage() { return nullptr; }
    virtual NativeImagePtr nativeImageOfSize(const IntSize&) { return nullptr; }
    virtual NativeImagePtr nativeImageForCurrentFrame() { return nullptr; }
    virtual ImageOrientation orientationForCurrentFrame() const { return ImageOrientation(); }
    virtual Vector<NativeImagePtr> framesNativeImages() { return { }; }

    // Accessors for native image formats.

#if USE(APPKIT)
    virtual NSImage *nsImage() { return nullptr; }
    virtual RetainPtr<NSImage> currentFrameNSImage() { return nullptr; }
#endif

#if PLATFORM(COCOA)
    virtual CFDataRef tiffRepresentation() { return nullptr; }
#endif

#if PLATFORM(WIN)
    virtual bool getHBITMAP(HBITMAP) { return false; }
    virtual bool getHBITMAPOfSize(HBITMAP, const IntSize*) { return false; }
#endif

#if PLATFORM(GTK)
    virtual GdkPixbuf* getGdkPixbuf() { return nullptr; }
#endif

#if PLATFORM(EFL)
    virtual Evas_Object* getEvasObject(Evas*) { return nullptr; }
#endif

    virtual void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
        const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendModeNormal);

#if ENABLE(IMAGE_DECODER_DOWN_SAMPLING)
    FloatRect adjustSourceRectForDownSampling(const FloatRect& srcRect, const IntSize& scaledSize) const;
#endif

#if !ASSERT_DISABLED
    virtual bool notSolidColor() { return true; }
#endif

    virtual void dump(TextStream&) const;

protected:
    Image(ImageObserver* = nullptr);

    static void fillWithSolidColor(GraphicsContext&, const FloatRect& dstRect, const Color&, CompositeOperator);

#if PLATFORM(WIN)
    virtual void drawFrameMatchingSourceSize(GraphicsContext&, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator) { }
#endif
    virtual void draw(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator, BlendMode, ImageOrientationDescription) = 0;
    void drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatPoint& srcPoint, const FloatSize& tileSize, const FloatSize& spacing, CompositeOperator, BlendMode);
    void drawTiled(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const FloatSize& tileScaleFactor, TileRule hRule, TileRule vRule, CompositeOperator);

    // Supporting tiled drawing
    virtual Color singlePixelSolidColor() const { return Color(); }
    
private:
    RefPtr<SharedBuffer> m_encodedImageData;
    ImageObserver* m_imageObserver;
};

TextStream& operator<<(TextStream&, const Image&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_IMAGE(ToClassName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::Image& image) { return image.is##ToClassName(); } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // Image_h
