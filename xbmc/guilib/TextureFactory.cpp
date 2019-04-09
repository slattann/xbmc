/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TextureFactory.h"

#include "threads/SingleLock.h"

CCriticalSection textureSection;
std::vector<::CreateTexture> CTextureFactory::m_textures;

CTexture* CTextureFactory::CreateTexture(unsigned int width, unsigned int height, unsigned int format)
{
  CSingleLock lock(textureSection);

  return m_textures.back()(width, height, format);
}

void CTextureFactory::RegisterTexture(::CreateTexture createFunc)
{
  CSingleLock lock(textureSection);

  m_textures.emplace_back(createFunc);
}

void CTextureFactory::ClearTextures()
{
  CSingleLock lock(textureSection);

  m_textures.clear();
}
