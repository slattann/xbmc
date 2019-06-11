/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  Copyright (C) 2003 Bobbin007
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"
#include "threads/Thread.h"
#include "storage/cdioSupport.h"

#include <memory>
#include <string>

namespace MEDIA_DETECT
{
class CCdInfo;

class CDetectDVDMedia : public CThread
{
public:
  CDetectDVDMedia();
  ~CDetectDVDMedia() = default;

  void Process() override;

  void WaitMediaReady();
  bool IsDiscInDrive();
  int DriveReady();
  CCdInfo* GetCdInfo();

  CEvent* GetAutoRun() { return &m_run; }

  const std::string &GetDVDLabel();
  const std::string &GetDVDPath();

  void UpdateState();
protected:
  void UpdateDvdrom();

  void DetectMediaType();
  void SetNewDVDShareUrl(const std::string& strNewUrl, const std::string& strDiscLabel);

private:
  int m_driveState = DRIVE_CLOSED_NO_MEDIA;

  CCriticalSection m_readingMedia;
  std::string m_diskLabel;
  std::string m_diskPath;

  bool m_startup{true};
  bool m_autorun{false};

  CEvent m_run;

  CCdIoSupport m_cdio;
};
}
