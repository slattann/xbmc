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

  void GetLocalTime(SystemTime* systemTime);

  void WINAPI Sleep(uint32_t dwMilliSeconds);

  int FileTimeToLocalFileTime(const FILETIME* lpFileTime, LPFILETIME lpLocalFileTime);
  int SystemTimeToFileTime(const SystemTime* systemTime,  LPFILETIME lpFileTime);
  long CompareFileTime(const FILETIME* lpFileTime1, const FILETIME* lpFileTime2);
  int FileTimeToSystemTime( const FILETIME* lpFileTime, SystemTime* systemTime);
  int LocalFileTimeToFileTime( const FILETIME* lpLocalFileTime, LPFILETIME lpFileTime);

  int FileTimeToTimeT(const FILETIME* lpLocalFileTime, time_t *pTimeT);
  int TimeTToFileTime(time_t timeT, FILETIME* lpLocalFileTime);
}
}
