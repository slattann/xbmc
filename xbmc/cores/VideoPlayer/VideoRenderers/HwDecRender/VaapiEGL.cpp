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

CVaapiTexture::CVaapiTexture()
{
  m_glSurface.vaImage.image_id = VA_INVALID_ID;
}

void CVaapiTexture::Init(InteropInfo &interop)
{
  m_interop = interop;
  m_hasPlaneModifiers = CEGLUtils::HasExtension(m_interop.eglDisplay, "EGL_EXT_image_dma_buf_import_modifiers");
}

GLuint CVaapiTexture::ImportImageToTexture(EGLImageKHR image)
{
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(m_interop.textureTarget, texture);
  m_interop.glEGLImageTargetTexture2DOES(m_interop.textureTarget, image);
  glBindTexture(m_interop.textureTarget, 0);
  return texture;
}

bool CVaapiTexture::MapEsh(CVaapiRenderPicture *pic)
{
#if VA_CHECK_VERSION(1, 1, 0)
  if (m_exportSurfaceUnimplemented)
    return false;
  
  VAStatus status;
  
  status = vaExportSurfaceHandle(pic->vadsp, pic->procPic.videoSurface,
                                 VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                 VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                 &m_glSurface.vaDrmPrimeSurface);
  
  if (status != VA_STATUS_SUCCESS)
  {
    if (status == VA_STATUS_ERROR_UNIMPLEMENTED)
    {
      CLog::LogFunction(LOGDEBUG, "CVaapiTexture::MapEsh", "No driver support for vaExportSurfaceHandle");
      // No need to retry at any time
      m_exportSurfaceUnimplemented = true;
    }
    CLog::LogFunction(LOGWARNING, "CVaapiTexture::MapEsh", "vaExportSurfaceHandle failed - Error: %s (%d)", vaErrorStr(status), status);
    return false;
  }
  
  auto const& surface = m_glSurface.vaDrmPrimeSurface;
  
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
  
  m_texWidth = surface.width;
  m_texHeight = surface.height;
  
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

    EGLImageKHR* eglImage{};
    GLuint* glTexture{};
    EGLint width{m_texWidth};
    EGLint height{m_texHeight};

    switch (surface.num_layers)
    {
      case 2:
        switch (layerNo)
        {
          case 0:
            eglImage = &m_glSurface.eglImageY;
            glTexture = &m_textureY;
            break;
          case 1:
            eglImage = &m_glSurface.eglImageVU;
            glTexture = &m_textureVU;
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
    
    *eglImage = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                            EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                            attribs);
    if (!*eglImage)
    {
      CEGLUtils::LogError("Failed to import VA DRM surface into EGL image");
      return false;
    }
    
    *glTexture = ImportImageToTexture(*eglImage);
   }
  return true;
#else
  return false;
#endif
}

bool CVaapiTexture::MapImage(CVaapiRenderPicture *pic)
{
  vaSyncSurface(pic->vadsp, pic->procPic.videoSurface);

  VAStatus status = vaDeriveImage(pic->vadsp, pic->procPic.videoSurface, &m_glSurface.vaImage);
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
  
  struct {
    EGLint drmFormat;
    EGLImageKHR* eglImage;
    GLuint* glTexture;
    EGLint width, heigth;
  } surfaceTargets[2] ={
    {DRM_FORMAT_R8, &m_glSurface.eglImageY, &m_textureY, m_texWidth, m_texHeight},
    {DRM_FORMAT_GR88, &m_glSurface.eglImageVU, &m_textureVU, (m_texWidth + 1) >> 1, (m_texHeight + 1) >> 1}
  };

  switch (m_glSurface.vaImage.format.fourcc)
  {
    case VA_FOURCC_NV12:
      // defaults are fine
      break;
    case VA_FOURCC_P010:
    case VA_FOURCC_P016:
      surfaceTargets[0].drmFormat = DRM_FORMAT_R16;
      surfaceTargets[1].drmFormat = DRM_FORMAT_GR1616;
      break;
    default:
      CLog::Log(LOGERROR, "CVaapiTexture::%s - Unknown VAImage format %x", __FUNCTION__, m_glSurface.vaImage.format.fourcc);
      return false;
  }

  if (m_glSurface.vaImage.num_planes != 2)
  {
    CLog::Log(LOGERROR, "CVaapiTexture::%s - VAImage has %d planes instead of expected 2", m_glSurface.vaImage.num_planes);
    return false;
  }

  for (int planeNo = 0; planeNo < m_glSurface.vaImage.num_planes; planeNo++)
  {
    auto const& targetAttribs = surfaceTargets[planeNo];
    EGLint attribs[] = {
      EGL_LINUX_DRM_FOURCC_EXT, targetAttribs.drmFormat,
      EGL_WIDTH, targetAttribs.width,
      EGL_HEIGHT, targetAttribs.heigth,
      EGL_DMA_BUF_PLANE0_FD_EXT, static_cast<EGLint> (m_glSurface.vBufInfo.handle),
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint> (m_glSurface.vaImage.offsets[planeNo]),
      EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint> (m_glSurface.vaImage.pitches[planeNo]),
      EGL_NONE
    };
    *targetAttribs.eglImage = m_interop.eglCreateImageKHR(m_interop.eglDisplay,
                                                          EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                                                          attribs);
    if (!*targetAttribs.eglImage)
    {
      CEGLUtils::LogError("Failed to import VA DRM surface into EGL image");
      return false;
    }

    *targetAttribs.glTexture = ImportImageToTexture(*targetAttribs.eglImage);
  }

  return true;
}

bool CVaapiTexture::Map(CVaapiRenderPicture *pic)
{
  if (m_vaapiPic)
    return true;
  
  if (!MapEsh(pic) && !MapImage(pic))
    return false;

  m_vaapiPic = pic;
  m_vaapiPic->Acquire();
  return true;
}

void CVaapiTexture::Unmap()
{
  if (!m_vaapiPic)
    return;

  if (m_glSurface.eglImageY != EGL_NO_IMAGE_KHR)
  {
    m_interop.eglDestroyImageKHR(m_interop.eglDisplay, m_glSurface.eglImageY);
    glDeleteTextures(1, &m_textureY);
    m_glSurface.eglImageY = EGL_NO_IMAGE_KHR;
  }
  if (m_glSurface.eglImageVU != EGL_NO_IMAGE_KHR)
  {
    m_interop.eglDestroyImageKHR(m_interop.eglDisplay, m_glSurface.eglImageVU);
    glDeleteTextures(1, &m_textureVU);
    m_glSurface.eglImageVU = EGL_NO_IMAGE_KHR;
  }

  if (m_glSurface.vaImage.image_id != VA_INVALID_ID)
  {
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
  }
  
  for (auto& fd : m_drmFDs)
  {
    fd.reset();
  }

  m_vaapiPic->Release();
  m_vaapiPic = nullptr;
}

bool CVaapiTexture::TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, VASurfaceID surface, int width, int height)
{
#if VA_CHECK_VERSION(1, 1, 0)
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");

  VADRMPRIMESurfaceDescriptor drmPrimeSurface;
  VAStatus status = vaExportSurfaceHandle(vaDpy, surface,
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
  return result;
#else
  return false;
#endif
}

void CVaapiTexture::TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc)
{
  general = false;
  hevc = false;

  int major_version, minor_version;
  if (vaInitialize(vaDpy, &major_version, &minor_version) != VA_STATUS_SUCCESS)
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
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  if (!eglCreateImageKHR || !eglDestroyImageKHR)
  {
    vaTerminate(vaDpy);
    return;
  }

  general = TestEsh(vaDpy, eglDisplay, surface, width, height);

  if (!general)
  {
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
  }
  vaDestroySurfaces(vaDpy, &surface, 1);

  if (general)
  {
    hevc = TestInteropHevc(vaDpy, eglDisplay);
  }

  vaTerminate(vaDpy);
}

bool CVaapiTexture::TestInteropHevc(VADisplay vaDpy, EGLDisplay eglDisplay)
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
  ret = TestEsh(vaDpy, eglDisplay, surface, width, height);

  if (!ret)
  {
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
  }
  
  vaDestroySurfaces(vaDpy, &surface, 1);
  
  return ret;
}
