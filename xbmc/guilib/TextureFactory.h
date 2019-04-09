/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/Texture.h"

#include <vector>

typedef CTexture* (*CreateTexture)(unsigned int width, unsigned int height, unsigned int format);

class CTextureFactory
{
public:
  static CTexture* CreateTexture(unsigned int width, unsigned int height, unsigned int format);

  static void RegisterTexture(::CreateTexture createFunc);
  static void ClearTextures();

protected:

  static std::vector<::CreateTexture> m_textures;
};
