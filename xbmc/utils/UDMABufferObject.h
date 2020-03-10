/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/BufferObject.h"

#include <memory>
#include <stdint.h>

class CUDMABufferObject : public CBufferObject
{
public:
  CUDMABufferObject() = default;
  virtual ~CUDMABufferObject() override;

  // Registration
  static std::unique_ptr<CBufferObject> Create();
  static void Register();

  // IBufferObject overrides via CBufferObject
  bool CreateBufferObject(int format, int width, int height) override;
  void DestroyBufferObject() override;
  uint8_t* GetMemory() override;
  void ReleaseMemory() override;

private:
  int m_memfd{-1};
  int m_udmafd{-1};
  uint64_t m_size{0};
  uint8_t* m_map{nullptr};
};
