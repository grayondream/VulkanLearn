cmake_minimum_required(VERSION 3.28)
project(vulkan-main)
set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_subdirectory(src)