/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MediaManager.h"
#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "guilib/LocalizeStrings.h"
#include "URL.h"
#include "utils/URIUtils.h"
#ifdef TARGET_WINDOWS
#include "platform/win32/WIN32Util.h"
#include "utils/CharsetConverter.h"
#endif
#include "guilib/GUIWindowManager.h"

//! @todo switch all ports to use auto sources
#include <map>
#include <utility>
#include "DetectDVDType.h"
// #include "filesystem/ISO9660File.h"

#include "Autorun.h"
#include "addons/VFSEntry.h"
#include "GUIUserMessages.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/XBMCTinyXML.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "utils/JobManager.h"
#include "utils/StringUtils.h"
#include "AutorunMediaJob.h"

#include "filesystem/File.h"
#include "cores/VideoPlayer/DVDInputStreams/DVDInputStreamNavigator.h"

#include <string>
#include <vector>

#ifdef HAVE_LIBBLURAY
#include "filesystem/BlurayDirectory.h"
#endif

using namespace XFILE;

using namespace MEDIA_DETECT;
#

const char MEDIA_SOURCES_XML[] = { "special://profile/mediasources.xml" };

CMediaManager::CMediaManager()
{
  m_bhasoptical = false;
  m_platformStorage = nullptr;
}

void CMediaManager::Stop()
{
  if (m_platformStorage)
    m_platformStorage->Stop();

  delete m_platformStorage;
  m_platformStorage = nullptr;
}

void CMediaManager::Initialize()
{
  if (!m_platformStorage)
  {
    m_platformStorage = IStorageProvider::CreateInstance();
  }

  m_platformStorage->Initialize();
}

bool CMediaManager::LoadSources()
{
  // clear our location list
  m_locations.clear();

  // load xml file...
  CXBMCTinyXML xmlDoc;
  if ( !xmlDoc.LoadFile( MEDIA_SOURCES_XML ) )
    return false;

  TiXmlElement* pRootElement = xmlDoc.RootElement();
  if ( !pRootElement || strcmpi(pRootElement->Value(), "mediasources") != 0)
  {
    CLog::Log(LOGERROR, "Error loading %s, Line %d (%s)", MEDIA_SOURCES_XML, xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
    return false;
  }

  // load the <network> block
  TiXmlNode *pNetwork = pRootElement->FirstChild("network");
  if (pNetwork)
  {
    TiXmlElement *pLocation = pNetwork->FirstChildElement("location");
    while (pLocation)
    {
      CNetworkLocation location;
      pLocation->Attribute("id", &location.id);
      if (pLocation->FirstChild())
      {
        location.path = pLocation->FirstChild()->Value();
        m_locations.push_back(location);
      }
      pLocation = pLocation->NextSiblingElement("location");
    }
  }
  return true;
}

bool CMediaManager::SaveSources()
{
  CXBMCTinyXML xmlDoc;
  TiXmlElement xmlRootElement("mediasources");
  TiXmlNode *pRoot = xmlDoc.InsertEndChild(xmlRootElement);
  if (!pRoot) return false;

  TiXmlElement networkNode("network");
  TiXmlNode *pNetworkNode = pRoot->InsertEndChild(networkNode);
  if (pNetworkNode)
  {
    for (std::vector<CNetworkLocation>::iterator it = m_locations.begin(); it != m_locations.end(); ++it)
    {
      TiXmlElement locationNode("location");
      locationNode.SetAttribute("id", (*it).id);
      TiXmlText value((*it).path);
      locationNode.InsertEndChild(value);
      pNetworkNode->InsertEndChild(locationNode);
    }
  }
  return xmlDoc.SaveFile(MEDIA_SOURCES_XML);
}

void CMediaManager::GetLocalDrives(VECSOURCES &localDrives, bool includeQ)
{
  CSingleLock lock(m_CritSecStorageProvider);
  m_platformStorage->GetLocalDrives(localDrives);
}

void CMediaManager::GetRemovableDrives(VECSOURCES &removableDrives)
{
  CSingleLock lock(m_CritSecStorageProvider);
  if (m_platformStorage)
    m_platformStorage->GetRemovableDrives(removableDrives);
}

void CMediaManager::GetNetworkLocations(VECSOURCES &locations, bool autolocations)
{
  // Load our xml file
  LoadSources();
  for (unsigned int i = 0; i < m_locations.size(); i++)
  {
    CMediaSource share;
    share.strPath = m_locations[i].path;
    CURL url(share.strPath);
    share.strName = url.GetWithoutUserDetails();
    locations.push_back(share);
  }
  if (autolocations)
  {
    CMediaSource share;
    share.m_ignore = true;
#ifdef HAS_FILESYSTEM_SMB
    share.strPath = "smb://";
    share.strName = g_localizeStrings.Get(20171);
    locations.push_back(share);
#endif

#ifdef HAS_FILESYSTEM_NFS
    share.strPath = "nfs://";
    share.strName = g_localizeStrings.Get(20259);
    locations.push_back(share);
#endif// HAS_FILESYSTEM_NFS

#ifdef HAS_UPNP
    if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_SERVICES_UPNP))
    {
      std::string strDevices = g_localizeStrings.Get(33040); //"% Devices"
      share.strPath = "upnp://";
      share.strName = StringUtils::Format(strDevices.c_str(), "UPnP"); //"UPnP Devices"
      locations.push_back(share);
    }
#endif

#ifdef HAS_ZEROCONF
    share.strPath = "zeroconf://";
    share.strName = g_localizeStrings.Get(20262);
    locations.push_back(share);
#endif

    if (CServiceBroker::IsBinaryAddonCacheUp())
    {
      for (const auto& addon : CServiceBroker::GetVFSAddonCache().GetAddonInstances())
      {
        const auto& info = addon->GetProtocolInfo();
        if (!info.type.empty() && info.supportBrowsing)
        {
          share.strPath = info.type + "://";
          share.strName = g_localizeStrings.Get(info.label);
          locations.push_back(share);
        }
      }
    }
  }
}

bool CMediaManager::AddNetworkLocation(const std::string &path)
{
  CNetworkLocation location;
  location.path = path;
  location.id = (int)m_locations.size();
  m_locations.push_back(location);
  return SaveSources();
}

bool CMediaManager::HasLocation(const std::string& path) const
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (URIUtils::CompareWithoutSlashAtEnd(m_locations[i].path, path))
      return true;
  }

  return false;
}


bool CMediaManager::RemoveLocation(const std::string& path)
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (URIUtils::CompareWithoutSlashAtEnd(m_locations[i].path, path))
    {
      // prompt for sources, remove, cancel,
      m_locations.erase(m_locations.begin()+i);
      return SaveSources();
    }
  }

  return false;
}

bool CMediaManager::SetLocationPath(const std::string& oldPath, const std::string& newPath)
{
  for (unsigned int i=0;i<m_locations.size();++i)
  {
    if (URIUtils::CompareWithoutSlashAtEnd(m_locations[i].path, oldPath))
    {
      m_locations[i].path = newPath;
      return SaveSources();
    }
  }

  return false;
}

void CMediaManager::AddAutoSource(const CMediaSource &share, bool bAutorun)
{
  CMediaSourceSettings::GetInstance().AddShare("files", share);
  CMediaSourceSettings::GetInstance().AddShare("video", share);
  CMediaSourceSettings::GetInstance().AddShare("pictures", share);
  CMediaSourceSettings::GetInstance().AddShare("music", share);
  CMediaSourceSettings::GetInstance().AddShare("programs", share);
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
  CGUIComponent *gui = CServiceBroker::GetGUI();
  if (gui)
    gui->GetWindowManager().SendThreadMessage( msg );

  if(bAutorun)
    MEDIA_DETECT::CAutorun::ExecuteAutorun(share.strPath);
}

void CMediaManager::RemoveAutoSource(const CMediaSource &share)
{
  CMediaSourceSettings::GetInstance().DeleteSource("files", share.strName, share.strPath, true);
  CMediaSourceSettings::GetInstance().DeleteSource("video", share.strName, share.strPath, true);
  CMediaSourceSettings::GetInstance().DeleteSource("pictures", share.strName, share.strPath, true);
  CMediaSourceSettings::GetInstance().DeleteSource("music", share.strName, share.strPath, true);
  CMediaSourceSettings::GetInstance().DeleteSource("programs", share.strName, share.strPath, true);
  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE_SOURCES);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage( msg );

  // delete cached CdInfo if any
  RemoveDiscInfo(TranslateDevicePath(share.strPath, true));
}

/////////////////////////////////////////////////////////////
// AutoSource status functions:
//! @todo translate cdda://<device>/

std::string CMediaManager::TranslateDevicePath(const std::string& devicePath, bool bReturnAsDevice)
{
  CSingleLock waitLock(m_muAutoSource);
  std::string strDevice = devicePath;
  // fallback for cdda://local/ and empty devicePath

  if(devicePath.empty() || StringUtils::StartsWith(devicePath, "cdda://local"))
    strDevice = m_strFirstAvailDrive;

#ifdef TARGET_WINDOWS
  if(!m_bhasoptical)
    return "";

  if(bReturnAsDevice == false)
    StringUtils::Replace(strDevice, "\\\\.\\","");
  else if(!strDevice.empty() && strDevice[1]==':')
    strDevice = StringUtils::Format("\\\\.\\%c:", strDevice[0]);

  URIUtils::RemoveSlashAtEnd(strDevice);
#endif
  return strDevice;
}

bool CMediaManager::IsDiscInDrive()
{
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return false;

  return media->IsDiscInDrive();
}

bool CMediaManager::IsAudio()
{
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return false;

  auto info = media->GetCdInfo();
  if (!info)
    return false;

  return info->IsAudio(1);
}

int CMediaManager::GetDriveStatus()
{
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return DRIVE_NOT_READY;

  return media->DriveReady();
}

CCdInfo* CMediaManager::GetCdInfo()
{
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return nullptr;

  return media->GetCdInfo();
}

std::string CMediaManager::GetDiskLabel()
{
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return "";

  return media->GetDVDLabel();
}

std::string CMediaManager::GetDiskUniqueId()
{
  std::string mediaPath;

  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return "";

  CCdInfo* pInfo = media->GetCdInfo();
  if (!pInfo)
    return "";

  if (mediaPath.empty() && pInfo->IsAudio(1))
    mediaPath = "cdda://local/";

  if (mediaPath.empty() && (pInfo->IsISOUDF(1) || pInfo->IsISOHFS(1) || pInfo->IsIso9660(1) || pInfo->IsIso9660Interactive(1)))
    mediaPath = "iso9660://";

  if (mediaPath.empty() && pInfo->IsUDF(1))
    mediaPath = "udf://";

  DiscInfo info = GetDiscInfo(mediaPath);
  if (info.empty())
  {
    CLog::Log(LOGDEBUG, "GetDiskUniqueId: Retrieving ID for path %s failed, ID is empty.", CURL::GetRedacted(mediaPath).c_str());
    return "";
  }

  std::string strID = StringUtils::Format("removable://%s_%s", info.name.c_str(), info.serial.c_str());
  CLog::Log(LOGDEBUG, "GetDiskUniqueId: Got ID %s for %s with path %s", strID.c_str(), info.type.c_str(), CURL::GetRedacted(mediaPath).c_str());

  return strID;
}

std::string CMediaManager::GetDiscPath()
{
  CSingleLock lock(m_CritSecStorageProvider);
  VECSOURCES drives;
  m_platformStorage->GetRemovableDrives(drives);
  for(unsigned i = 0; i < drives.size(); ++i)
  {
    if(drives[i].m_iDriveType == CMediaSource::SOURCE_TYPE_DVD && !drives[i].strPath.empty())
      return drives[i].strPath;
  }

  // iso9660://, cdda://local/ or D:\ depending on disc type
  auto media = CServiceBroker::GetDetectDVDMedia();
  if (!media)
    return "";

  return media->GetDVDPath();
}

void CMediaManager::SetHasOpticalDrive(bool bstatus)
{
  CSingleLock waitLock(m_muAutoSource);
  m_bhasoptical = bstatus;
}

bool CMediaManager::Eject(const std::string& mountpath)
{
  CSingleLock lock(m_CritSecStorageProvider);
  return m_platformStorage->Eject(mountpath);
}

void CMediaManager::EjectTray( const bool bEject, const char cDriveLetter )
{
  CCdIoSupport::EjectTray();
}

void CMediaManager::CloseTray(const char cDriveLetter)
{
  CCdIoSupport::CloseTray();
}

void CMediaManager::ToggleTray(const char cDriveLetter)
{
#ifdef HAS_DVD_DRIVE
#if defined(TARGET_WINDOWS)
  CWIN32Util::ToggleTray(cDriveLetter);
#else
  if (GetDriveStatus() == TRAY_OPEN || GetDriveStatus() == DRIVE_OPEN)
    CloseTray();
  else
    EjectTray();
#endif
#endif
}

void CMediaManager::ProcessEvents()
{
  CSingleLock lock(m_CritSecStorageProvider);
  if (m_platformStorage->PumpDriveChangeEvents(this))
  {
    CGUIMessage msg(GUI_MSG_NOTIFY_ALL,0,0,GUI_MSG_UPDATE_SOURCES);
    CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
  }
}

std::vector<std::string> CMediaManager::GetDiskUsage()
{
  CSingleLock lock(m_CritSecStorageProvider);
  return m_platformStorage->GetDiskUsage();
}

void CMediaManager::OnStorageAdded(const std::string &label, const std::string &path)
{
#ifdef HAS_DVD_DRIVE
  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  if (settings->GetInt(CSettings::SETTING_AUDIOCDS_AUTOACTION) != AUTOCD_NONE || settings->GetBool(CSettings::SETTING_DVDS_AUTORUN))
    if (settings->GetInt(CSettings::SETTING_AUDIOCDS_AUTOACTION) == AUTOCD_RIP)
      CJobManager::GetInstance().AddJob(new CAutorunMediaJob(label, path), this, CJob::PRIORITY_LOW);
    else
      CJobManager::GetInstance().AddJob(new CAutorunMediaJob(label, path), this, CJob::PRIORITY_HIGH);
  else
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(13021), label, TOAST_DISPLAY_TIME, false);
#endif
}

void CMediaManager::OnStorageSafelyRemoved(const std::string &label)
{
  CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(13023), label, TOAST_DISPLAY_TIME, false);
}

void CMediaManager::OnStorageUnsafelyRemoved(const std::string &label)
{
  CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(13022), label);
}

CMediaManager::DiscInfo CMediaManager::GetDiscInfo(const std::string& mediaPath)
{
  DiscInfo info;

  if (mediaPath.empty())
    return info;

  // Try finding VIDEO_TS/VIDEO_TS.IFO - this indicates a DVD disc is inserted
  std::string pathVideoTS = URIUtils::AddFileToFolder(mediaPath, "VIDEO_TS");
  if (CFile::Exists(URIUtils::AddFileToFolder(pathVideoTS, "VIDEO_TS.IFO")))
  {
    info.type = "DVD";
    // correct the filename if needed
    if (StringUtils::StartsWith(pathVideoTS, "dvd://") ||
      StringUtils::StartsWith(pathVideoTS, "iso9660://"))
      pathVideoTS = CServiceBroker::GetMediaManager().TranslateDevicePath("");


    CFileItem item(pathVideoTS, false);
    CDVDInputStreamNavigator dvdNavigator(nullptr, item);

    if (!dvdNavigator.Open())
      return info;

    info.name = dvdNavigator.GetDVDTitleString();
    info.serial = dvdNavigator.GetDVDSerialString();
  }
#ifdef HAVE_LIBBLURAY
  // check for Blu-ray discs
  else if (XFILE::CFile::Exists(URIUtils::AddFileToFolder(mediaPath, "BDMV", "index.bdmv")))
  {
    info.type = "Blu-ray";
    CBlurayDirectory bdDir;

    if (!bdDir.InitializeBluray(mediaPath))
      return info;

    info.name = bdDir.GetBlurayTitle();
    info.serial = bdDir.GetBlurayID();
  }
#endif

  return info;
}

void CMediaManager::RemoveDiscInfo(const std::string& devicePath)
{
  std::string strDevice = TranslateDevicePath(devicePath, false);

  auto it = m_mapDiscInfo.find(strDevice);
  if (it != m_mapDiscInfo.end())
    m_mapDiscInfo.erase(it);
}
