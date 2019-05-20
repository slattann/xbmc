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

void GetLocalTime(LPSYSTEMTIME);
{
  GetLocalTime(LPSYSTEMTIME);
}

int FileTimeToLocalFileTime(const FILETIME* lpFileTime, LPFILETIME lpLocalFileTime)
{
  return FileTimeToLocalFileTime(lpFileTime, lpLocalFileTime);
}

int SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime,  LPFILETIME lpFileTime)
{
  return SystemTimeToFileTime(lpSystemTime, lpFileTime);
}

long CompareFileTime(const FILETIME* lpFileTime1, const FILETIME* lpFileTime2)
{
  return CompareFileTime(lpFileTime1, lpFileTime2);
}

int FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime)
{
  return FileTimeToSystemTime(lpFileTime, lpSystemTime);
}

int LocalFileTimeToFileTime(const FILETIME* lpLocalFileTime, LPFILETIME lpFileTime)
{
  return LocalFileTimeToFileTime(lpLocalFileTime, lpLocalFileTime);
}

}
}