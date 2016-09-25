#.rst:
# FindIMX
# -------
# Finds the IMX codec
#
# This will will define the following variables::
#
# IMX_FOUND - system has IMX
# IMX_INCLUDE_DIRS - the IMX include directory
# IMX_DEFINITIONS - the IMX definitions
# IMX_LIBRARIES - the IMX libraries

if(PKG_CONFIG_FOUND)
  pkg_check_modules(IMX fslvpuwrap QUIET)
endif()

if(NOT IMX_FOUND)
  find_path(IMX_INCLUDE_DIR vpu_wrapper.h
                            PATH_SUFFIXES imx-mm/vpu)

  find_library(FSLVPUWRAP_LIBRARY fslvpuwrap)
  find_library(VPU_LIBRARY vpu)
  find_library(G2D_LIBRARY g2d)

  set(IMX_LIBRARIES ${FSLVPUWRAP_LIBRARY} ${VPU_LIBRARY} ${G2D_LIBRARY}
      CACHE STRING "imx libraries" FORCE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IMX
                                  REQUIRED_VARS IMX_INCLUDE_DIR IMX_LIBRARIES)

list(APPEND IMXVPU_DEFINITIONS -DHAS_IMXVPU=1 -DLINUX -DEGL_API_FB)

mark_as_advanced(IMX_INCLUDE_DIR IMX_LIBRARIES IMX_DEFINITIONS)
