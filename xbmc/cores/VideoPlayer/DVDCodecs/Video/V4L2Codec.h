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

#include "linux/V4L2.h"

#include "DVDVideoCodec.h"
#include "DVDStreamInfo.h"
#include "utils/log.h"
#include "utils/BitstreamConverter.h"

#define STREAM_BUFFER_SIZE              1048576 // compressed frame size. 1080p mpeg4 10Mb/s can be up to 786k in size, so this is to make sure frame fits into buffer
						// for unknown reason, possibly firmware bug, if set to other values, it corrupts adjacent value in the setup data structure for h264 streams
#define V4L2_OUTPUT_BUFFERS_CNT          2       // 1 doesn't work at all
#define V4L2_CAPTURE_EXTRA_BUFFER_CNT    16      // these are extra buffers, better keep their count as big as going to be simultaneous dequeued buffers number

class V4L2Codec
{
public:
  V4L2Codec();
  virtual ~V4L2Codec();

  bool OpenDecoder();
  void CloseDecoder();
  void Reset();

  int AddData(uint8_t *pData, size_t size, double dts, double pts);
  CDVDVideoCodec::VCReturn GetPicture(VideoPicture* pVideoPicture);

  bool SetupOutputFormat(CDVDStreamInfo &hints);
  const char* GetOutputName() { return m_name.c_str(); };
  bool SetupCaptureBuffers();
  int GetCaptureBuffersCount() { return m_CaptureBuffers.g_buffers(); };
  cv4l_queue GetCaptureBuffers();
  cv4l_queue GetOutputBuffers();
  cv4l_fd * GetFd();
  bool SetCaptureFormat();
  bool SetupOutputBuffers();
  int  GetOutputBuffersCount()  { return m_OutputBuffers.g_buffers();  };
  bool IsOutputBufferEmpty(int index);
  bool IsCaptureBufferQueued(int index);

  bool QueueOutputBuffer(int index, uint8_t* demuxer_content, int demuxer_bytes, double pts);
  bool DequeueOutputBuffer(int *result, timeval *timestamp);
  bool QueueCaptureBuffer(int index);
  bool DequeueCaptureBuffer(int *result, timeval *timestamp);

private:
  cv4l_fd *m_fd;
  std::string m_name;
  cv4l_queue m_OutputBuffers;
  cv4l_queue m_CaptureBuffers;
  int m_iDequeuedToPresentBufferNumber;
};
