/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RenderSystemVulkanWayland.h"

#include "utils/log.h"

#include <vulkan/vulkan_wayland.h>

using namespace KODI::RENDERING::VULKAN;

CRenderSystemVulkanWayland::CRenderSystemVulkanWayland() : CRenderSystemVulkan()
{
  m_instanceExtensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
  m_instanceExtensions[1] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
}

bool CRenderSystemVulkanWayland::CreateSurface(wl_display* display, wl_surface* surface)
{
  try
  {
    m_vulkanSurface = m_instance->createWaylandSurfaceKHRUnique(
        vk::WaylandSurfaceCreateInfoKHR({}, display, surface)); //, nullptr, &m_dynamicLoader);
  }
  catch (vk::SystemError& err)
  {
    CLog::LogF(LOGERROR, "vk::SystemError: {}", err.what());
    return false;
  }
  catch (std::runtime_error& err)
  {
    CLog::LogF(LOGERROR, "std::runtime_error: {}", err.what());
    return false;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "unknown error");
    return false;
  }

  return true;
}

void CRenderSystemVulkanWayland::DestroySurface()
{
  try
  {
    m_vulkanSurface.reset();
  }
  catch (vk::SystemError& err)
  {
    CLog::LogF(LOGERROR, "vk::SystemError: {}", err.what());
  }
  catch (std::runtime_error& err)
  {
    CLog::LogF(LOGERROR, "std::runtime_error: {}", err.what());
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "unknown error");
  }
}
