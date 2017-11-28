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

#include "RendererDRMPRIMEGLES.h"
#include "../RenderFactory.h"
#include "cores/VideoPlayer/DVDCodecs/DVDCodecUtils.h"
#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodecDRMPRIME.h"
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"
#include "utils/GLUtils.h"
#include "ServiceBroker.h"

CWinSystemGbmGLESContext *CRendererDRMPRIMEGLES::m_pWinSystem = nullptr;

CRendererDRMPRIMEGLES::CRendererDRMPRIMEGLES() = default;

CRendererDRMPRIMEGLES::~CRendererDRMPRIMEGLES()
{
  for (int i = 0; i < NUM_BUFFERS; ++i)
  {
    DeleteTexture(i);
  }
}

CBaseRenderer* CRendererDRMPRIMEGLES::Create(CVideoBuffer* buffer)
{
  if (buffer && dynamic_cast<CVideoBufferDRMPRIME*>(buffer))
    return new CRendererDRMPRIMEGLES();

  return nullptr;
}

bool CRendererDRMPRIMEGLES::Register(CWinSystemGbmGLESContext *winSystem)
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("drm_prime_gles", CRendererDRMPRIMEGLES::Create);
  m_pWinSystem = winSystem;

  return CServiceBroker::GetRenderSystem().IsExtSupported("GL_EXT_texture_rg");
}

bool CRendererDRMPRIMEGLES::Configure(const VideoPicture &picture, float fps, unsigned flags, unsigned int orientation)
{
  InteropInfo interop;
  interop.textureTarget = GL_TEXTURE_2D;
  interop.eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  interop.eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  interop.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  interop.eglDisplay = m_pWinSystem->GetEGLDisplay();

  for (auto &tex : m_DRMPRIMETextures)
  {
    tex.Init(interop);
  }

  for (auto &fence : m_fences)
  {
    fence = GL_NONE;
  }

  return CLinuxRendererGLES::Configure(picture, fps, flags, orientation);
}

bool CRendererDRMPRIMEGLES::ConfigChanged(const VideoPicture &picture)
{
  if (picture.videoBuffer->GetFormat() != m_format)
    return true;

  return false;
}

bool CRendererDRMPRIMEGLES::Supports(ERENDERFEATURE feature)
{
  return CLinuxRendererGLES::Supports(feature);
}

bool CRendererDRMPRIMEGLES::Supports(ESCALINGMETHOD method)
{
  return CLinuxRendererGLES::Supports(method);
}

EShaderFormat CRendererDRMPRIMEGLES::GetShaderFormat()
{
  return SHADER_NV12_RRG;
}

bool CRendererDRMPRIMEGLES::LoadShadersHook()
{
  return false;
}

bool CRendererDRMPRIMEGLES::RenderHook(int idx)
{
  return false;
}

bool CRendererDRMPRIMEGLES::CreateTexture(int index)
{
  YUVBUFFER &buf = m_buffers[index];
  YuvImage &im = buf.image;
  YUVPLANE (&planes)[YuvImage::MAX_PLANES] = buf.fields[0];

  DeleteTexture(index);

  memset(&im, 0, sizeof(im));
  memset(&planes, 0, sizeof(YUVPLANE[YuvImage::MAX_PLANES]));
  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;

  planes[0].id = 1;

  return true;
}

void CRendererDRMPRIMEGLES::DeleteTexture(int index)
{
  ReleaseBuffer(index);

  YUVBUFFER &buf = m_buffers[index];
  buf.fields[FIELD_FULL][0].id = 0;
  buf.fields[FIELD_FULL][1].id = 0;
  buf.fields[FIELD_FULL][2].id = 0;
}

bool CRendererDRMPRIMEGLES::UploadTexture(int index)
{
  YUVBUFFER &buf = m_buffers[index];

  CVideoBufferDRMPRIME *buffer = dynamic_cast<CVideoBufferDRMPRIME*>(buf.videoBuffer);

  if (!buffer)
  {
    return false;
  }

  m_DRMPRIMETextures[index].Map(buffer);

  YuvImage &im = buf.image;
  YUVPLANE (&planes)[3] = buf.fields[0];

  planes[0].texwidth  = m_DRMPRIMETextures[index].m_texWidth;
  planes[0].texheight = m_DRMPRIMETextures[index].m_texHeight;

  planes[1].texwidth  = planes[0].texwidth  >> im.cshift_x;
  planes[1].texheight = planes[0].texheight >> im.cshift_y;
  planes[2].texwidth  = planes[1].texwidth;
  planes[2].texheight = planes[1].texheight;

  for (int p = 0; p < 3; p++)
  {
    planes[p].pixpertex_x = 1;
    planes[p].pixpertex_y = 1;
  }

  // set textures
  planes[0].id = m_DRMPRIMETextures[index].m_textureY;
  planes[1].id = m_DRMPRIMETextures[index].m_textureVU;
  planes[2].id = m_DRMPRIMETextures[index].m_textureVU;

  glEnable(m_textureTarget);

  for (int p=0; p<2; p++)
  {
    glBindTexture(m_textureTarget, planes[p].id);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(m_textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(m_textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(m_textureTarget, 0);
    VerifyGLState();
  }

  CalculateTextureSourceRects(index, 3);
  glDisable(m_textureTarget);
  return true;
}

void CRendererDRMPRIMEGLES::AfterRenderHook(int index)
{
  if (glIsSync(m_fences[index]))
  {
    glDeleteSync(m_fences[index]);
    m_fences[index] = GL_NONE;
  }
  m_fences[index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool CRendererDRMPRIMEGLES::NeedBuffer(int index)
{
  if (glIsSync(m_fences[index]))
  {
    GLint state;
    GLsizei length;
    glGetSynciv(m_fences[index], GL_SYNC_STATUS, 1, &length, &state);
    if (state == GL_SIGNALED)
    {
      glDeleteSync(m_fences[index]);
      m_fences[index] = GL_NONE;
    }
    else
    {
      return true;
    }
  }

  return false;
}

void CRendererDRMPRIMEGLES::ReleaseBuffer(int index)
{
  if (glIsSync(m_fences[index]))
  {
    glDeleteSync(m_fences[index]);
    m_fences[index] = GL_NONE;
  }
  m_DRMPRIMETextures[index].Unmap();

  CLinuxRendererGLES::ReleaseBuffer(index);
}
