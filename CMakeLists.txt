cmake_minimum_required(VERSION 3.10)
project(telebit_telex LANGUAGES CXX)

# Most install paths (install.sh, manual `cmake -B build .`) never pass
# -DCMAKE_BUILD_TYPE, which for single-config generators means no
# optimization flags at all. Default to Release unless the caller opted
# into something else.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(telebit_telex_core
    vietnamese.cpp
    rime_table.cpp
    canonicalize.cpp
    render_utf8.cpp
    engine.cpp
)

target_include_directories(telebit_telex_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(telebit_telex_tests
    tests.cpp
)

target_link_libraries(telebit_telex_tests PRIVATE telebit_telex_core)

