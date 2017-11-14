set(SOURCES Peripheral.cpp
            PeripheralBluetooth.cpp
            PeripheralDisk.cpp
            PeripheralHID.cpp
            PeripheralImon.cpp
            PeripheralJoystick.cpp
            PeripheralJoystickEmulation.cpp
            PeripheralNIC.cpp
            PeripheralNyxboard.cpp
            PeripheralTuner.cpp)

set(HEADERS Peripheral.h
            PeripheralBluetooth.h
            PeripheralDisk.h
            PeripheralHID.h
            PeripheralImon.h
            PeripheralJoystick.h
            PeripheralJoystickEmulation.h
            PeripheralNIC.h
            PeripheralNyxboard.h
            PeripheralTuner.h)

if(CECFRAMEWORK_FOUND)
  list(APPEND SOURCES PeripheralCecFrameworkAdapter.cpp)
  list(APPEND HEADERS PeripheralCecFrameworkAdapter.h)
elseif(CEC_FOUND)
  list(APPEND SOURCES PeripheralCecAdapter.cpp)
  list(APPEND HEADERS PeripheralCecAdapter.h)
endif()

core_add_library(peripherals_devices)
