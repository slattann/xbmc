/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SessionUtils.h"

#include <fcntl.h>
#include <unistd.h>

uint32_t CSessionUtils::Open(std::string path, int flags)
{
  return open(path.c_str(), O_RDWR | flags);
}

void CSessionUtils::Close(int fd)
{
  close(fd);
}
