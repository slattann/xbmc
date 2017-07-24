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

#include "utils/log.h"
#include "V4L2Codec.h"

#include <dirent.h>
#include <errno.h>
#include <poll.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CV4L2Codec"

CV4L2Codec::CV4L2Codec()
  : m_fd(nullptr)
  , m_OutputType(-1)
  , m_CaptureType(-1)
{
}

CV4L2Codec::~CV4L2Codec()
{
  CloseDecoder();
}

void CV4L2Codec::CloseDecoder()
{
  int ret;

  if (m_fd)
  {
    if (m_fd->g_fd() > 0)
    {
      ret = m_fd->streamoff(m_OutputType);
      if (ret == 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT Stream OFF", CLASSNAME, __func__);
      }

      ret = m_fd->streamoff(m_CaptureType);
      if (ret == 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Stream OFF", CLASSNAME, __func__);
      }

      CLog::Log(LOGDEBUG, "%s::%s - Freeing memory allocated for buffers", CLASSNAME, __func__);

      ret = m_OutputBuffers.munmap_bufs(m_fd);
      if (ret < 0)
      {
        CLog::Log(LOGERROR, "%s::%s - error munmaping output buffer: %s", CLASSNAME, __func__, strerror(errno));
      }
      memset(&(m_OutputBuffers), 0, sizeof(m_OutputBuffers));

      ret = m_CaptureBuffers.munmap_bufs(m_fd);
      if (ret < 0)
      {
        CLog::Log(LOGERROR, "%s::%s - error munmaping capture buffer: %s", CLASSNAME, __func__, strerror(errno));
      }
      memset(&(m_CaptureBuffers), 0, sizeof(m_CaptureBuffers));

      CLog::Log(LOGDEBUG, "%s::%s - Closing V4L2 device", CLASSNAME, __func__);

      m_fd->close();
    }

    delete(m_fd);
    m_fd = nullptr;
  }
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
      ret = m_fd->enum_fmt(fmtdesc, true, fmtdesc.index);

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
      CLog::Log(LOGDEBUG, "%s::%s - using device %s", CLASSNAME, __func__, devname);
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
    if (ret == EINVAL)
    {
      CLog::Log(LOGERROR, "%s::%s - pixelformat %d not supported.", CLASSNAME, __func__, fmt.g_pixelformat());
    }

    return false;
  }

  return true;
}

bool CV4L2Codec::SetupOutputBuffers()
{
  int ret;
  memset(&(m_OutputBuffers), 0, sizeof(m_OutputBuffers));

  m_OutputBuffers.init(m_OutputType, V4L2_MEMORY_MMAP);

  ret =  m_OutputBuffers.reqbufs(m_fd, V4L2_OUTPUT_BUFFERS_COUNT);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error requesting output buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  ret =  m_OutputBuffers.mmap_bufs(m_fd);
  if(ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error mmapping output buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - output buffers successfully allocated", CLASSNAME, __func__);
  CLog::Log(LOGDEBUG, "%s::%s - output buffers %d of requested %d", CLASSNAME, __func__, m_OutputBuffers.g_buffers(), V4L2_OUTPUT_BUFFERS_COUNT);

  ret = m_fd->streamon(m_OutputType);
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
  int ret;
  memset(&(m_CaptureBuffers), 0, sizeof(m_CaptureBuffers));

  m_CaptureBuffers.init(m_CaptureType, V4L2_MEMORY_MMAP);

  ret =  m_CaptureBuffers.reqbufs(m_fd, V4L2_CAPTURE_BUFFERS_COUNT);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error requesting capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  ret =  m_CaptureBuffers.mmap_bufs(m_fd);
  if(ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error mmapping capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture buffers successfully allocated", CLASSNAME, __func__);
  CLog::Log(LOGDEBUG, "%s::%s - capture buffers %d of requested %d", CLASSNAME, __func__, m_CaptureBuffers.g_buffers(), V4L2_CAPTURE_BUFFERS_COUNT);

  ret = m_fd->streamon(m_CaptureType);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - capture VIDIOC_STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture stream on", CLASSNAME, __func__);
  return true;
}

int CV4L2Codec::GetOutputBufferSize(int index)
{
  cv4l_buffer buffer(m_OutputBuffers, index);
  return buffer.g_bytesused(0);
}

int CV4L2Codec::GetCaptureBufferSize(int index)
{
  cv4l_buffer buffer(m_CaptureBuffers, index);
  return buffer.g_bytesused(0);
}

cv4l_fd * CV4L2Codec::GetFd()
{
  return m_fd;
}

bool CV4L2Codec::IsOutputBufferEmpty(int index)
{
  cv4l_buffer buffer(m_OutputBuffers, index);
  m_fd->querybuf(buffer, index);
  return buffer.g_flags() & V4L2_BUF_FLAG_QUEUED ? false : true;
}

bool CV4L2Codec::IsCaptureBufferEmpty(int index)
{
  cv4l_buffer buffer(m_CaptureBuffers, index);
  m_fd->querybuf(buffer, index);
  return buffer.g_flags() & V4L2_BUF_FLAG_QUEUED ? false : true;
}

bool CV4L2Codec::QueueOutputBuffer(int index, uint8_t* pData, int size, double pts)
{
  timeval timestamp;
  timestamp.tv_usec = pts;

  CLog::Log(LOGDEBUG, "%s::%s - output buffer total size: %d", CLASSNAME, __func__, m_OutputBuffers.g_length(0));
//  if (size < m_OutputBuffers.g_length(0))
//  {
    cv4l_buffer buffer(m_OutputBuffers, index);
    buffer.s_bytesused(size, 0);
    buffer.s_timestamp(timestamp);
    buffer.or_flags(V4L2_BUF_FLAG_TIMESTAMP_COPY);

    CLog::Log(LOGDEBUG, "%s::%s - output buffer size: %d", CLASSNAME, __func__, size);
    memcpy((uint8_t *)m_OutputBuffers.g_mmapping(index, 0), pData, size);
    //m_OutputBuffers.s_userptr(index, 0, pData);

    auto ret = m_fd->qbuf(buffer);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error queuing output buffer: %s", CLASSNAME, __func__, strerror(errno));
      return false;
    }
//  }
//  else
//  {
//    CLog::Log(LOGERROR, "%s::%s - packet too big for stream buffer", CLASSNAME, __func__);
//    return false;
//  }

  return true;
}

bool CV4L2Codec::DequeueOutputBuffer(int *index, timeval *timestamp)
{
  int ret;
  int timeout = 1000/3;
  struct pollfd p;
  p.fd = m_fd->g_fd();
  p.events = POLLIN;
  p.revents = 0;

  while (true)
  {
    ret = poll(&p, 1, timeout);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error polling input: %s", CLASSNAME, __func__, strerror(errno));
      return false;
    }
    else if (p.revents & (POLLHUP | POLLERR))
    {
      CLog::Log(LOGERROR, "%s::%s - hangup polling input: %s", CLASSNAME, __func__, strerror(errno));
      return false;
    }
    else if(p.revents & POLLIN)
    {
      cv4l_buffer buffer(m_OutputBuffers);

      ret = m_fd->dqbuf(buffer);
      if (ret < 0)
      {
        CLog::Log(LOGERROR, "%s::%s - error dequeuing output buffer: %s", CLASSNAME, __func__, strerror(errno));
        return false;
      }

      *timestamp = buffer.g_timestamp();
      *index = buffer.g_index();

      return true;
    }
  }
}

bool CV4L2Codec::QueueCaptureBuffer(int index)
{
  cv4l_buffer buffer(m_CaptureBuffers, index);
  buffer.or_flags(V4L2_BUF_FLAG_TIMESTAMP_COPY);

  auto ret = m_fd->qbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error queuing capture buffer: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  return true;
}

bool CV4L2Codec::DequeueCaptureBuffer(int *index, timeval *timestamp)
{
  cv4l_buffer buffer(m_CaptureBuffers);

  auto ret = m_fd->dqbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error dequeuing capture buffer: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  *timestamp = buffer.g_timestamp();
  *index = buffer.g_index();
  return true;
}

void CV4L2Codec::GetPicture(VideoPicture *pVideoPicture, int index)
{
  cv4l_buffer buffer(m_CaptureBuffers, index);
  memcpy(pVideoPicture->videoBuffer->GetMemPtr(), (uint8_t *)m_CaptureBuffers.g_mmapping(index, 0), buffer.g_bytesused(0));
  CLog::Log(LOGDEBUG, "%s::%s - capture buffer size: %d", CLASSNAME, __func__, buffer.g_bytesused(0));
}
