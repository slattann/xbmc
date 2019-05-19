/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "utils/XTimeUtils.h"

#include <FileAPI.h>

namespace KODI
{
namespace TIME
{

void WINAPI Sleep(uint32_t dwMilliSeconds);
{
  Sleep(dwMilliSeconds);
}

void GetLocalTime(SystemTime* systemTime)
{
  SYSTEMTIME time;
  GetLocalTime(&time);

  systemTime->year = time.wYear;
  systemTime->month = time.wMonth;
  systemTime->dayOfWeek = time.wDayOfWeek;
  systemTime->day = time.wDay;
  systemTime->hour = time.wHour;
  systemTime->minute = time.wMinute;
  systemTime->second = time.wSecond;
  systemTime->milliseconds = time.wMilliseconds;
}

int FileTimeToLocalFileTime(const FileTime* fileTime, FileTime* localFileTime)
{
  const FILETIME file =
  {
    dwLowDateTime = fileTime->lowDateTime,
    dwHighDateTime = fileTime->highDateTime,
  };

  FILETIME localFile;
  int ret = FileTimeToLocalFileTime(&file, &localFile);

  localFileTime->lowDateTime = localFile.dwLowDateTime;
  localFileTime->highDateTime = localFile.dwHighDateTime;

  return ret;
}

int SystemTimeToFileTime(const SystemTime* systemTime, FileTime* fileTime)
{
  const SYSTEMTIME time =
  {
    wYear = systemTime->year,
    wMonth = systemTime->month,
    wDayOfWeek = systemTime->dayOfWeek,
    wDay = systemTime->day,
    wHour = systemTime->hour,
    wMinute = systemTime->minute,
    wSecond = systemTime->second,
    wMilliseconds = systemTime->milliseconds,
  };

  FILETIME file;
  int ret = SystemTimeToFileTime(&time, &file);

  fileTime->lowDateTime = file.dwLowDateTime;
  fileTime->highDateTime = file.dwHighDataTime;

  return ret;
}

long CompareFileTime(const FileTime* fileTime1, const FileTime* fileTime2)
{
  const FILETIME file1 =
  {
    dwLowDateTime = fileTime1->lowDateTime,
    dwHighDateTime = fileTime1->highDateTime,
  };

  const FILETIME file2 =
  {
    dwLowDateTime = fileTime2->lowDateTime,
    dwHighDateTime = fileTime2->highDateTime,
  };

  return CompareFileTime(&file1, &file2);
}

int FileTimeToSystemTime(const FileTime* fileTime, SystemTime* systemTime)
{
  const FILETIME file =
  {
    dwLowDateTime = fileTime->lowDateTime,
    dwHighDateTime = fileTime->highDateTime,
  };

  SYSTEMTIME time;
  int ret = FileTimeToSystemTime(&file, &time);

  systemTime->year = time.wYear;
  systemTime->month = time.wMonth;
  systemTime->dayOfWeek = time.wDayOfWeek;
  systemTime->day = time.wDay;
  systemTime->hour = time.wHour;
  systemTime->minute = time.wMinute;
  systemTime->second = time.wSecond;
  systemTime->milliseconds = time.wMilliseconds;

  return ret;
}

int LocalFileTimeToFileTime(const FileTime* localFileTime, FileTime* fileTime)
{
  const FILETIME localFile =
  {
    dwLowDateTime = localFileTime->lowDateTime,
    dwHighDateTime = localFileTime->highDateTime,
  };

  FILETIME file;

  int ret = LocalFileTimeToFileTime(&localFile, &file);

  fileTime->lowDateTime = file.dwLowDateTime;
  fileTime->highDateTime = file.dwHighDataTime;

  return ret;
}

}
}