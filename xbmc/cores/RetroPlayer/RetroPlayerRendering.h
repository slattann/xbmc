/*
 *      Copyright (C) 2012-2017 Team Kodi
 *      http://kodi.tv
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
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "games/addons/GameClientCallbacks.h"

// todo: remove
#define GLX_GLXEXT_PROTOTYPES
#include "system_gl.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
//

namespace KODI
{
namespace RETRO
{
  class CRPProcessInfo;
  class CRPRenderManager;

  class CRetroPlayerRendering : public GAME::IGameRenderingCallback
  {
  public:
    CRetroPlayerRendering(CRPRenderManager& m_renderManager, CRPProcessInfo& m_processInfo);

    ~CRetroPlayerRendering() override = default;

    // implementation of IGameRenderingCallback
    bool Create() override;
    void Destroy() override;
    uintptr_t GetCurrentFramebuffer() override;
    GAME::RetroGLProcAddress GetProcAddress(const char *sym) override { return glXGetProcAddress(reinterpret_cast<const GLubyte*>(sym)); } //eglGetProcAddress(sym); }
    void RenderFrame() override;

  private:
    // Construction parameters
    CRPRenderManager& m_renderManager;
    CRPProcessInfo&   m_processInfo;
  };
}
}
