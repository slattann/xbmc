/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "PlatformDefs.h"
#include "XBDateTime.h"

void GetLocalTime(LPSYSTEMTIME);

void WINAPI Sleep(uint32_t dwMilliSeconds);

int FileTimeToLocalFileTime(const FileTime* fileTime, FileTime* localFileTime);
int SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, FileTime* fileTime);
long CompareFileTime(const FileTime* fileTime1, const FileTime* fileTime2);
int FileTimeToSystemTime( const FileTime* fileTime, LPSYSTEMTIME lpSystemTime);
int LocalFileTimeToFileTime( const FileTime* localFileTime, FileTime* fileTime);

int FileTimeToTimeT(const FileTime* localFileTime, time_t *pTimeT);
int TimeTToFileTime(time_t timeT, FileTime* localFileTime);
