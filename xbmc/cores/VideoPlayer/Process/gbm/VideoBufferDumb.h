/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "utils/DumbBufferObject.h"
#include "utils/GBMBufferObject.h"
#include "VideoBufferDRMPRIME.h"

#include <array>

class CVideoBufferDumb : public IVideoBufferDRMPRIME
{
public:
  CVideoBufferDumb(IVideoBufferPool& pool, int id, AVPixelFormat format, int width, int height);
  ~CVideoBufferDumb();

  void GetMemory();
  void ReleaseMemory();

  void GetPlanes(uint8_t*(&planes)[YuvImage::MAX_PLANES]) override;
  void GetStrides(int(&strides)[YuvImage::MAX_PLANES]) override;
  void SetDimensions(int width, int height);

  AVDRMFrameDescriptor* GetDescriptor() override { return &m_descriptor; }
  uint32_t GetWidth() const override { return m_width; }
  uint32_t GetHeight() const override { return m_height; }

  bool Alloc();
  void Destroy();

  void SetDescriptor(int format);
  //bool ConvertYUV420ToNV12();

private:
  AVDRMFrameDescriptor m_descriptor = {};

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint8_t* m_addr = nullptr;
  std::unique_ptr<CGBMBufferObject> m_bo;
};

class CVideoBufferPoolDumb : public IVideoBufferPool
{
public:
  ~CVideoBufferPoolDumb() override;
  CVideoBuffer* Get() override;
  void Return(int id) override;
  void Configure(AVPixelFormat format, int width, int height);
  bool IsConfigured() override;

  void SetProcessInfo(CProcessInfo *processInfo) { m_processInfo = processInfo; }

protected:
  int m_width = 0;
  int m_height = 0;
  AVPixelFormat m_pixFormat = AV_PIX_FMT_NONE;
  bool m_configured = false;
  CCriticalSection m_critSection;
  std::vector<CVideoBufferDumb*> m_all;
  std::deque<int> m_used;
  std::deque<int> m_free;

  CProcessInfo *m_processInfo = nullptr;
};
