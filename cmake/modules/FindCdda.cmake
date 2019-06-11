#.rst:
# FindCDDA
# --------
# Finds the CDDA library
#
# This will define the following variables::
#
# CDDA_FOUND - system has CDDA
# CDDA_INCLUDE_DIRS - the CDDA include directory
# CDDA_LIBRARIES - the CDDA libraries
#
# and the following imported targets::
#
#   CDDA::CDDA - The CDDA library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_CDDA libcdio_cdda QUIET)
  pkg_check_modules(PC_PARANOIA libcdio_paranoia QUIET)
endif()

find_path(CDDA_INCLUDE_DIR NAMES cdio/cdda.h
                           PATHS ${PC_CDDA_libcdio_cdda_INCLUDEDIR})

find_library(CDDA_LIBRARY NAMES cdio_cdda
                          PATHS ${PC_CDDA_libcdio_cdda_LIBDIR})

find_path(PARANOIA_INCLUDE_DIR NAMES cdio/paranoia.h
                               PATHS ${PC_PARANOIA_libcdio_paranoia_INCLUDEDIR})

find_library(PARANOIA_LIBRARY NAMES cdio_paranoia
                              PATHS ${PC_PARANOIA_libcdio_paranoia_LIBDIR})

set(CDDA_VERSION ${PC_CDDA_libcdio_cdda_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Cdda
                                  REQUIRED_VARS CDDA_LIBRARY CDDA_INCLUDE_DIR PARANOIA_LIBRARY PARANOIA_INCLUDE_DIR
                                  VERSION_VAR CDDA_VERSION)

if(CDDA_FOUND)
  set(CDDA_LIBRARIES ${CDDA_LIBRARY} ${PARANOIA_LIBRARY})
  set(CDDA_INCLUDE_DIRS ${CDDA_INCLUDE_DIR} ${PARANOIA_INCLUDE_DIR})

  if(NOT TARGET CDDA::CDDA)
    add_library(CDDA::CDDA UNKNOWN IMPORTED)
    set_target_properties(CDDA::CDDA PROPERTIES
                                     IMPORTED_LOCATION "${CDDA_LIBRARY}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${CDDA_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(CDDA_INCLUDE_DIR CDDA_LIBRARY PARANOIA_INCLUDE_DIR PARANOIA_LIBRARY)
