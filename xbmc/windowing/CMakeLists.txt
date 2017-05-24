set(SOURCES WinEvents.cpp
            WinSystem.cpp)

set(HEADERS WindowingFactory.h
            WinEvents.h
            WinSystem.h
            XBMC_events.h
            VideoSync.h)

if((CORE_SYSTEM_NAME STREQUAL linux OR CORE_SYSTEM_NAME STREQUAL rbpi) AND NOT MIR_FOUND)
  list(APPEND SOURCES WinEventsLinux.cpp)
  list(APPEND HEADERS WinEventsLinux.h)
endif()

if(CORE_PLATFORM_NAME STREQUAL X11)
  add_subdirectory(X11)
elseif(CORE_PLATFORM_NAME STREQUAL MIR)
  add_subdirectory(mir)
elseif(CORE_PLATFORM_NAME STREQUAL AML)
  add_subdirectory(amlogic)
endif()

core_add_library(windowing)
