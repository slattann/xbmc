/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include <rfb/rfb.h>

class CVNCServer
{
public:
  CVNCServer(int maxWidth, int maxHeight);
  ~CVNCServer();

  void UpdateFrameBuffer(char *buffer);

private:
  rfbScreenInfoPtr m_screen{nullptr};
  int m_width = 0;
  int m_height = 0;
};
