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
#define CLASSNAME "V4L2Codec"

V4L2Codec::V4L2Codec()
{
  m_fd = nullptr;
  m_bVideoConvert = false;
  m_OutputBuffers = nullptr;
  m_CaptureBuffers = nullptr;
}

V4L2Codec::~V4L2Codec()
{
  CloseDecoder();
}

void V4L2Codec::CloseDecoder()
{
  if (m_fd != nullptr)
  {
    if (m_fd->g_fd() > 0)
    {
      CLog::Log(LOGDEBUG, "%s::%s - Freeing memory allocated for buffers", CLASSNAME, __func__);
      if (m_OutputBuffers)
      {
        CV4L2::FreeBuffers(m_fd, m_OutputBuffers);
        m_OutputBuffers = nullptr;
      }

      if (m_CaptureBuffers)
      {
        CV4L2::FreeBuffers(m_fd, m_CaptureBuffers);
        m_CaptureBuffers = nullptr;
      }

      CLog::Log(LOGDEBUG, "%s::%s - Closing V4L2 device", CLASSNAME, __func__);

      auto ret = m_fd->streamoff(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
      if (ret < 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT Stream OFF", CLASSNAME, __func__);
      }

      ret = m_fd->streamoff(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
      if (ret < 0)
      {
        CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Stream OFF", CLASSNAME, __func__);
      }

      m_fd->close();
    }
  }

  delete(m_OutputBuffers);
  m_OutputBuffers = nullptr;
  delete(m_CaptureBuffers);
  m_CaptureBuffers = nullptr;
  delete(m_fd);
  m_fd = nullptr;
}

bool V4L2Codec::OpenDecoder()
{
  CLog::Log(LOGDEBUG, "%s::%s - open", CLASSNAME, __func__);

  char devname[] = "/dev/video1";

  m_fd = new cv4l_fd;

  m_fd->open(devname, true);

  CLog::Log(LOGDEBUG, "%s::%s - m_fd->open", CLASSNAME, __func__);

  if (m_fd->g_fd() > 0)
  {
    CLog::Log(LOGDEBUG, "%s::%s - fd: %d", CLASSNAME, __func__, m_fd->g_fd());

    if ((m_fd->has_vid_m2m() || (m_fd->has_vid_cap() && m_fd->has_vid_out())) && m_fd->has_streaming())
    {
      CLog::Log(LOGDEBUG, "%s::%s - driver supports m2m and streaming", CLASSNAME, __func__, m_fd->g_fd());
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

bool V4L2Codec::SetupOutputFormat(CDVDStreamInfo &hints)
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

  fmt.s_type(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  auto ret = m_fd->s_fmt(fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - output VIDIOC_S_FMT failed", CLASSNAME, __func__);
    return false;
  }

  return true;
}

bool V4L2Codec::SetupCaptureBuffers()
{
  m_CaptureBuffers = new cv4l_queue;
  if (!CV4L2::MmapBuffers(m_fd, V4L2_CAPTURE_BUFFERS_COUNT, m_CaptureBuffers, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
  {
    CLog::Log(LOGERROR, "%s::%s - cannot mmap memory for capture buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture buffers successfully allocated", CLASSNAME, __func__);

  auto ret = m_fd->streamon(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - capture VIDIOC_STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture stream on", CLASSNAME, __func__);
  return true;
}

cv4l_queue * V4L2Codec::GetCaptureBuffer() //int index)
{
  /*
  cv4l_buffer buffer;
  buffer.init(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, index);
  return buffer;
  */
  //return m_CaptureBuffers != NULL && index < V4L2_CAPTURE_BUFFERS_COUNT ? &(m_CaptureBuffers[index]) : NULL;
  return m_CaptureBuffers;
}

bool V4L2Codec::IsCaptureBufferQueued(int index)
{
  cv4l_buffer buffer;
  m_fd->querybuf(buffer, index);
  return true ? buffer.g_flags() & V4L2_BUF_FLAG_QUEUED : false;
}

bool V4L2Codec::SetCaptureFormat()
{
  cv4l_fmt fmt = {};
  fmt.s_pixelformat(V4L2_PIX_FMT_NV12);
  auto ret = m_fd->s_fmt(fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - capture VIDIOC_S_FMT failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - capture VIDIOC_S_FMT 0x%x",  CLASSNAME, __func__, fmt.fmt.pix_mp.pixelformat);
  return true;
}

bool V4L2Codec::SetupOutputBuffers()
{
  m_OutputBuffers = new cv4l_queue;
  auto ret = CV4L2::MmapBuffers(m_fd, V4L2_OUTPUT_BUFFERS_COUNT, m_OutputBuffers, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!ret)
  {
    CLog::Log(LOGERROR, "%s::%s - cannot mmap memory for output buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - output buffers successfully allocated", CLASSNAME, __func__);

  return true;
}

cv4l_queue * V4L2Codec::GetOutputBuffer() //int index)
{
  /*
  cv4l_buffer buffer;
  buffer.init(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, index);
  */
  return m_OutputBuffers;
}


bool V4L2Codec::IsOutputBufferEmpty(int index)
{
  cv4l_buffer buffer;
  m_fd->querybuf(buffer, index);
  return (m_OutputBuffers != NULL && index < V4L2_OUTPUT_BUFFERS_COUNT ) ? buffer.g_flags() & V4L2_BUF_FLAG_QUEUED : false;
}


bool V4L2Codec::QueueHeader(CDVDStreamInfo &hints)
{
  unsigned int extraSize = 0;
  uint8_t *extraData = NULL;

  m_bVideoConvert = m_converter.Open(hints.codec, (uint8_t *)hints.extradata, hints.extrasize, true);
  if (m_bVideoConvert)
  {
    if(m_converter.GetExtraData() != NULL && m_converter.GetExtraSize() > 0)
    {
      extraSize = m_converter.GetExtraSize();
      extraData = m_converter.GetExtraData();
    }
  }
  else
  {
    if(hints.extrasize > 0 && hints.extradata != NULL)
    {
      extraSize = hints.extrasize;
      extraData = (uint8_t*)hints.extradata;
    }
  }

  // Prepare header
  cv4l_buffer buffer;
  buffer.init(10, 1, 0);
  buffer.s_bytesused(extraSize, 0);
  // todo
  memcpy((uint8_t *)m_OutputBuffers->g_mmapping(0, 0) , extraData, extraSize);
  //m_fd->write(extraData, extraSize);

  auto ret = CV4L2::QueueBuffer(m_fd, buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error queuing header: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - %d header of size %d", CLASSNAME, __func__, ret, extraSize);

  ret = m_fd->streamon(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - output VIDIOC_STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - output stream on", CLASSNAME, __func__);
  return true;
}

bool V4L2Codec::SendBuffer(int index, uint8_t* demuxer_content, int demuxer_bytes, double timestamp)
{
  if (m_bVideoConvert)
  {
    m_converter.Convert(demuxer_content, demuxer_bytes);
    demuxer_bytes = m_converter.GetConvertSize();
    demuxer_content = m_converter.GetConvertBuffer();
  }

  // todo
  if (demuxer_bytes < m_OutputBuffers->g_mem_offset(0, 0))
  {
    memcpy((uint8_t *)m_OutputBuffers->g_mmapping(index, 0), demuxer_content, demuxer_bytes);

    cv4l_buffer buffer;
    buffer.init(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, index);
    buffer.s_bytesused(demuxer_bytes, 0);
    //buffer.s_timestamp(timestamp);

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

bool V4L2Codec::DequeueOutputBuffer(int *result, timeval timestamp)
{
  auto ret = CV4L2::PollOutput(m_fd->g_fd(), 1000/3);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - poll output error %d: %s", CLASSNAME, __func__, ret, strerror(errno));
    *result = -1; //CDVDVideoCodec::VC_ERROR;
    return false;
  }
  else if (ret == 0)
  { // buffer is still busy
    CLog::Log(LOGERROR, "%s::%s - all output buffers are queued and busy, no space for new frames to decode.", CLASSNAME, __func__);
    *result = -1; //CDVDVideoCodec::VC_PICTURE;
    return false;
  }
  else if (ret > 1)
  {
    cv4l_buffer buf;
    buf.init(10, 1, 0);
    int index = CV4L2::DequeueBuffer(m_fd, buf);//, timestamp);
    if (index < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error dequeuing output buffer, got number %d: %s", CLASSNAME, __func__, index, strerror(errno));
      *result = -1; //CDVDVideoCodec::VC_FLUSHED;
      return false;
    }
    *result = index;
  }

  return true; // *result contains the buffer index
}

bool V4L2Codec::QueueCaptureBuffer(int index)
{
  cv4l_buffer buffer;
  buffer.init(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, index);
  auto ret = CV4L2::QueueBuffer(m_fd, buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - failed to queue capture buffer with index %d: %s", CLASSNAME, __func__, index, strerror(errno));
    return false;
  }
  return true;
}

bool V4L2Codec::DequeueDecodedFrame(int *result)//, timeval *timestamp)
{
  cv4l_buffer buffer;
  buffer.init(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP);
  auto index = CV4L2::DequeueBuffer(m_fd, buffer); //, &timestamp);
  if (index < 0)
  {
    if (errno == EAGAIN)
    {
      // Buffer is still busy, queue more
      *result = -1; //CDVDVideoCodec::VC_BUFFER;
      return false;
    }
    CLog::Log(LOGERROR, "%s::%s - error dequeuing capture buffer, got number %d: %s", CLASSNAME, __func__, index, strerror(errno));
    *result = -1; //CDVDVideoCodec::VC_FLUSHED;
    return false;
  }
  *result = index;
  return true; // *result contains the buffer index
}
