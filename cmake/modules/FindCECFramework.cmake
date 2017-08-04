#.rst:
# FindCECFramework
# -------
# Finds the CEC Framework HEADER
#
# This will will define the following variables::
#
# CECFRAMEWORK_FOUND - system has CEC Framework
# CECFRAMEWORK_INCLUDE_DIRS - the CEC Framework include directory
# CECFRAMEWORK_DEFINITIONS - the CEC Framework definitions
#

find_path(CECFRAMEWORK_INCLUDE_DIR NAMES linux/cec.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CECFRAMEWORK
                                  REQUIRED_VARS CECFRAMEWORK_INCLUDE_DIR)

if(CECFRAMEWORK_INCLUDE_DIR)
  set(CECFRAMEWORK_FOUND 1)
  set(CECFRAMEWORK_INCLUDE_DIRS ${CECFRAMEWORK_INCLUDE_DIR})
  set(CECFRAMEWORK_DEFINITIONS -DHAVE_CECFRAMEWORK=1)
endif()
