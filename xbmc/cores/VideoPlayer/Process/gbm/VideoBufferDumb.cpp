/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoBufferDumb.h"

#include "ServiceBroker.h"
#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "threads/SingleLock.h"
#include "utils/BufferObject.h"
#include "utils/log.h"
#include "windowing/gbm/WinSystemGbm.h"

#include <linux/dma-buf.h>
#include <sys/mman.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

#define DUMB_BUFFER_NV12 0

using namespace KODI::WINDOWING::GBM;

CVideoBufferDumb::CVideoBufferDumb(IVideoBufferPool& pool, int id)
  : CVideoBufferDRMPRIMEFFmpeg(pool, id), m_bo(CBufferObject::GetBufferObject())
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{}", __FUNCTION__, m_id);
}

CVideoBufferDumb::~CVideoBufferDumb()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{}", __FUNCTION__, m_id);
  Unref();
  Destroy();
}

AVDRMFrameDescriptor* CVideoBufferDumb::GetDescriptor() const
{
  return const_cast<AVDRMFrameDescriptor*>(&m_descriptor);
}

void CVideoBufferDumb::GetPlanes(uint8_t* (&planes)[YuvImage::MAX_PLANES])
{
  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  for (int i = 0; i < layer->nb_planes; i++)
    planes[i] = static_cast<uint8_t*>(m_addr) + layer->planes[i].offset;
}

void CVideoBufferDumb::GetStrides(int (&strides)[YuvImage::MAX_PLANES])
{
  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  for (int i = 0; i < layer->nb_planes; i++)
    strides[i] = layer->planes[i].pitch;
}

void CVideoBufferDumb::Create(uint32_t width, uint32_t height)
{
  if (m_width == width && m_height == height)
    return;

  Destroy();

  uint32_t stride;

  int format;
#if defined(DUMB_BUFFER_NV12)
  format = DRM_FORMAT_NV12;
#else
  format = DRM_FORMAT_YUV420;
#endif

  if (m_bo->CreateBufferObject(format, width, height))
  {
    m_addr = m_bo->GetMemory();

    if (m_addr)
    {
      stride = m_bo->GetStride();
    }
  }
  else
  {
    CLog::Log(LOGNOTICE, "CVideoBufferDumb::{} - failed to create buffer object", __FUNCTION__);
  }

  m_fd = m_bo->GetFd();
  m_size = stride * height;

  m_width = width;
  m_height = height;

  CLog::Log(LOGNOTICE, "CVideoBufferDumb::{} - id={} width={} height={} stride={} size={}",
            __FUNCTION__, m_id, m_width, m_height, stride, m_size);

  AVDRMFrameDescriptor* descriptor = &m_descriptor;
  descriptor->nb_objects = 1;
  descriptor->objects[0].fd = m_fd;
  descriptor->objects[0].size = m_size;
  descriptor->nb_layers = 1;
  AVDRMLayerDescriptor* layer = &descriptor->layers[0];
#if !DUMB_BUFFER_NV12
  layer->format = DRM_FORMAT_YUV420;
  layer->nb_planes = 3;
  layer->planes[0].offset = 0;
  layer->planes[0].pitch = m_width;
  layer->planes[1].offset = m_width * m_height;
  layer->planes[1].pitch = m_width >> 1;
  layer->planes[2].offset = layer->planes[1].offset + ((m_width * m_height) >> 2);
  layer->planes[2].pitch = m_width >> 1;
#else
  layer->format = DRM_FORMAT_NV12;
  layer->nb_planes = 2;
  layer->planes[0].offset = 0;
  layer->planes[0].pitch = m_width;
  layer->planes[1].offset = m_width * m_height;
  layer->planes[1].pitch = m_width;
#endif
}

void CVideoBufferDumb::Destroy()
{
  if (!m_width && !m_height)
    return;

  m_bo->DestroyBufferObject();

  m_width = 0;
  m_height = 0;
}

void CVideoBufferDumb::SyncStart()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{}", __FUNCTION__, m_id);

  struct dma_buf_sync sync = {.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW};
  int ret = drmIoctl(m_fd, DMA_BUF_IOCTL_SYNC, &sync);
  if (ret)
    CLog::Log(LOGERROR, "CVideoBufferDumb::{} - ioctl DMA_BUF_IOCTL_SYNC failed, ret={} errno={}",
              __FUNCTION__, ret, errno);
}

void CVideoBufferDumb::SyncEnd()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{}", __FUNCTION__, m_id);

  struct dma_buf_sync sync = {.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW};
  int ret = drmIoctl(m_fd, DMA_BUF_IOCTL_SYNC, &sync);
  if (ret)
    CLog::Log(LOGERROR, "CVideoBufferDumb::{} - ioctl DMA_BUF_IOCTL_SYNC failed, ret={} errno={}",
              __FUNCTION__, ret, errno);
}

void CVideoBufferDumb::Unref()
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{}", __FUNCTION__, m_id);
}

void CVideoBufferDumb::Alloc(AVCodecContext* avctx, AVFrame* frame)
{
  int width = (frame->width + 31) & ~31;
  int height = (frame->height + 15) & ~15;
  int linesize_align[AV_NUM_DATA_POINTERS];

  avcodec_align_dimensions2(avctx, &width, &height, linesize_align);

  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{} width:{} height:{}", __FUNCTION__, m_id, width,
            height);

  Create(width, height);
}

void CVideoBufferDumb::Export(AVFrame* frame)
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{} width:{} height:{}", __FUNCTION__, m_id,
            m_width, m_height);

  YuvImage image = {};
  GetPlanes(image.plane);
  GetStrides(image.stride);

  for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
  {
    frame->data[i] = i < YuvImage::MAX_PLANES ? image.plane[i] : nullptr;
    frame->linesize[i] = i < YuvImage::MAX_PLANES ? image.stride[i] : 0;
    frame->buf[i] = i == 0 ? frame->opaque_ref : nullptr;
  }

  frame->extended_data = frame->data;
  frame->opaque_ref = nullptr;
}

void CVideoBufferDumb::Import(AVFrame* frame)
{
  CLog::Log(LOGDEBUG, "CVideoBufferDumb::{} - id:{} width:{} height:{}", __FUNCTION__, m_id,
            m_width, m_height);

  YuvImage image = {};
  GetPlanes(image.plane);
  GetStrides(image.stride);

  memcpy(image.plane[0], frame->data[0], frame->linesize[0] * frame->height);
#if !DUMB_BUFFER_NV12
  memcpy(image.plane[1], frame->data[1], frame->linesize[1] * (frame->height >> 1));
  memcpy(image.plane[2], frame->data[2], frame->linesize[2] * (frame->height >> 1));
#else
  uint8_t* offset = static_cast<uint8_t*>(image.plane[1]);
  for (int y = 0, h = frame->height >> 1; y < h; y++)
  {
    uint8_t* dst = offset + image.stride[1] * y;
    uint8_t* src1 = frame->data[1] + frame->linesize[1] * y;
    uint8_t* src2 = frame->data[2] + frame->linesize[2] * y;

    for (int x = 0, w = frame->width >> 1; x < w; x++)
    {
      *dst++ = *(src1 + x);
      *dst++ = *(src2 + x);
    }
  }
#endif
}

CVideoBufferPoolDumb::~CVideoBufferPoolDumb()
{
  CLog::Log(LOGDEBUG, "CVideoBufferPoolDumb::{}", __FUNCTION__);
  for (auto buf : m_all)
    delete buf;
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
    buf = new CVideoBufferDumb(*this, id);
    m_all.push_back(buf);
    m_used.push_back(id);
  }

  CLog::Log(LOGDEBUG, "CVideoBufferPoolDumb::{} - id:{}", __FUNCTION__, buf->GetId());
  buf->Acquire(GetPtr());
  return buf;
}

void CVideoBufferPoolDumb::Return(int id)
{
  CSingleLock lock(m_critSection);

  CLog::Log(LOGDEBUG, "CVideoBufferPoolDumb::{} - id:{}", __FUNCTION__, id);
  m_all[id]->Unref();
  auto it = m_used.begin();
  while (it != m_used.end())
  {
    if (*it == id)
    {
      m_used.erase(it);
      break;
    }
    else
      ++it;
  }
  m_free.push_back(id);
}
