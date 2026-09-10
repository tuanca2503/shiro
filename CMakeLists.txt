cmake_minimum_required(VERSION 3.14...3.28)
project("shiro" C CXX)
include(CheckIncludeFileCXX)

#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#
add_subdirectory(vendor/llama.cpp)
add_subdirectory(engine)
add_subdirectory(daemon)