/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CVulkanDevice
{
public:
  CVulkanDevice() = default;
  ~CVulkanDevice() = default;

  bool CreateLogicalDevice(vk::PhysicalDevice* physicalDevice);
  bool ExtensionSupported(std::string extension);

private:
  vk::UniqueDevice m_device;
  vk::PhysicalDeviceProperties m_properties;
  vk::PhysicalDeviceFeatures m_features;
  vk::PhysicalDeviceFeatures m_enabledFeatures;
  vk::PhysicalDeviceMemoryProperties m_memoryProperties;
  std::vector<vk::QueueFamilyProperties> m_queueFamilyProperties;
  std::vector<vk::ExtensionProperties> m_extensionProperties;
  std::vector<std::string> m_supportedExtensions;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
