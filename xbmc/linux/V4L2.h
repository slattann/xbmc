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

#include <linux/videodev2.h>

#define V4L2_NUM_MAX_PLANES 3

struct V4L2Buffer
{
  int iSize[V4L2_NUM_MAX_PLANES];
  int iOffset[V4L2_NUM_MAX_PLANES];
  int iBytesUsed[V4L2_NUM_MAX_PLANES];
  void *cPlane[V4L2_NUM_MAX_PLANES];
  int iNumPlanes;
  int iIndex;
  bool bQueue;
  double timestamp;
};

class CV4L2
{
public:
  CV4L2();
  virtual ~CV4L2();

  static int         RequestBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, int numBuffers);
  static int         StreamOnOff(int device, enum v4l2_buf_type type, int onoff);
  static bool        MmapBuffers(int device, int count, V4L2Buffer *v4l2Buffers, enum v4l2_buf_type type, enum v4l2_memory memory, bool queue = true);
  static V4L2Buffer *FreeBuffers(int count, V4L2Buffer *v4l2Buffers);

  static int         DequeueBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, double *dequeuedTimestamp);
  static int         QueueBuffer(int device, enum v4l2_buf_type type, enum v4l2_memory memory, V4L2Buffer *buffer);

  static int         PollInput(int device, int timeout);
  static int         PollOutput(int device, int timeout);
  static int         SetControllValue(int device, int id, int value);
};

inline int v4l2_align(int v, int a) {
  return ((v + a - 1) / a) * a;
}
