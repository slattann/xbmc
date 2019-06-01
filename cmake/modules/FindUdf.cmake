#.rst:
# FindCdio
# --------
# Finds the udf library
#
# This will define the following variables::
#
# UDF_FOUND - system has udf
# UDF_INCLUDE_DIRS - the udf include directory
# UDF_LIBRARIES - the udf libraries
#
# and the following imported targets::
#
#   UDF::UDF - The udf library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_UDF libudf>=0.94 QUIET)
endif()

find_path(UDF_INCLUDE_DIR NAMES cdio/udf.h
                          PATHS ${PC_UDF_libudf_INCLUDEDIR})

find_library(UDF_LIBRARY NAMES udf libudf
                         PATHS ${PC_UDF_libudf_LIBDIR})

set(UDF_VERSION ${PC_UDF_libudf_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Udf
                                  REQUIRED_VARS UDF_LIBRARY UDF_INCLUDE_DIR
                                  VERSION_VAR UDF_VERSION)

if(UDF_FOUND)
  set(UDF_LIBRARIES ${UDF_LIBRARY})
  set(UDF_INCLUDE_DIRS ${UDF_INCLUDE_DIR})

  if(NOT TARGET UDF::UDF)
    add_library(UDF::UDF UNKNOWN IMPORTED)
    set_target_properties(UDF::UDF PROPERTIES
                                   IMPORTED_LOCATION "${UDF_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${UDF_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(UDF_INCLUDE_DIR UDF_LIBRARY)
