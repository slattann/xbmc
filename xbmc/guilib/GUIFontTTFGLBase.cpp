/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIFontTTFGLBase.h"

#include "GUIFont.h"
#include "GUIFontManager.h"
#include "Texture.h"
#include "TextureManager.h"
#include "windowing/GraphicContext.h"
#include "ServiceBroker.h"
#include "gui3d.h"
#include "utils/log.h"
#include "utils/GLUtils.h"

#include "rendering/MatrixGL.h"

#include <cassert>

// stuff for freetype
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

CGUIFontTTFGLBase::CGUIFontTTFGLBase(const std::string& strFileName)
: CGUIFontTTFBase(strFileName)
{
  m_updateY1 = 0;
  m_updateY2 = 0;
  m_textureStatus = TEXTURE_VOID;

  CreateStaticVertexBuffers();
}

CGUIFontTTFGLBase::~CGUIFontTTFGLBase()
{
  // It's important that all the CGUIFontCacheEntry objects are
  // destructed before the CGUIFontTTFGLBase goes out of scope, because
  // our virtual methods won't be accessible after this point
  m_dynamicCache.Flush();
  DeleteHardwareTexture();

  DestroyStaticVertexBuffers();
}

CVertexBuffer CGUIFontTTFGLBase::CreateVertexBuffer(const std::vector<SVertex> &vertices) const
{
  assert(vertices.size() % 4 == 0);
  GLuint bufferHandle = 0;

  // Do not create empty buffers, leave buffer as 0, it will be ignored in drawing stage
  if (!vertices.empty())
  {
    // Generate a unique buffer object name and put it in bufferHandle
    glGenBuffers(1, &bufferHandle);
    // Bind the buffer to the OpenGL context's GL_ARRAY_BUFFER binding point
    glBindBuffer(GL_ARRAY_BUFFER, bufferHandle);
    // Create a data store for the buffer object bound to the GL_ARRAY_BUFFER
    // binding point (i.e. our buffer object) and initialise it from the
    // specified client-side pointer
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SVertex), vertices.data(), GL_STATIC_DRAW);
    // Unbind GL_ARRAY_BUFFER
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }


  return CVertexBuffer(bufferHandle, vertices.size() / 4, this);
}

void CGUIFontTTFGLBase::DestroyVertexBuffer(CVertexBuffer &buffer) const
{
  if (buffer.bufferHandle != 0)
  {
    // Release the buffer name for reuse
    glDeleteBuffers(1, (GLuint *) &buffer.bufferHandle);
    buffer.bufferHandle = 0;
  }
}

CBaseTexture* CGUIFontTTFGLBase::ReallocTexture(unsigned int& newHeight)
{
  newHeight = CBaseTexture::PadPow2(newHeight);

  CBaseTexture* newTexture = new CTexture(m_textureWidth, newHeight, XB_FMT_A8);

  if (!newTexture || newTexture->GetPixels() == NULL)
  {
    CLog::Log(LOGERROR, "GUIFontTTFGL::CacheCharacter: Error creating new cache texture for size %f", m_height);
    delete newTexture;
    return NULL;
  }
  m_textureHeight = newTexture->GetHeight();
  m_textureScaleY = 1.0f / m_textureHeight;
  m_textureWidth = newTexture->GetWidth();
  m_textureScaleX = 1.0f / m_textureWidth;
  if (m_textureHeight < newHeight)
    CLog::Log(LOGWARNING, "%s: allocated new texture with height of %d, requested %d", __FUNCTION__, m_textureHeight, newHeight);
  m_staticCache.Flush();
  m_dynamicCache.Flush();

  memset(newTexture->GetPixels(), 0, m_textureHeight * newTexture->GetPitch());
  if (m_texture)
  {
    m_updateY1 = 0;
    m_updateY2 = m_texture->GetHeight();

    unsigned char* src = m_texture->GetPixels();
    unsigned char* dst = newTexture->GetPixels();
    for (unsigned int y = 0; y < m_texture->GetHeight(); y++)
    {
      memcpy(dst, src, m_texture->GetPitch());
      src += m_texture->GetPitch();
      dst += newTexture->GetPitch();
    }
    delete m_texture;
  }

  m_textureStatus = TEXTURE_REALLOCATED;

  return newTexture;
}

bool CGUIFontTTFGLBase::CopyCharToTexture(FT_BitmapGlyph bitGlyph, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
  FT_Bitmap bitmap = bitGlyph->bitmap;

  unsigned char* source = bitmap.buffer;
  unsigned char* target = m_texture->GetPixels() + y1 * m_texture->GetPitch() + x1;

  for (unsigned int y = y1; y < y2; y++)
  {
    memcpy(target, source, x2-x1);
    source += bitmap.width;
    target += m_texture->GetPitch();
  }

  switch (m_textureStatus)
  {
  case TEXTURE_UPDATED:
    {
      m_updateY1 = std::min(m_updateY1, y1);
      m_updateY2 = std::max(m_updateY2, y2);
    }
    break;

  case TEXTURE_READY:
    {
      m_updateY1 = y1;
      m_updateY2 = y2;
      m_textureStatus = TEXTURE_UPDATED;
    }
    break;

  case TEXTURE_REALLOCATED:
    {
      m_updateY2 = std::max(m_updateY2, y2);
    }
    break;

  case TEXTURE_VOID:
  default:
    break;
  }

  return true;
}

void CGUIFontTTFGLBase::DeleteHardwareTexture()
{
  if (m_textureStatus != TEXTURE_VOID)
  {
    if (glIsTexture(m_nTexture))
      CServiceBroker::GetGUI()->GetTextureManager().ReleaseHwTexture(m_nTexture);

    m_textureStatus = TEXTURE_VOID;
    m_updateY1 = m_updateY2 = 0;
  }
}

void CGUIFontTTFGLBase::CreateStaticVertexBuffers()
{
  glGenBuffers(1, &m_elementArrayHandle);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_elementArrayHandle);

  // Create an array holding the mesh indices to convert quads to triangles
  int i = 0;
  std::array<std::array<int, 6>, 1000> index;
  for (auto &x : index)
  {
    x[0] = 4 * i;
    x[1] = 4 * i + 1;
    x[2] = 4 * i + 2;
    x[3] = 4 * i + 1;
    x[4] = 4 * i + 3;
    x[5] = 4 * i + 2;

    i++;
  }

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, index.size() * 6 * sizeof(int), index.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void CGUIFontTTFGLBase::DestroyStaticVertexBuffers()
{
  glDeleteBuffers(1, &m_elementArrayHandle);
}
