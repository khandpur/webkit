/*
 * Copyright (C) 2006, 2010, 2011, 2012, 2014, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc
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

#include "config.h"
#include "ImageSource.h"

#if USE(CG)
#include "ImageDecoderCG.h"
#else
#include "ImageDecoder.h"
#endif

#include "ImageOrientation.h"

#include <wtf/CurrentTime.h>

namespace WebCore {

ImageSource::ImageSource(NativeImagePtr&& nativeImage)
    : m_frameCache(WTFMove(nativeImage))
{
}
    
ImageSource::ImageSource(Image* image, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_frameCache(image)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
{
}

ImageSource::~ImageSource()
{
}

void ImageSource::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (!isDecoderAvailable())
        return;
    m_decoder->clearFrameBufferCache(clearBeforeFrame);
}

void ImageSource::clear(bool destroyAll, size_t count, SharedBuffer* data)
{
    // There's no need to throw away the decoder unless we're explicitly asked
    // to destroy all of the frames.
    if (!destroyAll) {
        clearFrameBufferCache(count);
        return;
    }

    m_decoder = nullptr;
    m_frameCache.setDecoder(nullptr);
    setData(data, isAllDataReceived());
}

void ImageSource::destroyDecodedData(SharedBuffer* data, bool destroyAll, size_t count)
{
    m_frameCache.destroyDecodedData(destroyAll, count);
    clear(destroyAll, count, data);
}

bool ImageSource::destroyDecodedDataIfNecessary(SharedBuffer* data, bool destroyAll, size_t count)
{
    // If we have decoded frames but there is no encoded data, we shouldn't destroy
    // the decoded image since we won't be able to reconstruct it later.
    if (!data && m_frameCache.frameCount())
        return false;

    if (!m_frameCache.destroyDecodedDataIfNecessary(destroyAll, count))
        return false;

    clear(destroyAll, count, data);
    return true;
}

bool ImageSource::ensureDecoderAvailable(SharedBuffer* data)
{
    if (!data || isDecoderAvailable())
        return true;

    m_decoder = ImageDecoder::create(*data, m_alphaOption, m_gammaAndColorProfileOption);
    if (!isDecoderAvailable())
        return false;

    m_frameCache.setDecoder(m_decoder.get());
    return true;
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (!data || !ensureDecoderAvailable(data))
        return;

    m_decoder->setData(*data, allDataReceived);
}

bool ImageSource::dataChanged(SharedBuffer* data, bool allDataReceived)
{
    m_frameCache.destroyIncompleteDecodedData();

#if PLATFORM(IOS)
    // FIXME: We should expose a setting to enable/disable progressive loading and make this
    // code conditional on it. Then we can remove the PLATFORM(IOS)-guard.
    static const double chunkLoadIntervals[] = {0, 1, 3, 6, 15};
    double interval = chunkLoadIntervals[std::min(m_progressiveLoadChunkCount, static_cast<uint16_t>(4))];

    bool needsUpdate = false;

    // The first time through, the chunk time will be 0 and the image will get an update.
    if (currentTime() - m_progressiveLoadChunkTime > interval) {
        needsUpdate = true;
        m_progressiveLoadChunkTime = currentTime();
        ASSERT(m_progressiveLoadChunkCount <= std::numeric_limits<uint16_t>::max());
        ++m_progressiveLoadChunkCount;
    }

    if (needsUpdate || allDataReceived)
        setData(data, allDataReceived);
#else
    setData(data, allDataReceived);
#endif

    m_frameCache.clearMetadata();
    if (!isSizeAvailable())
        return false;

    m_frameCache.growFrames();
    return true;
}

bool ImageSource::isAllDataReceived()
{
    return isDecoderAvailable() ? m_decoder->isAllDataReceived() : m_frameCache.frameCount();
}

SubsamplingLevel ImageSource::maximumSubsamplingLevel()
{
    if (m_maximumSubsamplingLevel)
        return m_maximumSubsamplingLevel.value();

    if (!m_allowSubsampling || !isDecoderAvailable() || !m_decoder->frameAllowSubsamplingAtIndex(0))
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    const int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    SubsamplingLevel level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    return m_maximumSubsamplingLevel.value();
}

SubsamplingLevel ImageSource::subsamplingLevelForScale(float scale)
{
    if (!(scale > 0 && scale <= 1))
        return SubsamplingLevel::Default;

    int result = std::ceil(std::log2(1 / scale));
    return static_cast<SubsamplingLevel>(std::min(result, static_cast<int>(maximumSubsamplingLevel())));
}

NativeImagePtr ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return isDecoderAvailable() ? m_decoder->createFrameImageAtIndex(index, subsamplingLevel) : nullptr;
}

void ImageSource::dump(TextStream& ts)
{
    ts.dumpProperty("type", filenameExtension());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetitions", repetitionCount());
    ts.dumpProperty("solid-color", singlePixelSolidColor());

    ImageOrientation orientation = frameOrientationAtIndex(0);
    if (orientation != OriginTopLeft)
        ts.dumpProperty("orientation", orientation);
}

}
