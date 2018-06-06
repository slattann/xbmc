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

#include <EGL/eglext.h>
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

bool CEGLUtils::HasExtension(EGLDisplay eglDisplay, const std::string& name)
{
  auto exts = GetExtensions(eglDisplay);
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

/* --- CEGLImage -------------------------------------------*/

#ifndef DRM_FORMAT_MOD_INVALID
const uint64_t DRM_FORMAT_MOD_INVALID = 72057594037927935;
#endif

namespace
{
  const EGLint eglDmabufPlaneFdAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_FD_EXT,
    EGL_DMA_BUF_PLANE1_FD_EXT,
    EGL_DMA_BUF_PLANE2_FD_EXT,
  };

  const EGLint eglDmabufPlaneOffsetAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT,
    EGL_DMA_BUF_PLANE2_OFFSET_EXT,
  };

  const EGLint eglDmabufPlanePitchAttr[CEGLImage::MAX_NUM_PLANES] =
  {
    EGL_DMA_BUF_PLANE0_PITCH_EXT,
    EGL_DMA_BUF_PLANE1_PITCH_EXT,
    EGL_DMA_BUF_PLANE2_PITCH_EXT,
  };
} // namespace

CEGLImage::CEGLImage(EGLDisplay display) :
  m_display(display)
{
  m_eglCreateImageKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLCREATEIMAGEKHRPROC>("eglCreateImageKHR");
  m_eglDestroyImageKHR = CEGLUtils::GetRequiredProcAddress<PFNEGLDESTROYIMAGEKHRPROC>("eglDestroyImageKHR");
  m_glEGLImageTargetTexture2DOES = CEGLUtils::GetRequiredProcAddress<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>("glEGLImageTargetTexture2DOES");
}

bool CEGLImage::CreateImage(EglAttrs imageAttrs)
{
  CEGLAttributes<22> attribs;
  attribs.Add({{EGL_WIDTH, imageAttrs.width},
               {EGL_HEIGHT, imageAttrs.height},
               {EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(imageAttrs.format)}});

  /* this should be included later as some platforms need it */
  attribs.Add({{EGL_YUV_COLOR_SPACE_HINT_EXT, EGL_ITU_REC601_EXT},
               {EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_FULL_RANGE_EXT},
               {EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT},
               {EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT}});

  for (int i = 0; i < MAX_NUM_PLANES; i++)
  {
    if (imageAttrs.planes[i].fd != 0)
    {
      attribs.Add({{eglDmabufPlaneFdAttr[i], imageAttrs.planes[i].fd},
                   {eglDmabufPlaneOffsetAttr[i], imageAttrs.planes[i].offset},
                   {eglDmabufPlanePitchAttr[i], imageAttrs.planes[i].pitch}});
    }
  }

  m_image = m_eglCreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.Get());

  if(!m_image)
  {
    CLog::Log(LOGERROR, "CEGLImage::%s - failed to import buffer into EGL image: %d", __FUNCTION__, eglGetError());
    return false;
  }

  return true;
}

void CEGLImage::UploadImage(GLenum textureTarget)
{
  m_glEGLImageTargetTexture2DOES(textureTarget, m_image);
}

void CEGLImage::DestroyImage()
{
  m_eglDestroyImageKHR(m_display, m_image);
}
