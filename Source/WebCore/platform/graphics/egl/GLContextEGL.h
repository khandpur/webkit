/*
 * Copyright (C) 2012 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GLContextEGL_h
#define GLContextEGL_h

#if USE(EGL)

#include "GLContext.h"
#include <EGL/egl.h>

#if PLATFORM(X11)
#include "XUniqueResource.h"
#endif

#if PLATFORM(WAYLAND)
#include "WlUniquePtr.h"
#endif

namespace WebCore {

class GLContextEGL final : public GLContext {
    WTF_MAKE_NONCOPYABLE(GLContextEGL);
public:
    static std::unique_ptr<GLContextEGL> createContext(EGLNativeWindowType, PlatformDisplay&);
    static std::unique_ptr<GLContextEGL> createSharingContext(PlatformDisplay&);

    virtual ~GLContextEGL();

    bool makeContextCurrent() override;
    void swapBuffers() override;
    void waitNative() override;
    bool canRenderToDefaultFramebuffer() override;
    IntSize defaultFrameBufferSize() override;
    void swapInterval(int) override;
#if USE(CAIRO)
    cairo_device_t* cairoDevice() override;
#endif
    bool isEGLContext() const override { return true; }

#if ENABLE(GRAPHICS_CONTEXT_3D)
    PlatformGraphicsContext3D platformContext() override;
#endif

private:
    enum EGLSurfaceType { PbufferSurface, WindowSurface, PixmapSurface, Surfaceless };

    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, EGLSurfaceType);
#if PLATFORM(X11)
    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, XUniquePixmap&&);
#endif
#if PLATFORM(WAYLAND)
    GLContextEGL(PlatformDisplay&, EGLContext, EGLSurface, WlUniquePtr<struct wl_surface>&&, EGLNativeWindowType);
#endif

    static std::unique_ptr<GLContextEGL> createWindowContext(EGLNativeWindowType, PlatformDisplay&, EGLContext sharingContext = EGL_NO_CONTEXT);
    static std::unique_ptr<GLContextEGL> createPbufferContext(PlatformDisplay&, EGLContext sharingContext = EGL_NO_CONTEXT);
    static std::unique_ptr<GLContextEGL> createSurfacelessContext(PlatformDisplay&, EGLContext sharingContext = EGL_NO_CONTEXT);
#if PLATFORM(X11)
    static std::unique_ptr<GLContextEGL> createPixmapContext(PlatformDisplay&, EGLContext sharingContext = EGL_NO_CONTEXT);
#endif
#if PLATFORM(WAYLAND)
    static std::unique_ptr<GLContextEGL> createWaylandContext(PlatformDisplay&, EGLContext sharingContext = EGL_NO_CONTEXT);
#endif

    static bool getEGLConfig(EGLDisplay, EGLConfig*, EGLSurfaceType);

    EGLContext m_context { EGL_NO_CONTEXT };
    EGLSurface m_surface { EGL_NO_SURFACE };
    EGLSurfaceType m_type;
#if PLATFORM(X11)
    XUniquePixmap m_pixmap;
#endif
#if PLATFORM(WAYLAND)
    WlUniquePtr<struct wl_surface> m_wlSurface;
    struct wl_egl_window* m_wlWindow { nullptr };
#endif
#if USE(CAIRO)
    cairo_device_t* m_cairoDevice { nullptr };
#endif
};

} // namespace WebCore

#endif // USE(EGL)

#endif // GLContextEGL_h
