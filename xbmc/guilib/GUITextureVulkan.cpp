/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUITextureVulkan.h"

#include "Texture.h"

CGUITextureVulkan::CGUITextureVulkan(
    float posX, float posY, float width, float height, const CTextureInfo& texture)
  : CGUITextureBase(posX, posY, width, height, texture)
{
}

void CGUITextureVulkan::Begin(UTILS::Color color)
{
}

void CGUITextureVulkan::End()
{
}

void CGUITextureVulkan::Draw(
    float* x, float* y, float* z, const CRect& texture, const CRect& diffuse, int orientation)
{
}
