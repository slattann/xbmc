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

#include "VaapiEGL.h"

#include "cores/VideoPlayer/DVDCodecs/Video/VAAPI.h"
#include <va/va_drmcommon.h>
#include <drm_fourcc.h>
#include "utils/log.h"
#include "utils/EGLUtils.h"

using namespace VAAPI;

void CVaapiTexture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool& general, bool& hevc)
{
  general = hevc = false;
#if VA_CHECK_VERSION(1, 1, 0)
  CVaapi2Texture::TestInterop(vaDpy, eglDisplay, general, hevc);
  CLog::Log(LOGDEBUG, "Vaapi2 EGL interop test results: general %s, hevc %s", general ? "yes" : "no", hevc ? "yes" : "no");
#endif
  if (!general)
  {
    CVaapi1Texture::TestInterop(vaDpy, eglDisplay, general, hevc);
    CLog::Log(LOGDEBUG, "Vaapi1 EGL interop test results: general %s, hevc %s", general ? "yes" : "no", hevc ? "yes" : "no");
  }
}

CVaapiTexture* CVaapiTexture::CreateTexture(VADisplay vaDpy, EGLDisplay eglDisplay)
{
#if VA_CHECK_VERSION(1, 1, 0)
  bool general{}, hevc{};
  CVaapi2Texture::TestInterop(vaDpy, eglDisplay, general, hevc);
  if (general)
  {
    return new CVaapi2Texture;
  }
#endif
  return new CVaapi1Texture;
}

CVaapi1Texture::CVaapi1Texture()
{
  m_glSurface.vaImage.image_id = VA_INVALID_ID;
}

void CVaapi1Texture::Init(InteropInfo &interop)
{
  m_interop = interop;
}

bool CVaapi1Texture::Map(CVaapiRenderPicture *pic)
{
  VAStatus status;

  if (m_vaapiPic)
    return true;

  vaSyncSurface(pic->vadsp, pic->procPic.videoSurface);

  status = vaDeriveImage(pic->vadsp, pic->procPic.videoSurface, &m_glSurface.vaImage);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
    return false;
  }
  memset(&m_glSurface.vBufInfo, 0, sizeof(m_glSurface.vBufInfo));
  m_glSurface.vBufInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
  status = vaAcquireBufferHandle(pic->vadsp, m_glSurface.vaImage.buf, &m_glSurface.vBufInfo);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
    return false;
  }

  m_texWidth = m_glSurface.vaImage.width;
  m_texHeight = m_glSurface.vaImage.height;

  GLint attribs[23], *attrib;

  switch (m_glSurface.vaImage.format.fourcc)
  {
    case VA_FOURCC('N','V','1','2'):
    {
      attrib = attribs;
      *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
      *attrib++ = fourcc_code('R', '8', ' ', ' ');
      *attrib++ = EGL_WIDTH;
      *attrib++ = m_glSurface.vaImage.width;
      *attrib++ = EGL_HEIGHT;
      *attrib++ = m_glSurface.vaImage.height;
      *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
      *attrib++ = (intptr_t)m_glSurface.vBufInfo.handle;
      *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      *attrib++ = m_glSurface.vaImage.offsets[0];
      *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      *attrib++ = m_glSurface.vaImage.pitches[0];
      *attrib++ = EGL_NONE;
      m_glSurface.eglImageY = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                          EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                          attribs);
      if (!m_glSurface.eglImageY)
      {
        EGLint err = eglGetError();
        CLog::Log(LOGERROR, "failed to import VA buffer NV12 into EGL image: %d", err);
        return false;
      }

      attrib = attribs;
      *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
      *attrib++ = fourcc_code('G', 'R', '8', '8');
      *attrib++ = EGL_WIDTH;
      *attrib++ = (m_glSurface.vaImage.width + 1) >> 1;
      *attrib++ = EGL_HEIGHT;
      *attrib++ = (m_glSurface.vaImage.height + 1) >> 1;
      *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
      *attrib++ = (intptr_t)m_glSurface.vBufInfo.handle;
      *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      *attrib++ = m_glSurface.vaImage.offsets[1];
      *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      *attrib++ = m_glSurface.vaImage.pitches[1];
      *attrib++ = EGL_NONE;
      m_glSurface.eglImageVU = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                          EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                          attribs);
      if (!m_glSurface.eglImageVU)
      {
        EGLint err = eglGetError();
        CLog::Log(LOGERROR, "failed to import VA buffer NV12 into EGL image: %d", err);
        return false;
      }

      GLint format;

      glGenTextures(1, &m_textureY);
      glBindTexture(m_interop.textureTarget, m_textureY);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, m_glSurface.eglImageY);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glGenTextures(1, &m_textureVU);
      glBindTexture(m_interop.textureTarget, m_textureVU);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, m_glSurface.eglImageVU);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glBindTexture(m_interop.textureTarget, 0);

      break;
    }
    case VA_FOURCC('P','0','1','0'):
    {
      attrib = attribs;
      *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
      *attrib++ = fourcc_code('R', '1', '6', ' ');
      *attrib++ = EGL_WIDTH;
      *attrib++ = m_glSurface.vaImage.width;
      *attrib++ = EGL_HEIGHT;
      *attrib++ = m_glSurface.vaImage.height;
      *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
      *attrib++ = (intptr_t)m_glSurface.vBufInfo.handle;
      *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      *attrib++ = m_glSurface.vaImage.offsets[0];
      *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      *attrib++ = m_glSurface.vaImage.pitches[0];
      *attrib++ = EGL_NONE;
      m_glSurface.eglImageY = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                          EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                          attribs);
      if (!m_glSurface.eglImageY)
      {
        EGLint err = eglGetError();
        CLog::Log(LOGERROR, "failed to import VA buffer P010 into EGL image: %d", err);
        return false;
      }

      attrib = attribs;
      *attrib++ = EGL_LINUX_DRM_FOURCC_EXT;
      *attrib++ = fourcc_code('G', 'R', '3', '2');
      *attrib++ = EGL_WIDTH;
      *attrib++ = (m_glSurface.vaImage.width + 1) >> 1;
      *attrib++ = EGL_HEIGHT;
      *attrib++ = (m_glSurface.vaImage.height + 1) >> 1;
      *attrib++ = EGL_DMA_BUF_PLANE0_FD_EXT;
      *attrib++ = (intptr_t)m_glSurface.vBufInfo.handle;
      *attrib++ = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      *attrib++ = m_glSurface.vaImage.offsets[1];
      *attrib++ = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      *attrib++ = m_glSurface.vaImage.pitches[1];
      *attrib++ = EGL_NONE;
      m_glSurface.eglImageVU = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                          EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                          attribs);
      if (!m_glSurface.eglImageVU)
      {
        EGLint err = eglGetError();
        CLog::Log(LOGERROR, "failed to import VA buffer P010 into EGL image: %d", err);
        return false;
      }

      GLint format;

      glGenTextures(1, &m_textureY);
      glBindTexture(m_interop.textureTarget, m_textureY);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, m_glSurface.eglImageY);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glGenTextures(1, &m_textureVU);
      glBindTexture(m_interop.textureTarget, m_textureVU);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, m_glSurface.eglImageVU);
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);

      glBindTexture(m_interop.textureTarget, 0);

      break;
    }
    case VA_FOURCC('B','G','R','A'):
    {
      attrib = attribs;
      *attrib++ = EGL_DRM_BUFFER_FORMAT_MESA;
      *attrib++ = EGL_DRM_BUFFER_FORMAT_ARGB32_MESA;
      *attrib++ = EGL_WIDTH;
      *attrib++ = m_glSurface.vaImage.width;
      *attrib++ = EGL_HEIGHT;
      *attrib++ = m_glSurface.vaImage.height;
      *attrib++ = EGL_DRM_BUFFER_STRIDE_MESA;
      *attrib++ = m_glSurface.vaImage.pitches[0] / 4;
      *attrib++ = EGL_NONE;
      m_glSurface.eglImage = m_interop.eglCreateImageKHR(m_interop.eglDisplay, EGL_NO_CONTEXT,
                                         EGL_DRM_BUFFER_MESA,
                                         (EGLClientBuffer)m_glSurface.vBufInfo.handle,
                                         attribs);
      if (!m_glSurface.eglImage)
      {
        EGLint err = eglGetError();
        CLog::Log(LOGERROR, "failed to import VA buffer BGRA into EGL image: %d", err);
        return false;
      }

      glGenTextures(1, &m_texture);
      glBindTexture(m_interop.textureTarget, m_texture);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_interop.textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, m_glSurface.eglImage);

      glBindTexture(m_interop.textureTarget, 0);

      break;
    }
    default:
      return false;
  }

  m_vaapiPic = pic;
  m_vaapiPic->Acquire();
  return true;
}

void CVaapi1Texture::Unmap()
{
  if (!m_vaapiPic)
    return;

  if (m_glSurface.vaImage.image_id == VA_INVALID_ID)
    return;

  m_interop.eglDestroyImageKHR(m_interop.eglDisplay, m_glSurface.eglImageY);
  m_interop.eglDestroyImageKHR(m_interop.eglDisplay, m_glSurface.eglImageVU);

  VAStatus status;
  status = vaReleaseBufferHandle(m_vaapiPic->vadsp, m_glSurface.vaImage.buf);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
  }

  status = vaDestroyImage(m_vaapiPic->vadsp, m_glSurface.vaImage.image_id);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::Log(LOGERROR, "VAAPI::%s - Error: %s(%d)", __FUNCTION__, vaErrorStr(status), status);
  }

  m_glSurface.vaImage.image_id = VA_INVALID_ID;

  glDeleteTextures(1, &m_textureY);
  glDeleteTextures(1, &m_textureVU);

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

GLuint CVaapi1Texture::GetTextureY()
{
  return m_textureY;
}

GLuint CVaapi1Texture::GetTextureVU()
{
  return m_textureVU;
}

CSizeInt CVaapi1Texture::GetTextureSize()
{
  return {m_texWidth, m_texHeight};
}

void CVaapi1Texture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc)
{
  general = false;
  hevc = false;

  int major_version, minor_version;
  if (vaInitialize(vaDpy, &major_version, &minor_version) != VA_STATUS_SUCCESS)
  {
    vaTerminate(vaDpy);
    return;
  }

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  if (!eglCreateImageKHR || !eglDestroyImageKHR)
  {
    vaTerminate(vaDpy);
    return;
  }

  int width = 1920;
  int height = 1080;

  // create surfaces
  VASurfaceID surface;
  VAStatus status;
  VAImage image;
  VABufferInfo bufferInfo;

  if (vaCreateSurfaces(vaDpy,  VA_RT_FORMAT_YUV420,
                       width, height,
                       &surface, 1, NULL, 0) != VA_STATUS_SUCCESS)
  {
    vaTerminate(vaDpy);
    return;
  }

  // check interop

  status = vaDeriveImage(vaDpy, surface, &image);
  if (status == VA_STATUS_SUCCESS)
  {
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    status = vaAcquireBufferHandle(vaDpy, image.buf, &bufferInfo);
    if (status == VA_STATUS_SUCCESS)
    {
      EGLImageKHR eglImage;
      EGLint attribs[] = {
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_R8,
        EGL_WIDTH, image.width,
        EGL_HEIGHT, image.height,
        EGL_DMA_BUF_PLANE0_FD_EXT, static_cast<EGLint> (bufferInfo.handle),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint> (image.offsets[0]),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint> (image.pitches[0]),
        EGL_NONE
      };

      eglImage = eglCreateImageKHR(eglDisplay,
                                   EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                   attribs);
      if (eglImage)
      {
        eglDestroyImageKHR(eglDisplay, eglImage);
        general = true;
      }
    }
    vaDestroyImage(vaDpy, image.image_id);
  }
  vaDestroySurfaces(vaDpy, &surface, 1);

  if (general)
  {
    hevc = TestInteropHevc(vaDpy, eglDisplay);
  }

  vaTerminate(vaDpy);
}

bool CVaapi1Texture::TestInteropHevc(VADisplay vaDpy, EGLDisplay eglDisplay)
{
  bool ret = false;

  int width = 1920;
  int height = 1080;

  // create surfaces
  VASurfaceID surface;
  VAStatus status;
  VAImage image;
  VABufferInfo bufferInfo;

  VASurfaceAttrib attribs = { };
  attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs.type = VASurfaceAttribPixelFormat;
  attribs.value.type = VAGenericValueTypeInteger;
  attribs.value.value.i = VA_FOURCC_P010;

  if (vaCreateSurfaces(vaDpy,  VA_RT_FORMAT_YUV420_10BPP,
                       width, height,
                       &surface, 1, &attribs, 1) != VA_STATUS_SUCCESS)
  {
    return false;
  }

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  if (!eglCreateImageKHR || !eglDestroyImageKHR)
  {
    return false;
  }

  // check interop
  status = vaDeriveImage(vaDpy, surface, &image);
  if (status == VA_STATUS_SUCCESS)
  {
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    status = vaAcquireBufferHandle(vaDpy, image.buf, &bufferInfo);
    if (status == VA_STATUS_SUCCESS)
    {
      EGLImageKHR eglImage;
      EGLint attribs[] = {
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_GR1616,
        EGL_WIDTH, (image.width + 1) >> 1,
        EGL_HEIGHT, (image.height + 1) >> 1,
        EGL_DMA_BUF_PLANE0_FD_EXT, static_cast<EGLint> (bufferInfo.handle),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint> (image.offsets[1]),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint> (image.pitches[1]),
        EGL_NONE
      };

      eglImage = eglCreateImageKHR(eglDisplay,
                                   EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL,
                                   attribs);
      if (eglImage)
      {
        eglDestroyImageKHR(eglDisplay, eglImage);
        ret = true;
      }

    }
    vaDestroyImage(vaDpy, image.image_id);
  }

  vaDestroySurfaces(vaDpy, &surface, 1);

  return ret;
}

#if VA_CHECK_VERSION(1, 1, 0)
void CVaapi2Texture::Init(InteropInfo& interop)
{
  m_interop = interop;
  m_hasPlaneModifiers = CEGLUtils::HasExtension(m_interop.eglDisplay, "EGL_EXT_image_dma_buf_import_modifiers");
}

GLuint CVaapi2Texture::ImportImageToTexture(EGLImageKHR image)
{
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(m_interop.textureTarget, texture);
  m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, image);
  glBindTexture(m_interop.textureTarget, 0);
  return texture;
}

bool CVaapi2Texture::Map(CVaapiRenderPicture *pic)
{
  if (m_vaapiPic)
    return true;

  m_vaapiPic = pic;
  m_vaapiPic->Acquire();

  VAStatus status;

  VADRMPRIMESurfaceDescriptor surface;

  status = vaExportSurfaceHandle(pic->vadsp, pic->procPic.videoSurface,
                                 VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                 VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                 &surface);

  if (status != VA_STATUS_SUCCESS)
  {
    CLog::LogFunction(LOGWARNING, "CVaapi2Texture::Map", "vaExportSurfaceHandle failed - Error: %s (%d)", vaErrorStr(status), status);
    return false;
  }

  // Remember fds to close them later
  if (surface.num_objects > m_drmFDs.size())
    throw std::logic_error("Too many fds returned by vaExportSurfaceHandle");

  for (int object = 0; object < surface.num_objects; object++)
  {
    m_drmFDs[object].attach(surface.objects[object].fd);
  }

  status = vaSyncSurface(pic->vadsp, pic->procPic.videoSurface);
  if (status != VA_STATUS_SUCCESS)
  {
    CLog::LogFunction(LOGERROR, "CVaapiTexture::MapEsh", "vaSyncSurface - Error: %s (%d)", vaErrorStr(status), status);
    return false;
  }

  m_textureSize.Set(surface.width, surface.height);

  for (int layerNo = 0; layerNo < surface.num_layers; layerNo++)
  {
    int plane = 0;
    auto const& layer = surface.layers[layerNo];
    if (layer.num_planes != 1)
    {
      CLog::LogFunction(LOGDEBUG, "CVaapiTexture::MapEsh", "DRM-exported layer has %d planes - only 1 supported", layer.num_planes);
      return false;
    }
    auto const& object = surface.objects[layer.object_index[plane]];

    MappedTexture* texture{};
    EGLint width{m_textureSize.Width()};
    EGLint height{m_textureSize.Height()};

    switch (surface.num_layers)
    {
      case 2:
        switch (layerNo)
        {
          case 0:
            texture = &m_y;
            break;
          case 1:
            texture = &m_vu;
            if (surface.fourcc == VA_FOURCC_NV12 || surface.fourcc == VA_FOURCC_P010 || surface.fourcc == VA_FOURCC_P016)
            {
              // Adjust w/h for 4:2:0 subsampling on UV plane
              width = (width + 1) >> 1;
              height = (height + 1) >> 1;
            }
            break;
          default:
            throw std::logic_error("Impossible layer number");
        }
        break;
      default:
        CLog::LogFunction(LOGDEBUG, "CVaapiTexture::MapEsh", "DRM-exported surface %d layers - only 2 supported", surface.num_layers);
        return false;
    }

    EGLint attribs[13 + 4 /* modifiers */] = {
      EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint> (layer.drm_format),
      EGL_WIDTH, static_cast<EGLint> (width),
      EGL_HEIGHT, static_cast<EGLint> (height),
      EGL_DMA_BUF_PLANE0_FD_EXT, object.fd,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint> (layer.offset[plane]),
      EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint> (layer.pitch[plane]),
      // When adding parameters, be sure to update the &attribs reference for the
      // plane modifiers below
      EGL_NONE
    };
    if (m_hasPlaneModifiers)
    {
      EGLint* attrib = &attribs[12];
      *attrib++ = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
      *attrib++ = static_cast<EGLint> (object.drm_format_modifier);
      *attrib++ = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
      *attrib++ = static_cast<EGLint> (object.drm_format_modifier >> 32);
      *attrib++ = EGL_NONE;
    }

    texture->eglImage = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                                   EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                   attribs);
    if (!texture->eglImage)
    {
      CEGLUtils::LogError("Failed to import VA DRM surface into EGL image");
      return false;
    }

    texture->glTexture = ImportImageToTexture(texture->eglImage);
  }

  return true;
}

void CVaapi2Texture::Unmap(MappedTexture& texture)
{
  if (texture.eglImage != EGL_NO_IMAGE_KHR)
  {
    m_interop.eglDestroyImageKHR(m_interop.eglDisplay, texture.eglImage);
    texture.eglImage = EGL_NO_IMAGE_KHR;
    glDeleteTextures(1, &texture.glTexture);
  }
}

void CVaapi2Texture::Unmap()
{
  if (!m_vaapiPic)
    return;

  Unmap(m_y);
  Unmap(m_vu);
  for (auto& fd : m_drmFDs)
  {
    fd.reset();
  }

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

GLuint CVaapi2Texture::GetTextureY()
{
  return m_y.glTexture;
}

GLuint CVaapi2Texture::GetTextureVU()
{
  return m_vu.glTexture;
}

CSizeInt CVaapi2Texture::GetTextureSize()
{
  return m_textureSize;
}

bool CVaapi2Texture::TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, std::uint32_t rtFormat, std::int32_t pixelFormat)
{
  int width = 1920;
  int height = 1080;

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  if (!eglCreateImageKHR || !eglDestroyImageKHR)
  {
    return false;
  }

  // create surfaces
  VASurfaceID surface;
  VAStatus status;

  VASurfaceAttrib attribs = { };
  attribs.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs.type = VASurfaceAttribPixelFormat;
  attribs.value.type = VAGenericValueTypeInteger;
  attribs.value.value.i = pixelFormat;

  if (vaCreateSurfaces(vaDpy, rtFormat,
                       width, height,
                       &surface, 1, &attribs, 1) != VA_STATUS_SUCCESS)
  {
    return false;
  }

  // check interop
  VADRMPRIMESurfaceDescriptor drmPrimeSurface;
  status = vaExportSurfaceHandle(vaDpy, surface,
                                 VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                 VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                 &drmPrimeSurface);

  bool result = false;

  if (status == VA_STATUS_SUCCESS)
  {
    auto const& layer = drmPrimeSurface.layers[0];
    auto const& object = drmPrimeSurface.objects[layer.object_index[0]];
    EGLint attribs[] = {
      EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint> (drmPrimeSurface.layers[0].drm_format),
      EGL_WIDTH, width,
      EGL_HEIGHT, height,
      EGL_DMA_BUF_PLANE0_FD_EXT, static_cast<EGLint> (object.fd),
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint> (layer.offset[0]),
      EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint> (layer.pitch[0]),
      EGL_NONE
    };

    EGLImageKHR eglImage = eglCreateImageKHR(eglDisplay,
                                             EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                             attribs);
    if (eglImage)
    {
      eglDestroyImageKHR(eglDisplay, eglImage);
      result = true;
    }

    for (int object = 0; object < drmPrimeSurface.num_objects; object++)
    {
      close(drmPrimeSurface.objects[object].fd);
    }
  }

  vaDestroySurfaces(vaDpy, &surface, 1);

  return result;
}


void CVaapi2Texture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc)
{
  general = false;
  hevc = false;

  int major_version, minor_version;
  if (vaInitialize(vaDpy, &major_version, &minor_version) != VA_STATUS_SUCCESS)
  {
    vaTerminate(vaDpy);
    return;
  }

  general = TestEsh(vaDpy, eglDisplay, VA_RT_FORMAT_YUV420, VA_FOURCC_NV12);
  if (general)
  {
    hevc = TestEsh(vaDpy, eglDisplay, VA_RT_FORMAT_YUV420_10BPP, VA_FOURCC_P010);
  }

  vaTerminate(vaDpy);
}
#endif