/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "WinSystemWaylandVulkanContext.h"

#include "utils/log.h"

using namespace KODI::WINDOWING::WAYLAND;

std::unique_ptr<CWinSystemBase> CWinSystemBase::CreateWinSystem()
{
  std::unique_ptr<CWinSystemBase> winSystem(new CWinSystemWaylandVulkanContext());
  return winSystem;
}

bool CWinSystemWaylandVulkanContext::InitWindowSystem()
{
  if (!CWinSystemWayland::InitWindowSystem())
  {
    return false;
  }

  if (!InitRenderSystem())
  {
    return false;
  }

  return true;
}

bool CWinSystemWaylandVulkanContext::DestroyWindowSystem()
{
  return CWinSystemWayland::DestroyWindowSystem();
}

bool CWinSystemWaylandVulkanContext::CreateNewWindow(const std::string& name,
                                                     bool fullScreen,
                                                     RESOLUTION_INFO& res)
{
  if (!CWinSystemWayland::CreateNewWindow(name, fullScreen, res))
  {
    return false;
  }

  if (!CreateSurface(GetConnection()->GetDisplay(), GetMainSurface()))
  {
    return false;
  }

  return true;
}

bool CWinSystemWaylandVulkanContext::DestroyWindow()
{
  DestroySurface();

  return CWinSystemWayland::DestroyWindow();
}

void CWinSystemWaylandVulkanContext::PresentRender(bool rendered, bool videoLayer)
{
  PrepareFramePresentation();

  if (rendered)
  {
    // if (!m_eglContext.TrySwapBuffers())
    // {
    //   // For now we just hard fail if this fails
    //   // Theoretically, EGL_CONTEXT_LOST could be handled, but it needs to be checked
    //   // whether egl implementations actually use it (mesa does not)
    //   CEGLUtils::Log(LOGERROR, "eglSwapBuffers failed");
    //   throw std::runtime_error("eglSwapBuffers failed");
    // }
    // // eglSwapBuffers() (hopefully) calls commit on the surface and flushes
    // // ... well mesa does anyway
  }
  else
  {
    // For presentation feedback: Get notification of the next vblank even
    // when contents did not change
    GetMainSurface().commit();
    // Make sure it reaches the compositor
    GetConnection()->GetDisplay().flush();
  }

  FinishFramePresentation();
}

void CWinSystemWaylandVulkanContext::SetContextSize(CSizeInt size)
{
  // Change EGL surface size if necessary
  if (GetNativeWindowAttachedSize() != size)
  {
    CLog::LogF(LOGDEBUG, "Updating egl_window size to %dx%d", size.Width(), size.Height());
    // m_nativeWindow.resize(size.Width(), size.Height(), 0, 0);
  }

  // Propagate changed dimensions to render system if necessary
  if (CRenderSystemVulkan::m_width != size.Width() ||
      CRenderSystemVulkan::m_height != size.Height())
  {
    CLog::LogF(LOGDEBUG, "Resetting render system to %dx%d", size.Width(), size.Height());
    KODI::RENDERING::VULKAN::CRenderSystemVulkan::ResetRenderSystem(size.Width(), size.Height());
  }
}

CSizeInt CWinSystemWaylandVulkanContext::GetNativeWindowAttachedSize()
{
  int width, height;
  // m_nativeWindow.get_attached_size(width, height);
  return {width, height};
}
