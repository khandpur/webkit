/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef ImageBuffer_h
#define ImageBuffer_h

#include "AffineTransform.h"
#include "ColorSpace.h"
#include "GraphicsTypes.h"
#include "GraphicsTypes3D.h"
#include "IntSize.h"
#include "ImageBufferData.h"
#include "PlatformLayer.h"
#include <runtime/Uint8ClampedArray.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class GraphicsContext3D;
class Image;
class ImageData;
class IntPoint;
class IntRect;

enum Multiply {
    Premultiplied,
    Unmultiplied
};

enum BackingStoreCopy {
    CopyBackingStore, // Guarantee subsequent draws don't affect the copy.
    DontCopyBackingStore // Subsequent draws may affect the copy.
};

enum ScaleBehavior {
    Scaled,
    Unscaled
};

class ImageBuffer {
    WTF_MAKE_NONCOPYABLE(ImageBuffer); WTF_MAKE_FAST_ALLOCATED;
    friend class IOSurface;
public:
    // Will return a null pointer on allocation failure.
    static std::unique_ptr<ImageBuffer> create(const FloatSize& size, RenderingMode renderingMode, float resolutionScale = 1, ColorSpace colorSpace = ColorSpaceSRGB)
    {
        bool success = false;
        std::unique_ptr<ImageBuffer> buffer(new ImageBuffer(size, resolutionScale, colorSpace, renderingMode, success));
        if (!success)
            return nullptr;
        return buffer;
    }

    // Create an image buffer compatible with the context, with suitable resolution for drawing into the buffer and then into this context.
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, const GraphicsContext&);
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, ColorSpace, const GraphicsContext&);
    static std::unique_ptr<ImageBuffer> createCompatibleBuffer(const FloatSize&, float resolutionScale, ColorSpace, const GraphicsContext&);

    static IntSize compatibleBufferSize(const FloatSize&, const GraphicsContext&);
    bool isCompatibleWithContext(const GraphicsContext&) const;

    WEBCORE_EXPORT ~ImageBuffer();

    // The actual resolution of the backing store
    const IntSize& internalSize() const { return m_size; }
    const IntSize& logicalSize() const { return m_logicalSize; }

    FloatSize sizeForDestinationSize(FloatSize) const;

    float resolutionScale() const { return m_resolutionScale; }

    WEBCORE_EXPORT GraphicsContext& context() const;

    WEBCORE_EXPORT RefPtr<Image> copyImage(BackingStoreCopy = CopyBackingStore, ScaleBehavior = Scaled) const;
    WEBCORE_EXPORT static RefPtr<Image> sinkIntoImage(std::unique_ptr<ImageBuffer>, ScaleBehavior = Scaled);
    // Give hints on the faster copyImage Mode, return DontCopyBackingStore if it supports the DontCopyBackingStore behavior
    // or return CopyBackingStore if it doesn't.  
    static BackingStoreCopy fastCopyImageMode();

    enum CoordinateSystem { LogicalCoordinateSystem, BackingStoreCoordinateSystem };

    RefPtr<Uint8ClampedArray> getUnmultipliedImageData(const IntRect&, CoordinateSystem = LogicalCoordinateSystem) const;
    RefPtr<Uint8ClampedArray> getPremultipliedImageData(const IntRect&, CoordinateSystem = LogicalCoordinateSystem) const;

    void putByteArray(Multiply multiplied, Uint8ClampedArray*, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem = LogicalCoordinateSystem);
    
    void convertToLuminanceMask();
    
    String toDataURL(const String& mimeType, const double* quality = 0, CoordinateSystem = LogicalCoordinateSystem) const;
#if !USE(CG)
    AffineTransform baseTransform() const { return AffineTransform(); }
    void transformColorSpace(ColorSpace srcColorSpace, ColorSpace dstColorSpace);
    void platformTransformColorSpace(const Vector<int>&);
#else
    AffineTransform baseTransform() const { return AffineTransform(1, 0, 0, -1, 0, m_data.backingStoreSize.height()); }
#endif
    PlatformLayer* platformLayer() const;

#if USE(CAIRO)
    NativeImagePtr nativeImage() const;
#endif

    size_t memoryCost() const;
    size_t externalMemoryCost() const;

    // FIXME: current implementations of this method have the restriction that they only work
    // with textures that are RGB or RGBA format, and UNSIGNED_BYTE type.
    bool copyToPlatformTexture(GraphicsContext3D&, GC3Denum, Platform3DObject, GC3Denum, bool, bool);

    // These functions are used when clamping the ImageBuffer which is created for filter, masker or clipper.
    static bool sizeNeedsClamping(const FloatSize&);
    static bool sizeNeedsClamping(const FloatSize&, FloatSize& scale);
    static FloatSize clampedSize(const FloatSize&);
    static FloatSize clampedSize(const FloatSize&, FloatSize& scale);
    static FloatRect clampedRect(const FloatRect&);

private:
#if USE(CG)
    // The returned image might be larger than the internalSize(). If you want the smaller
    // image, crop the result.
    RetainPtr<CGImageRef> copyNativeImage(BackingStoreCopy = CopyBackingStore) const;
    static RetainPtr<CGImageRef> sinkIntoNativeImage(std::unique_ptr<ImageBuffer>);
    void flushContext() const;
#endif
    
    void draw(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendModeNormal);

    static void drawConsuming(std::unique_ptr<ImageBuffer>, GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1), CompositeOperator = CompositeSourceOver, BlendMode = BlendModeNormal);

    inline void genericConvertToLuminanceMask();

    friend class GraphicsContext;
    friend class GeneratedImage;
    friend class CrossfadeGeneratedImage;
    friend class NamedImageGeneratedImage;
    friend class GradientImage;

private:
    ImageBufferData m_data;
    IntSize m_size;
    IntSize m_logicalSize;
    float m_resolutionScale;

    // This constructor will place its success into the given out-variable
    // so that create() knows when it should return failure.
    WEBCORE_EXPORT ImageBuffer(const FloatSize&, float resolutionScale, ColorSpace, RenderingMode, bool& success);
#if USE(CG)
    ImageBuffer(const FloatSize&, float resolutionScale, CGColorSpaceRef, RenderingMode, bool& success);
#endif
};

#if USE(CG)
String ImageDataToDataURL(const ImageData&, const String& mimeType, const double* quality);
#endif

} // namespace WebCore

#endif // ImageBuffer_h
