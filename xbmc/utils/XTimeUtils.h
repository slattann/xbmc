/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#if !defined(TARGET_WINDOWS)
#include "PlatformDefs.h"
#endif

namespace KODI
{
namespace TIME
{
  struct SystemTime
  {
    unsigned short year;
    unsigned short month;
    unsigned short dayOfWeek;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
    unsigned short milliseconds;
  };

  struct TimeZoneInformation
  {
    long bias;
    wchar_t standardName[32];
    SystemTime standardDate;
    long standardBias;
    wchar_t daylightName[32];
    SystemTime daylightDate;
    long daylightBias;
  };

  const int TIME_ZONE_ID_INVALID{-1};
  const int TIME_ZONE_ID_UNKNOWN{0};
  const int TIME_ZONE_ID_STANDARD{1};
  const int TIME_ZONE_ID_DAYLIGHT{2};

  struct FileTime
  {
    unsigned int lowDateTime;
    unsigned int highDateTime;
  };

  void GetLocalTime(SystemTime* systemTime);

  void WINAPI Sleep(uint32_t dwMilliSeconds);

  int FileTimeToLocalFileTime(const FileTime* lpFileTime, FileTime* localFileTime);
  int SystemTimeToFileTime(const SystemTime* systemTime, FileTime* fileTime);
  long CompareFileTime(const FileTime* fileTime1, const FileTime* fileTime2);
  int FileTimeToSystemTime(const FileTime* fileTime, SystemTime* systemTime);
  int LocalFileTimeToFileTime(const FileTime* localFileTime, FileTime* fileTime);

  int FileTimeToTimeT(const FileTime* localFileTime, time_t *pTimeT);
  int TimeTToFileTime(time_t timeT, FileTime* localFileTime);
}
}
