set(SOURCES PeripheralBusAddon.cpp
            PeripheralBusApplication.cpp)

set(HEADERS PeripheralBusAddon.h
            PeripheralBusApplication.h)

if(CECFRAMEWORK_FOUND)
  list(APPEND SOURCES PeripheralBusCECFramework.cpp)
  list(APPEND HEADERS PeripheralBusCECFramework.h)
elseif(CEC_FOUND)
  list(APPEND SOURCES PeripheralBusCEC.cpp)
  list(APPEND HEADERS PeripheralBusCEC.h)
endif()

core_add_library(peripheral_bus_virtual)
