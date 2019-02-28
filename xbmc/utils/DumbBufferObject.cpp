/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DumbBufferObject.h"

#include "ServiceBroker.h"
#include "utils/log.h"
#include "windowing/gbm/WinSystemGbm.h"
#include "windowing/gbm/WinSystemGbmEGLContext.h"

#include <drm/drm_fourcc.h>
#include <sys/mman.h>

using namespace KODI::WINDOWING::GBM;

CDumbBufferObject::CDumbBufferObject(int format)
{
  m_device = static_cast<CWinSystemGbmEGLContext*>(CServiceBroker::GetWinSystem())->GetDrm()->GetFileDescriptor();

  switch (format)
  {
  case DRM_FORMAT_R8:
  {
    m_bpp = 8;
    break;
  }
  case DRM_FORMAT_RGB565:
  case DRM_FORMAT_R16:
  {
    m_bpp = 16;
    break;
  }
  case DRM_FORMAT_ARGB8888:
  {
    m_bpp = 32;
    break;
  }
  default:
    break; // we shouldn't even get this far if we are given an unsupported pixel format
  }

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - format={} bpp={}", __FUNCTION__, format, m_bpp);
}

CDumbBufferObject::~CDumbBufferObject()
{
  ReleaseMemory();
  DestroyBufferObject();
}

bool CDumbBufferObject::CreateBufferObject(int width, int height)
{
  if (m_fd >= 0)
    return true;

  m_width = width;
  m_height = height;

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - width={} height={} bpp={}", __FUNCTION__, m_width, m_height, m_bpp);

  struct drm_mode_create_dumb create_dumb =
  {
    .height = static_cast<uint32_t>(m_height),
    .width = static_cast<uint32_t>(m_width),
    .bpp = m_bpp
  };

  int ret = drmIoctl(m_device, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - ioctl DRM_IOCTL_MODE_CREATE_DUMB failed, ret={} errno={}", __FUNCTION__, ret, strerror(errno));
    return false;
  }

  m_handle = create_dumb.handle;
  m_size = create_dumb.size;
  m_stride = create_dumb.pitch;

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - handle={} size={} stride={}", __FUNCTION__, m_handle, m_size, m_stride);

  struct drm_prime_handle prime_handle =
  {
    .handle = m_handle
  };

  ret = drmIoctl(m_device, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_handle);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - ioctl DRM_IOCTL_PRIME_HANDLE_TO_FD failed, ret={} errno={}", __FUNCTION__, ret, strerror(errno));
    return false;
  }

  m_fd = prime_handle.fd;

  return true;
}

void CDumbBufferObject::DestroyBufferObject()
{
  if (m_fd < 0)
    return;

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - fd={}", __FUNCTION__, m_fd);

  int ret = close(m_fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - close failed, errno={}", __FUNCTION__, strerror(errno));
  }

  m_fd = -1;

  // struct drm_mode_destroy_dumb destroy_dumb =
  // {
  //   .handle = m_handle
  // };

  // ret = drmIoctl(m_device, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
  // if (ret < 0)
  // {
  //   CLog::Log(LOGERROR, "CDumbBufferObject::{} - ioctl DRM_IOCTL_MODE_DESTROY_DUMB failed, ret={} errno={}", __FUNCTION__, ret, strerror(errno));
  // }

  m_stride = 0;
  m_handle = 0;
  m_size = 0;
}

uint8_t* CDumbBufferObject::GetMemory()
{
  if (m_fd < 0)
    return nullptr;

  if (m_map)
  {
    CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - already mapped fd={} map={}", __FUNCTION__, m_fd, fmt::ptr(m_map));
    return m_map;
  }

  struct drm_mode_map_dumb map_dumb =
  {
    .handle = m_handle
  };

  int ret = drmIoctl(m_device, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - ioctl DRM_IOCTL_MODE_MAP_DUMB failed, ret={} errno={}", __FUNCTION__, ret, strerror(errno));
    return nullptr;
  }

  m_offset = map_dumb.offset;

  m_map = static_cast<uint8_t*>(mmap(nullptr, m_size, PROT_WRITE, MAP_SHARED, m_device, m_offset));
  if (m_map == MAP_FAILED)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - mmap failed, errno={}", __FUNCTION__, strerror(errno));
    return nullptr;
  }

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - fd={} map={}", __FUNCTION__, m_fd, fmt::ptr(m_map));

  return m_map;
}

void CDumbBufferObject::ReleaseMemory()
{
  if (!m_map)
    return;

  CLog::Log(LOGDEBUG, "CDumbBufferObject::{} - fd={} map={}", __FUNCTION__, m_fd, fmt::ptr(m_map));

  int ret = munmap(m_map, m_size);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CDumbBufferObject::{} - munmap failed, errno={}", __FUNCTION__, strerror(errno));
  }

  m_map = nullptr;
  m_offset = 0;
}

int CDumbBufferObject::GetFd()
{
  return m_fd;
}

int CDumbBufferObject::GetStride()
{
  return m_stride;
}

uint64_t CDumbBufferObject::GetModifier()
{
  return DRM_FORMAT_MOD_LINEAR;
}
