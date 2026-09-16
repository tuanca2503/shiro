cmake_minimum_required(VERSION 3.14...3.28)
project("shiro" LANGUAGES CXX)
include(CheckIncludeFileCXX)

#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#
# git submodule add https://github.com/ggml-org/llama.cpp.git vendor/llama.cpp
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/llama.cpp)

# git submodule add https://github.com/nlohmann/json.git third_party/nlohmann_json
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(vendor/nlohmann_json)

# --- engine: static library boc quanh llama.cpp + minja ---
add_subdirectory(engine)
# --- daemon: executable chinh, chi biet engine.h, khong biet llama.h ---
add_subdirectory(server)