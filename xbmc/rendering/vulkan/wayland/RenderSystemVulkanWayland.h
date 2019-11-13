/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#define VK_USE_PLATFORM_WAYLAND_KHR
#include "rendering/vulkan/RenderSystemVulkan.h"

struct wl_display;
struct wl_surface;

namespace KODI
{
namespace RENDERING
{
namespace VULKAN
{

class CRenderSystemVulkanWayland : public CRenderSystemVulkan
{
public:
  CRenderSystemVulkanWayland();
  ~CRenderSystemVulkanWayland() override = default;

protected:
  bool CreateSurface(wl_display* display, wl_surface* surface);
  void DestroySurface();

private:
  vk::UniqueSurfaceKHR m_vulkanSurface;
};

} // namespace VULKAN
} // namespace RENDERING
} // namespace KODI
