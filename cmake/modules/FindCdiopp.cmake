#.rst:
# FindCdio
# --------
# Finds the cdio++ library
#
# This will define the following variables::
#
# CDIOPP_FOUND - system has cdio++
# CDIOPP_INCLUDE_DIRS - the cdio++ include directory
# CDIOPP_LIBRARIES - the cdio++ libraries
#
# and the following imported targets::
#
#   CDIOPP::CDIOPP - The cdio++ library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_CDIOPP libcdio++>=0.94 QUIET)
  pkg_check_modules(PC_ISO9660PP libiso9660++>=0.94 QUIET)
endif()

find_path(CDIOPP_INCLUDE_DIR NAMES cdio++/cdio.hpp
                             PATHS ${PC_CDIOPP_libcdio++_INCLUDEDIR})

find_library(CDIOPP_LIBRARY NAMES libcdio++ cdio++
                            PATHS ${PC_CDIOPP_libcdio++_LIBDIR})

find_path(ISO9660PP_INCLUDE_DIR NAMES cdio++/iso9660.hpp
                                PATHS ${PC_CDIOPP_libiso9660++_INCLUDEDIR})

find_library(ISO9660PP_LIBRARY NAMES libiso9660++ iso9660++
                               PATHS ${PC_CDIOPP_libiso9660++_LIBDIR})

set(CDIOPP_VERSION ${PC_CDIOPP_libcdio++_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cdiopp
                                  REQUIRED_VARS CDIOPP_LIBRARY CDIOPP_INCLUDE_DIR ISO9660PP_INCLUDE_DIR ISO9660PP_LIBRARY
                                  VERSION_VAR CDIOPP_VERSION)

if(CDIOPP_FOUND)
  set(CDIOPP_LIBRARIES ${CDIOPP_LIBRARY} ${ISO9660PP_LIBRARY})
  set(CDIOPP_INCLUDE_DIRS ${CDIOPP_INCLUDE_DIR} ${ISO9660PP_INCLUDE_DIR})

  if(NOT TARGET CDIOPP::CDIOPP)
    add_library(CDIOPP::CDIOPP UNKNOWN IMPORTED)
    set_target_properties(CDIOPP::CDIOPP PROPERTIES
                                     IMPORTED_LOCATION "${CDIOPP_LIBRARY}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${CDIOPP_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(CDIOPP_INCLUDE_DIR CDIOPP_LIBRARY ISO9660PP_INCLUDE_DIR ISO9660PP_LIBRARY)
