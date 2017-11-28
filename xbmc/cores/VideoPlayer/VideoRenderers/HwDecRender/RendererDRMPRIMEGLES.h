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

#include "cores/VideoPlayer/VideoRenderers/LinuxRendererGLES.h"
#include "DRMPRIMEEGL.h"
#include "windowing/gbm/WinSystemGbmGLESContext.h"

class CRendererDRMPRIMEGLES : public CLinuxRendererGLES
{
public:
  CRendererDRMPRIMEGLES() = default;
  ~CRendererDRMPRIMEGLES();

  // Registration
  static CBaseRenderer* Create(CVideoBuffer* buffer);
  static bool Register(CWinSystemGbmGLESContext *winSystem);

  // CLinuxRendererGLES overrides
  bool Configure(const VideoPicture &picture, float fps, unsigned int orientation) override;
  void ReleaseBuffer(int index) override;
  void AddVideoPicture(const VideoPicture& picture, int index, double currentClock);

  // Feature support
  CRenderInfo GetRenderInfo() override;

protected:
  // CLinuxRendererGLES overrides
  bool LoadShadersHook() override;
  bool RenderHook(int index) override;
  bool UploadTexture(int index) override;
  void DeleteTexture(int index) override;
  bool CreateTexture(int index) override;

  static const int m_numRenderBuffers = 4;
  CDRMPRIMETexture m_DRMPRIMETextures[NUM_BUFFERS];
};
