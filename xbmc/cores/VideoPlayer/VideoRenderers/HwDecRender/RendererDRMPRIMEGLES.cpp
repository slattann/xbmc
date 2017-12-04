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
  for (int i(0); i < NUM_BUFFERS; ++i)
    ReleaseBuffer(i);
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

  return true; //CServiceBroker::GetRenderSystem().IsExtSupported("GL_EXT_texture_rg");
}

bool CRendererDRMPRIMEGLES::Configure(const VideoPicture &picture, float fps, unsigned flags, unsigned int orientation)
{
  InteropInfo interop;
  interop.textureTarget = GL_TEXTURE_EXTERNAL_OES;
  interop.eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  interop.eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  interop.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
  interop.eglDisplay = m_pWinSystem->GetEGLDisplay();

  for (auto &texture : m_DRMPRIMETextures)
    texture.Init(interop);

  for (auto &fence : m_fences)
    fence = GL_NONE;

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
  return SHADER_NV12;
}

bool CRendererDRMPRIMEGLES::CreateTexture(int index)
{
  YUVBUFFER &buf(m_buffers[index]);

  buf.image.height = m_sourceHeight;
  buf.image.width  = m_sourceWidth;

  YUVPLANE  &plane  = buf.fields[0][0];

  plane.texwidth  = m_sourceWidth;
  plane.texheight = m_sourceHeight;
  plane.pixpertex_x = 1;
  plane.pixpertex_y = 1;

  return true;
}

void CRendererDRMPRIMEGLES::DeleteTexture(int index)
{
  ReleaseBuffer(index);
}

bool CRendererDRMPRIMEGLES::UploadTexture(int index)
{
  ReleaseBuffer(index);
  CalculateTextureSourceRects(index, 1);
  return true;
}

bool CRendererDRMPRIMEGLES::LoadShadersHook()
{
  CLog::Log(LOGNOTICE, "Using DRMPRIMEGLES render method");
  m_textureTarget = GL_TEXTURE_2D;
  m_renderMethod = RENDER_DRMPRIME;
  return true;
}

bool CRendererDRMPRIMEGLES::RenderHook(int index)
{
  YUVPLANE &plane = m_buffers[index].fields[0][0];
  YUVPLANE &planef = m_buffers[index].fields[m_currentField][0];
  YUVBUFFER &buf = m_buffers[index];

  CVideoBufferDRMPRIME *buffer = dynamic_cast<CVideoBufferDRMPRIME*>(buf.videoBuffer);

  if (!buffer)
  {
    return false;
  }

  m_DRMPRIMETextures[index].Map(buffer);

  plane.id = m_DRMPRIMETextures[index].m_texture;
  //planef.id = m_DRMPRIMETextures[index].m_texture;

  glDisable(GL_DEPTH_TEST);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, plane.id);

  CRenderSystemGLES& renderSystem = dynamic_cast<CRenderSystemGLES&>(CServiceBroker::GetRenderSystem());

  if (m_currentField != FIELD_FULL)
  {
    renderSystem.EnableGUIShader(SM_TEXTURE_RGBA_BOB_OES);
    GLint   fieldLoc = renderSystem.GUIShaderGetField();
    GLint   stepLoc = renderSystem.GUIShaderGetStep();

    // Y is inverted, so invert fields
    if     (m_currentField == FIELD_TOP)
      glUniform1i(fieldLoc, 0);
    else if(m_currentField == FIELD_BOT)
      glUniform1i(fieldLoc, 1);
    glUniform1f(stepLoc, 1.0f / (float)plane.texheight);
  }
  else
    renderSystem.EnableGUIShader(SM_TEXTURE_RGBA_OES);

  GLint   contrastLoc = renderSystem.GUIShaderGetContrast();
  glUniform1f(contrastLoc, m_videoSettings.m_Contrast * 0.02f);
  GLint   brightnessLoc = renderSystem.GUIShaderGetBrightness();
  glUniform1f(brightnessLoc, m_videoSettings.m_Brightness * 0.01f - 0.5f);

  glUniformMatrix4fv(renderSystem.GUIShaderGetCoord0Matrix(), 1, GL_FALSE, m_textureMatrix);

  GLubyte idx[4] = {0, 1, 3, 2};        //determines order of triangle strip
  GLfloat ver[4][4];
  GLfloat tex[4][4];

  GLint   posLoc = renderSystem.GUIShaderGetPos();
  GLint   texLoc = renderSystem.GUIShaderGetCoord0();

  glVertexAttribPointer(posLoc, 4, GL_FLOAT, 0, 0, ver);
  glVertexAttribPointer(texLoc, 4, GL_FLOAT, 0, 0, tex);

  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);

  // Set vertex coordinates
  for(int i = 0; i < 4; i++)
  {
    ver[i][0] = m_rotatedDestCoords[i].x;
    ver[i][1] = m_rotatedDestCoords[i].y;
    ver[i][2] = 0.0f;        // set z to 0
    ver[i][3] = 1.0f;
  }

  // Set texture coordinates (MediaCodec is flipped in y)
  if (m_currentField == FIELD_FULL)
  {
    tex[0][0] = tex[3][0] = plane.rect.x1;
    tex[0][1] = tex[1][1] = plane.rect.y2;
    tex[1][0] = tex[2][0] = plane.rect.x2;
    tex[2][1] = tex[3][1] = plane.rect.y1;
  }
  else
  {
    tex[0][0] = tex[3][0] = planef.rect.x1;
    tex[0][1] = tex[1][1] = planef.rect.y2 * 2.0f;
    tex[1][0] = tex[2][0] = planef.rect.x2;
    tex[2][1] = tex[3][1] = planef.rect.y1 * 2.0f;
  }

  for(int i = 0; i < 4; i++)
  {
    tex[i][2] = 0.0f;
    tex[i][3] = 1.0f;
  }

  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, idx);

  glDisableVertexAttribArray(posLoc);
  glDisableVertexAttribArray(texLoc);

  const float identity[16] = {
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
  };
  glUniformMatrix4fv(renderSystem.GUIShaderGetCoord0Matrix(),  1, GL_FALSE, identity);

  renderSystem.DisableGUIShader();
  VerifyGLState();

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  VerifyGLState();

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
