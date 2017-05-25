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

#include "system.h"
#include "V4L2.h"

#include "xbmc/utils/log.h"

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <linux/media.h>
#include <errno.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "CV4L2"

CV4L2::CV4L2()
{
}

CV4L2::~CV4L2()
{
}

int CV4L2::RequestBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, int numBuffers)
{
  struct v4l2_requestbuffers reqbuf;

  if(device < 0)
  {
    return -1;
  }

  reqbuf.type = type;
  reqbuf.memory = memory;
  reqbuf.count = numBuffers;

  auto ret = ioctl(device, VIDIOC_REQBUFS, &reqbuf);
  if (ret < 0)
  {
    if (errno == EINVAL)
    {
      CLog::Log(LOGERROR, "%s::%s - Video capturing or DMABUF streaming is not supported", CLASSNAME, __func__);
    }
    else
    {
      CLog::Log(LOGERROR, "%s::%s - Request buffers", CLASSNAME, __func__);
    }

    return -1;
  }

  return reqbuf.count;
}

int CV4L2::StreamOnOff(int device, enum v4l2_buf_type type, int onoff)
{
  enum v4l2_buf_type setType = type;

  if(device < 0)
  {
    return -1;
  }

  auto ret = ioctl(device, onoff, &setType);
  if (ret < 0)
  {
    return -1;
  }

  return 0;
}

bool CV4L2::MmapBuffers(int device, int count, V4L2Buffer *v4l2Buffers, enum v4l2_buf_type type, enum v4l2_memory memory, bool queue)
{
  struct v4l2_buffer buf;
  struct v4l2_plane planes[V4L2_NUM_MAX_PLANES];

  if(device < 0 || !v4l2Buffers || count == 0)
  {
    return false;
  }

  for(auto i = 0; i < count; i++)
  {
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * V4L2_NUM_MAX_PLANES);
    buf.type      = type;
    buf.memory    = memory;
    buf.index     = i;
    buf.m.planes  = planes;
    buf.length    = V4L2_NUM_MAX_PLANES;

    auto ret = ioctl(device, VIDIOC_QUERYBUF, &buf);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - Query buffer", CLASSNAME, __func__);
      return false;
    }

    V4L2Buffer *buffer = &v4l2Buffers[i];

    buffer->iNumPlanes = 0;
    buffer->bQueue = false;
    for (auto j = 0; j < buf.length; j++)
    {
      buffer->iSize[j]       = buf.m.planes[j].length;
      buffer->iBytesUsed[j]  = buf.m.planes[j].bytesused;
      if(buffer->iSize[j])
      {
        buffer->cPlane[j] = mmap(NULL, buf.m.planes[j].length, PROT_READ | PROT_WRITE,
                       MAP_SHARED, device, buf.m.planes[j].m.mem_offset);
        if(buffer->cPlane[j] == MAP_FAILED)
        {
          CLog::Log(LOGERROR, "%s::%s - Mmapping buffer", CLASSNAME, __func__);
          return false;
        }
        memset(buffer->cPlane[j], 0, buf.m.planes[j].length);
        buffer->iNumPlanes++;
      }
    }
    buffer->iIndex = i;

    if(queue)
    {
      QueueBuffer(device, type, memory, buffer);
    }
  }

  return true;
}

V4L2Buffer * CV4L2::FreeBuffers(int count, V4L2Buffer *v4l2Buffers)
{
  int i, j;

  if(v4l2Buffers != NULL)
  {
    for(i = 0; i < count; i++)
    {
      V4L2Buffer *buffer = &v4l2Buffers[i];

      for (j = 0; j < buffer->iNumPlanes; j++)
      {
        if(buffer->cPlane[j] && buffer->cPlane[j] != MAP_FAILED)
        {
          munmap(buffer->cPlane[j], buffer->iSize[j]);
          CLog::Log(LOGDEBUG, "%s::%s - unmap convert buffer", CLASSNAME, __func__);
        }
      }
    }
    free(v4l2Buffers);
  }
  return NULL;
}

int CV4L2::DequeueBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, double *dequeuedTimestamp)
{
  struct v4l2_buffer buf;
  struct v4l2_plane  planes[V4L2_NUM_MAX_PLANES];

  if(device < 0)
  {
    return -1;
  }

  memset(&planes, 0, sizeof(struct v4l2_plane) * V4L2_NUM_MAX_PLANES);
  memset(&buf, 0, sizeof(struct v4l2_buffer));
  buf.type     = type;
  buf.memory   = memory;
  buf.m.planes = planes;
  buf.length   = V4L2_NUM_MAX_PLANES;

  auto ret = ioctl(device, VIDIOC_DQBUF, &buf);
  if (ret < 0)
  {
    if (errno != EAGAIN)
    {
      CLog::Log(LOGERROR, "%s::%s - Dequeue buffer", CLASSNAME, __func__);
    }
    return -1;
  }

  if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
  {
    long pts[2] = { buf.timestamp.tv_sec, buf.timestamp.tv_usec };
    *dequeuedTimestamp = *((double*)&pts[0]);;
  }
  return buf.index;
}

int CV4L2::QueueBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, struct V4L2Buffer *buffer)
{
  struct v4l2_buffer buf;
  struct v4l2_plane  planes[buffer->iNumPlanes];

  if(!buffer || device < 0)
  {
    return -1;
  }

  memset(&buf, 0, sizeof(struct v4l2_buffer));
  buf.type     = type;
  buf.memory   = memory;
  buf.index    = buffer->iIndex;
  buf.m.planes = planes;
  buf.length   = buffer->iNumPlanes;

  if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
  {
    buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
    long* pts = (long*)&buffer->timestamp;
    buf.timestamp.tv_sec = pts[0];
    buf.timestamp.tv_usec = pts[1];
  }

  memset(&planes, 0, sizeof(struct v4l2_plane) * buffer->iNumPlanes);

  for (int i = 0; i < buffer->iNumPlanes; i++)
  {
    planes[i].m.userptr = (unsigned long)buffer->cPlane[i];
    planes[i].length = buffer->iSize[i];
    planes[i].bytesused = buffer->iBytesUsed[i];
  }

  auto ret = ioctl(device, VIDIOC_QBUF, &buf);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - Queue buffer", CLASSNAME, __func__);
    return -1;
  }

  buffer->bQueue = true;

  return buf.index;
}

int CV4L2::PollInput(int device, int timeout)
{
  struct pollfd p;
  p.fd = device;
  p.events = POLLIN | POLLERR;

  auto ret = poll(&p, 1, timeout);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - Polling input", CLASSNAME, __func__);
    return -1;
  }
  else if (ret == 0)
  {
    // todo ???
    return 1;
  }

  return 2;
}

int CV4L2::PollOutput(int device, int timeout)
{
  struct pollfd p;
  p.fd = device;
  p.events = POLLOUT | POLLERR;

  auto ret = poll(&p, 1, timeout);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - Polling output", CLASSNAME, __func__);
    return -1;
  }
  else if (ret == 0)
  {
    // todo ???
    return 1;
  }

  return 2;
}

int CV4L2::SetControllValue(int device, int id, int value)
{
  struct v4l2_control control;

  control.id = id;
  control.value = value;

  auto ret = ioctl(device, VIDIOC_S_CTRL, &control);

  if(ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - Set control if %d value %d\n", CLASSNAME, __func__, id, value);
    return -1;
  }

  return 0;
}
