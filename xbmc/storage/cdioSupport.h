/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

//  CCdInfo   -  Information about media type of an inserted cd
//  CCdIoSupport -  Wrapper class for libcdio with the interface of CIoSupport
//     and detecting the filesystem on the Disc.
//
// by Bobbin007 in 2003
//  CD-Text support by Mog - Oct 2004

#include "PlatformDefs.h" // for ssize_t typedef

#include <cdio/cdio.h>
#include "threads/CriticalSection.h"
#include <memory>
#include <map>
#include <string>

#include <cdio/cd_types.h>

class CdioDevice;

namespace MEDIA_DETECT
{

// todo: remove
#define TRAY_OPEN     16
#define TRAY_CLOSED_NO_MEDIA  64
#define TRAY_CLOSED_MEDIA_PRESENT 96

#define DRIVE_OPEN      0 // Open...
#define DRIVE_NOT_READY     1 // Opening.. Closing...
#define DRIVE_READY      2
#define DRIVE_CLOSED_NO_MEDIA   3 // CLOSED...but no media in drive
#define DRIVE_CLOSED_MEDIA_PRESENT  4 // Will be send once when the drive just have closed
#define DRIVE_NONE  5 // system doesn't have an optical drive

typedef std::map<cdtext_field_t, std::string> xbmc_cdtext_t;

struct TrackInfo
{
  int fsInfo;          // Information of the Tracks Filesystem
  int jolietLevel;     // Jouliet Level
  int ms_offset;        // Multisession Offset
  int isofs_size;       // Size of the ISO9660 Filesystem
  int frames;          // Can be used for cddb query
  int mins;            // minutes playtime part of Track
  int secs;            // seconds playtime part of Track
  xbmc_cdtext_t cdtext; // CD-Text for this track
};

class CCdInfo
{
public:
  CCdInfo() = default;

  TrackInfo GetTrackInformation( int track ) { return m_ti[track -1]; }
  xbmc_cdtext_t GetDiscCDTextInformation() { return m_cdtext; }

  bool HasDataTracks() { return (m_numData > 0); }
  bool HasAudioTracks() { return (m_numAudio > 0); }
  int GetFirstTrack() { return m_firstTrack; }
  int GetTrackCount() { return m_numTrack; }
  int GetFirstAudioTrack() { return m_firstAudio; }
  int GetFirstDataTrack() { return m_firstData; }
  int GetDataTrackCount() { return m_numData; }
  int GetAudioTrackCount() { return m_numAudio; }
  uint32_t GetCddbDiscId() { return m_cddbDiscId; }
  int GetDiscLength() { return m_length; }
  std::string GetDiscLabel(){ return m_strDiscLabel; }

  // CD-ROM with ISO 9660 filesystem
  bool IsIso9660( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_ISO_9660); }
  // CD-ROM with joliet extension
  bool IsJoliet( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_JOLIET) ? false : true; }
  // Joliet extension level
  int GetJolietLevel( int track ) { return m_ti[track - 1].jolietLevel; }
  // ISO filesystem size
  int GetIsoSize( int track ) { return m_ti[track - 1].isofs_size; }
  // CD-ROM with rockridge extensions
  bool IsRockridge( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_ROCKRIDGE) ? false : true; }

  // CD-ROM with CD-RTOS and ISO 9660 filesystem
  bool IsIso9660Interactive( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_ISO_9660_INTERACTIVE); }

  // CD-ROM with High Sierra filesystem
  bool IsHighSierra( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_HIGH_SIERRA); }

  // CD-Interactive, with audiotracks > 0 CD-Interactive/Ready
  bool IsCDInteractive( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_INTERACTIVE); }

  // CD-ROM with Macintosh HFS
  bool IsHFS( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_HFS); }

  // CD-ROM with both Macintosh HFS and ISO 9660 filesystem
  bool IsISOHFS( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_ISO_HFS); }

  // CD-ROM with both UDF and ISO 9660 filesystem
  bool IsISOUDF( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_ISO_UDF); }

  // CD-ROM with Unix UFS
  bool IsUFS( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_UFS); }

  // CD-ROM with Linux second extended filesystem
  bool IsEXT2( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_EXT2); }

  // CD-ROM with Panasonic 3DO filesystem
  bool Is3DO( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_3DO); }

  // Mixed Mode CD-ROM
  bool IsMixedMode( int track ) { return (m_firstData == 1 && m_numAudio > 0); }

  // CD-ROM with XA sectors
  bool IsXA( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_XA) ? false : true; }

  // Multisession CD-ROM
  bool IsMultiSession( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_MULTISESSION) ? false : true; }
  // Gets multisession offset
  int GetMultisessionOffset( int track ) { return m_ti[track - 1].ms_offset; }

  // Hidden Track on Audio CD
  bool IsHiddetrack( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_HIDDEN_TRACK) ? false : true; }

  // Photo CD, with audiotracks > 0 Portfolio Photo CD
  bool IsPhotoCd( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_PHOTO_CD) ? false : true; }

  // CD-ROM with Commodore CDTV
  bool IsCdTv( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_CDTV) ? false : true; }

  // CD-Plus/Extra
  bool IsCDExtra( int track ) { return (m_firstData > 1); }

  // Bootable CD
  bool IsBootable( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_BOOTABLE) ? false : true; }

  // Video CD
  bool IsVideoCd( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_VCD_ANY && m_numAudio == 0); }

  // Chaoji Video CD
  bool IsChaojiVideoCD( int track ) { return (m_ti[track - 1].fsInfo & CDIO_FS_ANAL_CVD) ? false : true; }

  // Audio Track
  bool IsAudio( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_AUDIO); }

  // UDF filesystem
  bool IsUDF( int track ) { return ((m_ti[track - 1].fsInfo & CDIO_FS_MASK) == CDIO_FS_UDF); }

  // Has the cd a filesystem that is readable by the xbox
  bool IsValidFs() { return (IsISOHFS(1) || IsIso9660(1) || IsIso9660Interactive(1) || IsISOUDF(1) || IsUDF(1) || IsAudio(1)); }

  void SetFirstTrack( int track ) { m_firstTrack = track; }
  void SetTrackCount( int count ) { m_numTrack = count; }
  void SetFirstAudioTrack( int track ) { m_firstAudio = track; }
  void SetFirstDataTrack( int track ) { m_firstData = track; }
  void SetDataTrackCount( int count ) { m_numData = count; }
  void SetAudioTrackCount( int count ) { m_numAudio = count; }
  void SetTrackInformation( int track, TrackInfo info ) { if ( track > 0 && track <= 99 ) m_ti[track - 1] = info; }
  void SetDiscCDTextInformation( xbmc_cdtext_t cdtext ) { m_cdtext = cdtext; }

  void SetCddbDiscId( uint32_t cddbDiscId ) { m_cddbDiscId = cddbDiscId; }
  void SetDiscLength( int length ) { m_length = length; }
  bool HasCDDBInfo() { return m_hasCDDBInfo; }
  void SetNoCDDBInfo() { m_hasCDDBInfo = false; }

  void SetDiscLabel(const std::string& strDiscLabel){ m_strDiscLabel = strDiscLabel; }

private:
  bool m_hasCDDBInfo{true};
  int m_length{0};
  int m_firstTrack{0};
  int m_numTrack{0};
  int m_numAudio{0};
  int m_firstAudio{0};
  int m_numData{0};
  int m_firstData{0};

  TrackInfo m_ti[100];
  uint32_t m_cddbDiscId;
  std::string m_strDiscLabel;
  xbmc_cdtext_t m_cdtext;
};

class CCdIoSupport
{
public:
  CCdIoSupport();
  ~CCdIoSupport() = default;

  int GetTrayState();

  static bool EjectTray();
  static bool CloseTray();

  std::shared_ptr<CCdInfo> GetCdInfo();

private:
  void PrintAnalysis(int fs, int num_audio);
  void GetCdTextInfo(xbmc_cdtext_t &xcdt, int trackNum);

  int GuessFilesystem(int start_session, track_t track_num);

  uint32_t CddbDiscId();
  int CddbDecDigitSum(int n);
  unsigned int MsfSeconds(msf_t *msf);

  int m_startTrack{0};
  int m_isofsSize{0};
  int m_jolietLevel{0};
  int m_msOffset{0};
  int m_dataStart{0};
  int m_fs{0};
  int m_udfVerMinor{0};
  int m_udfVerMajor{0};

  track_t m_numTracks = CDIO_INVALID_TRACK;
  track_t m_firstTrackNum = CDIO_INVALID_TRACK;

  std::string m_discLabel;

  int m_firstData{-1};
  int m_numData{0};
  int m_firstAudio{-1};
  int m_numAudio{0};

  int m_state{-1};

  std::shared_ptr<CdioDevice> m_cdio;
  std::shared_ptr<CCdInfo> m_info;

  mutable CCriticalSection m_lock;
};

}
