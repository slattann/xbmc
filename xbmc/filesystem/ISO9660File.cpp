/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ISO9660File.h"

#include "URL.h"

#include <cmath>

using namespace XFILE;

CISO9660File::CISO9660File()
  : m_iso(new ISO9660::IFS())
{
}

bool CISO9660File::Open(const CURL& url)
{
  if (m_iso && m_stat)
    return true;

  if (!m_iso->open(url.GetHostName().c_str()))
    return false;

  m_stat.reset(m_iso->stat(url.GetFileName().c_str()));

  if (!m_stat)
    return false;

  if (!m_stat->p_stat)
    return false;

  return true;
}

int CISO9660File::Stat(const CURL& url, struct __stat64* buffer)
{
  if (!m_iso)
    return -1;

  if (!m_stat)
    return -1;

  if (!m_stat->p_stat)
    return -1;

  memset(buffer, 0, sizeof(struct stat64));
  buffer->st_size = m_stat->p_stat->size;

  switch (m_stat->p_stat->type)
  {
  case 2:
    buffer->st_mode = S_IFDIR;
    break;
  case 1:
  default:
    buffer->st_mode = S_IFREG;
    break;
  }

  return 0;
}

ssize_t CISO9660File::Read(void* buffer, size_t size)
{
  const int blocks = std::ceil(size / ISO_BLOCKSIZE);
  const lsn_t lsn = m_stat->p_stat->lsn;
  int block;
  for (block = 0; block < blocks; block++)
  {
    if (m_iso->seek_read(buffer, lsn + m_offset + block, 1) != ISO_BLOCKSIZE)
      break;
  }

  m_offset += block;

  return block * ISO_BLOCKSIZE;
}

int64_t CISO9660File::Seek(int64_t filePosition, int whence)
{
  int block = std::floor(filePosition / ISO_BLOCKSIZE);

  switch (whence)
  {
  case SEEK_SET:
    m_offset = block;
    break;
  case SEEK_CUR:
    m_offset += block;
    break;
  case SEEK_END:
    m_offset = std::ceil(GetLength() / ISO_BLOCKSIZE) + block;
    break;
  }

  return m_offset * ISO_BLOCKSIZE;
}

int64_t CISO9660File::GetLength()
{
  return m_stat->p_stat->size;
}

int64_t CISO9660File::GetPosition()
{
  return m_offset * ISO_BLOCKSIZE;
}

bool CISO9660File::Exists(const CURL& url)
{
  return Open(url);
}

int CISO9660File::GetChunkSize()
{
  return ISO_BLOCKSIZE;
}
