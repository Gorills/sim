cmake_minimum_required(VERSION 3.22)

project(sim
    VERSION 0.1.0
    DESCRIPTION "Godot-independent world simulation with a GDExtension visualizer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(SIM_BUILD_TESTS "Build sim_core tests" ON)
option(SIM_BUILD_CLI "Build the headless simulation runner" ON)
option(SIM_FETCH_GODOT_CPP "Download godot-cpp during configure so the GDExtension can build" OFF)
option(SIM_ENABLE_SANITIZERS "ASan+UBSan on sim_core, tests, and CLI (do not load into Godot)" OFF)
option(SIM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(SIM_GODOT_CPP_TAG "godot-4.5-stable" CACHE STRING "godot-cpp git tag when fetching or documenting the pin")
set(SIM_GODOT_CPP_DIR "${CMAKE_SOURCE_DIR}/third_party/godot-cpp" CACHE PATH "In-tree godot-cpp source")

include("${CMAKE_SOURCE_DIR}/cmake/SimWarnings.cmake")

add_subdirectory(sim_core)

if(SIM_BUILD_CLI)
    add_subdirectory(sim_cli)
endif()

if(SIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(sim_core/tests)
endif()

set(SIM_HAVE_GODOT_CPP OFF)
if(EXISTS "${SIM_GODOT_CPP_DIR}/CMakeLists.txt")
    set(SIM_HAVE_GODOT_CPP ON)
    add_subdirectory("${SIM_GODOT_CPP_DIR}" "${CMAKE_BINARY_DIR}/godot-cpp" SYSTEM)
elseif(SIM_FETCH_GODOT_CPP)
    include(FetchContent)
    FetchContent_Declare(
        godot-cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG ${SIM_GODOT_CPP_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(godot-cpp)
    set(SIM_HAVE_GODOT_CPP ON)
endif()

if(SIM_HAVE_GODOT_CPP)
    if(SIM_ENABLE_SANITIZERS)
        message(WARNING "SIM_ENABLE_SANITIZERS is ON while building the GDExtension. "
                        "Do not load a sanitized .so into a stock Godot editor.")
    endif()
    add_subdirectory(sim_godot)
else()
    message(STATUS "godot-cpp not found; skipping GDExtension. "
                   "Clone it with scripts/fetch_godot_cpp.sh or configure -DSIM_FETCH_GODOT_CPP=ON.")
endif()
