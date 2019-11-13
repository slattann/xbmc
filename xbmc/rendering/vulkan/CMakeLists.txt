set(SOURCES RenderSystemVulkan.cpp
            VulkanDebug.cpp
            VulkanDevice.cpp)

set(HEADERS RenderSystemVulkan.h
            VulkanDebug.h
            VulkanDevice.h)

core_add_library(rendering_vulkan)
