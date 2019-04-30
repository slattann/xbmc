/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace KODI
{
namespace UTILS
{

class CEDIDUtils
{
public:
  CEDIDUtils(std::vector<uint8_t> edid);
  ~CEDIDUtils() = default;

  bool SupportsColorSpace(int colorSpace);
  bool SupportsEOTF(int eotf);

  void LogInfo();

private:
  const uint8_t* FindCEAExtentionBlock(std::vector<uint8_t> data);
  std::vector<uint8_t> FindExtendedDataBlock(uint32_t blockTag);

  std::vector<uint8_t> m_edid;
};

}
}
