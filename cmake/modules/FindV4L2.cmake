#.rst:
# FindV4L2
# -------
# Finds the V4L2 header
#
# This will will define the following variables::
#
# V4L2_FOUND - system has V4L2
# V4L2_INCLUDE_DIRS - the V4L2 include directory
# V4L2_DEFINITIONS - the V4L2 definitions
#

find_path(V4L2_INCLUDE_DIR NAMES videodev2.h
                           PATH_SUFFIXES linux)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(V4L2
                                  REQUIRED_VARS V4L2_INCLUDE_DIR)

if(V4L2_FOUND)
  set(V4L2_DEFINITIONS -DHAS_V4L2=1)
endif()
