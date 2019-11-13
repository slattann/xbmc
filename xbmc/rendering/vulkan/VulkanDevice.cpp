/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VulkanDevice.h"

#include "utils/log.h"

using namespace KODI::RENDERING::VULKAN;

bool CVulkanDevice::CreateLogicalDevice(vk::PhysicalDevice* physicalDevice)
{
  m_properties = physicalDevice->getProperties();
  m_features = physicalDevice->getFeatures();
  m_memoryProperties = physicalDevice->getMemoryProperties();
  m_queueFamilyProperties = physicalDevice->getQueueFamilyProperties();
  m_extensionProperties = physicalDevice->enumerateDeviceExtensionProperties();

  CLog::Log(LOGDEBUG, "[VULKAN] apiVersion {}", m_properties.apiVersion);
  CLog::Log(LOGDEBUG, "[VULKAN] driverVersion {}", m_properties.driverVersion);
  CLog::Log(LOGDEBUG, "[VULKAN] vendorID {}", m_properties.vendorID);
  CLog::Log(LOGDEBUG, "[VULKAN] deviceID {}", m_properties.deviceID);
  CLog::Log(LOGDEBUG, "[VULKAN] deviceType {}", vk::to_string(m_properties.deviceType));
  CLog::Log(LOGDEBUG, "[VULKAN] devicename {}", m_properties.deviceName);
  /*
  uint8_t pipelineCacheUUID[VK_UUID_SIZE];
  PhysicalDeviceLimits limits;
  PhysicalDeviceSparseProperties sparseProperties;
  */

  for (const auto& extension : m_extensionProperties)
  {
    m_supportedExtensions.emplace_back(extension.extensionName);
  }

  size_t graphicsQueueFamilyIndex =
      std::distance(m_queueFamilyProperties.begin(),
                    std::find_if(m_queueFamilyProperties.begin(), m_queueFamilyProperties.end(),
                                 [](vk::QueueFamilyProperties const& qfp) {
                                   return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                                 }));

  vk::DeviceQueueCreateInfo queueInfo{};
  queueInfo.setQueueFamilyIndex(static_cast<uint32_t>(graphicsQueueFamilyIndex));
  queueInfo.setQueueCount(1);

  const float queuePriority{0.0f};
  queueInfo.setPQueuePriorities(&queuePriority);

  vk::DeviceCreateInfo deviceCreateInfo{};
  deviceCreateInfo.setQueueCreateInfoCount(1);
  deviceCreateInfo.setPQueueCreateInfos(&queueInfo);
  // deviceCreateInfo.setPEnabledFeatures()

  // Create the logical device representation
  std::vector<const char*> deviceExtensions;

  // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
  if (ExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
  {
    deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
  }

  if (deviceExtensions.size() > 0)
  {
    deviceCreateInfo.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions.size()));
    deviceCreateInfo.setPpEnabledExtensionNames(deviceExtensions.data());
  }

  m_device = physicalDevice->createDeviceUnique(deviceCreateInfo);

  return true;
  // vk::CommandPoolCreateInfo commandPoolCreateInfo{};
  // commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
  // commandPoolCreateInfo.setQueueFamilyIndex(static_cast<uint32_t>(graphicsQueueFamilyIndex));

  // m_commandPool = m_device.createCommandPool(commandPoolCreateInfo);

  // m_enabledFeatures = enabledFeatures;
}

bool CVulkanDevice::ExtensionSupported(std::string extension)
{
  return (std::find(m_supportedExtensions.begin(), m_supportedExtensions.end(), extension) !=
          m_supportedExtensions.end());
}
