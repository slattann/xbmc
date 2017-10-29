/*
 *      Copyright (C) 2007-2017 Team XBMC
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

#include <array>

#if defined(HAS_GL)
#include <GL/gl.h>
#elif defined(HAS_GLES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include "utils/posix/FileHandle.h"

namespace VAAPI
{

class CVaapiRenderPicture;

struct InteropInfo
{
  EGLDisplay eglDisplay = nullptr;
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
  GLenum textureTarget;
};

class CVaapiTexture
{
public:
  CVaapiTexture();
  bool Map(CVaapiRenderPicture *pic);
  void Unmap();
  void Init(InteropInfo &interop);
  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc);

  GLuint m_textureY = 0;
  GLuint m_textureVU = 0;
  int m_texWidth = 0;
  int m_texHeight = 0;

private:
  bool MapImage(CVaapiRenderPicture *pic);
  bool MapEsh(CVaapiRenderPicture *pic);
  GLuint ImportImageToTexture(EGLImageKHR image);
  static bool TestInteropHevc(VADisplay vaDpy, EGLDisplay eglDisplay);
  static bool TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, VASurfaceID surface, int width, int height);

  InteropInfo m_interop;
  CVaapiRenderPicture *m_vaapiPic = nullptr;
  struct GLSurface
  {
    VAImage vaImage;
    VABufferInfo vBufInfo;
    VADRMPRIMESurfaceDescriptor vaDrmPrimeSurface;
    EGLImageKHR eglImageY{EGL_NO_IMAGE_KHR}, eglImageVU{EGL_NO_IMAGE_KHR};
  } m_glSurface;
  bool m_exportSurfaceUnimplemented{false};
  bool m_hasPlaneModifiers{false};
  std::array<KODI::UTILS::POSIX::CFileHandle, 4> m_drmFDs;
};
}

