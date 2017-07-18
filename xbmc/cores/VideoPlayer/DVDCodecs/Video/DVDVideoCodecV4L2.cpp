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

#include "DVDVideoCodecV4L2.h"
#include "DVDCodecs/DVDFactoryCodec.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CDVDVideoCodecV4L2"

CDVDVideoCodecV4L2::CDVDVideoCodecV4L2(CProcessInfo &processInfo) : CDVDVideoCodec(processInfo)
{
  m_Codec = nullptr;
  m_bitstream = nullptr;
  b_ConvertVideo = false;
}

CDVDVideoCodecV4L2::~CDVDVideoCodecV4L2()
{
  Dispose();
}

CDVDVideoCodec* CDVDVideoCodecV4L2::Create(CProcessInfo &processInfo)
{
  return new CDVDVideoCodecV4L2(processInfo);
}

bool CDVDVideoCodecV4L2::Register()
{
  CDVDFactoryCodec::RegisterHWVideoCodec("v4l2_dec", &CDVDVideoCodecV4L2::Create);
  return true;
}

void CDVDVideoCodecV4L2::Dispose()
{
  m_processInfo.GetVideoBufferManager().ReleasePools();

  if (m_Codec)
  {
    m_Codec->CloseDecoder();
    delete m_Codec;
    m_Codec = nullptr;
  }

  if (m_bitstream)
  {
    delete m_bitstream;
    m_bitstream = nullptr;
  }

  b_ConvertVideo = false;
}

bool CDVDVideoCodecV4L2::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  m_hints = hints;
  m_options = options;

  Dispose();

  m_Codec = std::shared_ptr<CV4L2Codec>(new CV4L2Codec());

  if (!m_Codec->OpenDecoder())
  {
    CLog::Log(LOGDEBUG, "%s::%s - V4L2 device not found", CLASSNAME, __func__);
    return false;
  }

  if (!m_Codec->SetupOutputFormat(m_hints))
  {
    return false;
  }

  if (!m_Codec->SetupOutputBuffers())
  {
    return false;
  }

  if (!m_Codec->SetupCaptureBuffers())
  {
    return false;
  }

  /*
  m_videoBuffer.iFlags = 0;

  m_videoBuffer.color_range = 0;
  m_videoBuffer.color_matrix = 4;

  m_videoBuffer.iWidth  = m_hints.width;
  m_videoBuffer.iHeight = m_hints.height;
  m_videoBuffer.iDisplayWidth  = m_videoBuffer.iWidth;
  m_videoBuffer.iDisplayHeight = m_videoBuffer.iHeight;
  if (m_hints.aspect > 0.0 && !m_hints.forced_aspect)
  {
    m_videoBuffer.iDisplayWidth  = ((int)lrint(m_videoBuffer.iHeight * m_hints.aspect)) & ~3;
    if (m_videoBuffer.iDisplayWidth > m_videoBuffer.iWidth)
    {
      m_videoBuffer.iDisplayWidth  = m_videoBuffer.iWidth;
      m_videoBuffer.iDisplayHeight = ((int)lrint(m_videoBuffer.iWidth / m_hints.aspect)) & ~3;
    }
  }

  m_videoBuffer.pts = DVD_NOPTS_VALUE;
  m_videoBuffer.dts = DVD_NOPTS_VALUE;
  */

  m_processInfo. SetVideoDecoderName(m_Codec->GetOutputName(), true);
  //m_processInfo.SetVideoPixelFormat("nv12");
  m_processInfo.SetVideoDimensions(m_hints.width, m_hints.height);
  m_processInfo.SetVideoDeintMethod("hardware");
  m_processInfo.SetVideoDAR(m_hints.aspect);

  m_bitstream = new CBitstreamConverter();
  b_ConvertVideo = m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);

  return true;
}

bool CDVDVideoCodecV4L2::AddData(const DemuxPacket &packet)
{
  uint8_t *pData(packet.pData);
  int iSize(packet.iSize);

  if (pData)
  {
    if (b_ConvertVideo)
    {
      if(m_bitstream->GetExtraData() != NULL && m_bitstream->GetExtraSize() > 0)
      {
        m_hints.extrasize = m_bitstream->GetExtraSize();
        m_hints.extradata = m_bitstream->GetExtraData();
      }

      if (!m_bitstream->Convert(pData, iSize))
      {
        return true;
      }

      if (!m_bitstream->CanStartDecode())
      {
        CLog::Log(LOGDEBUG, "%s::%s - Decode waiting for keyframe (bitstream)", CLASSNAME, __func__);
        return true;
      }

      pData = m_bitstream->GetConvertBuffer();
      iSize = m_bitstream->GetConvertSize();
    }

    if (packet.pts == DVD_NOPTS_VALUE)
    {
      m_hints.ptsinvalid = true;
    }
  }

  CLog::Log(LOGDEBUG, "%s::%s - adding data", CLASSNAME, __func__);
  return m_Codec->AddData(pData, iSize, packet.dts, m_hints.ptsinvalid ? DVD_NOPTS_VALUE : packet.pts);
}

void CDVDVideoCodecV4L2::Reset()
{
  CDVDCodecOptions options;
  Open(m_hints, options);
}

CDVDVideoCodec::VCReturn CDVDVideoCodecV4L2::GetPicture(VideoPicture* pVideoPicture)
{
  if (!m_Codec)
  {
    return VC_ERROR;
  }

  if (pVideoPicture->videoBuffer)
  {
    pVideoPicture->videoBuffer->Release();
    pVideoPicture->videoBuffer = nullptr;
  }

  pVideoPicture->videoBuffer = m_processInfo.GetVideoBufferManager().Get(AV_PIX_FMT_NV12, pVideoPicture->iWidth * pVideoPicture->iHeight);

  VCReturn retVal = m_Codec->GetPicture(pVideoPicture);

  if (retVal == VC_PICTURE)
  {
    int strides[YuvImage::MAX_PLANES];
    strides[0] = pVideoPicture->iWidth;
    strides[1] = pVideoPicture->iHeight;
    strides[2] = 0;

    pVideoPicture->videoBuffer->SetDimensions(pVideoPicture->iWidth, pVideoPicture->iHeight, strides);
  }

  return retVal;
}
