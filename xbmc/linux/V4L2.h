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

#include <cv4l-helpers.h>

#define V4L2_CAPTURE_BUFFERS_COUNT 10
#define V4L2_OUTPUT_BUFFERS_COUNT 10

class CV4L2
{
public:
  CV4L2();
  virtual ~CV4L2();

  static bool        MmapBuffers(cv4l_fd *fd, int count, cv4l_queue *buffers, enum v4l2_buf_type type);
  static void        FreeBuffers(cv4l_fd *fd, cv4l_queue *buffers);

  static int         DequeueBuffer(cv4l_fd *fd, cv4l_buffer buffer); //, timeval timestamp);
  static int         QueueBuffer(cv4l_fd *fd, cv4l_buffer buffer);

  static int         PollInput(int device, int timeout);
  static int         PollOutput(int device, int timeout);
  static int         SetControlValue(cv4l_fd *fd, int id, int value);
};
