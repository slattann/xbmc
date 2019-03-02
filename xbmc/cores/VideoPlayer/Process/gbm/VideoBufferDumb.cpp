/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoBufferDumb.h"

#include "ServiceBroker.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "windowing/gbm/WinSystemGbm.h"

#include <sys/mman.h>

using namespace KODI::WINDOWING::GBM;

CVideoBufferDumb::CVideoBufferDumb(IVideoBufferPool& pool, int id, AVPixelFormat format, int width, int height)
  : IVideoBufferDRMPRIME(id)
{
  m_pixFormat = format;
  m_width = width;
  m_height = height;

  switch (format)
  {
  case AV_PIX_FMT_YUV420P:
  {
    m_bo.reset(new CGBMBufferObject(DRM_FORMAT_R8));
    break;
  }
  case AV_PIX_FMT_YUV420P9:
  case AV_PIX_FMT_YUV420P10:
  case AV_PIX_FMT_YUV420P12:
  case AV_PIX_FMT_YUV420P14:
  case AV_PIX_FMT_YUV420P16:
  {
    m_bo.reset(new CGBMBufferObject(DRM_FORMAT_R8));
    break;
  }
  default:
    break; // we shouldn't even get this far if we are given an unsupported pixel format
  }

  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={} format={} width={} height={}", __FUNCTION__, m_id, format, m_width, m_height);
}

CVideoBufferDumb::~CVideoBufferDumb()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={}", __FUNCTION__, m_id);
  ReleaseMemory();
  Destroy();
}

void CVideoBufferDumb::GetPlanes(uint8_t*(&planes)[YuvImage::MAX_PLANES])
{
  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  for (int i = 0; i < layer->nb_planes; i++)
  {
    planes[i] = m_addr + layer->planes[i].offset;
    CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - plane={} addr={}", __FUNCTION__, i, fmt::ptr(planes[i]));
  }
}

void CVideoBufferDumb::GetStrides(int(&strides)[YuvImage::MAX_PLANES])
{
  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  for (int i = 0; i < layer->nb_planes; i++)
  {
    strides[i] = layer->planes[i].pitch;
    CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - plane={} stride={}", __FUNCTION__, i, strides[i]);
  }
}

void CVideoBufferDumb::SetDescriptor(int format)
{
  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  descriptor->nb_layers = 1;

  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} descriptor={} layers={}", __FUNCTION__, fmt::ptr(descriptor), descriptor->nb_layers);

  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  switch (format)
  {
  case AV_PIX_FMT_YUV420P:
  case AV_PIX_FMT_YUV420P9:
  case AV_PIX_FMT_YUV420P10:
  case AV_PIX_FMT_YUV420P12:
  case AV_PIX_FMT_YUV420P14:
  case AV_PIX_FMT_YUV420P16:
  {
    descriptor->nb_objects = 1;

    descriptor->objects[0].fd = m_bo->GetFd();
    descriptor->objects[0].format_modifier = m_bo->GetModifier();

    layer->format = DRM_FORMAT_YUV420;
    layer->nb_planes = 3;

    layer->planes[0].object_index = 0;
    layer->planes[0].offset = 0;
    layer->planes[0].pitch = m_bo->GetStride();

    layer->planes[1].object_index = 0;
    layer->planes[1].offset = m_width * m_height;
    layer->planes[1].pitch = m_bo->GetStride() >> 1;

    layer->planes[2].object_index = 0;
    layer->planes[2].offset = layer->planes[1].offset + ((m_width * m_height) >> 2);
    layer->planes[2].pitch = m_bo->GetStride() >> 1;

    break;
  }
  case AV_PIX_FMT_NV12:
  {
    descriptor->nb_objects = 1;

    descriptor->objects[0].fd = m_bo->GetFd();
    descriptor->objects[0].format_modifier = m_bo->GetModifier();

    layer->format = DRM_FORMAT_NV12;
    layer->nb_planes = 2;

    layer->planes[0].object_index = 0;
    layer->planes[0].offset = 0;
    layer->planes[0].pitch = m_bo->GetStride();

    layer->planes[1].object_index = 0;
    layer->planes[1].offset = m_width * m_height;
    layer->planes[1].pitch = m_bo->GetStride();

    break;
  }
  default:
    break;
  }
}

void CVideoBufferDumb::GetMemory()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={}", __FUNCTION__, m_id);

  if (m_bo)
  {
    m_addr = m_bo->GetMemory();
  }
}

bool CVideoBufferDumb::Alloc()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={}", __FUNCTION__, m_id);

  if (m_bo)
  {
    if (!m_bo->CreateBufferObject(m_width, m_height * 2)) // YUV420
    {
      return false;
    }
  }

  return true;
}

void CVideoBufferDumb::ReleaseMemory()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={}", __FUNCTION__, m_id);

  if (m_bo)
  {
    m_bo->ReleaseMemory();
  }
}

void CVideoBufferDumb::Destroy()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id={} width={} height={}", __FUNCTION__, m_id, m_width, m_height);

  if (m_bo)
  {
    m_bo->DestroyBufferObject();
  }

  m_addr = nullptr;
}

// bool CVideoBufferDumb::ConvertYUV420ToNV12()
// {
//   // if (m_bo)
//   // {
//   //   m_addr = m_bo->GetMemory();
//   // }

//   // no idea if this will work
//   uint8_t* offset = static_cast<uint8_t*>(m_addr) + m_width * m_height;
//   for (int y = 0, h = (m_height >> 2); y < h; y++)
//   {
//     uint8_t* dst = offset + m_width * y;
//     uint8_t* src1 = offset + (m_width >> 1) * y;
//     uint8_t* src2 = offset + ((m_width * m_height) >> 2) + (m_width >> 1) * y;

//     for (int x = 0, w = (m_width >> 1); x < w; x++)
//     {
//       *dst++ = *(src1 + x);
//       *dst++ = *(src2 + x);
//     }
//   }

//   SetDescriptor(AV_PIX_FMT_NV12);

//   return true;
// }

CVideoBufferPoolDumb::~CVideoBufferPoolDumb()
{
  CSingleLock lock(m_critSection);

  for (auto buf : m_all)
  {
    delete buf;
  }
}

CVideoBuffer* CVideoBufferPoolDumb::Get()
{
  CSingleLock lock(m_critSection);

  CVideoBufferDumb* buf = nullptr;
  if (!m_free.empty())
  {
    int idx = m_free.front();
    m_free.pop_front();
    m_used.push_back(idx);
    buf = m_all[idx];
  }
  else
  {
    int id = m_all.size();
    buf = new CVideoBufferDumb(*this, id, m_pixFormat, m_width, m_height);
    buf->Alloc();
    buf->GetMemory();
    buf->SetDescriptor(m_pixFormat);
    m_all.push_back(buf);
    m_used.push_back(id);
  }

  CLog::Log(LOGDEBUG,"CVideoBufferPoolDumb::{} - m_all={} m_used={} m_free={}", __FUNCTION__, m_all.size(), m_used.size(), m_free.size());

  buf->Acquire(GetPtr());
  return buf;
}

void CVideoBufferPoolDumb::Return(int id)
{
  CSingleLock lock(m_critSection);

  auto it = m_used.begin();
  while (it != m_used.end())
  {
    if (*it == id)
    {
      m_used.erase(it);
      break;
    }
    else
    {
      ++it;
    }
  }

  m_free.push_back(id);
}

void CVideoBufferPoolDumb::Configure(AVPixelFormat format, int width, int height)
{
  m_pixFormat = format;
  m_width = width;
  m_height = height;
  m_configured = true;
  CLog::Log(LOGDEBUG, "CVideoBufferPoolDumb::{} - format={} width={} height={}", __FUNCTION__, format, width, height);
}

inline bool CVideoBufferPoolDumb::IsConfigured()
{
  return m_configured;
}
