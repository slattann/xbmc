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

#include "linux/videodev2.h"

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

  bool SetupOutputFormat(CDVDStreamInfo &hints);
  const char* GetOutputName() { return m_name.c_str(); };
  bool RequestBuffers();
  bool SetupCaptureBuffers();
  int GetCaptureBuffersCount() { return m_v4l2CaptureBuffersCount; };
  V4L2Buffer* GetCaptureBuffer(int index);
  bool SetCaptureFormat();
  bool GetCaptureFormat(struct v4l2_format* fmt);
  bool GetCaptureCrop(struct v4l2_crop* crop);
  bool SetupOutputBuffers();
  int  GetOutputBuffersCount()  { return m_v4l2OutputBuffersCount;  };
  bool IsOutputBufferEmpty(int index);

  bool QueueHeader(CDVDStreamInfo &hints);
  bool SendBuffer(int index, uint8_t* demuxer_content, int demuxer_bytes, double pts);
  bool DequeueOutputBuffer(int *result, double *timestamp);
  bool QueueCaptureBuffer(int index);
  bool DequeueDecodedFrame(int *result, double *timestamp);

private:
  int m_iDecoderHandle;
  std::string m_name;
  int m_v4l2OutputBuffersCount;
  V4L2Buffer *m_v4l2OutputBuffers;
  int m_v4l2CaptureBuffersCount;
  V4L2Buffer *m_v4l2CaptureBuffers;
  bool m_bVideoConvert;
  CBitstreamConverter m_converter;
};
