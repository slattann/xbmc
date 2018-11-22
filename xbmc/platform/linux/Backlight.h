/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/IBacklight.h"

#include <memory>
#include <string>

class CBacklight : public IBacklight
{
public:
  CBacklight() = default;
  ~CBacklight() = default;

  static void Register(std::shared_ptr<CBacklight> backlight);

  bool Init(const std::string& drmDevicePath);

  // CBacklight Overrides
  bool SetBrightness(int brightness) override;
  int GetBrightness() override;
  int GetMaxBrightness() override;

private:
  std::string m_backlightPath;
};
