/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "BufferObjectFactory.h"

std::vector<std::function<std::unique_ptr<CBufferObject>()>> CBufferObjectFactory::m_bufferObjects;

std::unique_ptr<CBufferObject> CBufferObjectFactory::CreateBufferObject()
{
  return m_bufferObjects.back()();
}

void CBufferObjectFactory::RegisterBufferObject(
    std::function<std::unique_ptr<CBufferObject>()> createFunc)
{
  m_bufferObjects.emplace_back(createFunc);
}

void CBufferObjectFactory::ClearBufferObjects()
{
  m_bufferObjects.clear();
}
