/*
 *      Copyright (C) 2017 Team XBMC
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

#include "EGLUtils.h"
#include "log.h"

#include "guilib/IDirtyRegionSolver.h"
#include "settings/AdvancedSettings.h"

#include <string.h>

std::set<std::string> CEGLUtils::GetClientExtensions()
{
  const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  if (!extensions)
  {
    return {};
  }
  std::set<std::string> result;
  StringUtils::SplitTo(std::inserter(result, result.begin()), extensions, " ");
  return result;
}

std::set<std::string> CEGLUtils::GetExtensions(EGLDisplay eglDisplay)
{
  const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
  if (!extensions)
  {
    throw std::runtime_error("Could not query EGL for extensions");
  }
  std::set<std::string> result;
  StringUtils::SplitTo(std::inserter(result, result.begin()), extensions, " ");
  return result;
}

std::set<std::string> CEGLUtils::GetDeviceExtensions(EGLDeviceEXT device)
{
  auto eglQueryDeviceStringEXT = GetRequiredProcAddress<PFNEGLQUERYDEVICESTRINGEXTPROC>("eglQueryDeviceStringEXT");

  const char* extensions = eglQueryDeviceStringEXT(device, EGL_EXTENSIONS);
  if (!extensions)
  {
    return {};
  }
  std::set<std::string> result;
  StringUtils::SplitTo(std::inserter(result, result.begin()), extensions, " ");
  return result;
}

bool CEGLUtils::HasClientExtension(const std::string& name)
{
  auto exts = GetClientExtensions();
  return (exts.find(name) != exts.end());
}

bool CEGLUtils::HasExtension(EGLDisplay eglDisplay, const std::string& name)
{
  auto exts = GetExtensions(eglDisplay);
  return (exts.find(name) != exts.end());
}

bool CEGLUtils::HasDeviceExtension(EGLDeviceEXT device, const std::string& name)
{
  auto exts = GetDeviceExtensions(device);
  return (exts.find(name) != exts.end());
}

void CEGLUtils::LogError(const std::string& what)
{
  CLog::Log(LOGERROR, "%s (EGL error %d)", what.c_str(), eglGetError());
}

CEGLContextUtils::CEGLContextUtils() :
  m_eglDisplay(EGL_NO_DISPLAY),
  m_eglSurface(EGL_NO_SURFACE),
  m_eglContext(EGL_NO_CONTEXT),
  m_eglConfig(0)
{
}

CEGLContextUtils::~CEGLContextUtils()
{
  Destroy();
}

bool CEGLContextUtils::CreateDisplay(EGLDisplay display,
                                     EGLint renderable_type,
                                     EGLint rendering_api)
{
  EGLint neglconfigs = 0;
  int major, minor;

  EGLint surface_type = EGL_WINDOW_BIT;
  // for the non-trivial dirty region modes, we need the EGL buffer to be preserved across updates
  if (g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_COST_REDUCTION ||
      g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_UNION)
    surface_type |= EGL_SWAP_BEHAVIOR_PRESERVED_BIT;

  EGLint attribs[] =
  {
    EGL_RED_SIZE,        8,
    EGL_GREEN_SIZE,      8,
    EGL_BLUE_SIZE,       8,
    EGL_ALPHA_SIZE,      8,
    EGL_DEPTH_SIZE,     16,
    EGL_STENCIL_SIZE,    0,
    EGL_SAMPLE_BUFFERS,  0,
    EGL_SAMPLES,         0,
    EGL_SURFACE_TYPE,    surface_type,
    EGL_RENDERABLE_TYPE, renderable_type,
    EGL_NONE
  };

#if defined(EGL_EXT_platform_base) && defined(EGL_KHR_platform_gbm) && defined(HAVE_GBM)
  if (m_eglDisplay == EGL_NO_DISPLAY &&
      CEGLUtils::HasExtension(EGL_NO_DISPLAY, "EGL_EXT_platform_base") &&
      CEGLUtils::HasExtension(EGL_NO_DISPLAY, "EGL_KHR_platform_gbm"))
  {
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (getPlatformDisplayEXT)
    {
      m_eglDisplay = getPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, (EGLNativeDisplayType)display, NULL);
    }
  }
#endif

  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    m_eglDisplay = eglGetDisplay((EGLNativeDisplayType)display);
  }

  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    CLog::Log(LOGERROR, "failed to get EGL display");
    return false;
  }

  if (!eglInitialize(m_eglDisplay, &major, &minor))
  {
    CLog::Log(LOGERROR, "failed to initialize EGL display");
    return false;
  }

  eglBindAPI(rendering_api);

  if (!eglChooseConfig(m_eglDisplay, attribs,
                       &m_eglConfig, 1, &neglconfigs))
  {
    CLog::Log(LOGERROR, "Failed to query number of EGL configs");
    return false;
  }

  if (neglconfigs <= 0)
  {
    CLog::Log(LOGERROR, "No suitable EGL configs found");
    return false;
  }

  return true;
}

bool CEGLContextUtils::CreateContext(const EGLint* contextAttribs)
{
  if (m_eglContext == EGL_NO_CONTEXT)
  {
    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig,
                                    EGL_NO_CONTEXT, contextAttribs);
  }

  if (m_eglContext == EGL_NO_CONTEXT)
  {
    CLog::Log(LOGERROR, "failed to create EGL context");
    return false;
  }

  return true;
}

bool CEGLContextUtils::BindContext()
{
  if (!eglMakeCurrent(m_eglDisplay, m_eglSurface,
                      m_eglSurface, m_eglContext))
  {
    CLog::Log(LOGERROR, "Failed to make context current %p %p %p",
                         m_eglDisplay, m_eglSurface, m_eglContext);
    return false;
  }

  return true;
}

bool CEGLContextUtils::SurfaceAttrib()
{
  // for the non-trivial dirty region modes, we need the EGL buffer to be preserved across updates
  if (g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_COST_REDUCTION ||
      g_advancedSettings.m_guiAlgorithmDirtyRegions == DIRTYREGION_SOLVER_UNION)
  {
    if ((m_eglDisplay == EGL_NO_DISPLAY) || (m_eglSurface == EGL_NO_SURFACE))
    {
      return false;
    }

    if (!eglSurfaceAttrib(m_eglDisplay, m_eglSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED))
    {
      CLog::Log(LOGDEBUG, "%s: Could not set EGL_SWAP_BEHAVIOR",__FUNCTION__);
    }
  }

  return true;
}

bool CEGLContextUtils::CreateSurface(EGLNativeWindowType surface)
{
  m_eglSurface = eglCreateWindowSurface(m_eglDisplay,
                                        m_eglConfig,
                                        surface,
                                        nullptr);

  if (m_eglSurface == EGL_NO_SURFACE)
  {
    CLog::Log(LOGERROR, "failed to create EGL window surface %d", eglGetError());
    return false;
  }

  return true;
}

void CEGLContextUtils::Destroy()
{
  if (m_eglContext != EGL_NO_CONTEXT)
  {
    eglDestroyContext(m_eglDisplay, m_eglContext);
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    m_eglContext = EGL_NO_CONTEXT;
  }

  if (m_eglSurface != EGL_NO_SURFACE)
  {
    eglDestroySurface(m_eglDisplay, m_eglSurface);
    m_eglSurface = EGL_NO_SURFACE;
  }

  if (m_eglDisplay != EGL_NO_DISPLAY)
  {
    eglTerminate(m_eglDisplay);
    m_eglDisplay = EGL_NO_DISPLAY;
  }
}

void CEGLContextUtils::Detach()
{
  if (m_eglContext != EGL_NO_CONTEXT)
  {
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  }

  if (m_eglSurface != EGL_NO_SURFACE)
  {
    eglDestroySurface(m_eglDisplay, m_eglSurface);
    m_eglSurface = EGL_NO_SURFACE;
  }
}

bool CEGLContextUtils::SetVSync(bool enable)
{
  if (!eglSwapInterval(m_eglDisplay, enable))
  {
    return false;
  }

  return true;
}

void CEGLContextUtils::SwapBuffers()
{
  if (m_eglDisplay == EGL_NO_DISPLAY || m_eglSurface == EGL_NO_SURFACE)
  {
    return;
  }

  eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

bool CEGLContextUtils::GetEGLDevice()
{
  EGLint numDevices;

  if (!CEGLUtils::HasClientExtension("EGL_EXT_device_base") &&
     (!CEGLUtils::HasClientExtension("EGL_EXT_device_enumeration") ||
      !CEGLUtils::HasClientExtension("EGL_EXT_device_query")))
  {
    CLog::Log(LOGERROR, "EGL_EXT_device base extensions not found");
    return false;
  }

  auto eglQueryDevicesEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLQUERYDEVICESEXTPROC>("eglQueryDevicesEXT");

  /* Query how many devices are present. */
  auto ret = eglQueryDevicesEXT(0, nullptr, &numDevices);

  if (!ret)
  {
    CLog::Log(LOGERROR, "failed to query EGL devices %d", eglGetError());
    return false;
  }

  if (numDevices < 1)
  {
    CLog::Log(LOGERROR, "no EGL devices found");
    return false;
  }

  EGLDeviceEXT *devices = new EGLDeviceEXT[numDevices];

  if (devices == nullptr)
  {
    CLog::Log(LOGERROR, "memory allocation failure");
    return false;
  }

  /* Query the EGLDeviceEXTs. */
  ret = eglQueryDevicesEXT(numDevices, devices, &numDevices);

  if (!ret)
  {
    CLog::Log(LOGERROR, "failed to query EGL devices %d", eglGetError());
    return false;
  }

  for (auto i = 0; i < numDevices; i++)
  {
    if (CEGLUtils::HasDeviceExtension(devices[i], "EGL_EXT_device_drm"))
    {
      m_eglDevice = devices[i];
      break;
    }
  }

  delete [] devices;
  devices = nullptr;

  if (m_eglDevice == EGL_NO_DEVICE_EXT)
  {
    CLog::Log(LOGERROR, "no EGL_EXT_device_drm-capable EGL device found");
    return false;
  }

  return true;
}

int CEGLContextUtils::GetDrmFd()
{
  if (!CEGLUtils::HasDeviceExtension(m_eglDevice, "EGL_EXT_device_drm"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_device_drm extension not found");
    return -1;
  }

  auto eglQueryDeviceStringEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLQUERYDEVICESTRINGEXTPROC>("eglQueryDeviceStringEXT");

  const char *drmDeviceFile = eglQueryDeviceStringEXT(m_eglDevice, EGL_DRM_DEVICE_FILE_EXT);

  if (drmDeviceFile == nullptr)
  {
    CLog::Log(LOGERROR, "no DRM device file found for EGL device");
    return -1;
  }

  auto fd = open(drmDeviceFile, O_RDWR, 0);

  if (fd < 0)
  {
    CLog::Log(LOGERROR, "unable to open DRM device file");
    return -1;
  }

  return fd;
}

/* XXX khronos eglext.h does not yet have EGL_DRM_MASTER_FD_EXT */
#if !defined(EGL_DRM_MASTER_FD_EXT)
#define EGL_DRM_MASTER_FD_EXT 0x333C
#endif

bool CEGLContextUtils::CreateEglDisplay(int drmFd)
{
  /*
   * Provide the DRM fd when creating the EGLDisplay, so that the
   * EGL implementation can make any necessary DRM calls using the
   * same fd as the application.
   */
  EGLint attribs[] =
  {
    EGL_DRM_MASTER_FD_EXT,
    drmFd,
    EGL_NONE
  };

  /*
   * eglGetPlatformDisplayEXT requires EGL client extension
   * EGL_EXT_platform_base.
   */
  if (!CEGLUtils::HasClientExtension("EGL_EXT_platform_base"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_platform_base not found");
    return false;
  }

  /*
   * EGL_EXT_platform_device is required to pass
   * EGL_PLATFORM_DEVICE_EXT to eglGetPlatformDisplayEXT().
   */

  if (!CEGLUtils::HasClientExtension("EGL_EXT_platform_device"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_platform_device not found");
    return false;
  }

  /*
   * Providing a DRM fd during EGLDisplay creation requires
   * EGL_EXT_device_drm.
   */
  if (!CEGLUtils::HasDeviceExtension(m_eglDevice, "EGL_EXT_device_drm"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_device_drm not found");
    return false;
  }

  auto eglGetPlatformDisplayEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLGETPLATFORMDISPLAYEXTPROC>("eglGetPlatformDisplayEXT");

  /* Get an EGLDisplay from the EGLDeviceEXT. */
  m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, (void*)m_eglDevice, attribs);

  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    CLog::Log(LOGERROR, "failed to get EGLDisplay from EGLDevice");
    return false;
  }

  if (!eglInitialize(m_eglDisplay, NULL, NULL))
  {
    CLog::Log(LOGERROR, "failed to initialize EGLDisplay");
    return false;
  }

  return true;
}

bool CEGLContextUtils::CreateEglSurface(uint32_t planeID, int width, int height)
{
  EGLint configAttribs[] =
  {
    EGL_SURFACE_TYPE, EGL_STREAM_BIT_KHR,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 16,
    EGL_STENCIL_SIZE, 0,
    EGL_SAMPLE_BUFFERS, 0,
    EGL_SAMPLES, 0,
    EGL_NONE,
  };

  EGLint contextAttribs[] = { EGL_NONE };

  EGLAttrib layerAttribs[] =
  {
    EGL_DRM_PLANE_EXT,
    planeID,
    EGL_NONE,
  };

  EGLint streamAttribs[] = { EGL_NONE };

  EGLint surfaceAttribs[] =
  {
    EGL_WIDTH, width,
    EGL_HEIGHT, height,
    EGL_NONE
  };

  EGLint n = 0;

  if (!CEGLUtils::HasExtension(m_eglDisplay, "EGL_EXT_output_base"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_output_base not found");
    return false;
  }

  if (!CEGLUtils::HasExtension(m_eglDisplay, "EGL_EXT_output_drm"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_output_drm not found");
    return false;
  }

  /*
   * EGL_KHR_stream, EGL_EXT_stream_consumer_egloutput, and
   * EGL_KHR_stream_producer_eglsurface are needed to create an
   * EGLStream connecting an EGLSurface and an EGLOutputLayer.
   */

  if (!CEGLUtils::HasExtension(m_eglDisplay, "EGL_KHR_stream"))
  {
    CLog::Log(LOGERROR, "EGL_KHR_stream not found");
    return false;
  }

  if (!CEGLUtils::HasExtension(m_eglDisplay, "EGL_EXT_stream_consumer_egloutput"))
  {
    CLog::Log(LOGERROR, "EGL_EXT_stream_consumer_egloutput not found");
    return false;
  }

  if (!CEGLUtils::HasExtension(m_eglDisplay, "EGL_KHR_stream_producer_eglsurface"))
  {
    CLog::Log(LOGERROR, "EGL_KHR_stream_producer_eglsurface not found");
    return false;
  }

  /* Bind full OpenGL as EGL's client API. */
  eglBindAPI(EGL_OPENGL_API);

  /* Find a suitable EGL config. */

  auto ret = eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &n);

  if (!ret || !n)
  {
    CLog::Log(LOGERROR, "eglChooseConfig() failed");
    return false;
  }

  /* Create an EGL context using the EGL config. */

  m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);

  if (m_eglContext == nullptr)
  {
    CLog::Log(LOGERROR, "eglCreateContext() failed");
    return false;
  }

  auto eglGetOutputLayersEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLGETOUTPUTLAYERSEXTPROC>("eglGetOutputLayersEXT");

  /* Find the EGLOutputLayer that corresponds to the DRM KMS plane. */

  ret = eglGetOutputLayersEXT(m_eglDisplay, layerAttribs, &m_eglLayer, 1, &n);

  if (!ret || !n)
  {
    CLog::Log(LOGERROR, "Unable to get EGLOutputLayer for plane %d", planeID);
    return false;
  }

  auto eglCreateStreamKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATESTREAMKHRPROC>("eglCreateStreamKHR");

  /* Create an EGLStream. */

  m_eglStream = eglCreateStreamKHR(m_eglDisplay, streamAttribs);

  if (m_eglStream == EGL_NO_STREAM_KHR)
  {
    CLog::Log(LOGERROR, "Unable to create stream");
    return false;
  }

  auto eglStreamConsumerOutputEXT = CEGLUtils::GetRequiredProcAddress<PFNEGLSTREAMCONSUMEROUTPUTEXTPROC>("eglStreamConsumerOutputEXT");

  /* Set the EGLOutputLayer as the consumer of the EGLStream. */

  ret = eglStreamConsumerOutputEXT(m_eglDisplay, m_eglStream, m_eglLayer);

  if (!ret)
  {
    CLog::Log(LOGERROR, "Unable to create EGLOutput stream consumer");
    return false;
  }

  /*
   * EGL_KHR_stream defines that normally stream consumers need to
   * explicitly retrieve frames from the stream.  That may be useful
   * when we attempt to better integrate
   * EGL_EXT_stream_consumer_egloutput with DRM atomic KMS requests.
   * But, EGL_EXT_stream_consumer_egloutput defines that by default:
   *
   *   On success, <layer> is bound to <stream>, <stream> is placed
   *   in the EGL_STREAM_STATE_CONNECTING_KHR state, and EGL_TRUE is
   *   returned.  Initially, no changes occur to the image displayed
   *   on <layer>. When the <stream> enters state
   *   EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR, <layer> will begin
   *   displaying frames, without further action required on the
   *   application's part, as they become available, taking into
   *   account any timestamps, swap intervals, or other limitations
   *   imposed by the stream or producer attributes.
   *
   * So, eglSwapBuffers() (to produce new frames) is sufficient for
   * the frames to be displayed.  That behavior can be altered with
   * the EGL_EXT_stream_acquire_mode extension.
   */

  /*
   * Create an EGLSurface as the producer of the EGLStream.  Once
   * the stream's producer and consumer are defined, the stream is
   * ready to use.  eglSwapBuffers() calls for the EGLSurface will
   * deliver to the stream's consumer, i.e., the DRM KMS plane
   * corresponding to the EGLOutputLayer.
   */

  auto eglCreateStreamProducerSurfaceKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC>("eglCreateStreamProducerSurfaceKHR");

  m_eglSurface = eglCreateStreamProducerSurfaceKHR(m_eglDisplay, m_eglConfig, m_eglStream, surfaceAttribs);

  if (m_eglSurface == EGL_NO_SURFACE)
  {
    CLog::Log(LOGERROR, "Unable to create EGLSurface stream producer");
    return false;
  }

  /*
   * Make current to the EGLSurface, so that OpenGL rendering is
   * directed to it.
   */

  ret = eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

  if (!ret)
  {
    CLog::Log(LOGERROR, "Unable to make context and surface current");
    return false;
  }

  return true;
}
