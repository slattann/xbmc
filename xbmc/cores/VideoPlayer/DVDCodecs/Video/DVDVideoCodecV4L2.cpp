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
#include "xbmc/utils/Base64.h"

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CDVDVideoCodecV4L2"

CDVDVideoCodecV4L2::CDVDVideoCodecV4L2(CProcessInfo &processInfo) : CDVDVideoCodec(processInfo)
{
  m_Codec = new V4L2Codec();

  m_iDequeuedToPresentBufferNumber = -1;
}

CDVDVideoCodecV4L2::~CDVDVideoCodecV4L2()
{
  Dispose();
  delete m_Codec;
}

void CDVDVideoCodecV4L2::Dispose()
{
  m_iDequeuedToPresentBufferNumber = -1;
  memset(&(m_videoBuffer), 0, sizeof (m_videoBuffer));

  if (m_Codec != nullptr)
  {
    m_Codec->CloseDecoder();
  }
}

bool CDVDVideoCodecV4L2::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  m_hints = hints;

  Dispose();

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

  if (!m_Codec->QueueHeader(hints))
  {
    return false;
  }

  if (!m_Codec->SetCaptureFormat())
  {
    return false;
  }

  if (!m_Codec->SetupCaptureBuffers())
  {
    return false;
  }

  m_videoBuffer.iFlags = DVP_FLAG_ALLOCATED;

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

  unsigned int iPixels = m_hints.width * m_hints.height;
  unsigned int iChromaPixels = iPixels;
  m_videoBuffer.data[0] = (uint8_t*)malloc(16 + iPixels);
  m_videoBuffer.data[1] = (uint8_t*)malloc(16 + iChromaPixels);
  m_videoBuffer.data[2] = NULL;
  m_videoBuffer.data[3] = NULL;

  m_videoBuffer.iLineSize[0] = m_hints.width;
  m_videoBuffer.iLineSize[1] = m_hints.width;
  m_videoBuffer.iLineSize[2] = 0;
  m_videoBuffer.iLineSize[3] = 0;

  m_videoBuffer.format = RENDER_FMT_NV12;
  m_videoBuffer.pts = DVD_NOPTS_VALUE;
  m_videoBuffer.dts = DVD_NOPTS_VALUE;

  m_processInfo.SetVideoDecoderName("v4l2", true);
  m_processInfo.SetVideoPixelFormat("nv12");
  m_processInfo.SetVideoDimensions(m_hints.width, m_hints.height);
  m_processInfo.SetVideoDeintMethod("hardware");
  m_processInfo.SetVideoDAR(m_hints.aspect);

  return true;
}

bool CDVDVideoCodecV4L2::AddData(const DemuxPacket &packet)
{
  // Handle Input, add demuxer packet to input queue, we must accept it or
  // it will be discarded as VideoPlayerVideo has no concept of "try again".

  uint8_t *pData(packet.pData);
  int iSize(packet.iSize);

  timeval timestamp;

  /*
  long* pts = (long*)&buffer->timestamp;
  buf.timestamp.tv_sec = pts[0];
  buf.timestamp.tv_usec = pts[1];

  long pts[2] = { buf.timestamp.tv_sec, buf.timestamp.tv_usec };
  *dequeuedTimestamp = *((double*)&pts[0]);

  timestamp.tv_sec = packet.pts[0];
  timestamp.tv_usec = packet.pts[0];
  */

  int ret = -1;
  size_t index = 0;
  //double dequeuedTimestamp;

  if (pData)
  {
    // Find buffer ready to be filled
    for (index = 0; index < m_Codec->GetOutputBuffersCount(); index++)
    {
      if (m_Codec->IsOutputBufferEmpty(index))
      {
        break;
      }
    }

    if (index == m_Codec->GetOutputBuffersCount())
    {
      // all input buffers are busy, dequeue needed
      if (!m_Codec->DequeueOutputBuffer(&ret, timestamp))
      {
        return false;
      }
      index = ret;
    }

    if (!m_Codec->SendBuffer(index, pData, iSize, packet.pts))
    {
      return false;
    }
  }

  if (m_iDequeuedToPresentBufferNumber >= 0)
  {
    if (!m_Codec->IsCaptureBufferQueued(m_iDequeuedToPresentBufferNumber))
    {
      if (!m_Codec->QueueCaptureBuffer(m_iDequeuedToPresentBufferNumber))
      {
        return false;
      }
      m_iDequeuedToPresentBufferNumber = -1;
    }
  }

  if (!m_Codec->DequeueDecodedFrame(&ret))//, &timestamp))
  {
    return false;
  }
  index = ret;

  m_videoBuffer.data[0] = (uint8_t*) m_Codec->GetCaptureBuffer()->g_mmapping(0, 0);
  m_videoBuffer.data[1] = m_videoBuffer.data[0] + (m_videoBuffer.iWidth * m_videoBuffer.iHeight);
  //m_videoBuffer.pts = m_Codec->GetCaptureBuffer()-> ->g_timestamp().tv_sec;

  m_iDequeuedToPresentBufferNumber = index;

  std::string bufferstr = Base64::Encode((char*)(m_videoBuffer.data[0]), m_videoBuffer.iWidth * m_videoBuffer.iHeight);
  CLog::Log(LOGERROR, "%s::%s - buffer: %s", CLASSNAME, __func__, bufferstr);

  return true; // Picture is finally ready to be processed further
}

void CDVDVideoCodecV4L2::Reset()
{
  CDVDCodecOptions options;
  Open(m_hints, options);
}

CDVDVideoCodec::VCReturn CDVDVideoCodecV4L2::GetPicture(VideoPicture* pVideoPicture)
{
  *pVideoPicture = m_videoBuffer;
  return CDVDVideoCodec::VC_PICTURE;
}

