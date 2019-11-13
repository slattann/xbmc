if (VULKAN_FOUND)
  set(SOURCES RenderSystemVulkanWayland.cpp)
  set(HEADERS RenderSystemVulkanWayland.h)
endif()

core_add_library(rendering_vulkan_wayland)
