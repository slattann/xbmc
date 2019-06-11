/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "CDDAFile.h"
#include <sys/stat.h>
#include "URL.h"
#include "ServiceBroker.h"
#include "storage/MediaManager.h"
#include "utils/log.h"
#include "utils/URIUtils.h"

#include <algorithm>

using namespace MEDIA_DETECT;
using namespace XFILE;

CFileCDDA::CFileCDDA(void)
{
  m_pCdIo = NULL;
  m_lsnStart = CDIO_INVALID_LSN;
  m_lsnCurrent = CDIO_INVALID_LSN;
  m_lsnEnd = CDIO_INVALID_LSN;
  m_iSectorCount = 52;
}

CFileCDDA::~CFileCDDA(void)
{
  Close();
}

bool CFileCDDA::Open(const CURL& url)
{
  std::string strURL = url.GetWithoutFilename();

  if (!CServiceBroker::GetMediaManager().IsDiscInDrive() || !IsValidFile(url))
    return false;

  // Open the dvd drive
  m_pCdIo = cdio_open(nullptr, DRIVER_UNKNOWN);

  if (!m_pCdIo)
  {
    CLog::Log(LOGERROR, "file cdda: Opening the dvd drive failed");
    return false;
  }

  m_drive = cdio_cddap_identify_cdio(m_pCdIo, 1, nullptr);

  if (cdda_open(m_drive) != 0)
  {
    Close();
    return false;
  }

  m_paranoia = paranoia_init(m_drive);
  paranoia_modeset(m_paranoia, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);

  int iTrack = GetTrackNum(url);

  m_lsnStart = cdio_cddap_track_firstsector(m_drive, iTrack);
  m_lsnEnd = cdio_cddap_track_lastsector(m_drive, iTrack);
  m_lsnCurrent = m_lsnStart;

  if (m_lsnStart == CDIO_INVALID_LSN || m_lsnEnd == CDIO_INVALID_LSN)
  {
    Close();
    return false;
  }

  return true;
}

bool CFileCDDA::Exists(const CURL& url)
{
  if (!IsValidFile(url))
    return false;

  int iTrack = GetTrackNum(url);

  if (!Open(url))
    return false;

  int iLastTrack = cdio_get_last_track_num(m_pCdIo);
  if (iLastTrack == CDIO_INVALID_TRACK)
    return false;

  return (iTrack > 0 && iTrack <= iLastTrack);
}

int CFileCDDA::Stat(const CURL& url, struct __stat64* buffer)
{
  if (Open(url))
  {
    memset(buffer, 0, sizeof(struct __stat64));
    buffer->st_size = GetLength();
    buffer->st_mode = _S_IFREG;
    Close();
    return 0;
  }
  errno = ENOENT;
  return -1;
}

ssize_t CFileCDDA::Read(void* lpBuf, size_t uiBufSize)
{
  if (!m_pCdIo || !CServiceBroker::GetMediaManager().IsDiscInDrive())
    return -1;

  if (uiBufSize > SSIZE_MAX)
    uiBufSize = SSIZE_MAX;

  // limit number of sectors that fits in buffer by m_iSectorCount
  int iSectorCount = std::min((int)uiBufSize / CDIO_CD_FRAMESIZE_RAW, m_iSectorCount);

  if (iSectorCount <= 0)
    return -1;

  // Are there enough sectors left to read
  if (m_lsnCurrent + iSectorCount > m_lsnEnd)
    iSectorCount = m_lsnEnd - m_lsnCurrent;

  int block;
  for (block = 1; block <= iSectorCount; block++)
  {
    lpBuf = paranoia_read(m_paranoia, nullptr);
  }

  m_lsnCurrent += block;

  return iSectorCount * CDIO_CD_FRAMESIZE_RAW;
}

int64_t CFileCDDA::Seek(int64_t iFilePosition, int iWhence /*=SEEK_SET*/)
{
  if (!m_pCdIo)
    return -1;

  lsn_t lsnPosition = (int)iFilePosition / CDIO_CD_FRAMESIZE_RAW;

  switch (iWhence)
  {
  case SEEK_SET:
    // cur = pos
    m_lsnCurrent = m_lsnStart + lsnPosition;
    break;
  case SEEK_CUR:
    // cur += pos
    m_lsnCurrent += lsnPosition;
    break;
  case SEEK_END:
    // end += pos
    m_lsnCurrent = m_lsnEnd + lsnPosition;
    break;
  default:
    return -1;
  }

  paranoia_seek(m_paranoia, lsnPosition, iWhence);

  return ((int64_t)(m_lsnCurrent - m_lsnStart) * CDIO_CD_FRAMESIZE_RAW);
}

void CFileCDDA::Close()
{
  if (m_paranoia)
  {
    paranoia_free(m_paranoia);
    m_paranoia = nullptr;
  }

  if (m_drive)
  {
    cdda_close(m_drive);
    m_drive = nullptr;
  }

  if (m_pCdIo)
  {
    cdio_destroy(m_pCdIo);
    m_pCdIo = nullptr;
  }
}

int64_t CFileCDDA::GetPosition()
{
  if (!m_pCdIo)
    return 0;

  return ((int64_t)(m_lsnCurrent -m_lsnStart)*CDIO_CD_FRAMESIZE_RAW);
}

int64_t CFileCDDA::GetLength()
{
  if (!m_pCdIo)
    return 0;

  return ((int64_t)(m_lsnEnd - m_lsnStart) * CDIO_CD_FRAMESIZE_RAW);
}

bool CFileCDDA::IsValidFile(const CURL& url)
{
  // Only .cdda files are supported
  return URIUtils::HasExtension(url.Get(), ".cdda");
}

int CFileCDDA::GetTrackNum(const CURL& url)
{
  std::string strFileName = url.Get();

  // get track number from "cdda://local/01.cdda"
  return atoi(strFileName.substr(13, strFileName.size() - 13 - 5).c_str());
}

#define SECTOR_COUNT 52 // max. sectors that can be read at once
int CFileCDDA::GetChunkSize()
{
  return SECTOR_COUNT * CDIO_CD_FRAMESIZE_RAW;
}
