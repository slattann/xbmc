/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <gbm.h>

class CGBMUtils
{
public:
  CGBMUtils() = default;
  ~CGBMUtils() = default;
  bool CreateDevice(int fd);
  void DestroyDevice();
  bool CreateSurface(int width, int height, const uint64_t *modifiers, const int modifiers_count);
  void DestroySurface();
  struct gbm_bo *LockFrontBuffer();
  void ReleaseBuffer();

  char* GetMemory();
  void ReleaseMemory();

  struct gbm_device* GetDevice() const { return m_device; }
  struct gbm_surface* GetSurface() const { return m_surface; }

protected:
  struct gbm_device *m_device = nullptr;
  struct gbm_surface *m_surface = nullptr;
  struct gbm_bo *m_bo = nullptr;
  struct gbm_bo *m_next_bo = nullptr;

private:
  uint32_t m_stride = 0;
  char *m_map = nullptr;
  void *m_map_data = nullptr;
  int m_width = 0;
  int m_height = 0;
};
