/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GBMUtils.h"
#include "utils/log.h"

#include "utils/EGLImage.h"
#include "utils/FrameBufferObject.h"

#include "ServiceBroker.h"
#include "windowing/gbm/WinSystemGbmGLESContext.h"


using namespace KODI::WINDOWING::GBM;

bool CGBMUtils::CreateDevice(int fd)
{
  if (m_device)
    CLog::Log(LOGWARNING, "CGBMUtils::%s - device already created", __FUNCTION__);

  m_device = gbm_create_device(fd);
  if (!m_device)
  {
    CLog::Log(LOGERROR, "CGBMUtils::%s - failed to create device", __FUNCTION__);
    return false;
  }

  return true;
}

void CGBMUtils::DestroyDevice()
{
  if (!m_device)
    CLog::Log(LOGWARNING, "CGBMUtils::%s - device already destroyed", __FUNCTION__);

  if (m_device)
  {
    gbm_device_destroy(m_device);
    m_device = nullptr;
  }
}

bool CGBMUtils::CreateSurface(int width, int height, uint32_t format, const uint64_t *modifiers, const int modifiers_count)
{
  CWinSystemGbmGLESContext* winSystem = dynamic_cast<CWinSystemGbmGLESContext*>(CServiceBroker::GetWinSystem());
  if (!winSystem)
  {
    return false;
  }

  for (int i = 0; i < NUM_BUFS; i++)
  {
#if defined(HAS_GBM_MODIFIERS)
    m_surface[i] = gbm_bo_create_with_modifiers(m_device, width, height, format, modifiers, modifiers_count);
#endif

    if (!m_surface[i])
    {
      m_surface[i] = gbm_bo_create(m_device, width, height, format, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    }

    if (!m_surface[i])
    {
      CLog::Log(LOGERROR, "CGBMUtils::%s - failed to create surface", __FUNCTION__);
      return false;
    }

    CLog::Log(LOGDEBUG, "CGBMUtils::%s - created surface with size %dx%d", __FUNCTION__, width, height);

    m_eglImage[i].reset(new CEGLImage(winSystem->GetEGLDisplay()));

    std::array<CEGLImage::EglPlane, CEGLImage::MAX_NUM_PLANES> planes;
    for (int j = 0; j < gbm_bo_get_plane_count(m_surface[i]); j++)
    {
      planes[j].fd = gbm_bo_get_fd(m_surface[i]),
      planes[j].offset = gbm_bo_get_offset(m_surface[i], j);
      planes[j].pitch = gbm_bo_get_stride_for_plane(m_surface[i], j);
      planes[j].modifier = gbm_bo_get_modifier(m_surface[i]);
    }

    CEGLImage::EglAttrs attribs;
    attribs.width = gbm_bo_get_width(m_surface[i]);
    attribs.height = gbm_bo_get_height(m_surface[i]);
    attribs.format = gbm_bo_get_format(m_surface[i]);
    attribs.planes = planes;

    if (!m_eglImage[i]->CreateImage(attribs))
    {
      return false;
    }

    m_fbo[i].reset(new CFrameBufferObject());

    m_fbo[i]->Initialize();

    m_eglImage[i]->UploadImage(GL_TEXTURE_2D);

    m_fbo[i]->CreateAndBindToTexture(GL_TEXTURE_2D, width, height, GL_RGBA);
  }

  return true;
}

void CGBMUtils::DestroySurface()
{
  for (int i = 0; i < NUM_BUFS; i++)
  {
    if (m_fbo[i])
    {
      m_fbo[i]->Cleanup();
    }

    if (m_eglImage[i])
    {
      m_eglImage[i]->DestroyImage();
    }

    if (!m_surface[i])
      CLog::Log(LOGWARNING, "CGBMUtils::%s - surface already destroyed", __FUNCTION__);

    if (m_surface[i])
    {
      ReleaseBuffer();

      gbm_bo_destroy(m_surface[i]);
      m_surface[i] = nullptr;
    }
  }
}

struct gbm_bo *CGBMUtils::LockFrontBuffer()
{
  uint32_t back_buffer = (front_buffer + 1) % NUM_BUFS;

  m_next_bo = m_surface[back_buffer];

  front_buffer = back_buffer;

  // todo: egl fencing
  glFinish();

  return m_next_bo;
}

void CGBMUtils::ReleaseBuffer()
{
  uint32_t back_buffer = (front_buffer + 1) % NUM_BUFS;

  m_fbo[back_buffer]->BeginRender();
}
