cmake_minimum_required(VERSION 3.16)

add_library(ground_station_lib STATIC
  src/tcp_client.cpp
  src/ground_station_cli.cpp
)
target_include_directories(ground_station_lib PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(ground_station_lib PUBLIC Threads::Threads)

add_executable(ground_station src/main.cpp)
target_link_libraries(ground_station PRIVATE ground_station_lib)
