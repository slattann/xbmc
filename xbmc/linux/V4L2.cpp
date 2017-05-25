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

//#include <sys/mman.h>
//#include <unistd.h>
//#include <sys/stat.h>
//#include <fcntl.h>
#include <poll.h>
//#include <sys/mman.h>
//#include <linux/media.h>
//#include <errno.h>

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

bool CV4L2::MmapBuffers(cv4l_fd *fd, int count, cv4l_queue *buffers, enum v4l2_buf_type type)
{
  buffers->init(type, V4L2_MEMORY_MMAP);

  auto ret = buffers->reqbufs(fd, count);
  if (ret < 0)
  {
    if (errno == EINVAL)
    {
      CLog::Log(LOGERROR, "%s::%s - video capturing or streaming is not supported", CLASSNAME, __func__);
    }
    else
    {
      CLog::Log(LOGERROR, "%s::%s - error requesting buffers: %s", CLASSNAME, __func__, strerror(errno));
    }

    return false;
  }

  ret = buffers->alloc_bufs(fd);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - cannot allocate memory for buffers: %s", CLASSNAME, __func__, strerror(errno));
    return false;
  }

  ret = buffers->mmap_bufs(fd);
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

int CV4L2::DequeueBuffer(cv4l_fd *fd, cv4l_buffer buffer) //, timeval timestamp)
{
  //buffer.s_timestamp(timestamp);
  auto ret = fd->dqbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error dequeuing buffer: %s", CLASSNAME, __func__, strerror(errno));
  }

  return buffer.g_index();
}

int CV4L2::QueueBuffer(cv4l_fd *fd, cv4l_buffer buffer)
{
  buffer.or_flags(V4L2_BUF_FLAG_TIMESTAMP_COPY);
  auto ret = fd->qbuf(buffer);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error queuing buffer: %s", CLASSNAME, __func__, strerror(errno));
  }

  return buffer.g_index();
}

int CV4L2::PollInput(int fd, int timeout)
{
  struct pollfd p;
  p.fd = fd;
  p.events = POLLIN | POLLERR;

  auto ret = poll(&p, 1, timeout);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error polling input: %s", CLASSNAME, __func__, strerror(errno));
    return -1;
  }
  else if (ret == 0)
  {
    return 1;
  }

  return 2;
}

int CV4L2::PollOutput(int fd, int timeout)
{
  struct pollfd p;
  p.fd = fd;
  p.events = POLLOUT | POLLERR;

  auto ret = poll(&p, 1, timeout);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - error polling output: %s", CLASSNAME, __func__, strerror(errno));
    return -1;
  }
  else if (ret == 0)
  {
    return 1;
  }

  return 2;
}

int CV4L2::SetControlValue(cv4l_fd *fd, int id, int value)
{
  struct v4l2_control control;

  control.id = id;
  control.value = value;

  auto ret = fd->s_ctrl(control);
  if(ret < 0)
  {
    CLog::Log(LOGERROR, "%s::%s - Set control if %d value %d\n", CLASSNAME, __func__, id, value);
    return -1;
  }

  return 0;
}
