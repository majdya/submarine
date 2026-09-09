cmake_minimum_required(VERSION 3.16)

add_library(ground_station_lib STATIC
  src/tcp_client.cpp
  src/ground_station_cli.cpp
  src/http_server.cpp
  src/gs_dashboard_api.cpp
)
target_include_directories(ground_station_lib PUBLIC include)
# Set explicitly on this target rather than relying on a parent scope's
# CMAKE_CXX_STANDARD - json_util.h uses C++17 nested-namespace syntax
# (namespace ground_station::json { ... }), so this target must not fall
# back to the compiler's default C++ standard (MSVC's default predates
# nested-namespace support and fails with C2429).
set_target_properties(ground_station_lib PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

find_package(Threads REQUIRED)
target_link_libraries(ground_station_lib PUBLIC Threads::Threads)

add_executable(ground_station src/main.cpp)
target_link_libraries(ground_station PRIVATE ground_station_lib)
set_target_properties(ground_station PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# Ships the read-only web dashboard's static page next to the built exe,
# regardless of the build generator's output layout - same pattern as
# central-computer's own CMakeLists.txt for web/dashboard.html.
add_custom_command(TARGET ground_station POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/web"
          "$<TARGET_FILE_DIR:ground_station>/web"
  COMMENT "Copying web/dashboard.html next to the ground_station executable"
)
