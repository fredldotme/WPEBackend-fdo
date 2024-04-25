/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ws-hybrisegl.h"

#include "linux-hybris/server_wlegl.h"
#include "linux-hybris/server_wlegl_buffer.h"
#include <epoxy/egl.h>
#include <cassert>
#include <cstring>
#include <hybris/eglplatformcommon/hybris_nativebufferext.h>

#include "linux-hybris/logging.h"

static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR;

namespace WS {

ImplHybrisEGL::ImplHybrisEGL()
{
    m_egl.display = EGL_NO_DISPLAY;

    wl_list_init(&m_hybris.buffers);
}

ImplHybrisEGL::~ImplHybrisEGL()
{
    if (m_hybris.global) {
        struct server_wlegl_buffer* buffer;
        struct server_wlegl_buffer* tmp;
        wl_list_for_each_safe(buffer, tmp, &m_hybris.buffers, link) {
            assert(buffer);
            wl_list_remove(&buffer->link);
            server_wlegl_buffer_dtor(buffer->resource);
        }
        server_wlegl_destroy(m_hybris.global);
    }
}

void ImplHybrisEGL::surfaceAttach(Surface& surface, struct wl_resource* bufferResource)
{
    surface.hybrisBuffer = server_wlegl_buffer_from(bufferResource); //getHybrisBuffer(bufferResource);

    if (surface.bufferResource)
        wl_buffer_send_release(surface.bufferResource);
    surface.bufferResource = bufferResource;
}

void ImplHybrisEGL::surfaceCommit(Surface& surface)
{
    if (!surface.apiClient)
        return;

    surface.bufferResource = nullptr;
    surface.apiClient->exportHybrisBuffer(surface.hybrisBuffer);
}

bool ImplHybrisEGL::initialize(EGLDisplay eglDisplay)
{
    if (m_egl.display == eglDisplay)
        return true;

    if (m_egl.display != EGL_NO_DISPLAY) {
        g_warning("Multiple EGL displays are not supported.\n");
        return false;
    }

    decltype(m_egl.extensions) extensions;
    extensions.KHR_image_base = epoxy_has_egl_extension(eglDisplay, "EGL_KHR_image_base");

    if (extensions.KHR_image_base) {
        s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
        assert(s_eglCreateImageKHR && s_eglDestroyImageKHR);
    }

    m_initialized = true;
    m_egl.display = eglDisplay;
    m_egl.extensions = extensions;

    if (m_hybris.global)
        assert(!"hybris buffer support has already been initialized");
    m_hybris.global = server_wlegl_create(display());
    
    return true;
}

EGLImageKHR ImplHybrisEGL::createImage(struct wl_resource* resourceBuffer)
{
    if (m_egl.display == EGL_NO_DISPLAY)
        return EGL_NO_IMAGE_KHR;

    auto buffer = server_wlegl_buffer_from(resourceBuffer); //getHybrisBuffer(bufferResource);
    if (!buffer)
        return EGL_NO_IMAGE_KHR;

    assert(m_egl.extensions.KHR_image_base);
    TRACE("Creating and returning image")
    return s_eglCreateImageKHR(m_egl.display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_HYBRIS, buffer->buf->getNativeBuffer(), nullptr);
}

EGLImageKHR ImplHybrisEGL::createImage(struct server_wlegl_buffer* hybrisBuffer)
{
    if (!hybrisBuffer)
        return EGL_NO_IMAGE_KHR;
    return createImage(hybrisBuffer->resource);
}

void ImplHybrisEGL::destroyImage(EGLImageKHR image)
{
    if (m_egl.display == EGL_NO_DISPLAY)
        return;

    assert(m_egl.extensions.KHR_image_base);
    s_eglDestroyImageKHR(m_egl.display, image);
    TRACE("Destroyed image")
}

void ImplHybrisEGL::queryBufferSize(struct wl_resource* bufferResource, uint32_t* width, uint32_t* height)
{
    if (m_egl.display == EGL_NO_DISPLAY) {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    const auto buffer = server_wlegl_buffer_from(bufferResource); //getHybrisBuffer(bufferResource);
    if (!buffer) {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    if (width) {
        *width = buffer->buf->width;
    }

    if (height) {
        *height = buffer->buf->height;
    }
}

void ImplHybrisEGL::importHybrisBuffer(struct server_wlegl_buffer* buffer)
{
    if (!m_hybris.global)
        return;
    //wl_list_insert(&m_hybris.buffers, &buffer->link);
}

/*
struct server_wlegl_buffer* ImplHybrisEGL::getHybrisBuffer(struct wl_resource* bufferResource) const
{
    if (!m_hybris.global || !bufferResource)
        return nullptr;

    struct server_wlegl_buffer* buffer;
    wl_list_for_each(buffer, &m_hybris.buffers, link) {
        if (buffer->resource == bufferResource)
            return buffer;
    }

    return nullptr;
}
*/

} // namespace WS
