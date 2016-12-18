#.rst:
# FindSmbClient
# -------------
# Finds the SMB Client library
#
# This will will define the following variables::
#
# SMBCLIENT_FOUND - system has SmbClient
# SMBCLIENT_INCLUDE_DIRS - the SmbClient include directory
# SMBCLIENT_LIBRARIES - the SmbClient libraries
# SMBCLIENT_DEFINITIONS - the SmbClient definitions
#
# and the following imported targets::
#
#   SmbClient::SmbClient   - The SmbClient library

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SMBCLIENT smbclient QUIET)
endif()

find_path(SMBCLIENT_INCLUDE_DIR NAMES libsmbclient.h
                                PATHS ${PC_SMBCLIENT_INCLUDEDIR})
find_library(SMBCLIENT_LIBRARY NAMES smbclient
                               PATHS ${PC_SMBCLIENT_LIBDIR})

# check if smbclient libs are statically linked
set(SMBCLIENT_LIB_TYPE SHARED)
if(PC_SMBCLIENT_STATIC_LDFLAGS)
  set(SMBCLIENT_LDFLAGS ${PC_SMBCLIENT_STATIC_LDFLAGS} CACHE STRING "smbclient linker flags" FORCE)
  set(SMBCLIENT_LIB_TYPE STATIC)
#  string(REGEX REPLACE ";" " " SMBCLIENT_STATIC_LIBRARIES ${PC_SMBCLIENTS_STATIC_LIBRARIES})
#  foreach(_smblib IN LISTS ${SMBCLIENT_STATIC_LIBRARIES})
#    string(TOUPPER ${_smblib}_LIBRARY SMBCLIENT_STATIC_LIBRARIES)
#    find_library(${SMBCLIENT_STATIC_LIBRARIES} ${_smblib})
#  endforeach()

  find_library(TALLOC_LIBRARY talloc)
  find_library(TDB_LIBRARY tdb)
  find_library(TEVENT_LIBRARY tevent)
  find_library(WBCLIENT_LIBRARY wbclient)
  find_library(RESOLV_LIBRARY resolv)

endif()

set(SMBCLIENT_VERSION ${PC_SMBCLIENT_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SmbClient
                                  REQUIRED_VARS SMBCLIENT_LIBRARY SMBCLIENT_INCLUDE_DIR
                                  TALLOC_LIBRARY TDB_LIBRARY TEVENT_LIBRARY WBCLIENT_LIBRARY RESOLV_LIBRARY
                                  VERSION_VAR SMBCLIENT_VERSION)

if(SMBCLIENT_FOUND)
  set(SMBCLIENT_LIBRARIES ${SMBCLIENT_LIBRARY} ${TALLOC_LIBRARY} ${TDB_LIBRARY} ${TEVENT_LIBRARY} ${WBCLIENT_LIBRARY} ${RESOLV_LIBRARY})
  set(SMBCLIENT_INCLUDE_DIRS ${SMBCLIENT_INCLUDE_DIR})
  set(SMBCLIENT_DEFINITIONS -DHAVE_LIBSMBCLIENT=1)

  if(NOT TARGET SmbClient::SmbClient)
    add_library(SmbClient::SmbClient UNKNOWN IMPORTED)
    set_target_properties(SmbClient::SmbClient PROPERTIES
                                   IMPORTED_LOCATION "${SMBCLIENT_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${SMBCLIENT_INCLUDE_DIR}"
                                   INTERFACE_COMPILE_DEFINITIONS HAVE_LIBSMBCLIENT=1)
  endif()
endif()

mark_as_advanced(LIBSMBCLIENT_INCLUDE_DIR LIBSMBCLIENT_LIBRARY)
