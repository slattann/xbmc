/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/GUITexture.h"

#include <vector>

namespace GUITEXTURE
{
typedef CGUITexture* (*CreateTextureCopy)(const CGUITexture& left);
typedef CGUITexture* (*CreateTexture)(float posX, float posY, float width, float height, const CTextureInfo& texture);
}

class CGUITextureFactory
{
public:
  static CGUITexture* CreateTexture(const CGUITexture& left);
  static CGUITexture* CreateTexture(float posX, float posY, float width, float height, const CTextureInfo& texture);

  static void RegisterTexture(GUITEXTURE::CreateTextureCopy createFunc);
  static void RegisterTexture(GUITEXTURE::CreateTexture createFunc);
  static void ClearTextures();

protected:

  static std::vector<GUITEXTURE::CreateTextureCopy> m_texturesCopy;
  static std::vector<GUITEXTURE::CreateTexture> m_textures;
};
