/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class SysfsUtils
{
public:
  static bool SetString(const std::string& path, const std::string& value);
  static const std::string GetString(const std::string& path);
  static bool SetInt(const std::string& path, const int value);
  static const int GetInt(const std::string& path);

  static bool Has(const std::string& path);
  static bool HasRW(const std::string& path);
};
