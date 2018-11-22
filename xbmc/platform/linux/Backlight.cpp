/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "Backlight.h"

#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/SysfsUtils.h"

#include <dirent.h>
#include <unistd.h>
#include <vector>

void CBacklight::Register(std::shared_ptr<CBacklight> backlight)
{
  IBacklight::Register(backlight);
}

int CBacklight::GetBrightness()
{
  return SysfsUtils::GetInt(m_backlightPath + "/actual_brightness");
}

int CBacklight::GetMaxBrightness()
{
  return SysfsUtils::GetInt(m_backlightPath + "/max_brightness");
}

bool CBacklight::SetBrightness(int brightness)
{
  if (!SysfsUtils::SetInt(m_backlightPath + "/brightness", brightness))
  {
    return false;
  }

  return true;
}

bool CBacklight::Init(const std::string& drmDevicePath)
{
  std::string backlightPath{"/sys/class/backlight"};

  std::vector<std::string> drmDevicePathSplit = StringUtils::Split(drmDevicePath, "/");

  std::string drmDeviceSysPath{"/sys/class/drm/" + drmDevicePathSplit.back() + "/device"};

  CLog::Log(LOGDEBUG, "{}: using drm sys path {}", __FUNCTION__, drmDeviceSysPath);

  std::string buffer(256, '\0');
  auto length = readlink(drmDeviceSysPath.c_str(), &buffer[0], buffer.size());
  if (length < 0)
  {
    CLog::Log(LOGERROR, "{}: failed to readlink for {} - {}", __FUNCTION__, drmDeviceSysPath, strerror(errno));
    return false;
  }

  buffer.resize(length);

  std::string drmPciName = basename(buffer.c_str());

  CLog::Log(LOGDEBUG, "{}: drm pci name {}", __FUNCTION__, drmPciName);

  auto backlightDir = opendir(backlightPath.c_str());

  if (!backlightDir)
  {
    CLog::Log(LOGERROR, "{}: failed to open directory {} - {}", __FUNCTION__, backlightPath, strerror(errno));
    return false;
  }

  dirent* entry{nullptr};
  while ((entry = readdir(backlightDir)))
  {
    if (entry->d_name[0] == '.')
    {
      continue;
    }

    std::string backlightDirEntryPath{backlightPath + "/" + entry->d_name};

    CLog::Log(LOGDEBUG, "{}: backlight path {}", __FUNCTION__, backlightDirEntryPath);

    std::string backlightType = SysfsUtils::GetString(backlightDirEntryPath + "/type");
    if (backlightType.empty())
    {
      continue;
    }

    CLog::Log(LOGDEBUG, "{}: backlight type {}", __FUNCTION__, backlightType);

    std::string backlightDirEntryDevicePath{backlightDirEntryPath + "/device"};

    CLog::Log(LOGDEBUG, "{}: backlight device path {}", __FUNCTION__, backlightDirEntryDevicePath);

    buffer.resize(256);
    length = readlink(backlightDirEntryDevicePath.c_str(), &buffer[0], buffer.size());
    if (length < 0)
    {
      CLog::Log(LOGERROR, "{}: failed to readlink for {} - {}", __FUNCTION__, backlightDirEntryDevicePath, strerror(errno));
      continue;
    }

    buffer.resize(length);

    std::string backlightPciName = basename(buffer.c_str());

    if (backlightType == "raw\n" || backlightType == "firmware\n")
    {
      if (!drmPciName.empty() && drmPciName != backlightPciName)
      {
        continue;
      }
    }

    m_backlightPath = backlightDirEntryPath;

    CLog::Log(LOGDEBUG, "{}: backlight path {}", __FUNCTION__, m_backlightPath);

    return true;
  }

  return false;
}
