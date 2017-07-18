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

#include "V4L2Codec.h"

#include <dirent.h>
#include <errno.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CV4L2Codec"

CV4L2Codec::CV4L2Codec()
{
  m_fd = nullptr;
  m_iDequeuedToPresentBufferNumber = -1;
  m_OutputType = -1;
  m_CaptureType = -1;
}

CV4L2Codec::~CV4L2Codec()
{
  CloseDecoder();
}

void CV4L2Codec::CloseDecoder()
{
  if (m_fd)
  {
    if (m_fd->g_fd() > 0)
    {
      CLog::Log(LOGDEBUG, "%s::%s - Freeing memory allocated for buffers", CLASSNAME, __func__);
      CV4L2::FreeBuffers(m_fd, &m_OutputBuffers);
      memset(&(m_OutputBuffers), 0, sizeof (m_OutputBuffers));

      CV4L2::FreeBuffers(m_fd, &m_CaptureBuffers);
      memset(&(m_CaptureBuffers), 0, sizeof (m_CaptureBuffers));

      CLog::Log(LOGDEBUG, "%s::%s - Closing V4L2 device", CLASSNAME, __func__);

      auto ret = m_fd->streamoff(m_OutputType);
      if (ret < 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT Stream OFF", CLASSNAME, __func__);
      }

      ret = m_fd->streamoff(m_CaptureType);
      if (ret < 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Stream OFF", CLASSNAME, __func__);
      }

      m_fd->close();
    }

    delete(m_fd);
    m_fd = nullptr;
  }

  m_iDequeuedToPresentBufferNumber = -1;

}

bool CV4L2Codec::OpenDecoder()
{
  CLog::Log(LOGDEBUG, "%s::%s - open", CLASSNAME, __func__);

  m_fd = new cv4l_fd();

  for (auto i = 0; i < 10; i++)
  {
    char devname[] = "/dev/video";
    strcat(devname, std::to_string(i).c_str());

    m_fd->open(devname, true);

    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;

    int ret = 0;
    bool found = false;

    while (ret == 0)
    {
      ret = m_fd->enum_fmt(fmtdesc, true, fmtdesc.index); //, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

      if (fmtdesc.pixelformat == V4L2_PIX_FMT_NV12)
      {
        CLog::Log(LOGDEBUG, "found pixel format[%d]: %s", fmtdesc.index, fmtdesc.description);
        found = true;
        break;
      }

      fmtdesc.index++;
    }

    if (found)
    {
      CLog::Log(LOGDEBUG, "using device %s", devname);
      break;
    }

    m_fd->close();
  }

  if (m_fd->g_fd() > 0)
  {
    if ((m_fd->has_vid_m2m() || (m_fd->has_vid_cap() && m_fd->has_vid_out())) && m_fd->has_streaming())
    {
      v4l2_capability caps;
      m_fd->querycap(caps);
      CLog::Log(LOGDEBUG, "%s::%s - driver:       %s", CLASSNAME, __func__, caps.driver);
      CLog::Log(LOGDEBUG, "%s::%s - card:         %s", CLASSNAME, __func__, caps.card);
      CLog::Log(LOGDEBUG, "%s::%s - bus_info:     %s", CLASSNAME, __func__, caps.bus_info);
      CLog::Log(LOGDEBUG, "%s::%s - version:      %08x", CLASSNAME, __func__, caps.version);
      CLog::Log(LOGDEBUG, "%s::%s - capabilities: %08x", CLASSNAME, __func__, caps.device_caps);
    }

    if (m_fd->has_vid_mplane())
    {
      CLog::Log(LOGDEBUG, "%s::%s - mplane device detected", CLASSNAME, __func__);
      m_CaptureType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      m_OutputType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    }
    else
    {
      CLog::Log(LOGDEBUG, "%s::%s - splane device detected", CLASSNAME, __func__);
      m_CaptureType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      m_OutputType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    }
  }

  if (m_fd->g_fd() < 0)
  {
    m_fd->close();
  }

  if (m_fd->g_fd() >= 0)
  {
    return true;
  }

  return false;
}

bool CV4L2Codec::AddData(uint8_t *pData, size_t size, double dts, double pts)
{
  timeval timestamp;
  timestamp.tv_usec = pts;

  int index;

  if (pData)
  {
    for (index = 0; index < GetOutputBuffersCount(); index++)
    {
      if (IsOutputBufferEmpty(index))
      {
        CLog::Log(LOGDEBUG, "%s::%s - using output buffer index: %d", CLASSNAME, __func__, index);
        break;
      }
    }

    CLog::Log(LOGDEBUG, "%s::%s - dequeuing buffer", CLASSNAME, __func__);
    if (!DequeueOutputBuffer(&timestamp))
    {
      return false;
    }

    CLog::Log(LOGDEBUG, "%s::%s - queuing output buffer index: %d", CLASSNAME, __func__, index);
    if (!QueueOutputBuffer(index, pData, size, pts))
    {
      return false;
    }
  }

  return true; // Picture is finally ready to be processed further
}

CDVDVideoCodec::VCReturn CV4L2Codec::GetPicture(VideoPicture* pVideoPicture)
{
  int index;
  timeval timestamp;
  CDVDVideoCodec::VCReturn ret = CDVDVideoCodec::VC_NONE;

  CLog::Log(LOGDEBUG, "%s::%s - dequeued decoded frame", CLASSNAME, __func__);
  ret = DequeueCaptureBuffer(&timestamp);
  if (ret != CDVDVideoCodec::VC_PICTURE)
  {
    return ret;
  }

  CLog::Log(LOGDEBUG, "%s::%s - saving to videobuffer", CLASSNAME, __func__);

  memcpy(pVideoPicture->videoBuffer->GetMemPtr(), (uint8_t*)m_CaptureBuffers.g_mmapping(index, 0), m_CaptureBuffers.g_length(0));

  pVideoPicture->pts = timestamp.tv_sec;

  CLog::Log(LOGDEBUG, "%s::%s - queuing capture buffer index: %d", CLASSNAME, __func__, index);
  if (!QueueCaptureBuffer(index))
  {
    return CDVDVideoCodec::VC_NOBUFFER;
  }

  return ret;
}

bool CV4L2Codec::SetupOutputFormat(CDVDStreamInfo &hints)
{
  cv4l_fmt fmt = {};
  switch (hints.codec)
  {
    case AV_CODEC_ID_MJPEG:
      fmt.s_pixelformat(V4L2_PIX_FMT_MJPEG);
      m_name = "v4l2-mjpeg";
      break;
    case AV_CODEC_ID_VC1:
      fmt.s_pixelformat(V4L2_PIX_FMT_VC1_ANNEX_G);
      m_name = "v4l2-vc1";
      break;
    case AV_CODEC_ID_MPEG1VIDEO:
      fmt.s_pixelformat(V4L2_PIX_FMT_MPEG1);
      m_name = "v4l2-mpeg1";
      break;
    case AV_CODEC_ID_MPEG2VIDEO:
      fmt.s_pixelformat(V4L2_PIX_FMT_MPEG2);
      m_name = "v4l2-mpeg2";
      break;
    case AV_CODEC_ID_MPEG4:
      fmt.s_pixelformat(V4L2_PIX_FMT_MPEG4);
      m_name = "v4l2-mpeg4";
      break;
    case AV_CODEC_ID_H263:
      fmt.s_pixelformat(V4L2_PIX_FMT_H263);
      m_name = "v4l2-h263";
      break;
    case AV_CODEC_ID_H264:
      fmt.s_pixelformat(V4L2_PIX_FMT_H264);
      m_name = "v4l2-h264";
      break;
    case AV_CODEC_ID_VP8:
      fmt.s_pixelformat(V4L2_PIX_FMT_VP8);
      m_name = "v4l2-vp8";
      break;
    case AV_CODEC_ID_VP9:
      fmt.s_pixelformat(V4L2_PIX_FMT_VP9);
      m_name = "v4l2-vp9";
      break;
      /*
    case AV_CODEC_ID_HEVC:
      fmt.s_pixelformat(V4L2_PIX_FMT_HEVC);
      //fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_HEVC;
      m_name = "v4l2-hevc";
      break;
      */
    default:
      return false;
  }

  fmt.s_type(m_OutputType);

  auto ret = m_fd->s_fmt(fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - output VIDIOC_S_FMT failed", CLASSNAME, __func__);
    return false;
  }

  return true;
}

bool CV4L2Codec::SetupOutputBuffers()
{
  if (!CV4L2::MmapBuffers(m_fd, V4L2_OUTPUT_BUFFERS_COUNT, &m_OutputBuffers, m_OutputType))
  {
    CLog::Log(LOGERROR, "%s::%s - cannot mmap memory for output buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - output buffers successfully allocated", CLASSNAME, __func__);

  /*
  if (!CV4L2::RequestBuffers(m_fd, V4L2_CAPTURE_BUFFERS_COUNT, &m_CaptureBuffers, m_CaptureType, V4L2_MEMORY_USERPTR))
  {
    CLog::Log(LOGERROR, "%s::%s - error requesting capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }
  */

  CLog::Log(LOGDEBUG, "%s::%s - output buffers %d of requested %d", CLASSNAME, __func__, m_OutputBuffers.g_buffers(), V4L2_OUTPUT_BUFFERS_COUNT);

  auto ret = m_fd->streamon(m_OutputType);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - output VIDIOC_STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - output stream on", CLASSNAME, __func__);
  return true;
}

bool CV4L2Codec::SetupCaptureBuffers()
{

  if (!CV4L2::MmapBuffers(m_fd, V4L2_CAPTURE_BUFFERS_COUNT, &m_CaptureBuffers, m_CaptureType))
  {
    CLog::Log(LOGERROR, "%s::%s - cannot mmap memory for capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture buffers successfully allocated", CLASSNAME, __func__);

  /*
  if (!CV4L2::RequestBuffers(m_fd, V4L2_CAPTURE_BUFFERS_COUNT, &m_CaptureBuffers, m_CaptureType, V4L2_MEMORY_USERPTR))
  {
    CLog::Log(LOGERROR, "%s::%s - error requesting capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }
  */

  CLog::Log(LOGDEBUG, "%s::%s - capture buffers %d of requested %d", CLASSNAME, __func__, m_CaptureBuffers.g_buffers(), V4L2_CAPTURE_BUFFERS_COUNT);

  auto ret = m_fd->streamon(m_CaptureType);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - capture VIDIOC_STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture stream on", CLASSNAME, __func__);
  return true;
}

cv4l_queue CV4L2Codec::GetOutputBuffers()
{
  return m_OutputBuffers;
}

cv4l_queue CV4L2Codec::GetCaptureBuffers()
{
  return m_CaptureBuffers;
}

cv4l_fd * CV4L2Codec::GetFd()
{
  return m_fd;
}

bool CV4L2Codec::IsOutputBufferEmpty(int index)
{
  cv4l_buffer buffer;
  buffer.init(m_OutputBuffers);
  m_fd->querybuf(buffer, index);
  return buffer.g_flags() & V4L2_BUF_FLAG_QUEUED ? false : true;
}

bool CV4L2Codec::IsCaptureBufferEmpty(int index)
{
  cv4l_buffer buffer;
  buffer.init(m_CaptureBuffers);
  m_fd->querybuf(buffer, index);
  return buffer.g_flags() & V4L2_BUF_FLAG_QUEUED ? false : true;
}

bool CV4L2Codec::QueueOutputBuffer(int index, uint8_t* pData, int size, double pts)
{
  if (size < m_OutputBuffers.g_length(0))
  {
    memcpy((uint8_t *)m_OutputBuffers.g_mmapping(index, 0), pData, size);
    //m_OutputBuffers.s_userptr(index, 0, pData);

    cv4l_buffer buffer;
    buffer.init(m_OutputBuffers, index);
    buffer.s_bytesused(size, 0);
    timeval timestamp;
    timestamp.tv_usec = pts;
    buffer.s_timestamp(timestamp);

    auto ret = CV4L2::QueueBuffer(m_fd, buffer);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - failed to queue output buffer with index %d: %s", CLASSNAME, __func__, index, strerror(errno));
      return false;
    }
  }
  else
  {
    CLog::Log(LOGERROR, "%s::%s - packet too big for streambuffer", CLASSNAME, __func__);
    return false;
  }

  return true;
}

bool CV4L2Codec::DequeueOutputBuffer(timeval *timestamp)
{
  auto ret = CV4L2::PollOutput(m_fd->g_fd(), 1000/3);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - poll capture error %d: %s", CLASSNAME, __func__, ret, strerror(errno));
    return false;
  }
  else if (ret == 0)
  {
    CLog::Log(LOGERROR, "%s::%s - all output buffers are queued and busy, no space for new frames to decode.", CLASSNAME, __func__);
    return false;
  }
  else if (ret > 1)
  {
    cv4l_buffer buffer;
    buffer.init(m_OutputBuffers);
    ret = CV4L2::DequeueBuffer(m_fd, buffer, timestamp);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error dequeuing output buffer: %s", CLASSNAME, __func__, strerror(errno));
      return false;
    }
  }

  return true;
}

bool CV4L2Codec::QueueCaptureBuffer(int index)
{
  cv4l_buffer buffer;
  buffer.init(m_CaptureBuffers, index);
  auto ret = CV4L2::QueueBuffer(m_fd, buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - failed to queue capture buffer with index %d: %s", CLASSNAME, __func__, index, strerror(errno));
    return false;
  }
  return true;
}

CDVDVideoCodec::VCReturn CV4L2Codec::DequeueCaptureBuffer(timeval *timestamp)
{
  cv4l_buffer buffer;
  buffer.init(m_CaptureBuffers);
  auto ret = CV4L2::DequeueBuffer(m_fd, buffer, timestamp);
  if (ret < 0)
  {
    if (errno == EAGAIN)
    {
      // Buffer is still busy, queue more
      return CDVDVideoCodec::VC_BUFFER;
    }
    CLog::Log(LOGERROR, "%s::%s - error dequeuing capture buffer: %s", CLASSNAME, __func__, strerror(errno));
    return CDVDVideoCodec::VC_FLUSHED;
  }

  return CDVDVideoCodec::VC_PICTURE;
}
