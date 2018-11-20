/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SysfsUtils.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

static const unsigned int PAGESIZE = getpagesize();

bool SysfsUtils::SetString(const std::string& path, const std::string& value)
{
  auto oldValue = GetString(path);
  if (oldValue == value)
  {
    return true;
  }

  if (value.size() > PAGESIZE)
  {
    CLog::Log(LOGERROR, "{}: string length larger than page size {}", __FUNCTION__, PAGESIZE);
    return false;
  }

  int fd = open(path.c_str(), O_RDWR);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "{}: error opening {}", __FUNCTION__, path);
    return false;
  }

  auto ret = write(fd, value.c_str(), value.size());
  close(fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "{}: error writing {}", __FUNCTION__, path);
    return false;
  }

  return true;
}

const std::string SysfsUtils::GetString(const std::string& path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "{}: error opening {}", __FUNCTION__, path);
    return "";
  }

  char buffer[PAGESIZE];
  auto length = read(fd, buffer, sizeof(buffer));
  close(fd);
  if (length < 0)
  {
    CLog::Log(LOGERROR, "{}: error reading {}", __FUNCTION__, path);
    return "";
  }

  std::string value(buffer, length);

  return StringUtils::Trim(value);
}

bool SysfsUtils::SetInt(const std::string& path, const int value)
{
  auto oldValue = GetInt(path);
  if (oldValue == value)
  {
    return true;
  }

  std::string buffer{std::to_string(value)};

  if (buffer.size() > PAGESIZE)
  {
    CLog::Log(LOGERROR, "{}: int length larger than page size {}", __FUNCTION__, PAGESIZE);
    return false;
  }

  int fd = open(path.c_str(), O_RDWR);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "{}: error opening {}", __FUNCTION__, path);
    return false;
  }

  auto ret = write(fd, buffer.c_str(), buffer.size());
  close(fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "{}: error writing {}", __FUNCTION__, path);
    return false;
  }

  return true;
}

const int SysfsUtils::GetInt(const std::string& path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
  {
    CLog::Log(LOGERROR, "{}: error opening {}", __FUNCTION__, path);
    return -1;
  }

  char buffer[PAGESIZE];
  auto length = read(fd, buffer, sizeof(buffer));
  close(fd);
  if (length < 0)
  {
    CLog::Log(LOGERROR, "{}: error reading {}", __FUNCTION__, path);
    return -1;
  }

  std::string value(buffer, length);

  return stoi(value);
}

bool SysfsUtils::Has(const std::string &path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
  {
    return false;
  }

  close(fd);
  return true;
}

bool SysfsUtils::HasRW(const std::string &path)
{
  int fd = open(path.c_str(), O_RDWR);
  if (fd < 0)
  {
    return false;
  }

  close(fd);
  return true;
}
