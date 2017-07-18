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

#include <poll.h>

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

bool CV4L2::RequestBuffers(cv4l_fd *fd, int count, cv4l_queue *buffers, int type, int memory)
{
  buffers->init(type, memory);

  auto ret = buffers->reqbufs(fd, count);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error requesting buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  CLog::Log(LOGDEBUG, "%s::%s - got %d of requested %d", CLASSNAME, __func__, buffers->g_buffers(), V4L2_OUTPUT_BUFFERS_COUNT);
  return true;
}

bool CV4L2::MmapBuffers(cv4l_fd *fd, int count, cv4l_queue *buffers, int type)
{
  if (!RequestBuffers(fd, count, buffers, type, V4L2_MEMORY_MMAP))
  {
    return false;
  }

  auto ret = buffers->mmap_bufs(fd);
  if(ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error mmapping buffer: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  return true;
}

void CV4L2::FreeBuffers(cv4l_fd *fd, cv4l_queue *buffers)
{
  auto ret = buffers->munmap_bufs(fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error munmaping buffer: %s", CLASSNAME, __func__, strerror(errno));
  }
}

int CV4L2::DequeueBuffer(cv4l_fd *fd, cv4l_buffer buffer, timeval *timestamp)
{
  *timestamp = buffer.g_timestamp();
  auto ret = fd->dqbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error dequeuing buffer: %s", CLASSNAME, __func__, strerror(errno));
  }

  return ret;
}

int CV4L2::QueueBuffer(cv4l_fd *fd, cv4l_buffer buffer)
{
  buffer.or_flags(V4L2_BUF_FLAG_TIMESTAMP_COPY);
  auto ret = fd->qbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error queuing buffer: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  return ret;
}

int CV4L2::PollInput(int fd, int timeout)
{
  int ret;
  struct pollfd p;
  p.fd = fd;
  p.events = POLLIN;
  p.revents = 0;

  while (true)
  {
    ret = poll(&p, 1, timeout);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error polling input: %s", CLASSNAME, __func__, strerror(errno));
      return -1;
    }
    else if (p.revents & (POLLHUP | POLLERR))
    {
      CLog::Log(LOGERROR, "%s::%s - hangup polling input: %s", CLASSNAME, __func__, strerror(errno));
      return 0;
    }
    else if(p.revents & POLLIN)
    {
      break;
    }
  }

  return ret;
}

int CV4L2::PollOutput(int fd, int timeout)
{
  int ret;
  struct pollfd p;
  p.fd = fd;
  p.events = POLLOUT;
  p.revents = 0;

  while (true)
  {
    ret = poll(&p, 1, timeout);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "%s::%s - error polling output: %s", CLASSNAME, __func__, strerror(errno));
      return -1;
    }
    else if (p.revents & (POLLHUP | POLLERR))
    {
      CLog::Log(LOGERROR, "%s::%s - hangup polling output: %s", CLASSNAME, __func__, strerror(errno));
      return 0;
    }
    else if(p.revents & POLLOUT)
    {
      break;
    }
  }

  return ret;
}
