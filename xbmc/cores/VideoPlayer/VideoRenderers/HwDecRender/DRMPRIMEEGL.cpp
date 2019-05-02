/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DRMPRIMEEGL.h"

#include "utils/log.h"

void CDRMPRIMETexture::Init(EGLDisplay eglDisplay)
{
  for (auto& eglImage : m_eglImage)
    eglImage.reset(new CEGLImage(eglDisplay));
}

bool CDRMPRIMETexture::Map(IVideoBufferDRMPRIME* buffer)
{
  if (m_primebuffer)
    return true;

  if (!buffer->Map())
    return false;

  m_texWidth = buffer->GetWidth();
  m_texHeight = buffer->GetHeight();

  AVDRMFrameDescriptor* descriptor = buffer->GetDescriptor();
  if (descriptor && descriptor->nb_layers)
  {
    if (descriptor->nb_layers >= 2)
      m_textureTarget = GL_TEXTURE_2D;

    for (int layer = 0; layer < descriptor->nb_layers; layer++)
    {
      std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;

      for (int plane = 0; plane < descriptor->layers[layer].nb_planes; plane++)
      {
        planes[plane].fd = descriptor->objects[descriptor->layers[layer].planes[plane].object_index].fd;
        planes[plane].offset = descriptor->layers[layer].planes[plane].offset;
        planes[plane].pitch = descriptor->layers[layer].planes[plane].pitch;
      }

      CEGLImage::EglAttrs attribs;

      attribs.width = m_texWidth;
      attribs.height = m_texHeight;

      if (layer >= 1)
      {
        switch(descriptor->layers[layer].format)
        {
        case DRM_FORMAT_R8:
          attribs.width = m_texWidth >> 1;
          attribs.height = m_texHeight >> 1;
        case DRM_FORMAT_GR88:
        case DRM_FORMAT_GR1616:
          attribs.width = m_texWidth >> 1;
          attribs.height = m_texHeight >> 1;
        }
      }

      attribs.format = descriptor->layers[layer].format;
      attribs.colorSpace = GetColorSpace(buffer->GetColorEncoding());
      attribs.colorRange = GetColorRange(buffer->GetColorRange());
      attribs.planes = planes;

      if (!m_eglImage[layer]->CreateImage(attribs))
        return false;

      glGenTextures(1, &m_texture[layer]);
      glBindTexture(m_textureTarget, m_texture[layer]);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      m_eglImage[layer]->UploadImage(m_textureTarget);
      glBindTexture(m_textureTarget, 0);
    }
  }

  m_primebuffer = buffer;
  m_primebuffer->Acquire();

  return true;
}

void CDRMPRIMETexture::Unmap()
{
  if (!m_primebuffer)
    return;

  for (auto& eglImage : m_eglImage)
    eglImage->DestroyImage();

  for (auto texture : m_texture)
    glDeleteTextures(1, &texture);

  m_primebuffer->Unmap();

  m_primebuffer->Release();
  m_primebuffer = nullptr;
}

int CDRMPRIMETexture::GetColorSpace(int colorSpace)
{
  switch(colorSpace)
  {
    case DRM_COLOR_YCBCR_BT2020:
      return EGL_ITU_REC2020_EXT;
    case DRM_COLOR_YCBCR_BT601:
      return EGL_ITU_REC601_EXT;
    case DRM_COLOR_YCBCR_BT709:
      return EGL_ITU_REC709_EXT;
    default:
      CLog::Log(LOGERROR, "CEGLImage::%s - failed to get colorspace for: %d", __FUNCTION__, colorSpace);
      break;
  }

  return -1;
}

int CDRMPRIMETexture::GetColorRange(int colorRange)
{
  switch(colorRange)
  {
    case DRM_COLOR_YCBCR_FULL_RANGE:
      return EGL_YUV_FULL_RANGE_EXT;
    case DRM_COLOR_YCBCR_LIMITED_RANGE:
      return EGL_YUV_NARROW_RANGE_EXT;
    default:
      CLog::Log(LOGERROR, "CEGLImage::%s - failed to get colorrange for: %d", __FUNCTION__, colorRange);
      break;
  }

  return -1;
}
