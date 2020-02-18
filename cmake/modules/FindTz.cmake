#.rst:
# FindTz
# -------
# Finds the TZ library
#
# This will define the following variables::
#
# TZ_FOUND - system has TZ
# TZ_INCLUDE_DIRS - the TZ include directory
# TZ_LIBRARIES - the TZ libraries

find_path(TZ_INCLUDE_DIR date/tz.h)

find_library(TZ_LIBRARY NAMES tz)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tz
                                  REQUIRED_VARS TZ_LIBRARY TZ_INCLUDE_DIR)

if(TZ_FOUND)
  set(TZ_LIBRARIES ${TZ_LIBRARY})
  set(TZ_INCLUDE_DIRS ${TZ_INCLUDE_DIR})
endif()

mark_as_advanced(TZ_INCLUDE_DIR TZ_LIBRARY)
