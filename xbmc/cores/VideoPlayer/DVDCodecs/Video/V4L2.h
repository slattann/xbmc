/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "DVDVideoCodecFFmpeg.h"
#include "cores/VideoPlayer/Process/VideoBuffer.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

class CProcessInfo;

namespace V4L2
{
class CDecoder : public IHardwareDecoder
{
public:
  CDecoder(CProcessInfo& processInfo);
  ~CDecoder() override;
  bool Open(AVCodecContext* avctx, AVCodecContext* mainctx, const enum AVPixelFormat) override;
  CDVDVideoCodec::VCReturn Decode(AVCodecContext* avctx, AVFrame* frame) override;
  bool GetPicture(AVCodecContext* avctx, VideoPicture* picture) override;
  CDVDVideoCodec::VCReturn Check(AVCodecContext* avctx) override;
  unsigned GetAllowedReferences() override { return 4; }
  const std::string Name() override { return "v4l2"; }

  static IHardwareDecoder* Create(CDVDStreamInfo &hint, CProcessInfo &processInfo, AVPixelFormat fmt);
  static void Register();

protected:
  CProcessInfo& m_processInfo;
  CVideoBuffer *m_videoBuffer;
};

}
