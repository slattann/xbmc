/*
 *      Copyright (C) 2007-2017 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DRMPRIMEEGL.h"
#include "xbmc/cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodecDRMPRIME.h"
#include "xbmc/windowing/gbm/DRMUtils.h"

#include <drm_fourcc.h>
#include "utils/log.h"

extern "C" {
#include "libavutil/frame.h"
#include "libavutil/hwcontext_drm.h"
}

void CDRMPRIMETexture::Init(InteropInfo &interop)
{
  m_interop = interop;
}

bool CDRMPRIMETexture::Map(CVideoBufferDRMPRIME *buffer)
{
  AVDRMFrameDescriptor* descriptor = buffer->GetDescriptor();
  if (descriptor && descriptor->nb_layers)
  {
    AVDRMLayerDescriptor* layer = &descriptor->layers[0];

    m_texWidth = buffer->GetWidth();
    m_texHeight = buffer->GetHeight();

    switch (layer->format)
    {
      case DRM_FORMAT_NV12:
      {
        GLint attribsY[] =
        {
          EGL_LINUX_DRM_FOURCC_EXT, fourcc_code('R', '8', ' ', ' '),
          EGL_WIDTH, buffer->GetWidth(),
          EGL_HEIGHT, buffer->GetHeight(),
          EGL_DMA_BUF_PLANE0_FD_EXT, descriptor->objects[layer->planes[0].object_index].fd,
          EGL_DMA_BUF_PLANE0_OFFSET_EXT, layer->planes[0].offset,
          EGL_DMA_BUF_PLANE0_PITCH_EXT, layer->planes[0].pitch,
          EGL_NONE
        };

        eglImageY = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                                EGL_NO_CONTEXT,
                                                EGL_LINUX_DMA_BUF_EXT,
                                                (EGLClientBuffer)NULL,
                                                attribsY);
        if (!eglImageY)
        {
          EGLint err = eglGetError();
          CLog::Log(LOGERROR, "CRendererDRMPRIMEGLES::%s - failed to import PRIME buffer NV12 into EGL image: %d", __FUNCTION__, err);
          return false;
        }

        GLint attribsVU[] =
        {
          EGL_LINUX_DRM_FOURCC_EXT, fourcc_code('G', 'R', '8', '8'),
          EGL_WIDTH, (buffer->GetWidth() + 1) >> 1,
          EGL_HEIGHT, (buffer->GetHeight() + 1) >> 1,
          EGL_DMA_BUF_PLANE0_FD_EXT, descriptor->objects[layer->planes[1].object_index].fd,
          EGL_DMA_BUF_PLANE0_OFFSET_EXT, layer->planes[1].offset,
          EGL_DMA_BUF_PLANE0_PITCH_EXT, layer->planes[1].pitch,
          EGL_NONE
        };

        eglImageVU = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                                 EGL_NO_CONTEXT,
                                                 EGL_LINUX_DMA_BUF_EXT,
                                                 (EGLClientBuffer)NULL,
                                                 attribsVU);
        if (!eglImageVU)
        {
          EGLint err = eglGetError();
          CLog::Log(LOGERROR, "CRendererDRMPRIMEGLES::%s - failed to import PRIME buffer NV12 into EGL image: %d", __FUNCTION__, err);
          return false;
        }

        GLint format;

        glGenTextures(1, &m_textureY);
        glBindTexture(m_interop.textureTarget, m_textureY);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, (GLeglImageOES)eglImageY);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

        glGenTextures(1, &m_textureVU);
        glBindTexture(m_interop.textureTarget, m_textureVU);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, (GLeglImageOES)eglImageVU);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

        glBindTexture(m_interop.textureTarget, 0);

        break;
      }
      default:
        return false;
    }

    m_primebuffer = buffer;
    m_primebuffer->Acquire();
    return true;
  }

  return false;
}

void CDRMPRIMETexture::Unmap()
{
  if (!m_primebuffer)
    return;

  m_interop.eglDestroyImageKHR(m_interop.eglDisplay, eglImageY);
  m_interop.eglDestroyImageKHR(m_interop.eglDisplay, eglImageVU);

  glDeleteTextures(1, &m_textureY);
  glDeleteTextures(1, &m_textureVU);

  m_primebuffer->Release();
  m_primebuffer = nullptr;
}
