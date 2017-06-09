#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DVDCodecs/DVDCodecs.h"
#include "DVDClock.h"
#include "utils/log.h"
#include "guilib/GraphicContext.h"
#include "settings/DisplaySettings.h"
#include "V4L2Codec.h"

class CDVDVideoCodecV4L2 : public CDVDVideoCodec
{
public:
  CDVDVideoCodecV4L2(CProcessInfo &processInfo);
  virtual ~CDVDVideoCodecV4L2();

  virtual bool Open(CDVDStreamInfo &hints, CDVDCodecOptions &options) override;
  virtual bool AddData(const DemuxPacket &packet) override;
  virtual void Reset() override;
  virtual CDVDVideoCodec::VCReturn GetPicture(VideoPicture* pVideoPicture) override;
  virtual const char* GetName() override { return m_Codec != NULL ? m_Codec->GetOutputName() : ""; };

private:
  void Dispose();

  V4L2Codec *m_Codec;

  CDVDStreamInfo m_hints;
  VideoPicture m_videoBuffer;
  CBitstreamConverter *m_bitstream;
  bool b_ConvertVideo;
};
