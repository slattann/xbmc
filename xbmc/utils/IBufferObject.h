/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <stdint.h>

class IBufferObject
{
public:
  virtual ~IBufferObject() = default;

  virtual bool CreateBufferObject(int format, int width, int height) = 0;
  virtual void DestroyBufferObject() = 0;
  virtual uint8_t *GetMemory() = 0;
  virtual void ReleaseMemory() = 0;
  virtual int GetFd() = 0;
  virtual int GetStride() = 0;
  virtual uint64_t GetModifier() = 0;

protected:
  int m_width = 0;
  int m_height = 0;
};
