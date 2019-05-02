/*
 *  Copyright (C) 2007-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RendererDRMPRIME1Image.h"

#include "cores/VideoPlayer/VideoRenderers/RenderFactory.h"
#include "rendering/MatrixGL.h"
#include "ServiceBroker.h"
#include "utils/EGLFence.h"
#include "utils/log.h"
#include "windowing/gbm/WinSystemGbmGLESContext.h"

using namespace KODI::WINDOWING::GBM;
using namespace KODI::UTILS::EGL;

CRendererDRMPRIME1Image::~CRendererDRMPRIME1Image()
{
  for (int i = 0; i < NUM_BUFFERS; ++i)
    DeleteTexture(i);
}

CBaseRenderer* CRendererDRMPRIME1Image::Create(CVideoBuffer* buffer)
{
  if (buffer && dynamic_cast<IVideoBufferDRMPRIME*>(buffer))
    return new CRendererDRMPRIME1Image();

  return nullptr;
}

void CRendererDRMPRIME1Image::Register()
{
  VIDEOPLAYER::CRendererFactory::RegisterRenderer("drm_prime_1_image", CRendererDRMPRIME1Image::Create);
}

bool CRendererDRMPRIME1Image::Configure(const VideoPicture &picture, float fps, unsigned int orientation)
{
  CWinSystemGbmGLESContext* winSystem = dynamic_cast<CWinSystemGbmGLESContext*>(CServiceBroker::GetWinSystem());
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

void CRendererDRMPRIME1Image::ReleaseBuffer(int index)
{
  m_fences[index]->DestroyFence();

  m_DRMPRIMETextures[index].Unmap();
  CLinuxRendererGLES::ReleaseBuffer(index);
}

bool CRendererDRMPRIME1Image::NeedBuffer(int index)
{
  return !m_fences[index]->IsSignaled();
}

bool CRendererDRMPRIME1Image::CreateTexture(int index)
{
  CPictureBuffer &buf = m_buffers[index];
  YuvImage &im = buf.image;
  CYuvPlane &plane = buf.fields[0][0];

  DeleteTexture(index);

  im = {};
  plane = {};

  im.height = m_sourceHeight;
  im.width  = m_sourceWidth;
  im.cshift_x = 1;
  im.cshift_y = 1;

  plane.id = 1;

  return true;
}

void CRendererDRMPRIME1Image::DeleteTexture(int index)
{
  ReleaseBuffer(index);

  CPictureBuffer &buf = m_buffers[index];
  buf.fields[0][0].id = 0;
}

bool CRendererDRMPRIME1Image::UploadTexture(int index)
{
  CPictureBuffer &buf = m_buffers[index];

  IVideoBufferDRMPRIME* buffer = dynamic_cast<IVideoBufferDRMPRIME*>(buf.videoBuffer);

  if (!buffer || !buffer->IsValid())
  {
    CLog::Log(LOGNOTICE, "CRendererDRMPRIME1Image::%s - no buffer", __FUNCTION__);
    return false;
  }

  m_DRMPRIMETextures[index].Map(buffer);

  CYuvPlane &plane = buf.fields[0][0];

  auto size = m_DRMPRIMETextures[index].GetTextureSize();
  plane.texwidth  = size.Width();
  plane.texheight = size.Height();
  plane.pixpertex_x = 1;
  plane.pixpertex_y = 1;

  plane.id = m_DRMPRIMETextures[index].GetTextureY();

  CalculateTextureSourceRects(index, 1);

  return true;
}

void CRendererDRMPRIME1Image::RenderToFBO(int index, int field)
{
  CRenderSystemGLES *renderSystem = dynamic_cast<CRenderSystemGLES*>(CServiceBroker::GetRenderSystem());
  assert(renderSystem);

  CYuvPlane &plane = m_buffers[index].fields[0][0];

  glDisable(GL_DEPTH_TEST);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, plane.id);

  m_fbo.fbo.BeginRender();

  glMatrixModview.Push();
  glMatrixModview->LoadIdentity();
  glMatrixModview.Load();

  glMatrixProject.Push();
  glMatrixProject->LoadIdentity();
  glMatrixProject->Ortho2D(0, m_sourceWidth, 0, m_sourceHeight);
  glMatrixProject.Load();

  CRect viewport;
  m_renderSystem->GetViewPort(viewport);
  glViewport(0, 0, m_sourceWidth, m_sourceHeight);
  glScissor(0, 0, m_sourceWidth, m_sourceHeight);

  renderSystem->EnableGUIShader(SM_TEXTURE_RGBA_OES);

  m_fbo.width  = plane.rect.x2 - plane.rect.x1;
  m_fbo.height = plane.rect.y2 - plane.rect.y1;

  if (m_textureTarget == GL_TEXTURE_2D)
  {
    m_fbo.width  *= plane.texwidth;
    m_fbo.height *= plane.texheight;
  }

  m_fbo.width  *= plane.pixpertex_x;
  m_fbo.height *= plane.pixpertex_y;

  GLubyte idx[4] = {0, 1, 3, 2}; // Determines order of triangle strip
  GLuint vertexVBO;
  GLuint indexVBO;
  struct PackedVertex
  {
    float x, y, z;
    float u1, v1;
  };

  std::array<PackedVertex, 4> vertex;

  GLint vertLoc = renderSystem->GUIShaderGetPos();
  GLint loc = renderSystem->GUIShaderGetCoord0();

  // top left
  vertex[0].x = 0.0f;
  vertex[0].y = 0.0f;
  vertex[0].z = 0.0f;
  vertex[0].u1 = plane.rect.x1;
  vertex[0].v1 = plane.rect.y1;

  // top right
  vertex[1].x = m_fbo.width;
  vertex[1].y = 0.0f;
  vertex[1].z = 0.0f;
  vertex[1].u1 = plane.rect.x2;
  vertex[1].v1 = plane.rect.y1;

  // bottom right
  vertex[2].x = m_fbo.width;
  vertex[2].y = m_fbo.height;
  vertex[2].z = 0.0f;
  vertex[2].u1 = plane.rect.x2;
  vertex[2].v1 = plane.rect.y2;

  // bottom left
  vertex[3].x = 0.0f;
  vertex[3].y = m_fbo.height;
  vertex[3].z = 0.0f;
  vertex[3].u1 = plane.rect.x1;
  vertex[3].v1 = plane.rect.y2;;

  glGenBuffers(1, &vertexVBO);
  glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(PackedVertex) * vertex.size(), vertex.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(vertLoc, 3, GL_FLOAT, 0, sizeof(PackedVertex), reinterpret_cast<const GLvoid*>(offsetof(PackedVertex, x)));
  glVertexAttribPointer(loc, 2, GL_FLOAT, 0, sizeof(PackedVertex), reinterpret_cast<const GLvoid*>(offsetof(PackedVertex, u1)));

  glEnableVertexAttribArray(vertLoc);
  glEnableVertexAttribArray(loc);

  glGenBuffers(1, &indexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * 4, idx, GL_STATIC_DRAW);

  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);

  glDisableVertexAttribArray(vertLoc);
  glDisableVertexAttribArray(loc);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &vertexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &indexVBO);

  renderSystem->DisableGUIShader();

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

  glMatrixModview.PopLoad();
  glMatrixProject.PopLoad();

  m_renderSystem->SetViewPort(viewport);

  m_fbo.fbo.EndRender();
}

void CRendererDRMPRIME1Image::AfterRenderHook(int index)
{
  m_fences[index]->CreateFence();
}

bool CRendererDRMPRIME1Image::Supports(ERENDERFEATURE feature)
{
  switch (feature)
  {
  case RENDERFEATURE_STRETCH:
  case RENDERFEATURE_ZOOM:
  case RENDERFEATURE_VERTICAL_SHIFT:
  case RENDERFEATURE_PIXEL_RATIO:
  case RENDERFEATURE_ROTATION:
    return true;
  default:
    return false;
  }
}

bool CRendererDRMPRIME1Image::Supports(ESCALINGMETHOD method)
{
  switch (method)
  {
  case VS_SCALINGMETHOD_LINEAR:
    return true;
  default:
    return false;
  }
}
