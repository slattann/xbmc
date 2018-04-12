#.rst:
# FindEGL
# -------
# Finds the EGL library
#
# This will will define the following variables::
#
# EGL_FOUND - system has EGL
# EGL_INCLUDE_DIRS - the EGL include directory
# EGL_LIBRARIES - the EGL libraries
# EGL_DEFINITIONS - the EGL definitions
#
# and the following imported targets::
#
#   EGL::EGL   - The EGL library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_EGLSTREAMS egl QUIET)
endif()

find_path(EGLSTREAMS_INCLUDE_DIR EGL/egl.h
                                 PATHS ${PC_EGLSTREAMS_INCLUDEDIR})

find_library(EGLSTREAMS_LIBRARY NAMES EGL egl
                                PATHS ${PC_EGLSTREAMS_LIBDIR})

set(EGLSTREAMS_VERSION ${PC_EGLSTREAMS_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EGLSTREAMS
                                  REQUIRED_VARS EGLSTREAMS_LIBRARY EGLSTREAMS_INCLUDE_DIR
                                  VERSION_VAR EGLSTREAMS_VERSION)
