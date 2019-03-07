/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EGLFence.h"

#include "EGLUtils.h"

CEGLFence::CEGLFence(EGLDisplay display) :
  m_display(display)
{
  m_eglCreateSyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATESYNCKHRPROC>("eglCreateSyncKHR");
  m_eglDestroySyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLDESTROYSYNCKHRPROC>("eglDestroySyncKHR");
  m_eglGetSyncAttribKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLGETSYNCATTRIBKHRPROC>("eglGetSyncAttribKHR");
}

bool CEGLFence::CreateFence()
{
  m_fence = m_eglCreateSyncKHR(m_display, EGL_SYNC_FENCE_KHR, nullptr);
  if (m_fence == EGL_NO_SYNC_KHR)
  {
    CEGLUtils::LogError("failed to create egl sync fence");
    return false;
  }

  return true;
}

void CEGLFence::DestroyFence()
{
  if (m_fence == EGL_NO_SYNC_KHR)
  {
    return;
  }

  if (m_eglDestroySyncKHR(m_display, m_fence) != EGL_TRUE)
  {
    CEGLUtils::LogError("failed to destroy egl sync fence");
  }

  m_fence = EGL_NO_SYNC_KHR;
}

bool CEGLFence::IsSignaled()
{
  if (m_fence == EGL_NO_SYNC_KHR)
  {
    return false;
  }

  EGLint status = EGL_UNSIGNALED_KHR;
  if (m_eglGetSyncAttribKHR(m_display, m_fence, EGL_SYNC_STATUS_KHR, &status) != EGL_TRUE)
  {
    CEGLUtils::LogError("failed to query egl sync fence");
    return false;
  }

  if (status == EGL_SIGNALED_KHR)
  {
    return true;
  }

  return false;
}
