/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "GraphicsContext3D.h"

#include "Extensions3DOpenGLES.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"

namespace WebCore {

void GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
    ::glReleaseShaderCompiler();
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    makeContextCurrent();
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    ::glFlush();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
         resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        ::glFlush();
    }

    ::glReadPixels(x, y, width, height, format, type, data);

    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContext3D::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
    ::glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    int totalBytes = width * height * 4;
    if (isGLES2Compliant()) {
        for (int i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]); // Convert to BGRA.
    }
}

bool GraphicsContext3D::reshapeFBOs(const IntSize& size)
{
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = 0, pixelDataType = 0;
    if (m_attrs.alpha) {
        m_internalColorFormat = GL_RGBA;
        colorFormat = GL_RGBA;
        pixelDataType = GL_UNSIGNED_BYTE;
    } else {
        m_internalColorFormat = GL_RGB;
        colorFormat = GL_RGB;
        pixelDataType = GL_UNSIGNED_SHORT_5_6_5;
    }

    // We don't allow the logic where stencil is required and depth is not.
    // See GraphicsContext3D::validateAttributes.
    bool supportPackedDepthStencilBuffer = (m_attrs.stencil || m_attrs.depth) && getExtensions()->supports("GL_OES_packed_depth_stencil");

    // Resize regular FBO.
    bool mustRestoreFBO = false;
    if (m_state.boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }

    ASSERT(m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, pixelDataType, 0);
    ::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    if (m_compositorTexture) {
        ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
        ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        ::glBindTexture(GL_TEXTURE_2D, 0);
    }

#if USE(COORDINATED_GRAPHICS_THREADED)
        ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
        ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        ::glBindTexture(GL_TEXTURE_2D, 0);
#endif

    Extensions3DOpenGLES* extensions = static_cast<Extensions3DOpenGLES*>(getExtensions());
    if (extensions->isImagination() && m_attrs.antialias) {
        GLint maxSampleCount;
        ::glGetIntegerv(Extensions3D::MAX_SAMPLES_IMG, &maxSampleCount); 
        GLint sampleCount = std::min(8, maxSampleCount);

        extensions->framebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0, sampleCount);

        if (m_attrs.stencil || m_attrs.depth) {
            // Use a 24 bit depth buffer where we know we have it.
            if (supportPackedDepthStencilBuffer) {
                ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
                extensions->renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH24_STENCIL8_OES, width, height);
                if (m_attrs.stencil)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
                if (m_attrs.depth)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (m_attrs.stencil) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
                    extensions->renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_STENCIL_INDEX8, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
                }
                if (m_attrs.depth) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
                    extensions->renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT16, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
                }
            }
            ::glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    } else {
        if (m_attrs.stencil || m_attrs.depth) {
            // Use a 24 bit depth buffer where we know we have it.
            if (supportPackedDepthStencilBuffer) {
                ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
                ::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
                if (m_attrs.stencil)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
                if (m_attrs.depth)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (m_attrs.stencil) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
                    ::glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
                }
                if (m_attrs.depth) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
                    ::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
                }
            }
            ::glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }

    return mustRestoreFBO;
}

void GraphicsContext3D::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    // FIXME: We don't support antialiasing yet.
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glRenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetIntegerv(pname, value);
}

void GraphicsContext3D::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision)
{
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();
    ::glGetShaderPrecisionFormat(shaderType, precisionType, range, precision);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    texImage2DDirect(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::validateAttributes()
{
    validateDepthStencil("GL_OES_packed_depth_stencil");

    if (m_attrs.antialias) {
        Extensions3D* extensions = getExtensions();
        if (!extensions->supports("GL_IMG_multisampled_render_to_texture"))
            m_attrs.antialias = false;
    }
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    ::glDepthRangef(zNear, zFar);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    ::glClearDepthf(depth);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = std::make_unique<Extensions3DOpenGLES>(this);
    return m_extensions.get();
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
