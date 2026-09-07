add_library(sim_godot SHARED
    src/register_types.cpp
    src/sim_entity_state.cpp
    src/sim_habitat_grid.cpp
    src/sim_snapshot.cpp
    src/sim_world.cpp
)

target_include_directories(sim_godot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(sim_godot PRIVATE sim::core godot-cpp)
target_compile_features(sim_godot PRIVATE cxx_std_20)

# godot-cpp headers and GDCLASS macros are not warning-clean under -Wpedantic.
if(MSVC)
    target_compile_options(sim_godot PRIVATE /W3)
else()
    target_compile_options(sim_godot PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
if(NOT GODOTCPP_SUFFIX)
    set(GODOTCPP_SUFFIX "")
endif()

set(_sim_godot_lib_prefix "lib")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_sim_godot_lib_prefix "")
endif()

set_target_properties(sim_godot PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${_sim_godot_lib_prefix}sim_godot${GODOTCPP_SUFFIX}"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/godot_ext>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/godot_ext>"
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/godot_ext>"
)

set(_godot_bin "${CMAKE_SOURCE_DIR}/godot/bin")
add_custom_command(
    TARGET sim_godot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_godot_bin}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:sim_godot>" "${_godot_bin}/"
    COMMENT "Copying GDExtension library into godot/bin"
)
