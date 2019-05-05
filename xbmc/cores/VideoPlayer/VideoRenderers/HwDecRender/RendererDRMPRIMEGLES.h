/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/VideoPlayer/VideoRenderers/BaseRenderer.h"
#include "DRMPRIMEEGL.h"
#include "rendering/gles/RenderSystemGLES.h"

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

class CRendererDRMPRIMEGLES : public CBaseRenderer
{
public:
  CRendererDRMPRIMEGLES() = default;
  ~CRendererDRMPRIMEGLES();

  // Registration
  static CBaseRenderer* Create(CVideoBuffer* buffer);
  static void Register();

  // Player functions
  bool Configure(const VideoPicture &picture, float fps, unsigned int orientation) override;
  bool IsConfigured() override { return m_bConfigured; }
  void AddVideoPicture(const VideoPicture &picture, int index) override;
  void UnInit() override {};
  bool Flush(bool saveBuffers) override;
  void ReleaseBuffer(int index) override;
  bool NeedBuffer(int index) override;
  void RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha) override;
  void Update() override;
  bool RenderCapture(CRenderCapture* capture) override;
  CRenderInfo GetRenderInfo() override;
  bool ConfigChanged(const VideoPicture &picture) override;

  bool SupportsMultiPassRendering() override { return true; } // todo: remove

  // Feature support
  bool Supports(ERENDERFEATURE feature) override;
  bool Supports(ESCALINGMETHOD method) override;

protected:
  void Render(unsigned int flags, int index, ESHADERMETHOD method);

  bool m_bConfigured = false;
  int m_iLastRenderBuffer = -1;
  float m_clearColour{0.0f};

  std::array<std::unique_ptr<KODI::UTILS::EGL::CEGLFence>, NUM_BUFFERS> m_fences;

  struct BUFFER
  {
    CVideoBuffer* videoBuffer = nullptr;
    CDRMPRIMETexture primeTexture;
  } m_buffers[NUM_BUFFERS];
};
