/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUITextureFactory.h"

#include "threads/SingleLock.h"

CCriticalSection guiTextureSection;
std::vector<GUITEXTURE::CreateTextureCopy> CGUITextureFactory::m_texturesCopy;
std::vector<GUITEXTURE::CreateTexture> CGUITextureFactory::m_textures;

CGUITexture* CGUITextureFactory::CreateTexture(const CGUITexture& left)
{
  CSingleLock lock(guiTextureSection);

  return m_texturesCopy.back()(left);
}

CGUITexture* CGUITextureFactory::CreateTexture(float posX, float posY, float width, float height, const CTextureInfo& texture)
{
  CSingleLock lock(guiTextureSection);

  return m_textures.back()(posX, posY, width, height, texture);
}

void CGUITextureFactory::RegisterTexture(GUITEXTURE::CreateTextureCopy createFunc)
{
  CSingleLock lock(guiTextureSection);

  m_texturesCopy.emplace_back(createFunc);
}

void CGUITextureFactory::RegisterTexture(GUITEXTURE::CreateTexture createFunc)
{
  CSingleLock lock(guiTextureSection);

  m_textures.emplace_back(createFunc);
}

void CGUITextureFactory::ClearTextures()
{
  CSingleLock lock(guiTextureSection);

  m_texturesCopy.clear();
  m_textures.clear();
}
