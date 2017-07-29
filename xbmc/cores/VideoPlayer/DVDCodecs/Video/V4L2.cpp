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

#include "V4L2.h"

#include "cores/VideoPlayer/DVDCodecs/DVDFactoryCodec.h"
#include "cores/VideoPlayer/Process/ProcessInfo.h"
#include "DVDVideoCodec.h"
#include "threads/SingleLock.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

using namespace V4L2;

//------------------------------------------------------------------------------
// main class
//------------------------------------------------------------------------------


CDecoder::CDecoder(CProcessInfo& processInfo)
  : m_processInfo(processInfo)
  , m_videoBuffer(nullptr)
{
}

CDecoder::~CDecoder()
{
}

bool CDecoder::Open(AVCodecContext* avctx, AVCodecContext* mainctx, const enum AVPixelFormat fmt)
{
  m_processInfo.SetVideoDeintMethod("none");

  std::list<EINTERLACEMETHOD> deintMethods;
  deintMethods.push_back(EINTERLACEMETHOD::VS_INTERLACEMETHOD_NONE);
  m_processInfo.UpdateDeinterlacingMethods(deintMethods);

  return true;
}

CDVDVideoCodec::VCReturn CDecoder::Decode(AVCodecContext* avctx, AVFrame* frame)
{
  CDVDVideoCodec::VCReturn status = Check(avctx);
  if (status)
  {
    return status;
  }

  if(frame)
  {
    if (m_videoBuffer)
    {
      m_videoBuffer->Release();
    }
    m_videoBuffer = m_processInfo.GetVideoBufferManager().Get((AVPixelFormat)frame->format, frame->width * frame->height);
    memcpy(m_videoBuffer->GetMemPtr(), frame->data, frame->pkt_size);
    return CDVDVideoCodec::VC_PICTURE;
  }
  else
  {
    return CDVDVideoCodec::VC_BUFFER;
  }
}

bool CDecoder::GetPicture(AVCodecContext* avctx, VideoPicture* pPicture)
{
  ((ICallbackHWAccel*)avctx->opaque)->GetPictureCommon(pPicture);

  if (pPicture->videoBuffer)
  {
    pPicture->videoBuffer->Release();
  }

  pPicture->videoBuffer = m_videoBuffer;
  pPicture->videoBuffer->Acquire();
  return true;
}

CDVDVideoCodec::VCReturn CDecoder::Check(AVCodecContext* avctx)
{
  return CDVDVideoCodec::VC_NONE;
}

IHardwareDecoder* CDecoder::Create(CDVDStreamInfo &hint, CProcessInfo &processInfo, AVPixelFormat fmt)
{
  return new V4L2::CDecoder(processInfo);
}

void CDecoder::Register()
{
  CDVDFactoryCodec::RegisterHWAccel("v4l2", CDecoder::Create);
}
