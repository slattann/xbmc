/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "VulkanDebug.h"
#include "VulkanDevice.h"
#include "rendering/RenderSystem.h"
#include "utils/Color.h"
#include "vulkan/vulkan.hpp"

#include <array>

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CRenderSystemVulkan : public CRenderSystemBase
{
public:
  CRenderSystemVulkan();
  virtual ~CRenderSystemVulkan() = default;

  bool InitRenderSystem() override;
  bool DestroyRenderSystem() override;
  bool ResetRenderSystem(int width, int height) override { return false; }

  bool BeginRender() override { return false; }
  bool EndRender() override { return false; }

  bool ClearBuffers(::UTILS::Color color) override { return false; }
  bool IsExtSupported(const char* extension) const override { return false; }

  void SetViewPort(const CRect& viewPort) override {}
  void GetViewPort(CRect& viewPort) override {}

  void SetScissors(const CRect& rect) override {}
  void ResetScissors() override {}

  void CaptureStateBlock() override {}
  void ApplyStateBlock() override {}

  void SetCameraPosition(const CPoint& camera,
                         int screenWidth,
                         int screenHeight,
                         float stereoFactor = 0.f) override
  {
  }

protected:
  int m_width;
  int m_height;

  std::array<const char*, 3> m_instanceExtensions;

  vk::UniqueInstance m_instance;

private:
  bool m_initialized{false};

  CVulkanDevice m_vulkanDevice;
  CVulkanDebug m_debug;

  vk::UniqueDebugUtilsMessengerEXT m_debugUtilsMessenger;

  vk::PhysicalDevice m_physicalDevice;
  std::vector<vk::PhysicalDevice> m_physicalDevices;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
