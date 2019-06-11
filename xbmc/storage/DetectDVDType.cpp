/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DetectDVDType.h"

#include "Application.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "threads/SingleLock.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace MEDIA_DETECT;

CDetectDVDMedia::CDetectDVDMedia() : CThread("DetectDVDMedia")
{
}

void CDetectDVDMedia::Process()
{
  while (!m_bStop)
  {
    if (g_application.GetAppPlayer().IsPlayingVideo())
    {
      Sleep(10000);
    }
    else
    {
      UpdateDvdrom();
      m_startup = false;
      if (m_autorun)
      {
        Sleep(1500); // Media in drive, wait 1.5s more to be sure the device is ready for playback
        m_run.Set();
        m_autorun = false;
      }
    }
  }
}

// Gets state of the DVD drive
void CDetectDVDMedia::UpdateDvdrom()
{
  // Signal for WaitMediaReady()
  // that we are busy detecting the
  // newly inserted media.
  {
    CSingleLock waitLock(m_readingMedia);
    switch (m_cdio.GetTrayState())
    {
      case DRIVE_OPEN:
      {
        // Send Message to GUI that disc been ejected
        SetNewDVDShareUrl("D:\\", g_localizeStrings.Get(502));
        CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REMOVED_MEDIA);
        CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage( msg );
        waitLock.Leave();
        m_driveState = DRIVE_OPEN;
        return;
      }
      case DRIVE_NOT_READY:
      {
        // Drive is not ready (closing, opening)
        SetNewDVDShareUrl("D:\\", g_localizeStrings.Get(503));
        m_driveState = DRIVE_NOT_READY;

        waitLock.Leave();
        CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
        CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage( msg );
        // Do we really need sleep here? This will fix: [ 1530771 ] "Open tray" problem
        // Sleep(6000);
        return;
      }
      case DRIVE_CLOSED_NO_MEDIA:
      {
        // Nothing in there...
        SetNewDVDShareUrl("D:\\", g_localizeStrings.Get(504));
        m_driveState = DRIVE_CLOSED_NO_MEDIA;
        // Send Message to GUI that disc has changed
        CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
        waitLock.Leave();
        CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage( msg );
        return;
      }
      case DRIVE_CLOSED_MEDIA_PRESENT:
      {
        m_driveState = DRIVE_CLOSED_MEDIA_PRESENT;
        DetectMediaType();
        CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
        waitLock.Leave();
        CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage( msg );
        // Tell the application object that a new Cd is inserted
        // So autorun can be started.
        if (!m_startup)
          m_autorun = true;

        return;
      }
      case DRIVE_NONE:
      case DRIVE_READY:
      default:
      {
        return;
      }
    }
  }
}

// Generates the drive url, (like iso9660://)
// from the CCdInfo class
void CDetectDVDMedia::DetectMediaType()
{
  CLog::Log(LOGINFO, "Detecting DVD-ROM media filesystem...");

  std::string strNewUrl;

  // Detect new CD-Information
  auto cdInfo = m_cdio.GetCdInfo();

  if (!cdInfo)
  {
    CLog::Log(LOGERROR, "Detection of DVD-ROM media failed.");
    return;
  }

  CLog::Log(LOGINFO, "Tracks overall:{} Audio tracks:{} Data tracks:{}",
            cdInfo->GetTrackCount(),
            cdInfo->GetAudioTrackCount(),
            cdInfo->GetDataTrackCount());

  if (cdInfo->IsISOHFS(1) || cdInfo->IsIso9660(1) || cdInfo->IsIso9660Interactive(1) ||
      (cdInfo->IsISOUDF(1) && !CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_detectAsUdf))
  {
    strNewUrl = "iso9660://";
  }
  else if (cdInfo->IsAudio(1))
  {
    strNewUrl = "cdda://local/";
  }
  else
  {
    strNewUrl = "udf://";
  }

  CLog::Log(LOGINFO, "Using protocol {}", strNewUrl.c_str());

  if (cdInfo->IsValidFs())
  {
    if (!cdInfo->IsAudio(1))
      CLog::Log(LOGINFO, "Disc label: %s", cdInfo->GetDiscLabel().c_str());
  }
  else
  {
    CLog::Log(LOGWARNING, "Filesystem is not supported");
  }

  std::string strLabel = cdInfo->GetDiscLabel();
  StringUtils::TrimRight(strLabel);

  SetNewDVDShareUrl(strNewUrl, strLabel);
}

void CDetectDVDMedia::SetNewDVDShareUrl(const std::string& strNewUrl, const std::string& strDiscLabel)
{
  m_diskLabel = strDiscLabel;
  m_diskPath = strNewUrl;
}

void CDetectDVDMedia::UpdateState()
{
  CSingleLock waitLock(m_readingMedia);
  DetectMediaType();
}

void CDetectDVDMedia::WaitMediaReady()
{
  CSingleLock waitLock(m_readingMedia);
}

int CDetectDVDMedia::DriveReady()
{
  return m_driveState;
}

bool CDetectDVDMedia::IsDiscInDrive()
{
  return m_driveState == DRIVE_CLOSED_MEDIA_PRESENT;
}

CCdInfo* CDetectDVDMedia::GetCdInfo()
{
  CSingleLock waitLock(m_readingMedia);
  return m_cdio.GetCdInfo().get(); // todo: fix
}

const std::string &CDetectDVDMedia::GetDVDLabel()
{
  return m_diskLabel;
}

const std::string &CDetectDVDMedia::GetDVDPath()
{
  return m_diskPath;
}
