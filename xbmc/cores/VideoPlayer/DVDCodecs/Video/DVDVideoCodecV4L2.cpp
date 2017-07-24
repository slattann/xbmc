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
#include "TimingConstants.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CDVDVideoCodecV4L2"

CDVDVideoCodecV4L2::CDVDVideoCodecV4L2(CProcessInfo &processInfo) : CDVDVideoCodec(processInfo)
  , m_decoder(nullptr)
  , m_bitstream(nullptr)
  , b_convertVideo(false)
{
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
  if (m_decoder)
  {
    m_decoder->CloseDecoder();
    delete m_decoder;
    m_decoder = nullptr;
  }

  if (m_bitstream)
  {
    delete m_bitstream;
    m_bitstream = nullptr;
  }

  b_convertVideo = false;
}

bool CDVDVideoCodecV4L2::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  m_hints = hints;
  m_options = options;

  Dispose();

  m_decoder = new CV4L2Codec();

  if (!m_decoder->OpenDecoder())
  {
    CLog::Log(LOGDEBUG, "%s::%s - V4L2 device not found", CLASSNAME, __func__);
    return false;
  }

  if (!m_decoder->SetupOutputFormat(m_hints))
  {
    return false;
  }

  if (!m_decoder->SetupOutputBuffers())
  {
    return false;
  }

  if (!m_decoder->SetupCaptureBuffers())
  {
    return false;
  }

  memset(&(m_videoBuffer), 0, sizeof(m_videoBuffer));

  m_videoBuffer.pts = DVD_NOPTS_VALUE;
  m_videoBuffer.dts = DVD_NOPTS_VALUE;

  m_processInfo. SetVideoDecoderName(m_decoder->GetOutputName(), true);
  //m_processInfo.SetVideoPixelFormat("nv12");
  m_processInfo.SetVideoDimensions(m_hints.width, m_hints.height);
  m_processInfo.SetVideoDeintMethod("hardware");
  m_processInfo.SetVideoDAR(m_hints.aspect);

  m_bitstream = new CBitstreamConverter();
  b_convertVideo = m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true);

  return true;
}

bool CDVDVideoCodecV4L2::AddData(const DemuxPacket &packet)
{
  uint8_t *pData(packet.pData);
  int iSize(packet.iSize);

  if (pData)
  {
    if (b_convertVideo)
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
  timeval timestamp;
  timestamp.tv_usec = m_hints.ptsinvalid ? DVD_NOPTS_VALUE : packet.pts;

  int index = 0;
  int ret;

  if (pData)
  {
    for (index = 0; index < m_decoder->GetOutputBuffersCount(); index++)
    {
      if (m_decoder->IsOutputBufferEmpty(index))
      {
        CLog::Log(LOGDEBUG, "%s::%s - using output buffer index: %d", CLASSNAME, __func__, index);
        break;
      }
    }

    if (index == m_decoder->GetOutputBuffersCount())
    {
      // all input buffers are busy, dequeue needed
      CLog::Log(LOGDEBUG, "%s::%s - dequeuing output buffer", CLASSNAME, __func__);
      if (!m_decoder->DequeueOutputBuffer(&ret, &timestamp))
      {
        return false;
      }
      index = ret;
    }

    CLog::Log(LOGDEBUG, "%s::%s - queuing output buffer index: %d", CLASSNAME, __func__, index);
    if (!m_decoder->QueueOutputBuffer(index, pData, iSize, m_hints.ptsinvalid ? DVD_NOPTS_VALUE : packet.pts))
    {
      return false;
    }
  }

  if (!m_decoder->QueueCaptureBuffer(index))
  {
    return false;
  }

  return true; // Picture is finally ready to be processed further
}

void CDVDVideoCodecV4L2::Reset()
{
  CDVDCodecOptions options;
  Open(m_hints, options);
}

CDVDVideoCodec::VCReturn CDVDVideoCodecV4L2::GetPicture(VideoPicture* pVideoPicture)
{
  if (!m_decoder)
  {
    return VC_ERROR;
  }

  if (pVideoPicture->videoBuffer)
  {
    pVideoPicture->videoBuffer->Release();
    pVideoPicture->videoBuffer = nullptr;
  }

  timeval timestamp;
  int index;

  if (!m_decoder->DequeueCaptureBuffer(&index, &timestamp))
  {
    return VC_ERROR;
  }

  cv4l_queue captureBuffers = m_decoder->GetCaptureBuffers();
  cv4l_buffer buffer(captureBuffers, index);

  CVideoBuffer *videoBuffer = m_processInfo.GetVideoBufferManager().Get(AV_PIX_FMT_NV12, buffer.g_bytesused(0));
  if (!videoBuffer)
  {
    CLog::Log(LOGDEBUG, "%s::%s - failed to allocate buffer", CLASSNAME, __func__);
    return VC_ERROR;
  }

  pVideoPicture->videoBuffer = static_cast<CVideoBuffer*>(videoBuffer);

  memcpy(pVideoPicture->videoBuffer->GetMemPtr(), captureBuffers.g_mmapping(index, 0), buffer.g_bytesused(0));

  int strides[YuvImage::MAX_PLANES];
  strides[0] = m_videoBuffer.iWidth;
  strides[1] = m_videoBuffer.iWidth;
  strides[2] = 0;


  pVideoPicture->videoBuffer->SetDimensions(m_videoBuffer.iWidth, m_videoBuffer.iHeight, strides);

  pVideoPicture->iFlags = 0;

  pVideoPicture->color_range = 0;
  pVideoPicture->color_matrix = 4;

  pVideoPicture->iWidth  = m_hints.width;
  pVideoPicture->iHeight = m_hints.height;
  pVideoPicture->iDisplayWidth  = pVideoPicture->iWidth;
  pVideoPicture->iDisplayHeight = pVideoPicture->iHeight;
  if (m_hints.aspect > 0.0 && !m_hints.forced_aspect)
  {
    pVideoPicture->iDisplayWidth  = ((int)lrint(pVideoPicture->iHeight * m_hints.aspect)) & ~3;
    if (pVideoPicture->iDisplayWidth > pVideoPicture->iWidth)
    {
      pVideoPicture->iDisplayWidth  = pVideoPicture->iWidth;
      pVideoPicture->iDisplayHeight = ((int)lrint(pVideoPicture->iWidth / m_hints.aspect)) & ~3;
    }
  }

  pVideoPicture->pts = static_cast<double>(timestamp.tv_usec);
  pVideoPicture->dts = DVD_NOPTS_VALUE;

  return VC_PICTURE;
}
