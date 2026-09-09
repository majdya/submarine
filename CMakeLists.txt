cmake_minimum_required(VERSION 3.16)
project(submarine LANGUAGES C CXX)

enable_testing()

add_subdirectory(central-computer)
add_subdirectory(ground_station)
