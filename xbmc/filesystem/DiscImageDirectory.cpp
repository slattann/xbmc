/*
 *  Copyright (C) 2010 Team Boxee
 *      http://www.boxee.tv
 *
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DiscImageDirectory.h"

#include "filesystem/ISO9660Directory.h"
#include "filesystem/UDFDirectory.h"

using namespace XFILE;

IFileDirectory* CDiscImageDirectory::GetDiscImageDirectory(const CURL& url)
{
  CISO9660Directory iso;
  if (iso.Exists(url))
    return new CISO9660Directory();

  CUDFDirectory udf;
  if (udf.Exists(url))
    return new CUDFDirectory();

  return nullptr;
}
