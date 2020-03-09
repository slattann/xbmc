/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IBufferObject.h"

#include <memory>
#include <stdint.h>

class CBufferObject : public IBufferObject
{
public:
  static std::unique_ptr<CBufferObject> GetBufferObject();

  virtual int GetFd() override;
  virtual int GetStride() override;
  virtual uint64_t GetModifier() override;

protected:
  int m_fd{-1};
  uint32_t m_stride{0};
};
