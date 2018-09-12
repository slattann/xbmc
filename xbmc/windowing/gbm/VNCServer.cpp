/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VNCServer.h"

#include "utils/log.h"

CVNCServer::CVNCServer(int maxWidth, int maxHeight)
{
  m_width = maxWidth;
  m_height = maxHeight;

  m_screen = rfbGetScreen(nullptr, nullptr, m_width, m_height, 8, 3, 4);
  if (!m_screen)
    CLog::Log(LOGERROR, "CVNCServer::%s - failed to call rfbGetScreen", __FUNCTION__);

  m_screen->desktopName = "Kodi";
  m_screen->alwaysShared = TRUE;

  rfbPixelFormat pixelFormat;

  pixelFormat.bitsPerPixel = 32;
  pixelFormat.depth = 32;
  pixelFormat.bigEndian = 0;
  pixelFormat.trueColour = 1;
  pixelFormat.redMax = 255;
  pixelFormat.greenMax = 255;
  pixelFormat.blueMax = 255;
  pixelFormat.redShift = 16;
  pixelFormat.greenShift = 8;
  pixelFormat.blueShift = 0;

  m_screen->httpDir = (char*)"/usr/share/novnc";

  m_screen->serverFormat = pixelFormat;

  rfbInitServer(m_screen);
}

CVNCServer::~CVNCServer()
{
  rfbShutdownServer(m_screen, TRUE);
  rfbScreenCleanup(m_screen);
}

void CVNCServer::UpdateFrameBuffer(char *buffer)
{
  m_screen->frameBuffer = buffer;

  rfbMarkRectAsModified(m_screen, 0, 0, m_width, m_height);
  rfbProcessEvents(m_screen, 0);
}
