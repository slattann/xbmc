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

#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "V4L2Codec"

V4L2Codec::V4L2Codec()
{
  m_iDecoderHandle = -1;
  m_bVideoConvert = false;
  m_v4l2OutputBuffersCount = 0;
  m_v4l2OutputBuffers = nullptr;
  m_v4l2CaptureBuffersCount = 0;
  m_v4l2CaptureBuffers = nullptr;
}

V4L2Codec::~V4L2Codec()
{
  CloseDecoder();
}

void V4L2Codec::CloseDecoder()
{
  CLog::Log(LOGDEBUG, "%s::%s - Freeing memory allocated for buffers", CLASSNAME, __func__);
  if (m_v4l2OutputBuffers)
  {
    m_v4l2OutputBuffers = CV4L2::FreeBuffers(m_v4l2OutputBuffersCount, m_v4l2OutputBuffers);
  }
  if (m_v4l2CaptureBuffers)
  {
    m_v4l2CaptureBuffers = CV4L2::FreeBuffers(m_v4l2CaptureBuffersCount, m_v4l2CaptureBuffers);
  }

  CLog::Log(LOGDEBUG, "%s::%s - Closing V4L2 device", CLASSNAME, __func__);
  if (m_iDecoderHandle >= 0)
  {
    if (CV4L2::StreamOnOff(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, VIDIOC_STREAMOFF))
    {
      CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT Stream OFF", CLASSNAME, __func__);
    }

    if (CV4L2::StreamOnOff(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, VIDIOC_STREAMOFF))
    {
      CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Stream OFF", CLASSNAME, __func__);
    }

    close(m_iDecoderHandle);
  }

  m_v4l2OutputBuffersCount = 0;
  m_v4l2OutputBuffers = nullptr;
  m_v4l2CaptureBuffersCount = 0;
  m_v4l2CaptureBuffers = nullptr;
  m_iDecoderHandle = -1;
}

bool V4L2Codec::OpenDecoder()
{
  char devname[] = "/dev/video0";

  struct v4l2_capability cap;
  int fd = open(devname, O_RDWR | O_NONBLOCK, 0);
  if (fd > 0)
  {
    memset(&(cap), 0, sizeof (cap));
    auto ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret == 0)
    {
      if ((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE ||
         ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) && (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE))) &&
          (cap.capabilities & V4L2_CAP_STREAMING))
      {
        m_iDecoderHandle = fd;
        CLog::Log(LOGDEBUG, "%s::%s - driver:       %s", CLASSNAME, __func__, cap.driver);
        CLog::Log(LOGDEBUG, "%s::%s - card:         %s", CLASSNAME, __func__, cap.card);
        CLog::Log(LOGDEBUG, "%s::%s - bus_info:     %s", CLASSNAME, __func__, cap.bus_info);
        CLog::Log(LOGDEBUG, "%s::%s - version:      %08x", CLASSNAME, __func__, cap.version);
        CLog::Log(LOGDEBUG, "%s::%s - capabilities: %08x", CLASSNAME, __func__, cap.device_caps);
      }
    }
  }

  if (m_iDecoderHandle < 0)
  {
    close(fd);
  }

  if (m_iDecoderHandle >= 0)
  {
    return true;
  }

  return false;
}

bool V4L2Codec::SetupOutputFormat(CDVDStreamInfo &hints)
{
  struct v4l2_format fmt = {};
  switch (hints.codec)
  {
    case AV_CODEC_ID_MJPEG:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
      m_name = "v4l2-mjpeg";
      break;
    case AV_CODEC_ID_VC1:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_VC1_ANNEX_G;
      m_name = "v4l2-vc1";
      break;
    case AV_CODEC_ID_MPEG1VIDEO:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG1;
      m_name = "v4l2-mpeg1";
      break;
    case AV_CODEC_ID_MPEG2VIDEO:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG2;
      m_name = "v4l2-mpeg2";
      break;
    case AV_CODEC_ID_MPEG4:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MPEG4;
      m_name = "v4l2-mpeg4";
      break;
    case AV_CODEC_ID_H263:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H263;
      m_name = "v4l2-h263";
      break;
    case AV_CODEC_ID_H264:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
      m_name = "v4l2-h264";
      break;
    case AV_CODEC_ID_VP8:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_VP8;
      m_name = "v4l2-vp8";
      break;
    case AV_CODEC_ID_VP9:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_VP9;
      m_name = "v4l2-vp9";
      break;
      /*
    case AV_CODEC_ID_HEVC:
      fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_HEVC;
      m_name = "v4l2-hevc";
      break;
      */
    default:
      return false;
  }

  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  fmt.fmt.pix_mp.plane_fmt[0].sizeimage = STREAM_BUFFER_SIZE;

  auto ret = ioctl(m_iDecoderHandle, VIDIOC_S_FMT, &fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - v4l2 OUTPUT S_FMT failed", CLASSNAME, __func__);
    return false;
  }

  return true;
}

bool V4L2Codec::RequestBuffers()
{
  struct v4l2_control ctrl = {};
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  auto ret = ioctl(m_iDecoderHandle, VIDIOC_G_CTRL, &ctrl);
  if (ret < 0)
  {
    CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Failed to get the number of buffers required", CLASSNAME, __func__);
    m_v4l2CaptureBuffersCount = V4L2_OUTPUT_BUFFERS_CNT;
  }
  else
  {
    CLog::Log(LOGDEBUG, "%s::%s - driver requires a minimum of %d buffers", CLASSNAME, __func__, ctrl.value);
    m_v4l2CaptureBuffersCount = ctrl.value;
  }

  return true;
}

bool V4L2Codec::SetupCaptureBuffers()
{
  m_v4l2CaptureBuffersCount = CV4L2::RequestBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, m_v4l2CaptureBuffersCount);
  if (m_v4l2CaptureBuffersCount < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - VIDIOC_REQBUFS failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  // Allocate, Memory Map and queue mfc capture buffers
  m_v4l2CaptureBuffers = (V4L2Buffer *)calloc(m_v4l2CaptureBuffersCount, sizeof(V4L2Buffer));
  if (!m_v4l2CaptureBuffers)
  {
    CLog::Log(LOGERROR, "%s::%s - v4l2 CAPTURE Cannot allocate memory for buffers", CLASSNAME, __func__);
    return false;
  }
  if (!CV4L2::MmapBuffers(m_iDecoderHandle, m_v4l2CaptureBuffersCount, m_v4l2CaptureBuffers, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, true))
  {
    CLog::Log(LOGERROR, "%s::%s - v4l2 CAPTURE Cannot mmap memory for buffers", CLASSNAME, __func__);
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - video capture buffers successfully allocated", CLASSNAME, __func__);

  auto ret = CV4L2::StreamOnOff(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, VIDIOC_STREAMON);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE Stream ON", CLASSNAME, __func__);
  return true;
}

V4L2Buffer* V4L2Codec::GetCaptureBuffer(int index)
{
  return m_v4l2CaptureBuffers != NULL && index < m_v4l2CaptureBuffersCount ? &(m_v4l2CaptureBuffers[index]) : NULL;
}

bool V4L2Codec::SetCaptureFormat()
{
  struct v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
  auto ret = ioctl(m_iDecoderHandle, VIDIOC_S_FMT, &fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - VIDIOC_S_FMT failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE S_FMT 0x%x",  CLASSNAME, __func__, fmt.fmt.pix_mp.pixelformat);
  return true;
}

bool V4L2Codec::GetCaptureFormat(struct v4l2_format* fmt)
{
  fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  auto ret = ioctl(m_iDecoderHandle, VIDIOC_G_FMT, fmt);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - VIDIOC_G_FMT failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  return true;
}

bool V4L2Codec::GetCaptureCrop(struct v4l2_crop* crop)
{
  crop->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  auto ret = ioctl(m_iDecoderHandle, VIDIOC_G_CROP, crop);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - VIDIOC_G_CROP Failed to get crop information: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - V4L2 CAPTURE G_CROP (%dx%d)", CLASSNAME, __func__, crop->c.width, crop->c.height);
  return true;
}

bool V4L2Codec::SetupOutputBuffers()
{
  m_v4l2OutputBuffersCount = CV4L2::RequestBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, V4L2_OUTPUT_BUFFERS_CNT);
  if (m_v4l2OutputBuffersCount < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - VIDIOC_REQBUFS failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  m_v4l2OutputBuffers = (V4L2Buffer *) calloc(m_v4l2OutputBuffersCount, sizeof(V4L2Buffer));
  if (!m_v4l2OutputBuffers)
  {
    CLog::Log(LOGERROR, "%s::%s - MFC cannot allocate OUTPUT buffers in memory", CLASSNAME, __func__);
    return false;
  }
  if (!CV4L2::MmapBuffers(m_iDecoderHandle, m_v4l2OutputBuffersCount, m_v4l2OutputBuffers, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, false))
  {
    CLog::Log(LOGERROR, "%s::%s - MFC Cannot mmap OUTPUT buffers", CLASSNAME, __func__);
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - video output buffers successfully allocated", CLASSNAME, __func__);

  return true;
}

bool V4L2Codec::IsOutputBufferEmpty(int index)
{
  return (m_v4l2OutputBuffers != NULL && index < m_v4l2OutputBuffersCount ) ? !m_v4l2OutputBuffers[index].bQueue : false;
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
  m_v4l2OutputBuffers[0].iBytesUsed[0] = extraSize;
  memcpy((uint8_t *)m_v4l2OutputBuffers[0].cPlane[0], extraData, extraSize);

  auto ret = CV4L2::QueueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, &m_v4l2OutputBuffers[0]);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - V4L2 Error queuing header: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }
  CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT <- %d header of size %d", CLASSNAME, __func__, ret, extraSize);

  ret = CV4L2::StreamOnOff(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, VIDIOC_STREAMON);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - STREAMON failed: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - V4L2 OUTPUT Stream ON", CLASSNAME, __func__);
  return true;
}

bool V4L2Codec::SendBuffer(int index, uint8_t* demuxer_content, int demuxer_bytes, double pts)
{
  if (m_bVideoConvert)
  {
    m_converter.Convert(demuxer_content, demuxer_bytes);
    demuxer_bytes = m_converter.GetConvertSize();
    demuxer_content = m_converter.GetConvertBuffer();
  }

  if (demuxer_bytes < m_v4l2OutputBuffers[index].iSize[0])
  {
    memcpy((uint8_t *)m_v4l2OutputBuffers[index].cPlane[0], demuxer_content, demuxer_bytes);
    m_v4l2OutputBuffers[index].iBytesUsed[0] = demuxer_bytes;
    m_v4l2OutputBuffers[index].timestamp = pts;

    auto ret = CV4L2::QueueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, &m_v4l2OutputBuffers[index]);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - V4L2 OUTPUT Failed to queue buffer with index %d, errno %d", CLASSNAME, __func__, index, errno);
      return false;
    }
  }
  else
  {
    CLog::Log(LOGERROR, "%s::%s - Packet to big for streambuffer", CLASSNAME, __func__);
    return false;
  }

  return true;
}

bool V4L2Codec::DequeueOutputBuffer(int *result, double *timestamp)
{
  auto ret = CV4L2::PollOutput(m_iDecoderHandle, 1000/3);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - V4L2 OUTPUT PollOutput Error", CLASSNAME, __func__);
    *result = CDVDVideoCodec::VC_ERROR;
    return false;
  }
  else if (ret == 2)
  {
    int index = CV4L2::DequeueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, timestamp);
    if (index < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - V4L2 OUTPUT error dequeue output buffer, got number %d, errno %d", CLASSNAME, __func__, index, errno);
      *result = CDVDVideoCodec::VC_FLUSHED;
      return false;
    }
    m_v4l2OutputBuffers[index].bQueue = false;
    *result = index;
  }
  else if (ret == 1)
  { // buffer is still busy
    CLog::Log(LOGERROR, "%s::%s - V4L2 OUTPUT All buffers are queued and busy, no space for new frame to decode. Very broken situation.", CLASSNAME, __func__);
    *result = CDVDVideoCodec::VC_PICTURE;
    return false;
  }
  else
  {
    CLog::Log(LOGERROR, "%s::%s - V4L2 OUTPUT PollOutput error %d, errno %d", CLASSNAME, __func__, ret, errno);
    *result = CDVDVideoCodec::VC_ERROR;
    return false;
  }
  return true; // *result contains the buffer index
}

bool V4L2Codec::QueueCaptureBuffer(int index)
{
  auto ret = CV4L2::QueueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, &m_v4l2CaptureBuffers[index]);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - V4L2 CAPTURE Failed to queue buffer with index %d, errno %d", CLASSNAME, __func__, index, errno);
    return false;
  }
  return true;
}

bool V4L2Codec::DequeueDecodedFrame(int *result, double *timestamp)
{
  auto index = CV4L2::DequeueBuffer(m_iDecoderHandle, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, timestamp);
  if (index < 0)
  {
    if (errno == EAGAIN)
    {
      // Buffer is still busy, queue more
      *result = CDVDVideoCodec::VC_BUFFER;
      return false;
    }
    CLog::Log(LOGERROR, "%s::%s - V4L2 CAPTURE error dequeue output buffer, got number %d, errno %d", CLASSNAME, __func__, index, errno);
    *result = CDVDVideoCodec::VC_FLUSHED;
    return false;
  }
  m_v4l2CaptureBuffers[index].bQueue = false;
  m_v4l2CaptureBuffers[index].timestamp = *timestamp;
  *result = index;
  return true; // *result contains the buffer index
}
