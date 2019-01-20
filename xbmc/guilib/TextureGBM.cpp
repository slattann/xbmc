/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TextureGBM.h"

#include "guilib/TextureManager.h"
#include "ServiceBroker.h"
#include "rendering/RenderSystem.h"
#include "windowing/gbm/WinSystemGbmEGLContext.h"

using namespace KODI::WINDOWING::GBM;

CGBMTexture::CGBMTexture(unsigned int width, unsigned int height, unsigned int format) :
  CBaseTexture(width, height, format)
{
  switch(format)
  {
  case XB_FMT_A8:
    m_fourcc = DRM_FORMAT_C8;
    break;
  case XB_FMT_RGBA8:
    m_fourcc = DRM_FORMAT_RGBA8888;
    break;
  case XB_FMT_RGB8:
    m_fourcc = DRM_FORMAT_RGB888;
    break;
  case XB_FMT_A8R8G8B8:
    m_fourcc = DRM_FORMAT_ARGB8888;
    break;
  }

  m_buffer.reset(new CGBMBufferObject(m_fourcc));
  m_eglImage.reset(new CEGLImage(static_cast<CWinSystemGbmEGLContext*>(CServiceBroker::GetWinSystem())->GetEGLDisplay()));
}

CGBMTexture::~CGBMTexture()
{
  ReleaseMemory();
  DestroyTextureObject();
}

void CGBMTexture::CreateTextureObject()
{
  glGenTextures(1, &m_texture);
}

void CGBMTexture::DestroyTextureObject()
{
  CServiceBroker::GetGUI()->GetTextureManager().ReleaseHwTexture(m_texture);
}

void CGBMTexture::LoadToGPU()
{
  ReleaseMemory();

  if (m_buffer->GetFd() < 0)
    return;

  if (!glIsTexture(m_texture))
    CreateTextureObject();

  glBindTexture(GL_TEXTURE_2D, m_texture);

  GLenum filter = (m_scalingMethod == TEXTURE_SCALING::NEAREST ? GL_NEAREST : GL_LINEAR);

  // Set the texture's stretching properties
  if (IsMipmapped())
  {
    GLenum mipmapFilter = (m_scalingMethod == TEXTURE_SCALING::NEAREST ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapFilter);
  }
  else
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;
  planes[0].fd = m_buffer->GetFd();
  planes[0].offset = 0;
  planes[0].pitch = m_buffer->GetStride();
  planes[0].modifier = m_buffer->GetModifier();

  CEGLImage::EglAttrs attribs;

  attribs.width = m_textureWidth;
  attribs.height = m_textureHeight;
  attribs.format = m_fourcc;
  attribs.planes = planes;

  if (m_eglImage->CreateImage(attribs))
  {
    m_eglImage->UploadImage(GL_TEXTURE_2D);
  }

  m_eglImage->DestroyImage();

  if (IsMipmapped())
  {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  m_loadedToGPU = true;
}

void CGBMTexture::BindToUnit(unsigned int unit)
{
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_texture);
}

void CGBMTexture::GetMemory()
{
  if (m_buffer)
  {
    if (m_buffer->CreateBufferObject(m_textureWidth, m_textureHeight))
    {
      m_pixels = m_buffer->GetMemory();
    }
  }
}

void CGBMTexture::ReleaseMemory()
{
  m_pixels = nullptr;

  if (m_buffer)
  {
    m_buffer->ReleaseMemory();
  }
}

unsigned int CGBMTexture::GetDestPitch() const
{
  if (m_buffer->GetFd() < 0)
  {
    return CBaseTexture::GetDestPitch();
  }

  return m_buffer->GetStride();
}
