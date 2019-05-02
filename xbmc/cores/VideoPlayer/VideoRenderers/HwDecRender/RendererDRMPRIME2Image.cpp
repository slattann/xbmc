/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RendererDRMPRIME2Image.h"
#include "../RenderFactory.h"
#include "cores/VideoPlayer/DVDCodecs/DVDCodecUtils.h"
#include "settings/Settings.h"
#include "settings/AdvancedSettings.h"
#include "utils/EGLFence.h"
#include "utils/log.h"
#include "utils/GLUtils.h"
#include "windowing/gbm/WinSystemGbmEGLContext.h"

using namespace KODI::WINDOWING::GBM;
using namespace KODI::UTILS::EGL;

CBaseRenderer* CRendererDRMPRIME2Image::Create(CVideoBuffer *buffer)
{
  if (buffer && dynamic_cast<IVideoBufferDRMPRIME*>(buffer))
    return new CRendererDRMPRIME2Image();

  return nullptr;
}

void CRendererDRMPRIME2Image::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("drm_prime_2_image", CRendererDRMPRIME2Image::Create);
}

CRendererDRMPRIME2Image::CRendererDRMPRIME2Image() = default;

CRendererDRMPRIME2Image::~CRendererDRMPRIME2Image()
{
  for (int i = 0; i < NUM_BUFFERS; ++i)
  {
    DeleteTexture(i);
  }
}

bool CRendererDRMPRIME2Image::Configure(const VideoPicture &picture, float fps, unsigned int orientation)
{
  CWinSystemGbmEGLContext* winSystem = dynamic_cast<CWinSystemGbmEGLContext*>(CServiceBroker::GetWinSystem());
  if (!winSystem)
    return false;

  for (auto &texture : m_DRMPRIMETextures)
    texture.Init(winSystem->GetEGLDisplay());

  for (auto& fence : m_fences)
  {
    fence.reset(new CEGLFence(winSystem->GetEGLDisplay()));
  }

  return CLinuxRendererGLES::Configure(picture, fps, orientation);
}

EShaderFormat CRendererDRMPRIME2Image::GetShaderFormat()
{
  return SHADER_NV12_RRG;
}

bool CRendererDRMPRIME2Image::CreateTexture(int index)
{
  CPictureBuffer &buf = m_buffers[index];
  YuvImage &im = buf.image;
  CYuvPlane (&planes)[YuvImage::MAX_PLANES] = buf.fields[0];

  DeleteTexture(index);

  im = {};
  std::fill(std::begin(planes), std::end(planes), CYuvPlane{});
  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;

  planes[0].id = 1;

  return true;
}

void CRendererDRMPRIME2Image::DeleteTexture(int index)
{
  ReleaseBuffer(index);

  CPictureBuffer &buf = m_buffers[index];
  buf.fields[FIELD_FULL][0].id = 0;
  buf.fields[FIELD_FULL][1].id = 0;
  buf.fields[FIELD_FULL][2].id = 0;
}

bool CRendererDRMPRIME2Image::UploadTexture(int index)
{
  CPictureBuffer &buf = m_buffers[index];

  IVideoBufferDRMPRIME* buffer = dynamic_cast<IVideoBufferDRMPRIME*>(buf.videoBuffer);

  if (!buffer)
    return false;

  m_DRMPRIMETextures[index].Map(buffer);

  YuvImage &im = buf.image;
  CYuvPlane (&planes)[3] = buf.fields[0];

  auto size = m_DRMPRIMETextures[index].GetTextureSize();
  planes[0].texwidth  = size.Width();
  planes[0].texheight = size.Height();

  planes[1].texwidth  = planes[0].texwidth >> im.cshift_x;
  planes[1].texheight = planes[0].texheight >> im.cshift_y;
  planes[2].texwidth  = planes[1].texwidth;
  planes[2].texheight = planes[1].texheight;

  for (int p = 0; p < 3; p++)
  {
    planes[p].pixpertex_x = 1;
    planes[p].pixpertex_y = 1;
  }

  // set textures
  planes[0].id = m_DRMPRIMETextures[index].GetTextureY();
  planes[1].id = m_DRMPRIMETextures[index].GetTextureU();
  planes[2].id = m_DRMPRIMETextures[index].GetTextureU();

  for (int p = 0; p < 2; p++)
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
  return true;
}

void CRendererDRMPRIME2Image::AfterRenderHook(int index)
{
  m_fences[index]->CreateFence();
}

bool CRendererDRMPRIME2Image::NeedBuffer(int index)
{
  return !m_fences[index]->IsSignaled();
}

void CRendererDRMPRIME2Image::ReleaseBuffer(int index)
{
  m_fences[index]->DestroyFence();

  m_DRMPRIMETextures[index].Unmap();

  CLinuxRendererGLES::ReleaseBuffer(index);
}
