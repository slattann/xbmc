#.rst:
# FindVulkan
# ------------
# Finds the VULKAN library
#
# This will define the following variables::
#
# VULKAN_FOUND - system has Vulkan
# VULKAN_INCLUDE_DIRS - the Vulkan include directory
# VULKAN_LIBRARIES - the Vulkan libraries
# VULKAN_DEFINITIONS - the Vulkan definitions

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_VULKAN vulkan>=1.1.114 QUIET)
endif()

find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.hpp
                             PATHS "$ENV{VULKAN_SDK}/include"
                                   ${PC_VULKAN_INCLUDEDIR})

find_library(VULKAN_LIBRARY NAMES vulkan
                            PATHS "$ENV{VULKAN_SDK}/lib"
                                  ${PC_VULKAN_LIBDIR})

set(VULKAN_VERSION ${PC_VULKAN_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vulkan
                                  REQUIRED_VARS VULKAN_LIBRARY VULKAN_INCLUDE_DIR VULKAN_VERSION)

if(VULKAN_FOUND)
  set(VULKAN_LIBRARIES ${VULKAN_LIBRARY})
  set(VULKAN_INCLUDE_DIRS ${VULKAN_INCLUDE_DIR})
  set(VULKAN_DEFINITIONS -DHAS_VULKAN=1)
endif()

mark_as_advanced(VULKAN_INCLUDE_DIR VULKAN_LIBRARY)
