/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "platform/linux/DBusUtil.h"

class CLogindUtils
{
public:
  CLogindUtils() = default;
  ~CLogindUtils() = default;

  bool Connect();
  void Destroy();

  static std::string GetSessionPath();
  static int TakeDevice(const std::string& sessionPath, uint32_t major, uint32_t minor);
  static void ReleaseDevice(const std::string& sessionPath, uint32_t major, uint32_t minor);

private:
  bool TakeControl();
  bool Activate();

  void ReleaseControl();

  std::string m_sessionPath;

  CDBusConnection m_connection;
};
