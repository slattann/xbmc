/*
 *      Copyright (C) 2005-2013 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "cores/VideoPlayer/DVDCodecs/DVDFactoryCodec.h"
#include "cores/VideoPlayer/VideoRenderers/LinuxRendererGL.h"
#include "cores/VideoPlayer/VideoRenderers/RenderFactory.h"

#include "WinSystemEGLStreamsGLContext.h"
#include "utils/log.h"

using namespace KODI;

std::unique_ptr<CWinSystemBase> CWinSystemBase::CreateWinSystem()
{
  std::unique_ptr<CWinSystemBase> winSystem(new CWinSystemEGLStreamsGLContext());
  return winSystem;
}

bool CWinSystemEGLStreamsGLContext::InitWindowSystem()
{
  CLinuxRendererGL::Register();

  if (!m_pGLContext.GetEGLDevice())
  {
    return false;
  }

  m_DRM->m_fd = m_pGLContext.GetDrmFd();

  if (m_DRM->m_fd < 0)
  {
    return false;
  }

  if (!CWinSystemEGLStreams::InitWindowSystem())
  {
    return false;
  }

  if (!m_pGLContext.CreateEglDisplay(m_DRM->m_fd))
  {
    return false;
  }

  return true;
}

bool CWinSystemEGLStreamsGLContext::DestroyWindowSystem()
{
  CDVDFactoryCodec::ClearHWAccels();
  VIDEOPLAYER::CRendererFactory::ClearRenderer();

  m_pGLContext.Destroy();

  return CWinSystemEGLStreams::DestroyWindowSystem();
}

bool CWinSystemEGLStreamsGLContext::CreateNewWindow(const std::string& name,
                                                      bool fullScreen,
                                                      RESOLUTION_INFO& res)
{
  m_pGLContext.Detach();

  if (!CWinSystemEGLStreams::DestroyWindow())
  {
    return false;
  }

  if (!CWinSystemEGLStreams::CreateNewWindow(name, fullScreen, res))
  {
    return false;
  }

  if (!m_pGLContext.CreateEglSurface(m_DRM->m_primary_plane->plane->plane_id,
                                     m_DRM->m_mode->hdisplay,
                                     m_DRM->m_mode->vdisplay))
  {
    return false;
  }

  return true;
}

bool CWinSystemEGLStreamsGLContext::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  if (res.iWidth != m_DRM->m_mode->hdisplay ||
      res.iHeight != m_DRM->m_mode->vdisplay)
  {
    CLog::Log(LOGDEBUG, "CWinSystemEGLStreamsGLContext::%s - resolution changed, creating a new window", __FUNCTION__);
    CreateNewWindow("", fullScreen, res);
  }

  //m_pGLContext.SwapBuffers();

  CWinSystemEGLStreams::SetFullScreen(fullScreen, res, blankOtherDisplays);
  CRenderSystemGL::ResetRenderSystem(res.iWidth, res.iHeight);

  return true;
}

void CWinSystemEGLStreamsGLContext::PresentRender(bool rendered, bool videoLayer)
{
  if (!m_bRenderCreated)
    return;

  if (rendered || videoLayer)
  {
    if (rendered)
      m_pGLContext.SwapBuffers();
  }
}

EGLDisplay CWinSystemEGLStreamsGLContext::GetEGLDisplay() const
{
  return m_pGLContext.m_eglDisplay;
}

EGLSurface CWinSystemEGLStreamsGLContext::GetEGLSurface() const
{
  return m_pGLContext.m_eglSurface;
}

EGLContext CWinSystemEGLStreamsGLContext::GetEGLContext() const
{
  return m_pGLContext.m_eglContext;
}

EGLConfig  CWinSystemEGLStreamsGLContext::GetEGLConfig() const
{
  return m_pGLContext.m_eglConfig;
}
