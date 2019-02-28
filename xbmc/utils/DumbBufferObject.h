/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/IBufferObject.h"

#include <stdint.h>

class CDumbBufferObject : public IBufferObject
{
public:
  CDumbBufferObject(int format);
  virtual ~CDumbBufferObject() override;

  bool CreateBufferObject(int width, int height) override;
  void DestroyBufferObject() override;
  uint8_t* GetMemory() override;
  void ReleaseMemory() override;
  int GetFd() override;
  int GetStride() override;
  uint64_t GetModifier();

  int GetSize() const { return m_size; }

private:
  int m_device = -1;
  int m_fd = -1;
  uint32_t m_bpp = 0;
  uint32_t m_handle = 0;
  uint32_t m_stride = 0;
  uint64_t m_size = 0;
  uint64_t m_offset = 0;
  uint8_t *m_map = nullptr;
};
