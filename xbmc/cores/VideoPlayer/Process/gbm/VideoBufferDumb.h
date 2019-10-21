/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VideoBufferDRMPRIME.h"

#include <memory>

class CBufferObject;

class CVideoBufferDumb : public CVideoBufferDRMPRIMEFFmpeg
{
public:
  CVideoBufferDumb(IVideoBufferPool& pool, int id);
  ~CVideoBufferDumb();
  AVDRMFrameDescriptor* GetDescriptor() const override;
  void GetPlanes(uint8_t* (&planes)[YuvImage::MAX_PLANES]) override;
  void GetStrides(int (&strides)[YuvImage::MAX_PLANES]) override;
  void Unref();

  void Alloc(struct AVCodecContext* avctx, AVFrame* frame);
  void Export(AVFrame* frame);
  void Import(AVFrame* frame);

  void SyncStart();
  void SyncEnd();

private:
  void Create(uint32_t width, uint32_t height);
  void Destroy();

  std::unique_ptr<CBufferObject> m_bo;

  AVDRMFrameDescriptor m_descriptor = {};
  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint32_t m_handle = 0;
  uint64_t m_size = 0;
  uint64_t m_offset = 0;
  void* m_addr = nullptr;
  int m_fd = -1;
};

class CVideoBufferPoolDumb : public IVideoBufferPool
{
public:
  ~CVideoBufferPoolDumb();
  void Return(int id) override;
  CVideoBuffer* Get() override;

protected:
  CCriticalSection m_critSection;
  std::vector<CVideoBufferDumb*> m_all;
  std::deque<int> m_used;
  std::deque<int> m_free;
};
