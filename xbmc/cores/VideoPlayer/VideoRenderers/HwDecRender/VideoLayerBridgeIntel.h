/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VideoLayerBridgeDRMPRIME.h"

namespace KODI
{
namespace WINDOWING
{
namespace GBM
{
class CDRMUtils;
}
}
}

class CVideoLayerBridgeIntel : public CVideoLayerBridgeDRMPRIME
{
public:
  CVideoLayerBridgeIntel(std::shared_ptr<KODI::WINDOWING::GBM::CDRMUtils> drm);
  ~CVideoLayerBridgeIntel() = default;

  void Disable() override;
  void Configure(IVideoBufferDRMPRIME* buffer) override;
};
