/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "vulkan/vulkan.hpp"

#include <string>

namespace vk
{
// todo: remove after updating vulkan.hpp
using DebugUtilsMessengerEXTCustom = UniqueHandle<DebugUtilsMessengerEXT, DispatchLoaderDynamic>;
} // namespace vk

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CVulkanDebug
{
public:
  CVulkanDebug() = default;
  ~CVulkanDebug() = default;

  void SetupDebugging(vk::UniqueInstance& instance);

private:
  // todo: change to UniqueHandle type after updating vulkan.hpp
  vk::DebugUtilsMessengerEXTCustom m_debug;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
