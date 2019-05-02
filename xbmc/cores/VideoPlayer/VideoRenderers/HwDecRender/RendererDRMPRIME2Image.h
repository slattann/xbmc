/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/VideoPlayer/VideoRenderers/LinuxRendererGLES.h"
#include "DRMPRIMEEGL.h"

#include <array>
#include <memory>

namespace KODI
{
namespace UTILS
{
namespace EGL
{
class CEGLFence;
}
}
}

namespace VAAPI
{
class IVaapiWinSystem;
}

class CRendererDRMPRIME2Image : public CLinuxRendererGLES
{
public:
  CRendererDRMPRIME2Image();
  ~CRendererDRMPRIME2Image() override;

  static CBaseRenderer* Create(CVideoBuffer *buffer);
  static void Register();

  bool Configure(const VideoPicture &picture, float fps, unsigned int orientation) override;

  // Player functions
  void ReleaseBuffer(int idx) override;
  bool NeedBuffer(int idx) override;

protected:
  void AfterRenderHook(int idx) override;

  // textures
  bool UploadTexture(int index) override;
  void DeleteTexture(int index) override;
  bool CreateTexture(int index) override;

  EShaderFormat GetShaderFormat() override;

  std::array<std::unique_ptr<KODI::UTILS::EGL::CEGLFence>, NUM_BUFFERS> m_fences;
  CDRMPRIMETexture m_DRMPRIMETextures[NUM_BUFFERS];
};
