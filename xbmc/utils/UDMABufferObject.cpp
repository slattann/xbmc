/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "UDMABufferObject.h"

#include "utils/BufferObjectFactory.h"
#include "utils/log.h"

#include <drm/drm_fourcc.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

std::unique_ptr<CBufferObject> CUDMABufferObject::Create()
{
  return std::make_unique<CUDMABufferObject>();
}

void CUDMABufferObject::Register()
{
  CBufferObjectFactory::RegisterBufferObject(CUDMABufferObject::Create);
}

CUDMABufferObject::~CUDMABufferObject()
{
  ReleaseMemory();
  DestroyBufferObject();

  close(m_udmafd);
}

static int round_up(int num, int factor)
{
  return num + factor - 1 - (num - 1) % factor;
}

bool CUDMABufferObject::CreateBufferObject(int format, int width, int height)
{
  if (m_fd >= 0)
    return true;

  m_width = ((width + 31) & ~31);
  m_height = ((height + 15) & ~15);

  uint32_t bpp{1};

  // I'm not sure about this yet. It seems that the RGB formats don't work properly without extra size
  // testing a snes emu with 256x224 res result in a strange image, simply multiplying the size by 2
  // makes it work. I'll investigate more later.

  switch (format)
  {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YUV420:
      m_height = height + (height >> 1);
      break;
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_RGB565:
      bpp = 2;
      break;
    case DRM_FORMAT_ARGB8888:
    default:
      bpp = 2;
      break;
  }

  m_size = round_up(m_width * m_height * bpp, getpagesize());
  m_stride = m_size / m_height;

  m_memfd = memfd_create("kodi", MFD_CLOEXEC | MFD_ALLOW_SEALING);

  if (m_memfd < 0)
  {
    throw std::system_error(errno, std::generic_category(), "memfd_create");
  }

  if (ftruncate(m_memfd, m_size) < 0)
  {
    throw std::system_error(errno, std::generic_category(), "ftruncate");
  }

  if (fcntl(m_memfd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
  {
    close(m_memfd);
    return false;
  }

  m_udmafd = open("/dev/udmabuf", O_RDWR);
  if (m_udmafd < 0)
  {
    throw std::system_error(errno, std::generic_category(), "open");
  }

  struct udmabuf_create_item create = {
      .memfd = static_cast<uint32_t>(m_memfd),
      .offset = 0,
      .size = m_size,
  };

  CLog::LogF(LOGERROR, "size={}", m_size);
  CLog::LogF(LOGERROR, "stride={}", m_stride);

  m_fd = ioctl(m_udmafd, UDMABUF_CREATE, &create);
  if (m_fd < 0)
  {
    throw std::system_error(errno, std::generic_category(), "UDMABUF_CREATE");
  }

  return true;
}

void CUDMABufferObject::DestroyBufferObject()
{
  if (m_fd < 0)
    return;

  int ret = close(m_memfd);
  if (ret < 0)
    CLog::LogF(LOGERROR, "close memfd failed, errno={}", strerror(errno));

  ret = close(m_udmafd);
  if (ret < 0)
    CLog::LogF(LOGERROR, "close /dev/udmabuf failed, errno={}", strerror(errno));

  m_memfd = -1;
  m_udmafd = -1;
  m_fd = -1;
  m_stride = 0;
  m_size = 0;
}

uint8_t* CUDMABufferObject::GetMemory()
{
  if (m_fd < 0)
    return nullptr;

  if (m_map)
  {
    CLog::LogF(LOGDEBUG, "already mapped fd={} map={}", m_fd, fmt::ptr(m_map));
    return m_map;
  }

  m_map = static_cast<uint8_t*>(mmap(nullptr, m_size, PROT_WRITE, MAP_SHARED, m_memfd, 0));
  if (m_map == MAP_FAILED)
  {
    CLog::LogF(LOGERROR, "mmap failed, errno={}", strerror(errno));
    return nullptr;
  }

  return m_map;
}

void CUDMABufferObject::ReleaseMemory()
{
  if (!m_map)
    return;

  int ret = munmap(m_map, m_size);
  if (ret < 0)
    CLog::LogF(LOGERROR, "munmap failed, errno={}", strerror(errno));

  m_map = nullptr;
}
