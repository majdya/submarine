cmake_minimum_required(VERSION 3.16)
project(central_computer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# --- protocol: the LNC firmware's own wire-format source, unmodified -------
# See include/protocol_bridge.h for why these files are consumed directly
# from the firmware tree rather than copied: it keeps exactly one
# implementation of the TLV/frame protocol in the whole system.
set(FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../submarine-final-project"
    CACHE PATH "Path to submarine-final-project (for the shared protocol sources)")

add_library(protocol STATIC
  ${FIRMWARE_DIR}/Core/Src/app/tlv.c
  ${FIRMWARE_DIR}/Core/Src/app/comm_frame.c
)
target_include_directories(protocol PUBLIC ${FIRMWARE_DIR}/Core/Inc/app)

# --- central_computer_lib: everything except main() ------------------------
add_library(central_computer_lib STATIC
  src/protocol_bridge.cpp
  src/transport/serial_transport.cpp
  src/comm_link.cpp
  src/log_module.cpp
  src/data_store.cpp
  src/data_collection.cpp
  src/management_command.cpp
  src/central_computer.cpp
  src/submarine.cpp
  src/research_submarine.cpp
  src/combat_submarine.cpp
  src/fleet.cpp
  src/menu.cpp
)
target_include_directories(central_computer_lib PUBLIC include)
target_link_libraries(central_computer_lib PUBLIC protocol)

find_package(Threads REQUIRED)
target_link_libraries(central_computer_lib PUBLIC Threads::Threads)

# --- main app ---------------------------------------------------------------
add_executable(central_computer src/main.cpp)
target_link_libraries(central_computer PRIVATE central_computer_lib)

# --- tests -------------------------------------------------------------------
enable_testing()
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp)
foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE central_computer_lib)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
