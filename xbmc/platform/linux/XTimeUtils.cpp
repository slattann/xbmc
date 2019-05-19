/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "XTimeUtils.h"
#include "LinuxTimezone.h"

#if defined(TARGET_DARWIN)
#include "threads/Atomics.h"
#endif

#if defined(TARGET_ANDROID) && !defined(__LP64__)
#include <time64.h>
#endif

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/times.h>
#include <sched.h>

#define WIN32_TIME_OFFSET ((unsigned long long)(369 * 365 + 89) * 24 * 3600 * 10000000)

/*
 * A Leap year is any year that is divisible by four, but not by 100 unless also
 * divisible by 400
 */
#define IsLeapYear(y) ((!(y % 4)) ? (((!(y % 400)) && (y % 100)) ? 1 : 0) : 0)

void WINAPI Sleep(uint32_t dwMilliSeconds)
{
#if _POSIX_PRIORITY_SCHEDULING
  if(dwMilliSeconds == 0)
  {
    sched_yield();
    return;
  }
#endif

  usleep(dwMilliSeconds * 1000);
}

void GetLocalTime(LPSYSTEMTIME sysTime)
{
  const time_t t = time(NULL);
  struct tm now;

  localtime_r(&t, &now);
  sysTime->wYear = now.tm_year + 1900;
  sysTime->wMonth = now.tm_mon + 1;
  sysTime->wDayOfWeek = now.tm_wday;
  sysTime->wDay = now.tm_mday;
  sysTime->wHour = now.tm_hour;
  sysTime->wMinute = now.tm_min;
  sysTime->wSecond = now.tm_sec;
  sysTime->wMilliseconds = 0;
  // NOTE: localtime_r() is not required to set this, but we Assume that it's set here.
  g_timezone.m_IsDST = now.tm_isdst;
}

int FileTimeToLocalFileTime(const FileTime* fileTime, FileTime* localFileTime)
{
  ULARGE_INTEGER l;
  l.u.LowPart = fileTime->lowDateTime;
  l.u.HighPart = fileTime->highDateTime;

  time_t ft;
  struct tm tm_ft;
  FileTimeToTimeT(fileTime, &ft);
  localtime_r(&ft, &tm_ft);

  l.QuadPart += static_cast<unsigned long long>(tm_ft.tm_gmtoff) * 10000000;

  localFileTime->lowDateTime = l.u.LowPart;
  localFileTime->highDateTime = l.u.HighPart;
  return 1;
}

int SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, FileTime* fileTime)
{
  static const int dayoffset[12] = {0, 31, 59, 90, 120, 151, 182, 212, 243, 273, 304, 334};
#if defined(TARGET_DARWIN)
  static std::atomic_flag timegm_lock = ATOMIC_FLAG_INIT;
#endif

  struct tm sysTime = {};
  sysTime.tm_year = lpSystemTime->wYear - 1900;
  sysTime.tm_mon = lpSystemTime->wMonth - 1;
  sysTime.tm_wday = lpSystemTime->wDayOfWeek;
  sysTime.tm_mday = lpSystemTime->wDay;
  sysTime.tm_hour = lpSystemTime->wHour;
  sysTime.tm_min = lpSystemTime->wMinute;
  sysTime.tm_sec = lpSystemTime->wSecond;
  sysTime.tm_yday = dayoffset[sysTime.tm_mon] + (sysTime.tm_mday - 1);
  sysTime.tm_isdst = g_timezone.m_IsDST;

  // If this is a leap year, and we're past the 28th of Feb, increment tm_yday.
  if (IsLeapYear(lpSystemTime->wYear) && (sysTime.tm_yday > 58))
    sysTime.tm_yday++;

#if defined(TARGET_DARWIN)
  CAtomicSpinLock lock(timegm_lock);
#endif

#if defined(TARGET_ANDROID) && !defined(__LP64__)
  time64_t t = timegm64(&sysTime);
#else
  time_t t = timegm(&sysTime);
#endif

  LARGE_INTEGER result;
  result.QuadPart = (long long) t * 10000000 + (long long) lpSystemTime->wMilliseconds * 10000;
  result.QuadPart += WIN32_TIME_OFFSET;

  fileTime->lowDateTime = result.u.LowPart;
  fileTime->highDateTime = result.u.HighPart;

  return 1;
}

long CompareFileTime(const FileTime* fileTime1, const FileTime* fileTime2)
{
  ULARGE_INTEGER t1;
  t1.u.LowPart = fileTime1->lowDateTime;
  t1.u.HighPart = fileTime1->highDateTime;

  ULARGE_INTEGER t2;
  t2.u.LowPart = fileTime2->lowDateTime;
  t2.u.HighPart = fileTime2->highDateTime;

  if (t1.QuadPart == t2.QuadPart)
     return 0;
  else if (t1.QuadPart < t2.QuadPart)
     return -1;
  else
     return 1;
}

int FileTimeToSystemTime( const FileTime* fileTime, LPSYSTEMTIME lpSystemTime)
{
  LARGE_INTEGER file;
  file.u.LowPart = fileTime->lowDateTime;
  file.u.HighPart = fileTime->highDateTime;

  file.QuadPart -= WIN32_TIME_OFFSET;
  file.QuadPart /= 10000; /* to milliseconds */
  lpSystemTime->wMilliseconds = file.QuadPart % 1000;
  file.QuadPart /= 1000; /* to seconds */

  time_t ft = file.QuadPart;

  struct tm tm_ft;
  gmtime_r(&ft,&tm_ft);

  lpSystemTime->wYear = tm_ft.tm_year + 1900;
  lpSystemTime->wMonth = tm_ft.tm_mon + 1;
  lpSystemTime->wDayOfWeek = tm_ft.tm_wday;
  lpSystemTime->wDay = tm_ft.tm_mday;
  lpSystemTime->wHour = tm_ft.tm_hour;
  lpSystemTime->wMinute = tm_ft.tm_min;
  lpSystemTime->wSecond = tm_ft.tm_sec;

  return 1;
}

int LocalFileTimeToFileTime( const FileTime* localFileTime, FileTime* fileTime)
{
  ULARGE_INTEGER l;
  l.u.LowPart = localFileTime->lowDateTime;
  l.u.HighPart = localFileTime->highDateTime;

  l.QuadPart += (unsigned long long) timezone * 10000000;

  fileTime->lowDateTime = l.u.LowPart;
  fileTime->highDateTime = l.u.HighPart;

  return 1;
}

int FileTimeToTimeT(const FileTime* localFileTime, time_t *pTimeT) {

  if (!localFileTime || !pTimeT)
    return false;

  ULARGE_INTEGER fileTime;
  fileTime.u.LowPart  = localFileTime->lowDateTime;
  fileTime.u.HighPart = localFileTime->highDateTime;

  fileTime.QuadPart -= WIN32_TIME_OFFSET;
  fileTime.QuadPart /= 10000; /* to milliseconds */
  fileTime.QuadPart /= 1000; /* to seconds */

  time_t ft = fileTime.QuadPart;

  struct tm tm_ft;
  localtime_r(&ft,&tm_ft);

  *pTimeT = mktime(&tm_ft);
  return 1;
}

int TimeTToFileTime(time_t timeT, FileTime* localFileTime) {

  if (!localFileTime)
    return false;

  ULARGE_INTEGER result;
  result.QuadPart = (unsigned long long) timeT * 10000000;
  result.QuadPart += WIN32_TIME_OFFSET;

  localFileTime->lowDateTime  = result.u.LowPart;
  localFileTime->highDateTime = result.u.HighPart;

  return 1;
}
