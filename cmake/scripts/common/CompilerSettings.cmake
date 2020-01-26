# Languages and global compiler settings
if(NOT DEFINED CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 14)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(ENABLE_SDBUSCPP)
  set(CMAKE_CXX_STANDARD 17)
endif()

message("-- Selecting C++${CMAKE_CXX_STANDARD} standard")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")

message("-- Checking to see if CXX compiler supports C++${CMAKE_CXX_STANDARD}")

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("int main() {
                            return 0;
                          }"
                          HAS_CXX_STANDARD)

if(NOT HAS_CXX_STANDARD)
  message(FATAL_ERROR "Your C++ compiler does not support C++${CMAKE_CXX_STANDARD}")
else()
  message("-- Your C++ compiler supports C++${CMAKE_CXX_STANDARD}")
endif()
