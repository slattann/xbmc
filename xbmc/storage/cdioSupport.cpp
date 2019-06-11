/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "cdioSupport.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "platform/Environment.h"
#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <cdio/mmc.h>
#include <cdio/cd_types.h>

#include <cdio++/cdio.hpp>

using namespace MEDIA_DETECT;

namespace
{

static void CdioLogHandler(cdio_log_level_t level, const char message[])
{
  switch (level)
  {
  case CDIO_LOG_ERROR:
    CLog::Log(LOGDEBUG,"**ERROR: {}", message);
    break;
  case CDIO_LOG_DEBUG:
    CLog::Log(LOGDEBUG,"--DEBUG: {}", message);
    break;
  case CDIO_LOG_WARN:
    CLog::Log(LOGDEBUG,"++ WARN: {}", message);
    break;
  case CDIO_LOG_INFO:
    CLog::Log(LOGDEBUG,"   INFO: {}", message);
    break;
  case CDIO_LOG_ASSERT:
    CLog::Log(LOGDEBUG,"!ASSERT: {}", message);
    break;
  default:
    break;
  }
}

}

CCdIoSupport::CCdIoSupport()
{
  cdio_log_set_handler(CdioLogHandler);
}

int CCdIoSupport::GetTrayState()
{
  if (!m_cdio)
  {
    m_cdio = std::make_shared<CdioDevice>();
    if (!m_cdio->open(nullptr, DRIVER_UNKNOWN))
      return DRIVE_NOT_READY;
  }

  int status = mmc_get_tray_status(m_cdio->getCdIo());
  auto discmode = m_cdio->getDiscmode();;

  int state;

  switch(status)
  {
  case 0: //closed
    if (discmode == CDIO_DISC_MODE_NO_INFO || discmode == CDIO_DISC_MODE_ERROR)
      state = TRAY_CLOSED_NO_MEDIA;
    else
      state = TRAY_CLOSED_MEDIA_PRESENT;
    break;

  case 1: //open
    state = TRAY_OPEN;
    break;
  }

  if (m_state == state)
    return m_state;

  m_state = state;

  if (state == TRAY_CLOSED_MEDIA_PRESENT)
  {
    m_info.reset();
    return DRIVE_CLOSED_MEDIA_PRESENT;
  }
  else if (state == TRAY_CLOSED_NO_MEDIA)
  {
    return DRIVE_CLOSED_NO_MEDIA;
  }
  else if (state == TRAY_OPEN)
  {
    return DRIVE_OPEN;
  }

  return DRIVE_NOT_READY;
}

bool CCdIoSupport::EjectTray()
{
  if (cdio_eject_media_drive(nullptr) != DRIVER_OP_SUCCESS)
    return false;

  return true;
}

bool CCdIoSupport::CloseTray()
{
  if (cdio_close_tray(nullptr, nullptr) != DRIVER_OP_SUCCESS)
    return false;

  return true;
}

void CCdIoSupport::PrintAnalysis(int fs, int num_audio)
{
  switch (fs & CDIO_FS_MASK)
  {
  case CDIO_FS_UDF:
    CLog::Log(LOGINFO, "CD-ROM with UDF filesystem");
    break;
  case CDIO_FS_AUDIO:
    CLog::Log(LOGINFO, "CD-ROM with audio tracks");
    break;
  case CDIO_FS_ISO_9660:
    CLog::Log(LOGINFO, "CD-ROM with ISO 9660 filesystem");
    if (fs & CDIO_FS_ANAL_JOLIET)
    {
      CLog::Log(LOGINFO, " with joliet extension level %d", m_jolietLevel);
    }
    if (fs & CDIO_FS_ANAL_ROCKRIDGE)
    {
      CLog::Log(LOGINFO, " and rockridge extensions");
    }
    break;
  case CDIO_FS_ISO_9660_INTERACTIVE:
    CLog::Log(LOGINFO, "CD-ROM with CD-RTOS and ISO 9660 filesystem");
    break;
  case CDIO_FS_HIGH_SIERRA:
    CLog::Log(LOGINFO, "CD-ROM with High Sierra filesystem");
    break;
  case CDIO_FS_INTERACTIVE:
    CLog::Log(LOGINFO, "CD-Interactive%s", num_audio > 0 ? "/Ready" : "");
    break;
  case CDIO_FS_HFS:
    CLog::Log(LOGINFO, "CD-ROM with Macintosh HFS");
    break;
  case CDIO_FS_ISO_HFS:
    CLog::Log(LOGINFO, "CD-ROM with both Macintosh HFS and ISO 9660 filesystem");
    break;
  case CDIO_FS_ISO_UDF:
    CLog::Log(LOGINFO, "CD-ROM with both UDF and ISO 9660 filesystem");
    break;
  case CDIO_FS_UFS:
    CLog::Log(LOGINFO, "CD-ROM with Unix UFS");
    break;
  case CDIO_FS_EXT2:
    CLog::Log(LOGINFO, "CD-ROM with Linux second extended filesystem");
    break;
  case CDIO_FS_3DO:
    CLog::Log(LOGINFO, "CD-ROM with Panasonic 3DO filesystem");
    break;
  case CDIO_FS_UNKNOWN:
    CLog::Log(LOGINFO, "CD-ROM with unknown filesystem");
    break;
  }

  switch (fs & CDIO_FS_MASK)
  {
  case CDIO_FS_ISO_9660:
  case CDIO_FS_ISO_9660_INTERACTIVE:
  case CDIO_FS_ISO_HFS:
  case CDIO_FS_ISO_UDF:
    CLog::Log(LOGINFO, "ISO 9660: {} blocks, label {}", m_isofsSize, m_discLabel);
    break;
  }

  switch (fs & CDIO_FS_MASK)
  {
  case CDIO_FS_UDF:
  case CDIO_FS_ISO_UDF:
    CLog::Log(LOGINFO, "UDF: version {}.{}", m_udfVerMajor, m_udfVerMinor);
    break;
  }

  if (m_firstData == 1 && num_audio > 0)
  {
    CLog::Log(LOGINFO, "mixed mode CD");
  }
  if (fs & CDIO_FS_ANAL_XA)
  {
    CLog::Log(LOGINFO, "XA sectors");
  }
  if (fs & CDIO_FS_ANAL_MULTISESSION)
  {
    CLog::Log(LOGINFO, "Multisession, offset = %i", m_msOffset);
  }
  if (fs & CDIO_FS_ANAL_HIDDEN_TRACK)
  {
    CLog::Log(LOGINFO, "Hidden Track");
  }
  if (fs & CDIO_FS_ANAL_PHOTO_CD)
  {
    CLog::Log(LOGINFO, "%sPhoto CD", num_audio > 0 ? "Portfolio " : "");
  }
  if (fs & CDIO_FS_ANAL_CDTV)
  {
    CLog::Log(LOGINFO, "Commodore CDTV");
  }
  if (m_firstData > 1)
  {
    CLog::Log(LOGINFO, "CD-Plus/Extra");
  }
  if (fs & CDIO_FS_ANAL_BOOTABLE)
  {
    CLog::Log(LOGINFO, "bootable CD");
  }
  if (fs & CDIO_FS_ANAL_VCD_ANY && num_audio == 0)
  {
    CLog::Log(LOGINFO, "Video CD");
  }
  if (fs & CDIO_FS_ANAL_CVD)
  {
    CLog::Log(LOGINFO, "Chaoji Video CD");
  }
}

int CCdIoSupport::GuessFilesystem(int start_session, track_t track_num)
{
  cdio_iso_analysis_t anal;
  auto fs = cdio_guess_cd_type(m_cdio->getCdIo(), start_session, track_num, &anal);

  switch(CDIO_FSTYPE(fs))
  {
  case CDIO_FS_UDF:
  case CDIO_FS_ISO_UDF:
    m_udfVerMinor = anal.UDFVerMinor;
    m_udfVerMajor = anal.UDFVerMajor;
    break;
  default:
    break;
  }

  m_discLabel = anal.iso_label;
  m_isofsSize = anal.isofs_size;
  m_jolietLevel = anal.joliet_level;

  return CDIO_FSTYPE(fs);
}

void CCdIoSupport::GetCdTextInfo(xbmc_cdtext_t &xcdt, int trackNum)
{
  CdioCDText* text = m_cdio->getCdtext();

  if (!text)
    return;

  for (int i = 0; i < MAX_CDTEXT_FIELDS; i++)
  {
    auto field = text->getConst(static_cast<cdtext_field_t>(i), trackNum);
    if (field)
      xcdt[static_cast<cdtext_field_t>(i)] = field;
  }
}

// Returns the sum of the decimal digits in a number. Eg. 1955 = 20
int CCdIoSupport::CddbDecDigitSum(int n)
{
  int ret = 0;

  for (;;)
  {
    ret += n % 10;
    n = n / 10;
    if (!n)
      return ret;
  }
}

// Return the number of seconds (discarding frame portion) of an MSF
unsigned int CCdIoSupport::MsfSeconds(msf_t *msf)
{
  return cdio_from_bcd8(msf->m) * 60 + cdio_from_bcd8(msf->s);
}

// Compute the CDDB disk ID for an Audio disk.
// This is a funny checksum consisting of the concatenation of 3 things:
//    The sum of the decimal digits of sizes of all tracks,
//    The total length of the disk, and
//    The number of tracks.

uint32_t CCdIoSupport::CddbDiscId()
{
  int n = 0;
  msf_t msf;
  for (int i = 1; i <= m_numTracks; i++)
  {
    CdioTrack track(m_cdio->getCdIo(), i);
    track.getMsf(msf);
    n += CddbDecDigitSum(MsfSeconds(&msf));
  }

  msf_t start_msf;
  CdioTrack startTrack(m_cdio->getCdIo(), 1);
  startTrack.getMsf(start_msf);

  CdioTrack track(m_cdio->getCdIo(), CDIO_CDROM_LEADOUT_TRACK);
  track.getMsf(msf);

  int t = MsfSeconds(&msf) - MsfSeconds(&start_msf);

  return ((n % 0xff) << 24 | t << 8 | m_numTracks);
}

std::shared_ptr<CCdInfo> CCdIoSupport::GetCdInfo()
{
  CSingleLock lock(m_lock);

  if (m_info)
    return m_info;

  m_cdio.reset();
  m_cdio = std::make_shared<CdioDevice>();

  if (!m_cdio->open(nullptr, DRIVER_UNKNOWN))
    return nullptr;

  m_firstTrackNum = m_cdio->getFirstTrackNum();
  if (m_firstTrackNum == CDIO_INVALID_TRACK)
    return nullptr;

  m_numTracks = m_cdio->getNumTracks();
  if (m_numTracks == CDIO_INVALID_TRACK)
    return nullptr;

  m_info.reset();
  m_info = std::make_shared<CCdInfo>();
  m_info->SetFirstTrack(m_firstTrackNum);
  m_info->SetTrackCount(m_numTracks);

  CdioTrack track(m_cdio->getCdIo(), m_firstTrackNum);
  m_fs = GuessFilesystem(track.getLsn(), m_firstTrackNum);

  auto mode = m_cdio->getDiscmode();
  if (m_cdio->isDiscmodeDvd(mode))
  {
    return m_info;
  }

  xbmc_cdtext_t xcdt;
  GetCdTextInfo(xcdt, 0);
  m_info->SetDiscCDTextInformation(xcdt);

  for (int i = m_firstTrackNum; i <= CDIO_CDROM_LEADOUT_TRACK; i++)
  {
    CdioTrack track(m_cdio->getCdIo(), i);
    TrackInfo ti;

    msf_t msf;
    track.getMsf(msf);

    if (TRACK_FORMAT_AUDIO == track.getFormat())
    {
      m_numAudio++;
      ti.fsInfo = CDIO_FS_AUDIO;
      ti.mins = msf.m;
      ti.secs = msf.s;
      ti.frames = track.getLsn();
      if (m_firstAudio == -1)
        m_firstAudio = i;

      // Get this tracks info
      GetCdTextInfo(ti.cdtext, i);
    }
    else
    {
      m_numData++;
      if (m_firstData == -1)
        m_firstData = i;
    }

    m_info->SetTrackInformation(i, ti);
    /* skip to leadout? */
    if (i == m_numTracks)
      i = CDIO_CDROM_LEADOUT_TRACK;
  }

  m_info->SetDiscLength(cdio_get_track_lba(m_cdio->getCdIo(), CDIO_CDROM_LEADOUT_TRACK) / CDIO_CD_FRAMES_PER_SEC);

  m_info->SetAudioTrackCount(m_numAudio);
  m_info->SetDataTrackCount(m_numData);
  m_info->SetFirstAudioTrack(m_firstAudio);
  m_info->SetFirstDataTrack(m_firstData);

  CLog::Log(LOGINFO, "CD Analysis Report");

  /* Try to find out what sort of CD we have */
  if (0 == m_numData)
  {
    m_info->SetCddbDiscId(CddbDiscId());
    PrintAnalysis(m_fs, m_numAudio);
  }
  else
  {
    /* We have data track(s) */
    for (int i = m_firstData; i <= m_numTracks; i++)
    {
      CdioTrack track(m_cdio->getCdIo(), i);

      auto format = track.getFormat();
      switch (format)
      {
      case TRACK_FORMAT_AUDIO:
      {
        TrackInfo ti;
        ti.fsInfo = CDIO_FS_AUDIO;
        m_info->SetTrackInformation(i, ti);
      }
      default:
        break;
      }

      m_startTrack = (i == 1) ? 0 : track.getLsn();

      /* Save the start of the data area */
      if (i == m_firstData)
        m_dataStart = m_startTrack;

      PrintAnalysis(m_fs, m_numAudio);
    }
  }

  return m_info;
}
