/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "EGLFence.h"

#include "EGLUtils.h"
//#include "log.h"

CEGLFence::CEGLFence(EGLDisplay display) :
  m_eglDisplay(display)
{
  m_eglCreateSyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATESYNCKHRPROC>("eglCreateSyncKHR");
  m_eglDupNativeFenceFDANDROID = CEGLUtils::GetRequiredProcAddress<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>("eglDupNativeFenceFDANDROID");
  m_eglDestroySyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLDESTROYSYNCKHRPROC>("eglDestroySyncKHR");
  m_eglClientWaitSyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCLIENTWAITSYNCKHRPROC>("eglClientWaitSyncKHR");
  m_eglWaitSyncKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLWAITSYNCKHRPROC>("eglWaitSyncKHR");
}

EGLSyncKHR CEGLFence::CreateFence(int fd)
{
  CEGLAttributes<1> attributeList;
  attributeList.Add({{EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd}});

  EGLSyncKHR fence = m_eglCreateSyncKHR(m_eglDisplay, EGL_SYNC_NATIVE_FENCE_ANDROID, attributeList.Get());

  if (fence == EGL_NO_SYNC_KHR)
  {
    CEGLUtils::LogError("failed to create EGL sync object");
    return nullptr;
  }

  return fence;
}

void CEGLFence::CreateGPUFence()
{
  m_gpuFence = CreateFence(EGL_NO_NATIVE_FENCE_FD_ANDROID);
}

void CEGLFence::CreateKMSFence(int fd)
{
  m_kmsFence = CreateFence(fd);
}

EGLint CEGLFence::FlushFence()
{
  auto fd = m_eglDupNativeFenceFDANDROID(m_eglDisplay, m_gpuFence);
  if (fd == EGL_NO_NATIVE_FENCE_FD_ANDROID)
    CEGLUtils::LogError("failed to duplicate EGL fence fd");

  m_eglDestroySyncKHR(m_eglDisplay, m_gpuFence);

  return fd;
}

void CEGLFence::WaitSyncGPU()
{
  if (!m_kmsFence)
    return;

  if (m_eglWaitSyncKHR(m_eglDisplay, m_kmsFence, 0) != EGL_TRUE)
    CEGLUtils::LogError("failed to create EGL sync point");
}

void CEGLFence::WaitSyncCPU()
{
  if (!m_kmsFence)
    return;

  EGLint status{EGL_FALSE};

  while (status != EGL_CONDITION_SATISFIED_KHR)
    status = m_eglClientWaitSyncKHR(m_eglDisplay, m_kmsFence, 0, EGL_FOREVER_KHR);

  m_eglDestroySyncKHR(m_eglDisplay, m_kmsFence);
}
