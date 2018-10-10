/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>

class CEGLFence
{
public:
  CEGLFence(EGLDisplay display);
  ~CEGLFence() = default;

  EGLSyncKHR CreateFence(int fd);
  void CreateGPUFence();
  void CreateKMSFence(int fd);
  EGLint FlushFence();
  void WaitSyncGPU();
  void WaitSyncCPU();

private:
  EGLDisplay m_eglDisplay{EGL_NO_DISPLAY};
  EGLSyncKHR m_gpuFence{EGL_NO_SYNC_KHR};
  EGLSyncKHR m_kmsFence{EGL_NO_SYNC_KHR};

  PFNEGLCREATESYNCKHRPROC m_eglCreateSyncKHR{nullptr};
  PFNEGLDESTROYSYNCKHRPROC m_eglDestroySyncKHR{nullptr};
  PFNEGLDUPNATIVEFENCEFDANDROIDPROC m_eglDupNativeFenceFDANDROID{nullptr};

  PFNEGLCLIENTWAITSYNCKHRPROC m_eglClientWaitSyncKHR{nullptr};
  PFNEGLWAITSYNCKHRPROC m_eglWaitSyncKHR{nullptr};
};
