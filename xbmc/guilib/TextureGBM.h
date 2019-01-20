/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/Texture.h"
#include "utils/EGLImage.h"
#include "utils/GBMBufferObject.h"

#include "system_gl.h"

#include <memory>

class CGBMTexture : public CBaseTexture
{
public:
  CGBMTexture(unsigned int width = 0, unsigned int height = 0, unsigned int format = XB_FMT_A8R8G8B8);
  ~CGBMTexture() override;

  void CreateTextureObject() override;
  void DestroyTextureObject() override;
  void LoadToGPU() override;
  void BindToUnit(unsigned int unit) override;

  unsigned int GetPitch() const override { return GetDestPitch(); }

  void GetMemory() override;

protected:
  unsigned int GetDestPitch() const override;

private:
  void ReleaseMemory();

  std::unique_ptr<CGBMBufferObject> m_buffer;
  std::unique_ptr<CEGLImage> m_eglImage;

  uint32_t m_fourcc;
  GLuint m_texture = 0;
};
