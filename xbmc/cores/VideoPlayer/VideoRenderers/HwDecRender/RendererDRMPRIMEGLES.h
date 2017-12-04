/*
 *      Copyright (C) 2007-2015 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "system.h"

#include "windowing/gbm/WinSystemGbmGLESContext.h"
#include "cores/VideoPlayer/VideoRenderers/LinuxRendererGLES.h"
#include "DRMPRIMEEGL.h"

class CRendererDRMPRIMEGLES : public CLinuxRendererGLES
{
public:
  CRendererDRMPRIMEGLES();
  ~CRendererDRMPRIMEGLES() override;

  // Registration
  static CBaseRenderer* Create(CVideoBuffer* buffer);
  static bool Register(CWinSystemGbmGLESContext *winSystem);

  bool Configure(const VideoPicture &picture, float fps, unsigned flags, unsigned int orientation) override;

  // Player functions
  bool ConfigChanged(const VideoPicture &picture) override;
  void ReleaseBuffer(int index) override;
  bool NeedBuffer(int index) override;

  // Feature support
  bool Supports(ERENDERFEATURE feature) override;
  bool Supports(ESCALINGMETHOD method) override;

protected:
  bool LoadShadersHook() override;
  bool RenderHook(int index) override;
  void AfterRenderHook(int index) override;

  // textures
  bool UploadTexture(int index) override;
  void DeleteTexture(int index) override;
  bool CreateTexture(int index) override;

  EShaderFormat GetShaderFormat() override;

  float m_textureMatrix[16];

  CDRMPRIMETexture m_DRMPRIMETextures[NUM_BUFFERS];
  GLsync m_fences[NUM_BUFFERS];

  static CWinSystemGbmGLESContext *m_pWinSystem;
};
